from fastapi import FastAPI, BackgroundTasks, UploadFile, File, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from typing import Dict
import os
import uuid
import wave
import asyncio

import logging

logging.basicConfig(level=logging.DEBUG)

app = FastAPI()

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

@app.post("/sensor_data")
async def receive_sensor_data(data: SensorData, background_tasks: BackgroundTasks):
    background_tasks.add_task(print_sensor_data, data)
    return {"status": "success", "message": "Sensor data received and processed"}

async def save_and_convert_audio_file(file: UploadFile):
    os.makedirs("audio_files", exist_ok=True)
    raw_file_name = f"audio_{uuid.uuid4()}.raw"
    wav_file_name = f"audio_{uuid.uuid4()}.wav"
    raw_file_path = os.path.join("audio_files", raw_file_name)
    wav_file_path = os.path.join("audio_files", wav_file_name)

    print(f"Saving RAW file to: {raw_file_path}")

    with open(raw_file_path, "wb") as buffer:
        content = await file.read()
        buffer.write(content)

    print(f"RAW file saved. Size: {len(content)} bytes")

    # RAWからWAVに変換
    print(f"Converting to WAV: {wav_file_path}")
    with open(raw_file_path, "rb") as raw_file:
        raw_data = raw_file.read()

    # WAVファイルのパラメータ設定
    n_channels = 1
    sample_width = 2  # 16-bit
    frame_rate = 16000  # M5StickC Plusの設定に合わせる
    n_frames = len(raw_data) // (n_channels * sample_width)

    with wave.open(wav_file_path, "wb") as wav_file:
        wav_file.setnchannels(n_channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(frame_rate)
        wav_file.setnframes(n_frames)
        wav_file.writeframes(raw_data)

    print(f"WAV file saved: {wav_file_path}")

    # RAWファイルを削除（オプション）
    os.remove(raw_file_path)
    print(f"RAW file deleted: {raw_file_path}")

    return wav_file_path

def raw_to_wav(raw_file_path, wav_file_path, channels=1, sample_width=2, frame_rate=16000):
    with open(raw_file_path, 'rb') as raw_file:
        raw_data = raw_file.read()

    with wave.open(wav_file_path, 'wb') as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(frame_rate)
        wav_file.writeframes(raw_data)

@app.post("/recording")
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

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=10000)
