/*
  ESP32-S3 I2C scanner for MPU6050 wiring/debug.

  Upload this when the GUI connects but reports MPU NOT FOUND. Open Arduino
  Serial Monitor at 115200 baud. The sketch scans several possible SDA/SCL pin
  pairs and reports any I2C devices it finds.

  Move the MPU6050 SDA/SCL wires to match a pair, reset the ESP32, and watch
  the output. An MPU6050 usually appears at address 0x68 or 0x69.
*/

#include <Arduino.h>
#include <Wire.h>

struct I2cPair {
  int sda;
  int scl;
  const char *label;
};

I2cPair pairs[] = {
  {8, 9, "current firmware default"},
  {1, 2, "alternate low-number GPIO pair"},
  {6, 7, "alternate GPIO pair"},
  {15, 16, "alternate GPIO pair"},
  {17, 18, "alternate GPIO pair"},
};

void scanPair(const I2cPair &pair) {
  Serial.println();
  Serial.print("Scanning SDA GPIO ");
  Serial.print(pair.sda);
  Serial.print(" / SCL GPIO ");
  Serial.print(pair.scl);
  Serial.print("  (");
  Serial.print(pair.label);
  Serial.println(")");

  Wire.end();
  Wire.begin(pair.sda, pair.scl);
  Wire.setClock(100000);
  delay(100);

  int found = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      found++;
      Serial.print("  Found I2C device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      if (address == 0x68 || address == 0x69) Serial.print("  <-- likely MPU6050");
      Serial.println();
    }
  }

  if (found == 0) Serial.println("  No I2C devices found on this pair.");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("ESP32-S3 I2C scanner starting.");
  Serial.println("MPU6050 expected address: 0x68 or 0x69.");
}

void loop() {
  for (const I2cPair &pair : pairs) {
    scanPair(pair);
    delay(500);
  }
  Serial.println();
  Serial.println("Scan cycle complete. Repeating in 5 seconds.");
  delay(5000);
}
