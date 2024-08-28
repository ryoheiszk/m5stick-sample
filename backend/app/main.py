from fastapi import FastAPI, BackgroundTasks, Request, APIRouter, Depends, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from typing import Dict, List
import os
import uuid
import wave
import logging
from sqlalchemy import create_engine, Column, Integer, String
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session

logging.basicConfig(level=logging.DEBUG)

app = FastAPI(
    title="Your API",
    description="API description",
    version="1.0.0",
    docs_url="/api/docs",
    openapi_url="/api/openapi.json"
)

# APIRouterのインスタンスを作成
api_router = APIRouter()

# データベース接続設定
SQLALCHEMY_DATABASE_URL = "mysql+pymysql://root:password@mysql:3306/mydatabase"
engine = create_engine(SQLALCHEMY_DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()

# Itemモデルの定義
class Item(Base):
    __tablename__ = "items"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(255), index=True)

# Pydanticモデル
class ItemBase(BaseModel):
    id: int
    name: str

# データベースセッションを取得する関数
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

class SensorData(BaseModel):
    device_id: str
    button_press_duration: int
    initial_gyro: Dict[str, float]
    final_gyro: Dict[str, float]
    accelerometer: Dict[str, float]
    gyroscope: Dict[str, float]
    temperature: float
    battery: Dict[str, float]
    screen_rotation: int
    rtc_time: str
    wifi_rssi: int

def print_sensor_data(data: SensorData):
    print("--------------------")
    print(f"端末ID: {data.device_id}")
    print(f"RTC時間: {data.rtc_time}")
    print(f"長押し時間: {data.button_press_duration} ms")
    print(f"ボタンを押したときのジャイロ: X={data.initial_gyro['x']:.2f}, Y={data.initial_gyro['y']:.2f}, Z={data.initial_gyro['z']:.2f}")
    print(f"ボタンを離したときのジャイロ: X={data.final_gyro['x']:.2f}, Y={data.final_gyro['y']:.2f}, Z={data.final_gyro['z']:.2f}")
    print(f"データを送信する時のジャイロ: X={data.gyroscope['x']:.2f}, Y={data.gyroscope['y']:.2f}, Z={data.gyroscope['z']:.2f}")
    print(f"加速度: X={data.accelerometer['x']:.2f}, Y={data.accelerometer['y']:.2f}, Z={data.accelerometer['z']:.2f}")
    print(f"端末温度: {data.temperature:.2f}°C")
    print(f"バッテリー状態={data.battery['voltage']:.2f}V, Percentage={data.battery['percentage']*100:.2f}%")
    print(f"Wi-Fi強度: {data.wifi_rssi} dBm")
    print("--------------------")

@api_router.post("/sensor_data")
async def receive_sensor_data(data: SensorData, background_tasks: BackgroundTasks):
    background_tasks.add_task(print_sensor_data, data)
    return {"status": "success", "message": "Sensor data received and processed"}

def raw_to_wav(raw_file_path, wav_file_path, channels=1, sample_width=2, frame_rate=16000):
    with open(raw_file_path, 'rb') as raw_file:
        raw_data = raw_file.read()

    with wave.open(wav_file_path, 'wb') as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(frame_rate)
        wav_file.writeframes(raw_data)

@api_router.post("/recording")
async def receive_audio(request: Request):
    try:
        content = await request.body()
        raw_file_name = f"audio_{uuid.uuid4()}.raw"
        wav_file_name = f"audio_{uuid.uuid4()}.wav"
        raw_file_path = os.path.join("audio_files", raw_file_name)
        wav_file_path = os.path.join("audio_files", wav_file_name)

        os.makedirs("audio_files", exist_ok=True)

        # Save RAW file
        with open(raw_file_path, "wb") as file:
            file.write(content)

        # Convert RAW to WAV
        raw_to_wav(raw_file_path, wav_file_path)

        # Optionally, remove the RAW file after conversion
        os.remove(raw_file_path)

        return JSONResponse(content={
            "status": "success",
            "message": "Audio file received, converted to WAV, and saved",
            "file_name": wav_file_name,
            "file_size": len(content)
        })
    except Exception as e:
        return JSONResponse(
            status_code=500,
            content={"status": "error", "message": str(e)}
        )

@api_router.get("/items", response_model=List[ItemBase])
def read_items(db: Session = Depends(get_db)):
    items = db.query(Item).all()
    return items

@api_router.get("/items/{item_id}", response_model=ItemBase)
def read_item(item_id: int, db: Session = Depends(get_db)):
    item = db.query(Item).filter(Item.id == item_id).first()
    if item is None:
        raise HTTPException(status_code=404, detail="Item not found")
    return item

@api_router.get("/")
def read_root():
    return {"Hello": "World"}

# アプリケーションの初期化時点でルーターをインクルード
app.include_router(api_router, prefix="/api")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
