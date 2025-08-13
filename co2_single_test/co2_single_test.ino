#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define RX 26
#define TX 14
#define SWITCH_PIN 23
#define PHOTORESP_PIN 33

const char* ssid1 = "CITRIS";
const char* password1 = "cores2025";

const char* serverUrl;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

byte readCO2[] = { 0xFE, 0x44, 0x00, 0x08, 0x02, 0x9F, 0x25 };
byte response[7];

bool fastMode;
unsigned long lastSendTime = 0;

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid1);
  WiFi.begin(ssid1, password1);
  delay(5000);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi.");
    serverUrl = "http://dirtviz.jlab.ucsc.edu/api/sensor_json/";
    //serverUrl = "http://10.0.0.33:5000/api/sensor_json/";
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    serverUrl = NULL;
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(PHOTORESP_PIN, INPUT);

  // Set initial mode based on physical pin state
  fastMode = (digitalRead(SWITCH_PIN) == HIGH);  // fastMode if switch is HIGH

  connectToWiFi();

  Serial2.begin(9600, SERIAL_8N1, RX, TX);
  delay(200);

  timeClient.begin();
  timeClient.update();

  Serial.println("Setup complete.");
}

void loop() {
  int switchState = digitalRead(SWITCH_PIN);
  fastMode = (switchState == LOW);  // fast mode when switch is LOW

  unsigned long now = millis();
  unsigned long interval = fastMode ? 20000 : 300000;  // 20s or 5min

  if (now - lastSendTime >= interval) {
    lastSendTime = now;

    if (fastMode) {
      sendRequest(readCO2);
      unsigned long valCO2 = getValue(response);
      timeClient.update();
      unsigned long unixTime = timeClient.getEpochTime();

      Serial.print("[FAST MODE] CO₂ reading: ");
      Serial.println(valCO2);

      sendCO2ToServer(valCO2, unixTime);
    } else {
      unsigned long co2Sum = 0;
      int validReadings = 0;

      Serial.println("[NORMAL MODE] Starting 10 CO₂ samples over 20 seconds...");

      for (int i = 0; i < 10; i++) {
        sendRequest(readCO2);
        unsigned long valCO2 = getValue(response);

        if (valCO2 > 0 && valCO2 < 10000) {
          co2Sum += valCO2;
          validReadings++;
          Serial.print("Reading ");
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.println(valCO2);
        } else {
          Serial.print("Invalid reading at ");
          Serial.println(i + 1);
        }

        delay(2000);
      }

      unsigned long co2Avg = (validReadings > 0) ? co2Sum / validReadings : 0;
      timeClient.update();
      unsigned long unixTime = timeClient.getEpochTime();

      Serial.print("[NORMAL MODE] CO₂ average: ");
      Serial.println(co2Avg);

      sendCO2ToServer(co2Avg, unixTime);
    }
  }
}

void sendRequest(byte packet[]) {
  while (!Serial2.available()) {
    Serial2.write(packet, 7);
    delay(50);
  }

  int timeout = 0;
  while (Serial2.available() < 7) {
    timeout++;
    if (timeout > 10) {
      while (Serial2.available()) Serial2.read();  // Flush
      return;
    }
    delay(50);
  }

  for (int i = 0; i < 7; i++) {
    response[i] = Serial2.read();
  }
}

unsigned long getValue(byte packet[]) {
  int high = packet[3];
  int low = packet[4];
  return (unsigned long)(high * 256 + low);
}

void sendCO2ToServer(unsigned long co2ppm, unsigned long unixTime) {
  if (WiFi.status() == WL_CONNECTED) {
    int state = fastMode ? 1 : 0;  // reflect mode in payload
    int photoRaw = analogRead(PHOTORESP_PIN);
    float Vout = photoRaw * (3.3 / 4095.0);

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-CO2Client/1.0");

    StaticJsonDocument<256> doc;
    doc["type"] = "co2";
    doc["loggerId"] = 200;
    doc["cellId"] = 1315;
    doc["ts"] = unixTime;

    JsonObject data = doc.createNestedObject("data");
    data["CO2"] = float(co2ppm);
    data["Photoresistivity"] = Vout;
    data["state"] = state;

    String payload;
    serializeJson(doc, payload);
    int responseCode = http.POST(payload);
    http.end();

    Serial.println("Payload sent:");
    Serial.println(payload);
    Serial.print("Response code: ");
    Serial.println(responseCode);
  } else {
    Serial.println("[Error] WiFi not connected");
  }
}
