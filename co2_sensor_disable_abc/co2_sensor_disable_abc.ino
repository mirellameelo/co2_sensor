#define RX 26  // ESP32 RX: receives from K30 TX
#define TX 14  // ESP32 TX: sends to K30 RX

void setup() {
  Serial.begin(9600);  // For debug output
  Serial2.begin(9600, SERIAL_8N1, RX, TX);  // Start UART to K30

  Serial.println("Starting...");
  delay(2000);  // Optional small delay before sending

  disableABC();
}

void loop() {
  // Nothing here
}

void disableABC() {
  byte disableABCcommand[] = { 0xFE, 0x06, 0x00, 0x1F, 0x00, 0x00, 0xAC, 0x03 };
  Serial.println("Disabling ABC...");
  delay(200);
  Serial2.write(disableABCcommand, sizeof(disableABCcommand));
  //delay(3000);  // Wait for response

  // Read response
  byte reply[8];
  int i = 0;
  unsigned long start = millis();
  while (i < 8 && millis() - start < 2000) {
    if (Serial2.available()) {
      reply[i++] = Serial2.read();
    }
  }

  // Validate reply
  if (i == 8 &&
      reply[0] == 0xFE && reply[1] == 0x06 &&
      reply[2] == 0x00 && reply[3] == 0x1F) {
    Serial.println(" ABC disabled successfully.");
  } else {
    Serial.print(" ABC disable failed. Got ");
    for (int j = 0; j < i; j++) {
      Serial.print("0x");
      if (reply[j] < 16) Serial.print("0");
      Serial.print(reply[j], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
