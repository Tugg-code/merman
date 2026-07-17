/*
  Arduino Uno servo sweep test.

  Use this to prove the servo, servo power supply, and signal wire are good
  before debugging the ESP32-S3 signal path.

  Wiring:
    Servo signal  -> Arduino Uno D9
    Servo red     -> external servo supply positive, usually 5-6 V
    Servo ground  -> external servo supply negative
    Servo supply negative -> Arduino Uno GND

  Do not power a large servo from the Uno 5 V pin.
*/

#include <Servo.h>

const int SERVO_PIN = 9;

Servo testServo;

void setup() {
  Serial.begin(115200);
  delay(1000);

  testServo.attach(SERVO_PIN);

  Serial.println();
  Serial.println("Arduino Uno servo sweep test started.");
  Serial.println("Expected movement: 0 deg, 90 deg, 180 deg, repeat.");
}

void loop() {
  Serial.println("0 deg");
  testServo.write(0);
  delay(1500);

  Serial.println("90 deg");
  testServo.write(90);
  delay(1500);

  Serial.println("180 deg");
  testServo.write(180);
  delay(1500);
}
