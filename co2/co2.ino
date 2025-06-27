#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Replace with your network credentials
const char* ssid = "<internet>";
const char* password = "<password>";
const char* serverUrl = "http://10.0.0.32:5000/api/sensor_json";

#define K30_RX 19  // ESP32 receives on GPIO 12 (connect to K-30 TX)
#define K30_TX 26  // ESP32 transmits on GPIO 13 (connect to K-30 RX)

byte readCO2[] = { 0xFE, 0x44, 0x00, 0x08, 0x02, 0x9F, 0x25 };  // Command to read CO₂
byte response[] = { 0, 0, 0, 0, 0, 0, 0 };                      // Response buffer

int valMultiplier = 1;  // Use 3 for K-30 3%, 10 for K-33 ICB

const long utcOffsetInSeconds = 0; // adjust for your timezone
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  Serial.begin(9600);  // Debug output
  delay(1000);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Start CO₂ sensor communication
  Serial2.begin(9600, SERIAL_8N1, K30_RX, K30_TX);
  Serial.println("ESP32: K-Series CO₂ sensor ready on GPIO 19/26");

  timeClient.begin();
  timeClient.update();
}


void loop() {
  timeClient.update();
  unsigned long unixTime = timeClient.getEpochTime();
  sendRequest(readCO2);
  unsigned long valCO2 = getValue(response);
  Serial.print("CO₂ ppm = ");
  Serial.println(valCO2);
  sendCO2ToServer(valCO2, unixTime);
  delay(2000);
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
      break;
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
  unsigned long val = high * 256 + low;
  return val * valMultiplier;
}

void sendCO2ToServer(unsigned long co2ppm, unsigned long unixTime) {
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["type"] = "co2";
    doc["loggerId"] = 1;
    doc["cellId"] = 1;
    doc["ts"] = unixTime;

    JsonObject data = doc.createNestedObject("data");
    data["ppm"] = co2ppm;

    JsonObject dtype = doc.createNestedObject("data_type");
    dtype["ppm"] = "int";

    String payload;
    serializeJson(doc, payload);
    int responseCode = http.POST(payload);
    Serial.print("Server responded with code: ");
    Serial.println(responseCode);
    Serial.println("Payload: " + payload);
    http.end();

  }else{
    Serial.println("[Error] WiFi not connected");
  }
}