#include <M5StickCPlus.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "BUFFALO_WSR-2.4G";
const char* password = "Your Wifi Password";
const char* serverUrl = "http://192.168.1.3:10000/recording";

#define I2S_WS 0
#define I2S_SD 34
#define I2S_SCK 26
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE (16000)
#define I2S_SAMPLE_BITS (32)
#define RECORD_TIME (2) // 1秒間の録音

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    M5.Lcd.println("Connecting to WiFi...");
  }
  M5.Lcd.println("Connected to WiFi");

  // I2S設定
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // I2Sドライバのインストールと開始
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    M5.Lcd.printf("Failed to install I2S driver: %d\n", err);
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    M5.Lcd.printf("Failed to set I2S pins: %d\n", err);
    return;
  }

  err = i2s_set_clk(I2S_PORT, I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
  if (err != ESP_OK) {
    M5.Lcd.printf("Failed to set I2S clock: %d\n", err);
    return;
  }

  M5.Lcd.println("I2S initialized");
  M5.Lcd.println("Press button A to record");
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    recordAndSend();
  }
}

void recordAndSend() {
  M5.Lcd.fillScreen(RED);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Recording...");

  size_t bytesRead = 0;
  size_t bufferSize = I2S_SAMPLE_RATE * (I2S_SAMPLE_BITS / 8) * RECORD_TIME;
  uint8_t* audioBuffer = (uint8_t*)malloc(bufferSize);

  if (audioBuffer == NULL) {
    M5.Lcd.println("Failed to allocate audio buffer");
    return;
  }

  // データ読み取り前にバッファをクリア
  memset(audioBuffer, 0, bufferSize);

  // I2Sからのデータ読み取り
  esp_err_t err = i2s_read(I2S_PORT, audioBuffer, bufferSize, &bytesRead, portMAX_DELAY);
  if (err != ESP_OK) {
    M5.Lcd.printf("Failed to read I2S data: %d\n", err);
    free(audioBuffer);
    return;
  }

  M5.Lcd.println("Recording complete");
  M5.Lcd.printf("Recorded %d bytes\n", bytesRead);

  // 音声データの簡単な解析（デバッグ用）
  int32_t* samples = (int32_t*)audioBuffer;
  int numSamples = bytesRead / 4;  // 32ビットサンプルなので4で割る
  int32_t maxSample = 0;
  int32_t minSample = 0;
  int32_t sum = 0;
  for (int i = 0; i < numSamples; i++) {
    if (samples[i] > maxSample) maxSample = samples[i];
    if (samples[i] < minSample) minSample = samples[i];
    sum += abs(samples[i]);
  }
  float average = (float)sum / numSamples;

  M5.Lcd.printf("Max: %d\n", maxSample);
  M5.Lcd.printf("Min: %d\n", minSample);
  M5.Lcd.printf("Avg: %.2f\n", average);

  sendAudioToServer(audioBuffer, bytesRead);

  free(audioBuffer);
}

void sendAudioToServer(uint8_t* buffer, size_t bufferSize) {
  M5.Lcd.println("Sending to server...");

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/octet-stream");

  int httpResponseCode = http.POST(buffer, bufferSize);

  if (httpResponseCode > 0) {
    M5.Lcd.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    M5.Lcd.println("Response: " + response);
  } else {
    M5.Lcd.printf("Error code: %d\n", httpResponseCode);
  }

  http.end();
}
