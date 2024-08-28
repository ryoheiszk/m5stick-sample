#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "BUFFALO_WSR-2.4G";
const char* password = "Your Wifi Password";
const char* serverName = "http://192.168.1.3:10000/sensor_data";

String deviceId = "";
float initialGyroX, initialGyroY, initialGyroZ;
float finalGyroX, finalGyroY, finalGyroZ;
unsigned long buttonPressStartTime = 0;

void setup() {
  M5.begin();
  M5.Imu.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  deviceId = String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("WiFi connected");
  M5.Lcd.println("IP: " + WiFi.localIP().toString());
  M5.Lcd.println("Device ID: " + deviceId);
  M5.Lcd.println("\nPress btn to send data");
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    buttonPressStartTime = millis();
    M5.Imu.getGyroData(&initialGyroX, &initialGyroY, &initialGyroZ);
  }

  if (M5.BtnA.wasReleased()) {
    unsigned long pressDuration = millis() - buttonPressStartTime;
    M5.Imu.getGyroData(&finalGyroX, &finalGyroY, &finalGyroZ);

    if (WiFi.status() == WL_CONNECTED) {
      sendSensorData(pressDuration);
    } else {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.println("WiFi Disconnected");
      M5.Lcd.println("Reconnecting...");
      WiFi.reconnect();
      delay(5000);  // Wait for reconnection
    }

    buttonPressStartTime = 0;

    delay(300);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Press btn to send data");
  }

  delay(10);
}

void sendSensorData(unsigned long pressDuration) {
  HTTPClient http;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Sending sensor data...");

  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<1024> doc;

  doc["device_id"] = deviceId;
  doc["button_press_duration"] = pressDuration;

  doc["initial_gyro"]["x"] = initialGyroX;
  doc["initial_gyro"]["y"] = initialGyroY;
  doc["initial_gyro"]["z"] = initialGyroZ;

  doc["final_gyro"]["x"] = finalGyroX;
  doc["final_gyro"]["y"] = finalGyroY;
  doc["final_gyro"]["z"] = finalGyroZ;

  float accX, accY, accZ;
  float gyroX, gyroY, gyroZ;
  float temp;
  M5.Imu.getAccelData(&accX, &accY, &accZ);
  M5.Imu.getGyroData(&gyroX, &gyroY, &gyroZ);
  M5.Imu.getTempData(&temp);

  doc["accelerometer"]["x"] = accX;
  doc["accelerometer"]["y"] = accY;
  doc["accelerometer"]["z"] = accZ;
  doc["gyroscope"]["x"] = gyroX;
  doc["gyroscope"]["y"] = gyroY;
  doc["gyroscope"]["z"] = gyroZ;
  doc["temperature"] = temp;

  float batVoltage = M5.Axp.GetBatVoltage();
  float batPercentage = M5.Axp.GetBatState();
  doc["battery"]["voltage"] = batVoltage;
  doc["battery"]["percentage"] = batPercentage;

  doc["screen_rotation"] = M5.Lcd.getRotation();

  RTC_TimeTypeDef RTCtime;
  M5.Rtc.GetTime(&RTCtime);
  doc["rtc_time"] = String(RTCtime.Hours) + ":" + String(RTCtime.Minutes) + ":" + String(RTCtime.Seconds);

  doc["wifi_rssi"] = WiFi.RSSI();

  String jsonString;
  serializeJson(doc, jsonString);

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    String payload = http.getString();

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Response:");
    M5.Lcd.println(payload);
  } else {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Error on sending");
    M5.Lcd.println("HTTP Response code: " + String(httpResponseCode));
  }

  http.end();
}
