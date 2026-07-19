/*
  ESP32-S3 fish finder gyro stabilizer proof-of-concept.

  This is the ESP32-S3 field-test sketch. It keeps the same serial JSON
  telemetry and command protocol used by the Python GUI, and it also serves a
  password-protected local Wi-Fi web portal for phone/tablet control.

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
#include <WiFi.h>
#include <WebServer.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

// ------------------------- Pin choices -------------------------
// These GPIOs are intentionally easy to move. ESP32-S3 dev boards vary.
// If your board labels different ADC/I2C pins, change them here only.
const int I2C_SDA_PIN = 1;
const int I2C_SCL_PIN = 2;
const int SERVO_PIN = 4;       // Servo signal only; servo power is separate.

// ------------------------- Local Wi-Fi portal -------------------------
// The ESP32 creates this network directly. No router or internet is required.
// Password must be at least 8 characters for WPA2.
const char *AP_SSID = "Merman-Stabilizer";
const char *AP_PASSWORD = "merman1234";
WebServer webServer(80);

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

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Merman Stabilizer</title>
  <style>
    :root { color-scheme: dark; --bg:#07111f; --panel:#101d2e; --line:#26364d; --text:#eef5ff; --muted:#9fb2ca; --accent:#4db3ff; --warn:#ffb84d; --bad:#ff5f6d; --ok:#55d889; }
    * { box-sizing: border-box; }
    body { margin:0; font-family: system-ui, -apple-system, Segoe UI, sans-serif; background:var(--bg); color:var(--text); }
    header { padding:14px 16px; border-bottom:1px solid var(--line); background:#0b1728; position:sticky; top:0; z-index:1; }
    h1 { margin:0; font-size:22px; letter-spacing:.03em; }
    .tabs { display:flex; gap:8px; margin-top:12px; }
    .tabs button { flex:1; padding:10px; border:1px solid var(--line); border-radius:10px; background:#132238; color:var(--text); font-size:16px; }
    .tabs button.active { border-color:var(--accent); background:#173253; }
    main { padding:14px; max-width:980px; margin:0 auto; }
    .grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:10px; }
    .card { background:var(--panel); border:1px solid var(--line); border-radius:14px; padding:14px; margin-bottom:12px; box-shadow:0 8px 24px #0004; }
    .big { font-size:34px; font-weight:700; }
    .muted { color:var(--muted); }
    .ok { color:var(--ok); }
    .bad { color:var(--bad); }
    .warn { color:var(--warn); }
    .controls { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:12px; }
    button.control { min-height:72px; border:0; border-radius:16px; background:#1d385c; color:var(--text); font-size:20px; font-weight:700; touch-action:none; }
    button.control:active { transform:translateY(1px); background:#255080; }
    button.lock { background:#145c3b; }
    button.unlock { background:#5c2d2d; }
    button.reset { background:#594515; }
    table { width:100%; border-collapse:collapse; }
    td { padding:8px 4px; border-bottom:1px solid var(--line); }
    td:last-child { text-align:right; font-family: ui-monospace, SFMono-Regular, Consolas, monospace; }
    .slider-row { display:grid; grid-template-columns:150px 1fr 70px; gap:10px; align-items:center; padding:9px 0; border-bottom:1px solid var(--line); }
    input[type=range] { width:100%; }
    .hidden { display:none; }
    @media (max-width:680px) {
      .grid, .controls { grid-template-columns:1fr; }
      .slider-row { grid-template-columns:1fr; }
      button.control { min-height:62px; }
    }
  </style>
</head>
<body>
  <header>
    <h1>Merman Stabilizer</h1>
    <div class="tabs">
      <button id="simpleTab" class="active" onclick="showTab('simple')">Simple</button>
      <button id="devTab" onclick="showTab('dev')">Dev</button>
    </div>
  </header>
  <main>
    <section id="simple">
      <div class="card">
        <div class="muted">Mode</div>
        <div id="modeBig" class="big">--</div>
        <div id="summary" class="muted">Waiting for telemetry...</div>
      </div>
      <div class="card controls">
        <button class="control" id="leftBtn">LEFT</button>
        <button class="control reset" onclick="sendCmd('CENTER')">RESET SERVO</button>
        <button class="control" id="rightBtn">RIGHT</button>
        <button class="control lock" onclick="sendCmd('FIX')">LOCK TARGET</button>
        <button class="control unlock" onclick="sendCmd('DISENGAGE')">UNLOCK</button>
        <button class="control" onclick="sendCmd('RECENTER')">RECENTER</button>
        <button class="control" onclick="sendCmd('ZERO')">ZERO GYRO</button>
        <button class="control" onclick="sendCmd('MANUAL')">MANUAL</button>
      </div>
    </section>

    <section id="dev" class="hidden">
      <div class="grid">
        <div class="card">
          <h2>Telemetry</h2>
          <table>
            <tr><td>Yaw</td><td id="yaw">--</td></tr>
            <tr><td>Yaw rate</td><td id="yawRate">--</td></tr>
            <tr><td>Servo</td><td id="servo">--</td></tr>
            <tr><td>Target</td><td id="target">--</td></tr>
            <tr><td>Error</td><td id="error">--</td></tr>
            <tr><td>MPU</td><td id="mpu">--</td></tr>
            <tr><td>Limit</td><td id="limit">--</td></tr>
          </table>
        </div>
        <div class="card">
          <h2>Dev controls</h2>
          <div class="controls">
            <button class="control" onclick="sendCmd('NUDGE LEFT')">NUDGE LEFT</button>
            <button class="control" onclick="sendCmd('CENTER')">CENTER</button>
            <button class="control" onclick="sendCmd('NUDGE RIGHT')">NUDGE RIGHT</button>
            <button class="control lock" onclick="sendCmd('FIX')">FIX</button>
            <button class="control" onclick="sendCmd('MANUAL')">MANUAL</button>
            <button class="control unlock" onclick="sendCmd('DISENGAGE')">DISENGAGE</button>
          </div>
        </div>
      </div>
      <div class="card">
        <h2>Tuning</h2>
        <div class="slider-row"><label>Servo min</label><input id="servoMin" type="range" min="0" max="90" step="1"><span id="servoMinV"></span></div>
        <div class="slider-row"><label>Servo max</label><input id="servoMax" type="range" min="90" max="180" step="1"><span id="servoMaxV"></span></div>
        <div class="slider-row"><label>Manual speed</label><input id="manualSpeed" type="range" min="0.1" max="60" step="0.5"><span id="manualSpeedV"></span></div>
        <div class="slider-row"><label>Kp</label><input id="kp" type="range" min="0" max="4" step="0.1"><span id="kpV"></span></div>
        <div class="slider-row"><label>Deadband</label><input id="deadband" type="range" min="0" max="15" step="0.5"><span id="deadbandV"></span></div>
        <div class="slider-row"><label>Max error</label><input id="maxError" type="range" min="5" max="180" step="1"><span id="maxErrorV"></span></div>
        <div class="slider-row"><label>Gyro trim</label><input id="gyroTrim" type="range" min="-5" max="5" step="0.05"><span id="gyroTrimV"></span></div>
      </div>
    </section>
  </main>
  <script>
    let firstTelemetry = true;
    function showTab(name) {
      document.getElementById('simple').classList.toggle('hidden', name !== 'simple');
      document.getElementById('dev').classList.toggle('hidden', name !== 'dev');
      document.getElementById('simpleTab').classList.toggle('active', name === 'simple');
      document.getElementById('devTab').classList.toggle('active', name === 'dev');
    }
    async function sendCmd(cmd) {
      try { await fetch('/api/command?cmd=' + encodeURIComponent(cmd)); }
      catch(e) { console.log(e); }
    }
    function holdButton(id, startCmd) {
      const button = document.getElementById(id);
      const stop = () => sendCmd('JOG STOP');
      button.addEventListener('pointerdown', e => { e.preventDefault(); sendCmd(startCmd); });
      button.addEventListener('pointerup', stop);
      button.addEventListener('pointercancel', stop);
      button.addEventListener('pointerleave', stop);
    }
    function setupSlider(id, commandBuilder) {
      const el = document.getElementById(id);
      const value = document.getElementById(id + 'V');
      el.addEventListener('input', () => value.textContent = el.value);
      el.addEventListener('change', () => sendCmd(commandBuilder(el.value)));
    }
    function setSlider(id, value) {
      const el = document.getElementById(id);
      const label = document.getElementById(id + 'V');
      if (document.activeElement !== el) el.value = value;
      label.textContent = el.value;
    }
    async function poll() {
      try {
        const r = await fetch('/api/telemetry', {cache:'no-store'});
        const t = await r.json();
        document.getElementById('modeBig').textContent = t.mode;
        document.getElementById('summary').textContent =
          `Servo ${t.servo.toFixed(1)} deg | Yaw ${t.yaw.toFixed(1)} deg | MPU ${t.mpu_ok ? 'OK 0x' + Number(t.mpu_addr).toString(16).toUpperCase() : 'NOT FOUND'} | Limit ${t.limit ? 'YES' : 'OK'}`;
        document.getElementById('modeBig').className = 'big ' + (t.mode === 'DISENGAGED' ? 'warn' : '');
        document.getElementById('yaw').textContent = t.yaw.toFixed(2) + ' deg';
        document.getElementById('yawRate').textContent = t.yaw_rate.toFixed(2) + ' deg/s';
        document.getElementById('servo').textContent = t.servo.toFixed(1) + ' deg';
        document.getElementById('target').textContent = t.target.toFixed(2) + ' deg';
        document.getElementById('error').textContent = t.error.toFixed(2) + ' deg';
        document.getElementById('mpu').textContent = t.mpu_ok ? 'OK 0x' + Number(t.mpu_addr).toString(16).toUpperCase() : 'NOT FOUND';
        document.getElementById('mpu').className = t.mpu_ok ? 'ok' : 'bad';
        document.getElementById('limit').textContent = t.limit ? 'AT LIMIT' : 'OK';
        document.getElementById('limit').className = t.limit ? 'bad' : 'ok';
        if (firstTelemetry) {
          setSlider('servoMin', t.servo_min);
          setSlider('servoMax', t.servo_max);
          setSlider('manualSpeed', t.manual_speed);
          setSlider('kp', t.kp);
          setSlider('deadband', t.deadband);
          setSlider('maxError', t.max_error);
          setSlider('gyroTrim', t.gyro_trim);
          firstTelemetry = false;
        }
      } catch(e) {
        document.getElementById('summary').textContent = 'Telemetry lost...';
      }
    }
    holdButton('leftBtn', 'JOG LEFT');
    holdButton('rightBtn', 'JOG RIGHT');
    setupSlider('servoMin', v => `SET LIMITS ${v} ${document.getElementById('servoMax').value}`);
    setupSlider('servoMax', v => `SET LIMITS ${document.getElementById('servoMin').value} ${v}`);
    setupSlider('manualSpeed', v => `SET MANUAL_SPEED ${v}`);
    setupSlider('kp', v => `SET KP ${v}`);
    setupSlider('deadband', v => `SET DEADBAND ${v}`);
    setupSlider('maxError', v => `SET MAX_ERROR ${v}`);
    setupSlider('gyroTrim', v => `SET GYRO_TRIM ${v}`);
    setInterval(poll, 250);
    poll();
  </script>
</body>
</html>
)rawliteral";

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

String telemetryJson() {
  String json = "{";
  json += "\"yaw\":"; json += String(yaw, 2);
  json += ",\"yaw_rate\":"; json += String(yawRate, 2);
  json += ",\"mpu_ok\":"; json += (mpuOk ? "true" : "false");
  json += ",\"mpu_addr\":"; json += String(mpuAddr);
  json += ",\"servo\":"; json += String(servoAngle, 1);
  json += ",\"target\":"; json += String(fixedYaw, 2);
  json += ",\"error\":"; json += String(errorAngle, 2);
  json += ",\"mode\":\""; json += modeName(); json += "\"";
  json += ",\"limit\":"; json += (atLimit ? "true" : "false");
  json += ",\"kp\":"; json += String(kp, 2);
  json += ",\"deadband\":"; json += String(deadband, 2);
  json += ",\"manual_speed\":"; json += String(manualSpeed, 2);
  json += ",\"max_error\":"; json += String(maxError, 1);
  json += ",\"gyro_trim\":"; json += String(gyroTrim, 2);
  json += ",\"servo_min\":"; json += String(servoMin, 1);
  json += ",\"servo_max\":"; json += String(servoMax, 1);
  json += "}";
  return json;
}

void handleRoot() {
  webServer.send_P(200, "text/html", INDEX_HTML);
}

void handleTelemetryApi() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", telemetryJson());
}

void handleCommandApi() {
  if (!webServer.hasArg("cmd")) {
    webServer.send(400, "text/plain", "missing cmd");
    return;
  }
  String command = webServer.arg("cmd");
  processCommand(command.c_str());
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  webServer.send(404, "text/plain", "Not found");
}

void setupWebPortal() {
  WiFi.mode(WIFI_AP);
  bool started = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("Wi-Fi AP: "); Serial.println(started ? "started" : "FAILED");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("Password: "); Serial.println(AP_PASSWORD);
  Serial.print("Open: http://"); Serial.println(WiFi.softAPIP());

  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/telemetry", HTTP_GET, handleTelemetryApi);
  webServer.on("/api/command", HTTP_GET, handleCommandApi);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("Web portal ready.");
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
  Serial.println(telemetryJson());
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Merman ESP32-S3 stabilizer starting");
  Serial.print("I2C SDA GPIO "); Serial.println(I2C_SDA_PIN);
  Serial.print("I2C SCL GPIO "); Serial.println(I2C_SCL_PIN);
  Serial.print("Servo GPIO "); Serial.println(SERVO_PIN);

  setupWebPortal();

  analogReadResolution(12);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  mpuOk = detectMpu();
  Serial.print("MPU detected: "); Serial.println(mpuOk ? "YES" : "NO");
  if (mpuOk) {
    Serial.print("MPU address: 0x");
    Serial.println(mpuAddr, HEX);
  }
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
  webServer.handleClient();
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
