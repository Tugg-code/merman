/*
  Fish finder gyro stabilizer proof-of-concept.

  This deliberately uses only a gyro-integrated yaw estimate. It will drift over
  time, which is expected for this first bench test. Use ZERO before each test.
*/

#include <Wire.h>
#include <Servo.h>

const byte MPU_ADDR = 0x68;
const byte JOY_X_PIN = A0;
const byte JOY_Y_PIN = A1;
const byte JOY_SW_PIN = 2;       // Optional joystick switch, active low.
const byte SERVO_PIN = 9;

enum Mode { MANUAL, FIXED, DISENGAGED, RECENTER };
Mode mode = MANUAL;

Servo headServo;

// Tunable values. They can also be updated from the GUI.
float servoMin = 20.0;
float servoMax = 160.0;
float manualSpeed = 2.0;         // Degrees/second at full joystick deflection.
float kp = 1.2;
float deadband = 2.0;            // Degrees of yaw error ignored in FIXED mode.
float maxError = 45.0;
float gyroTrim = 0.0;            // Deg/s, subtracted from the gyro reading.
const float maxServoRate = 25.0; // Deg/s; prevents abrupt correction steps.

float yaw = 0.0;
float yawRate = 0.0;
float servoAngle = 90.0;
float fixedYaw = 0.0;
float fixedServo = 90.0;
float errorAngle = 0.0;
bool atLimit = false;
int joyX = 512;
int joyY = 512;

unsigned long lastControlMs = 0;
unsigned long lastTelemetryMs = 0;
bool previousButton = HIGH;

char commandBuffer[80];
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
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t readGyroZRaw() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x47);              // Gyro Z high-byte register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (byte)2);
  if (Wire.available() < 2) return 0;
  return (int16_t)(Wire.read() << 8 | Wire.read());
}

float calibrateGyroZ() {
  // Keep the sensor still while the board starts. Returns its measured bias.
  const int samples = 300;
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += readGyroZRaw();
    delay(3);
  }
  return (total / (float)samples) / 131.0; // +/-250 deg/s scale
}

void setServo(float requested) {
  servoAngle = constrain(requested, servoMin, servoMax);
  headServo.write((int)(servoAngle + 0.5));
}

void enterFixed() {
  fixedYaw = yaw;
  fixedServo = servoAngle;
  errorAngle = 0.0;
  atLimit = false;
  mode = FIXED;
}

void processCommand(const char *rawCommand) {
  // String.toFloat() avoids the unreliable %f scanf support found on many Uno
  // builds. The GUI has no settings effect until this parser accepts them.
  String command(rawCommand);
  command.trim();
  if (command == "FIX") enterFixed();
  else if (command == "MANUAL") { mode = MANUAL; atLimit = false; }
  else if (command == "DISENGAGE") mode = DISENGAGED;
  else if (command == "RECENTER") { mode = RECENTER; atLimit = false; }
  else if (command == "ZERO") { yaw = 0.0; fixedYaw = 0.0; }
  else if (command == "CENTER") { setServo(90.0); }
  else if (command.startsWith("SET KP ")) kp = max(0.0, command.substring(7).toFloat());
  else if (command.startsWith("SET DEADBAND ")) deadband = max(0.0, command.substring(13).toFloat());
  else if (command.startsWith("SET MANUAL_SPEED ")) manualSpeed = max(0.0, command.substring(17).toFloat());
  else if (command.startsWith("SET MAX_ERROR ")) maxError = max(1.0, command.substring(14).toFloat());
  else if (command.startsWith("SET GYRO_TRIM ")) gyroTrim = command.substring(14).toFloat();
  else if (command.startsWith("SET LIMITS ")) {
    String values = command.substring(11);
    int space = values.indexOf(' ');
    if (space > 0) {
      float minimum = values.substring(0, space).toFloat();
      float maximum = values.substring(space + 1).toFloat();
      if (minimum < maximum) {
        servoMin = constrain(minimum, 0.0, 180.0);
        servoMax = constrain(maximum, 0.0, 180.0);
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
  joyX = analogRead(JOY_X_PIN);
  joyY = analogRead(JOY_Y_PIN);

  // At +/-250 deg/s sensitivity the MPU6050 produces 131 LSB per deg/s.
  yawRate = readGyroZRaw() / 131.0 - gyroTrim;
  yaw += yawRate * dt;

  bool button = digitalRead(JOY_SW_PIN);
  if (previousButton == HIGH && button == LOW) enterFixed();
  previousButton = button;

  if (mode == MANUAL) {
    // Joystick X alone controls heading. Small center noise is ignored.
    float normalized = (joyX - 512) / 512.0;
    if (abs(normalized) < 0.08) normalized = 0.0;
    setServo(servoAngle + normalized * manualSpeed * dt);
    errorAngle = 0.0;
    atLimit = false;
  }
  else if (mode == FIXED) {
    // Positive base yaw is countered with negative servo correction.
    float yawDelta = yaw - fixedYaw;
    errorAngle = -yawDelta;
    // Inside the deadband, retain the previous command. Returning to
    // fixedServo here caused the earlier threshold snap.
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
      setServo(requested);
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
  // JSON lines make this simple to inspect and parse on the PC.
  Serial.print("{\"yaw\":"); Serial.print(yaw, 2);
  Serial.print(",\"yaw_rate\":"); Serial.print(yawRate, 2);
  Serial.print(",\"joy_x\":"); Serial.print(joyX);
  Serial.print(",\"joy_y\":"); Serial.print(joyY);
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
  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  Wire.begin();
  writeRegister(0x6B, 0x00);     // Wake MPU6050.
  writeRegister(0x1B, 0x00);     // Gyro range: +/-250 deg/s.
  headServo.attach(SERVO_PIN);
  setServo(90.0);
  delay(250);
  gyroTrim = calibrateGyroZ();   // Initial stationary gyro bias removal.
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
