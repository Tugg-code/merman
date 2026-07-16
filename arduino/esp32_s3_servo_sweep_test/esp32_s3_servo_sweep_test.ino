/*
  ESP32-S3 servo sweep test for DS3245 / hobby servo debugging.

  Upload this before debugging the full stabilizer sketch. It proves the ESP32
  GPIO, servo signal wire, shared ground, and servo power are all working.

  Wiring:
    Servo signal  -> GPIO 4 by default
    Servo red     -> external 5-6 V servo supply positive
    Servo ground  -> external servo supply negative
    Servo supply negative -> ESP32 GND
*/

#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

const int SERVO_PIN = 4;
const int SERVO_PWM_HZ = 50;
const int SERVO_PWM_BITS = 16;
const int SERVO_PWM_CHANNEL = 0; // Used by Arduino-ESP32 2.x only.

void setupServoPwm() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SERVO_PIN, SERVO_PWM_HZ, SERVO_PWM_BITS);
#else
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_HZ, SERVO_PWM_BITS);
  ledcAttachPin(SERVO_PIN, SERVO_PWM_CHANNEL);
#endif
}

void writeServoUs(int pulseUs) {
  const uint32_t maxDuty = (1UL << SERVO_PWM_BITS) - 1;
  const uint32_t periodUs = 1000000UL / SERVO_PWM_HZ;
  uint32_t duty = (uint32_t)((pulseUs * (uint64_t)maxDuty) / periodUs);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(SERVO_PWM_CHANNEL, duty);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupServoPwm();

  Serial.println();
  Serial.println("ESP32-S3 servo sweep test started.");
  Serial.println("Expected movement: 1000 us, 1500 us, 2000 us, repeat.");
}

void loop() {
  Serial.println("1000 us");
  writeServoUs(1000);
  delay(1500);

  Serial.println("1500 us");
  writeServoUs(1500);
  delay(1500);

  Serial.println("2000 us");
  writeServoUs(2000);
  delay(1500);
}
