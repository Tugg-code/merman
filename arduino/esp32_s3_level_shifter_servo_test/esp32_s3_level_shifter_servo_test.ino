/*
  ESP32-S3 level-shifter servo signal test.

  Purpose:
    Verify that the ESP32 3.3 V servo signal is being shifted to a servo-friendly
    5 V signal and that the DS3245 servo responds.

  Wiring:
    ESP32 GPIO 4  -> level shifter LV/input channel
    ESP32 3V3     -> level shifter LV/VCCA
    5V logic      -> level shifter HV/VCCB
    ESP32 GND     -> level shifter GND

    level shifter HV/output channel -> servo signal wire
    servo red                       -> servo power positive
    servo brown/black               -> servo power negative
    servo power negative            -> ESP32 GND / common ground

  Serial Monitor:
    115200 baud

  Expected behavior:
    The servo should move between three positions:
      1000 us -> 1500 us -> 2000 us

  Safety:
    This uses conservative RC-servo pulses. If the mechanism hits a hard stop,
    disconnect power and reduce the pulse range.
*/

#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

const int SERVO_PIN = 4;
const int SERVO_PWM_HZ = 50;
const int SERVO_PWM_BITS = 16;
const int SERVO_PWM_CHANNEL = 0; // Used only by Arduino-ESP32 2.x.

const int LEFT_US = 1000;
const int CENTER_US = 1500;
const int RIGHT_US = 2000;

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

void moveAndReport(const char *label, int pulseUs) {
  Serial.print(label);
  Serial.print("  ");
  Serial.print(pulseUs);
  Serial.println(" us");
  writeServoUs(pulseUs);
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("ESP32-S3 level-shifter servo test starting.");
  Serial.print("Servo signal GPIO: ");
  Serial.println(SERVO_PIN);
  Serial.println("Expected positions: 1000 us, 1500 us, 2000 us.");

  setupServoPwm();
  writeServoUs(CENTER_US);
  delay(1000);
}

void loop() {
  moveAndReport("LEFT-ish", LEFT_US);
  moveAndReport("CENTER", CENTER_US);
  moveAndReport("RIGHT-ish", RIGHT_US);
  moveAndReport("CENTER", CENTER_US);
}
