/*
  ESP32-S3 servo calibrator for the DS3245 / fish finder head.

  Purpose:
    Find the safe servo pulse range and useful mechanical travel before using
    the full gyro-stabilizer firmware.

  Wiring:
    ESP32 GPIO 4 -> logic level shifter input
    Level shifter output -> servo signal wire
    Servo red -> external servo supply positive
    Servo brown/black -> external servo supply negative
    ESP32 GND -> common ground with servo supply and level shifter

  Serial Monitor:
    Baud: 115200
    Line ending: Newline

  Commands:
    CENTER
    MIN
    MAX
    US 1500
    DEG 90
    LIMITS 1000 2000
    STEP 10
    LEFT
    RIGHT
    HELP

  Start conservative. Do not use 500-2500 us until you know the mechanics and
  servo are safe at the ends. Good first range: 1000-2000 us.
*/

#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

const int SERVO_PIN = 4;
const int SERVO_FRAME_US = 20000;

int minUs = 1000;
int maxUs = 2000;
int currentUs = 1500;
int stepUs = 10;

char commandBuffer[80];
byte commandLength = 0;
bool servoPulseHigh = false;
unsigned long servoFrameStartUs = 0;

void setupServoPulseOutput() {
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  servoFrameStartUs = micros();
}

void writeServoUs(int pulseUs) {
  currentUs = constrain(pulseUs, minUs, maxUs);
}

void updateServoPulseOutput() {
  unsigned long nowUs = micros();

  if (!servoPulseHigh && (unsigned long)(nowUs - servoFrameStartUs) >= SERVO_FRAME_US) {
    servoFrameStartUs += SERVO_FRAME_US;
    digitalWrite(SERVO_PIN, HIGH);
    servoPulseHigh = true;
    return;
  }

  if (servoPulseHigh && (unsigned long)(nowUs - servoFrameStartUs) >= (unsigned long)currentUs) {
    digitalWrite(SERVO_PIN, LOW);
    servoPulseHigh = false;
  }
}

void printStatus() {
  Serial.print("current_us=");
  Serial.print(currentUs);
  Serial.print(" min_us=");
  Serial.print(minUs);
  Serial.print(" max_us=");
  Serial.print(maxUs);
  Serial.print(" step_us=");
  Serial.println(stepUs);
}

void printHelp() {
  Serial.println();
  Serial.println("ESP32-S3 servo calibrator commands:");
  Serial.println("  CENTER          -> 1500 us");
  Serial.println("  MIN             -> current min_us");
  Serial.println("  MAX             -> current max_us");
  Serial.println("  US 1500         -> direct pulse width");
  Serial.println("  DEG 90          -> map 0-180 deg into min/max us");
  Serial.println("  LIMITS 1000 2000");
  Serial.println("  STEP 10");
  Serial.println("  LEFT            -> current_us - step_us");
  Serial.println("  RIGHT           -> current_us + step_us");
  Serial.println("  HELP");
  printStatus();
}

void processCommand(const char *rawCommand) {
  String command(rawCommand);
  command.trim();
  command.toUpperCase();

  if (command == "HELP") {
    printHelp();
    return;
  }
  if (command == "CENTER") {
    writeServoUs(1500);
  } else if (command == "MIN") {
    writeServoUs(minUs);
  } else if (command == "MAX") {
    writeServoUs(maxUs);
  } else if (command == "LEFT") {
    writeServoUs(currentUs - stepUs);
  } else if (command == "RIGHT") {
    writeServoUs(currentUs + stepUs);
  } else if (command.startsWith("US ")) {
    writeServoUs(command.substring(3).toInt());
  } else if (command.startsWith("DEG ")) {
    int degrees = constrain(command.substring(4).toInt(), 0, 180);
    int pulse = map(degrees, 0, 180, minUs, maxUs);
    writeServoUs(pulse);
  } else if (command.startsWith("STEP ")) {
    stepUs = constrain(command.substring(5).toInt(), 1, 200);
  } else if (command.startsWith("LIMITS ")) {
    String values = command.substring(7);
    int space = values.indexOf(' ');
    if (space > 0) {
      int newMin = values.substring(0, space).toInt();
      int newMax = values.substring(space + 1).toInt();
      if (newMin >= 500 && newMax <= 2500 && newMin < newMax) {
        minUs = newMin;
        maxUs = newMax;
        writeServoUs(currentUs);
      } else {
        Serial.println("Invalid limits. Use something like: LIMITS 1000 2000");
      }
    }
  } else {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type HELP for commands.");
  }

  printStatus();
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
      commandLength = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  setupServoPulseOutput();
  writeServoUs(1500);

  Serial.println();
  Serial.println("Merman ESP32-S3 servo calibrator starting.");
  Serial.print("Servo signal GPIO ");
  Serial.println(SERVO_PIN);
  printHelp();
}

void loop() {
  updateServoPulseOutput();
  readSerialCommands();
}
