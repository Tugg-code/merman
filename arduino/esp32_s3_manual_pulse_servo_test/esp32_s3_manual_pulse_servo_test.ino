/*
  ESP32-S3 manual servo pulse test.

  This avoids ESP32 LEDC/PWM entirely. It manually creates classic RC servo
  pulses:

    HIGH for 1000/1500/2000 microseconds
    LOW for the rest of a 20 ms frame

  Use this after verifying the SN74AHCT125 output can toggle 0V/5V.

  Wiring:
    ESP32 GPIO 4 -> SN74AHCT125 pin 2, 1A
    SN74AHCT125 pin 3, 1Y -> servo yellow signal
    SN74AHCT125 pin 1, 1OE -> GND
    SN74AHCT125 pin 7, GND -> common ground
    SN74AHCT125 pin 14, VCC -> 5V

    Servo red -> servo power positive
    Servo brown/black -> servo power negative/common ground

  Serial Monitor:
    115200 baud
*/

#include <Arduino.h>

const int SERVO_PIN = 4;
const int FRAME_US = 20000;

void sendServoPulse(int pulseUs, int frames) {
  for (int i = 0; i < frames; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseUs);
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(FRAME_US - pulseUs);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);

  Serial.println();
  Serial.println("ESP32-S3 manual pulse servo test starting.");
  Serial.println("No LEDC/PWM library is used.");
  Serial.println("Expected movement: 1000 us, 1500 us, 2000 us, repeat.");
}

void loop() {
  Serial.println("1000 us for 2 seconds");
  sendServoPulse(1000, 100);

  Serial.println("1500 us for 2 seconds");
  sendServoPulse(1500, 100);

  Serial.println("2000 us for 2 seconds");
  sendServoPulse(2000, 100);

  Serial.println("1500 us for 2 seconds");
  sendServoPulse(1500, 100);
}
