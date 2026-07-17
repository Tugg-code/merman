/*
  ESP32-S3 servo sweep test using the ESP32Servo library.

  Use this if the hand-written LEDC servo tests do not move the servo.

  Arduino IDE library needed:
    Tools/Library Manager -> search "ESP32Servo" -> install "ESP32Servo"

  Wiring:
    Servo signal  -> ESP32 GPIO 4 by default
    Servo red     -> external servo supply positive
    Servo ground  -> external servo supply negative
    Servo supply negative -> ESP32 GND

  If this still does not move the servo, but the Arduino Uno servo sweep does,
  the servo likely needs a 5 V logic signal or a different ESP32 GPIO.
*/

#include <Arduino.h>
#include <ESP32Servo.h>

const int SERVO_PIN = 4;
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2500;

Servo testServo;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("ESP32-S3 ESP32Servo library sweep test started.");
  Serial.print("Servo signal GPIO ");
  Serial.println(SERVO_PIN);

  ESP32PWM::allocateTimer(0);
  testServo.setPeriodHertz(50);
  testServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
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
