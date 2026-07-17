/*
  ESP32-S3 fish finder gyro stabilizer proof-of-concept.

  This is the ESP32-S3 version of the Uno bench-test sketch. It keeps the same
  serial JSON telemetry and command protocol used by the Python GUI.

  Board target:
    - ESP32-S3 N16R8 dev board using the Arduino-ESP32 board package.

  Important power note:
    - The DS3245 / 45 kg servo must have its own high-current 5-6 V supply.
    - The ESP32-S3 can be powered from USB during bench testing.
    - Servo supply ground, ESP32 ground, and MPU6050 ground must all be tied
      together.
    - Do not power the big servo from the ESP32 board.
    - Manual movement comes from the GUI using JOG LEFT / JOG RIGHT / JOG STOP.
*/

#include <Arduino.h>
#include <Wire.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

// ------------------------- Pin choices -------------------------
// These GPIOs are intentionally easy to move. ESP32-S3 dev boards vary.
// If your board labels different ADC/I2C pins, change them here only.
const int I2C_SDA_PIN = 1;
const int I2C_SCL_PIN = 2;
const int SERVO_PIN = 4;       // Servo signal only; servo power is separate.

// If the MPU6050 is lying flat, boat/base heading changes are usually gyro Z.
// If your mechanical mounting makes the useful rotation axis X or Y, change
// this one letter to 'X' or 'Y'.
const char YAW_AXIS = 'Z';

byte mpuAddr = 0x68;

// ------------------------- Servo PWM -------------------------
// DS3245-style servos commonly accept 500-2500 us pulses, but if your servo
// sounds unhappy near the ends, change these to 1000 and 2000 for a gentler
// first test.
const int SERVO_PWM_HZ = 50;
const int SERVO_PWM_BITS = 16;
const int SERVO_PWM_CHANNEL = 0;       // Used by Arduino-ESP32 2.x API only.
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2500;

enum Mode { MANUAL, FIXED, DISENGAGED, RECENTER };
Mode mode = MANUAL;

// Tunable values. The Python GUI can update these over serial.
float servoMin = 20.0;
float servoMax = 160.0;
float manualSpeed = 25.0;       // Degrees/second while GUI left/right is held.
float kp = 0.7;
float deadband = 0.5;           // Degrees of yaw error ignored in FIXED mode.
float maxError = 45.0;
float gyroTrim = 0.0;           // Deg/s, subtracted from the gyro reading.
const float maxServoRate = 25.0; // Deg/s; prevents abrupt correction steps.
const float manualNudgeDegrees = 3.0;

float yaw = 0.0;
float yawRate = 0.0;
float servoAngle = 90.0;
float fixedYaw = 0.0;
float fixedServo = 90.0;
float errorAngle = 0.0;
bool atLimit = false;
bool mpuOk = false;
int manualJogDirection = 0;     // -1 = left, 0 = stopped, +1 = right.

unsigned long lastControlMs = 0;
unsigned long lastTelemetryMs = 0;
char commandBuffer[96];
byte commandLength = 0;

const char *modeName() {
  switch (mode) {
    case MANUAL: return "MANUAL";
    case FIXED: return "FIXED";
    case DISENGAGED: return "DISENGAGED";
    case RECENTER: return "RECENTER";
  }
  return "UNKNOWN";
}

void writeRegister(byte reg, byte value) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(value);
  mpuOk = (Wire.endTransmission() == 0);
}

bool pingAddress(byte address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool detectMpu() {
  if (pingAddress(0x68)) {
    mpuAddr = 0x68;
    return true;
  }
  if (pingAddress(0x69)) {
    mpuAddr = 0x69;
    return true;
  }
  mpuAddr = 0x00;
  return false;
}

byte gyroHighRegisterForAxis() {
  if (YAW_AXIS == 'X' || YAW_AXIS == 'x') return 0x43;
  if (YAW_AXIS == 'Y' || YAW_AXIS == 'y') return 0x45;
  return 0x47; // Z high-byte register.
}

int16_t readGyroRaw() {
  if (!mpuOk) {
    mpuOk = detectMpu();
    if (!mpuOk) return 0;
  }

  Wire.beginTransmission(mpuAddr);
  Wire.write(gyroHighRegisterForAxis());
  if (Wire.endTransmission(false) != 0) {
    mpuOk = false;
    return 0;
  }
  Wire.requestFrom(mpuAddr, (byte)2);
  if (Wire.available() < 2) {
    mpuOk = false;
    return 0;
  }
  mpuOk = true;
  return (int16_t)(Wire.read() << 8 | Wire.read());
}

float calibrateGyro() {
  // Keep the sensor still while the board starts. Returns its measured bias.
  const int samples = 300;
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += readGyroRaw();
    delay(3);
  }
  return (total / (float)samples) / 131.0; // +/-250 deg/s scale.
}

void setupServoPwm() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SERVO_PIN, SERVO_PWM_HZ, SERVO_PWM_BITS);
#else
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_HZ, SERVO_PWM_BITS);
  ledcAttachPin(SERVO_PIN, SERVO_PWM_CHANNEL);
#endif
}

void writeServoMicroseconds(int pulseUs) {
  pulseUs = constrain(pulseUs, SERVO_MIN_US, SERVO_MAX_US);
  const uint32_t maxDuty = (1UL << SERVO_PWM_BITS) - 1;
  const uint32_t periodUs = 1000000UL / SERVO_PWM_HZ;
  uint32_t duty = (uint32_t)((pulseUs * (uint64_t)maxDuty) / periodUs);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(SERVO_PWM_CHANNEL, duty);
#endif
}

void setServo(float requested) {
  servoAngle = constrain(requested, servoMin, servoMax);
  int pulse = map((int)(servoAngle + 0.5), 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  writeServoMicroseconds(pulse);
}

void enterFixed() {
  manualJogDirection = 0;
  fixedYaw = yaw;
  fixedServo = servoAngle;
  errorAngle = 0.0;
  atLimit = false;
  mode = FIXED;
}

void processCommand(const char *rawCommand) {
  String command(rawCommand);
  command.trim();

  if (command == "FIX") enterFixed();
  else if (command == "MANUAL") { mode = MANUAL; atLimit = false; manualJogDirection = 0; }
  else if (command == "DISENGAGE") { mode = DISENGAGED; manualJogDirection = 0; }
  else if (command == "RECENTER") { mode = RECENTER; atLimit = false; manualJogDirection = 0; }
  else if (command == "ZERO") { yaw = 0.0; fixedYaw = 0.0; }
  else if (command == "CENTER") { manualJogDirection = 0; setServo(90.0); }
  else if (command == "JOG LEFT") { mode = MANUAL; atLimit = false; manualJogDirection = -1; }
  else if (command == "JOG RIGHT") { mode = MANUAL; atLimit = false; manualJogDirection = 1; }
  else if (command == "JOG STOP") { manualJogDirection = 0; }
  else if (command == "NUDGE LEFT") { mode = MANUAL; atLimit = false; manualJogDirection = 0; setServo(servoAngle - manualNudgeDegrees); }
  else if (command == "NUDGE RIGHT") { mode = MANUAL; atLimit = false; manualJogDirection = 0; setServo(servoAngle + manualNudgeDegrees); }
  else if (command.startsWith("SET KP ")) kp = max(0.0f, command.substring(7).toFloat());
  else if (command.startsWith("SET DEADBAND ")) deadband = max(0.0f, command.substring(13).toFloat());
  else if (command.startsWith("SET MANUAL_SPEED ")) manualSpeed = max(0.0f, command.substring(17).toFloat());
  else if (command.startsWith("SET MAX_ERROR ")) maxError = max(1.0f, command.substring(14).toFloat());
  else if (command.startsWith("SET GYRO_TRIM ")) gyroTrim = command.substring(14).toFloat();
  else if (command.startsWith("SET LIMITS ")) {
    String values = command.substring(11);
    int space = values.indexOf(' ');
    if (space > 0) {
      float minimum = values.substring(0, space).toFloat();
      float maximum = values.substring(space + 1).toFloat();
      if (minimum < maximum) {
        servoMin = constrain(minimum, 0.0f, 180.0f);
        servoMax = constrain(maximum, 0.0f, 180.0f);
        setServo(servoAngle);
      }
    }
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    char incoming = (char)Serial.read();
    if (incoming == '\r') continue;
    if (incoming == '\n') {
      commandBuffer[commandLength] = '\0';
      if (commandLength > 0) processCommand(commandBuffer);
      commandLength = 0;
    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = incoming;
    } else {
      commandLength = 0; // Discard an overlong command.
    }
  }
}

void updateControl(float dt) {
  // At +/-250 deg/s sensitivity the MPU6050 produces 131 LSB per deg/s.
  yawRate = readGyroRaw() / 131.0 - gyroTrim;
  yaw += yawRate * dt;

  if (mode == MANUAL) {
    // Manual movement is driven by GUI JOG commands. LEFT decreases the servo
    // angle and RIGHT increases it. If your linkage is reversed, swap the signs
    // here or swap the labels in the GUI.
    setServo(servoAngle + manualJogDirection * manualSpeed * dt);
    errorAngle = 0.0;
    atLimit = false;
  }
  else if (mode == FIXED) {
    // Positive base yaw is countered with negative servo correction.
    float yawDelta = yaw - fixedYaw;
    errorAngle = -yawDelta;

    // Inside the deadband, retain the previous command. Do not snap back to
    // fixedServo or the servo will lurch at the deadband threshold.
    float requested = servoAngle;
    if (abs(errorAngle) > deadband) requested = fixedServo + kp * errorAngle;

    // A saturated servo cannot continue protecting the target direction.
    if (abs(errorAngle) > maxError || requested < servoMin || requested > servoMax) {
      atLimit = requested < servoMin || requested > servoMax;
      float maxStep = maxServoRate * dt;
      float step = constrain(requested - servoAngle, -maxStep, maxStep);
      setServo(servoAngle + step);
      mode = DISENGAGED;
    } else {
      atLimit = false;
      float maxStep = maxServoRate * dt;
      float step = constrain(requested - servoAngle, -maxStep, maxStep);
      setServo(servoAngle + step);
    }
  }
  else if (mode == RECENTER) {
    const float recenterSpeed = 60.0; // Degrees/second.
    float step = recenterSpeed * dt;
    if (abs(servoAngle - 90.0) <= step) {
      setServo(90.0);
      mode = MANUAL;
    } else {
      setServo(servoAngle + (servoAngle < 90.0 ? step : -step));
    }
    errorAngle = 0.0;
  }
  // DISENGAGED intentionally holds the last servo command.
}

void sendTelemetry() {
  Serial.print("{\"yaw\":"); Serial.print(yaw, 2);
  Serial.print(",\"yaw_rate\":"); Serial.print(yawRate, 2);
  Serial.print(",\"mpu_ok\":"); Serial.print(mpuOk ? "true" : "false");
  Serial.print(",\"mpu_addr\":"); Serial.print(mpuAddr);
  Serial.print(",\"servo\":"); Serial.print(servoAngle, 1);
  Serial.print(",\"target\":"); Serial.print(fixedYaw, 2);
  Serial.print(",\"error\":"); Serial.print(errorAngle, 2);
  Serial.print(",\"mode\":\""); Serial.print(modeName());
  Serial.print("\",\"limit\":"); Serial.print(atLimit ? "true" : "false");
  Serial.print(",\"kp\":"); Serial.print(kp, 2);
  Serial.print(",\"deadband\":"); Serial.print(deadband, 2);
  Serial.print(",\"manual_speed\":"); Serial.print(manualSpeed, 2);
  Serial.print(",\"gyro_trim\":"); Serial.print(gyroTrim, 2);
  Serial.println("}");
}

void setup() {
  Serial.begin(115200);
  delay(250);

  analogReadResolution(12);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  mpuOk = detectMpu();
  if (mpuOk) {
    writeRegister(0x6B, 0x00); // Wake MPU6050.
    writeRegister(0x1B, 0x00); // Gyro range: +/-250 deg/s.
  }

  setupServoPwm();
  setServo(90.0);

  delay(250);
  if (mpuOk) {
    gyroTrim = calibrateGyro(); // Initial stationary gyro bias removal.
  }
  lastControlMs = millis();
}

void loop() {
  readSerialCommands();

  unsigned long now = millis();
  if (now - lastControlMs >= 10) { // 100 Hz control loop.
    float dt = (now - lastControlMs) / 1000.0;
    lastControlMs = now;
    updateControl(dt);
  }

  if (now - lastTelemetryMs >= 50) { // 20 Hz JSON telemetry.
    lastTelemetryMs = now;
    sendTelemetry();
  }
}
