/*
  ESP32-S3 servo signal finder.

  Use this when the servo has power/ground but does not move on GPIO 4.
  The sketch drives one GPIO at a time with obvious servo pulses and prints
  which pin is currently active. Move the servo signal wire to the printed GPIO
  and watch/listen for servo movement.

  Wiring:
    Servo signal  -> currently printed ESP32 GPIO
    Servo red     -> external servo supply positive
    Servo ground  -> external servo supply negative
    Servo supply negative -> ESP32 GND

  Open Serial Monitor at 115200 baud.
*/

#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

const int SERVO_PWM_HZ = 50;
const int SERVO_PWM_BITS = 16;
const int SERVO_PWM_CHANNEL = 0; // Used by Arduino-ESP32 2.x only.

// Avoid GPIO 19/20 if they are your native USB D-/D+ pins.
// Avoid strapping/boot pins unless you know your board tolerates them.
const int TEST_PINS[] = {4, 5, 6, 7, 15, 16, 17, 18};
const int TEST_PIN_COUNT = sizeof(TEST_PINS) / sizeof(TEST_PINS[0]);

int activePin = -1;

void detachActivePin() {
  if (activePin < 0) return;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(activePin);
#else
  ledcDetachPin(activePin);
#endif

  pinMode(activePin, INPUT);
  activePin = -1;
}

void attachServoPin(int pin) {
  detachActivePin();
  activePin = pin;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(activePin, SERVO_PWM_HZ, SERVO_PWM_BITS);
#else
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_HZ, SERVO_PWM_BITS);
  ledcAttachPin(activePin, SERVO_PWM_CHANNEL);
#endif
}

void writeServoUs(int pulseUs) {
  const uint32_t maxDuty = (1UL << SERVO_PWM_BITS) - 1;
  const uint32_t periodUs = 1000000UL / SERVO_PWM_HZ;
  uint32_t duty = (uint32_t)((pulseUs * (uint64_t)maxDuty) / periodUs);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(activePin, duty);
#else
  ledcWrite(SERVO_PWM_CHANNEL, duty);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("ESP32-S3 servo signal finder starting.");
  Serial.println("Move the servo signal wire to the GPIO printed below.");
}

void loop() {
  for (int i = 0; i < TEST_PIN_COUNT; i++) {
    int pin = TEST_PINS[i];
    attachServoPin(pin);

    Serial.println();
    Serial.print("Testing servo signal on GPIO ");
    Serial.println(pin);

    for (int cycle = 0; cycle < 3; cycle++) {
      Serial.println("  1000 us");
      writeServoUs(1000);
      delay(800);

      Serial.println("  1500 us");
      writeServoUs(1500);
      delay(800);

      Serial.println("  2000 us");
      writeServoUs(2000);
      delay(800);
    }
  }

  detachActivePin();
  Serial.println();
  Serial.println("Signal finder cycle complete. Repeating in 3 seconds.");
  delay(3000);
}
