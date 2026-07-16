/*
  ESP32-S3 upload smoke test.

  Use this before the fish finder stabilizer sketch. It does not need the
  servo, joystick, or MPU6050 connected. It only proves that Arduino IDE can
  compile, upload, reset the board, and open serial telemetry.
*/

#include <Arduino.h>

#ifndef LED_BUILTIN
const int LED_PIN = -1;
#else
const int LED_PIN = LED_BUILTIN;
#endif

unsigned long lastPrintMs = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (LED_PIN >= 0) {
    pinMode(LED_PIN, OUTPUT);
  }

  Serial.println();
  Serial.println("ESP32-S3 smoke test started.");
  Serial.println("If you can read this, upload and USB serial are working.");
}

void loop() {
  unsigned long now = millis();

  if (LED_PIN >= 0 && now - lastPrintMs >= 500) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  if (now - lastPrintMs >= 1000) {
    lastPrintMs = now;
    Serial.print("millis=");
    Serial.println(now);
  }
}
