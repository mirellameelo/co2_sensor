#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>


const char* ssid1 = "HARE_Lab";
const char* password1 = "";

const char* ssid2 = "valleyview";
const char* password2 = "haveaniceday";

const char* serverUrl;

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid1);
  WiFi.begin(ssid1, password1);

  unsigned long startAttemptTime = millis();

  // Try for 10 seconds
  // while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
  //   delay(500);
  //   Serial.print(".");
  // }

  // if (WiFi.status() == WL_CONNECTED) {
  //   Serial.println("\nConnected to HARE_Lab.");
  //   serverUrl = "http://dirtviz.jlab.ucsc.edu/api/sensor_json/";
  //   return;
  // }

  // Serial.println("\nFailed to connect to HARE_Lab. Trying valleyview...");

  WiFi.begin(ssid2, password2);
  startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to valleyview.");
    serverUrl = "http://10.0.0.32:8000/api/sensor_json/";
  } else {
    Serial.println("\nFailed to connect to any WiFi.");
    serverUrl = NULL;
  }
}

#define RX 26  // ESP32 receives on GPIO 12 (connect to K-30 TX)
#define TX 14 // chenge this to 25 for room sensor  // ESP32 transmits on GPIO 13 (connect to K-30 RX)
#define SWITCH_PIN 23 // it marks the begining of measuments
#define PHOTORESP_PIN 33
const float R_fixed = 100000.0;

byte readCO2[] = { 0xFE, 0x44, 0x00, 0x08, 0x02, 0x9F, 0x25 };  // Command to read CO₂
byte response[] = { 0, 0, 0, 0, 0, 0, 0 };                      // Response buffer

int valMultiplier = 1;  // Use 3 for K-30 3%, 10 for K-33 ICB

const long utcOffsetInSeconds = 0; // adjust for your timezone
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  Serial.begin(9600);  // Debug output
  delay(1000);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(PHOTORESP_PIN, INPUT);
  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  connectToWiFi();

  if (serverUrl != NULL) {
    Serial.print("Using server URL: ");
    Serial.println(serverUrl);
  } else {
    Serial.println("No valid server URL configured.");
  }

  // Start CO₂ sensor communication
  Serial2.begin(9600, SERIAL_8N1, RX, TX);

  timeClient.begin();
  timeClient.update();
}


void loop() {
  int state = digitalRead(SWITCH_PIN);
  // if (state == LOW) {  // Button pressed pulls pin LOW
  //   Serial.println("Switch is pressed");
  // } else {
  //   Serial.println("Switch is not pressed");
  // }
  int photoRaw = analogRead(PHOTORESP_PIN);
  //Serial.print("Photoresistor ADC raw: ");
  //Serial.println(photoRaw);
  timeClient.update();
  unsigned long unixTime = timeClient.getEpochTime();
  sendRequest(readCO2);
  unsigned long valCO2 = getValue(response);
  Serial.print("CO₂ ppm = ");
  Serial.println(valCO2);
  sendCO2ToServer(valCO2, unixTime);
  delay(5000);
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

    int state = digitalRead(SWITCH_PIN);        // Button state
    int photoRaw = analogRead(PHOTORESP_PIN);
    float Vout = photoRaw * (3.3 / 4095.0);  // Convert ADC to voltage
    float Vin = 3.3;

    float R_photo = (Vout > 0.01) ? R_fixed * ((Vin - Vout) / Vout) : -1; // Avoid divide-by-zero

    // Serial.print("Photoresistor resistance (ohms): ");
    // Serial.println(R_photo);

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-CO2Client/1.0");

    StaticJsonDocument<256> doc;
    doc["type"] = "co2";
    doc["loggerId"] = 200;
    doc["cellId"] = 2;
    doc["ts"] = unixTime;

    JsonObject data = doc.createNestedObject("data");
    data["CO2"] = float(co2ppm);
    data["Photoresistivity"] = R_photo;
    data["state"] = state;

    String payload;
    serializeJson(doc, payload);
    int responseCode = http.POST(payload);
    //Serial.print("Server responded with code: ");
    //Serial.println(responseCode);
    //Serial.println("Payload: " + payload);
    http.end();

  } else {
    Serial.println("[Error] WiFi not connected");
  }
}
