/*
  ESP32-S3 SN74AHCT125 signal probe.

  This is not a servo PWM test. It slowly toggles GPIO 4 HIGH/LOW so a normal
  multimeter can verify the buffer wiring.

  Wiring for channel 1:
    SN74AHCT125 pin 14 VCC -> 5 V
    SN74AHCT125 pin 7  GND -> common ground
    SN74AHCT125 pin 1  1OE -> common ground
    SN74AHCT125 pin 2  1A  -> ESP32 GPIO 4
    SN74AHCT125 pin 3  1Y  -> probe/output to servo signal later

  Meter checks:
    Pin 14 to pin 7: about 5 V
    Pin 1 to pin 7:  about 0 V
    Pin 2 to pin 7:  alternates about 0 V / 3.3 V
    Pin 3 to pin 7:  alternates about 0 V / 5 V
*/

#include <Arduino.h>

const int TEST_PIN = 4;

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(TEST_PIN, OUTPUT);

  Serial.println();
  Serial.println("ESP32-S3 AHCT125 signal probe starting.");
  Serial.println("GPIO 4 will toggle LOW/HIGH every 2 seconds.");
  Serial.println("Measure AHCT pin 2 and pin 3 relative to pin 7/GND.");
}

void loop() {
  Serial.println("GPIO 4 LOW: expect pin 2 ~0V and pin 3 ~0V");
  digitalWrite(TEST_PIN, LOW);
  delay(2000);

  Serial.println("GPIO 4 HIGH: expect pin 2 ~3.3V and pin 3 ~5V");
  digitalWrite(TEST_PIN, HIGH);
  delay(2000);
}
