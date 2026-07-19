# Fish Finder Stabilizer Test POC

Bench-test proof of concept for a manually aimed, gyro-stabilized fish finder/sonar head.

The idea: aim the head manually, press **FIX TARGET**, and let the controller rotate the servo opposite the boat/base rotation so the head keeps pointing roughly toward the same target.

This project currently supports:

- Windows Python GUI using tkinter and matplotlib
- Arduino Uno firmware
- ESP32-S3 firmware
- ESP32-S3 local Wi-Fi web portal
- MPU6050 gyro/accelerometer module
- GUI-controlled manual movement
- RC servo output
- CSV telemetry logging

## Project status

The original Arduino Uno + small servo proof-of-concept worked after fixing grounding and tuning issues. The project is now being moved to an ESP32-S3 N16R8 and a DS3245 45 kg waterproof servo.

Current active bench focus:

- ESP32-S3 connects to the Python GUI and telemetry looks good.
- Servo wiring/power/PWM is being debugged with the larger DS3245 servo.
- Use `arduino/esp32_s3_servo_sweep_test/esp32_s3_servo_sweep_test.ino` before blaming the full stabilizer code.

For future maintainers or future Codex sessions, start with:

- `docs/HANDOFF.md`
- `docs/HARDWARE_CHECKLIST.md`
- `docs/ESP32_S3_FINAL_WIRING.md`
- `docs/GITHUB_SETUP.md`

## Safety and limits

This is a bench prototype. An MPU6050 gyro-only yaw estimate drifts over time and is not suitable for navigation or unattended use.

Keep fingers, wires, and loose parts clear of the servo. A DS3245-class servo is strong enough to hurt you or break a mount.

Important power rules:

- Do not power a servo from the Uno or ESP32 board.
- Use a separate servo supply or UBEC for the servo.
- Tie servo supply ground to controller ground.
- Do not feed 7.4 V directly into the ESP32.
- Do not feed 5 V into an ESP32 `3V3` pin.
- Power joystick analog modules from 3.3 V if you re-enable them later on ESP32 ADC pins.

## Repository layout

| Path | Purpose |
| --- | --- |
| `main.py` | Tkinter GUI |
| `serial_interface.py` | Threaded serial connection |
| `telemetry.py` | JSON telemetry parsing and CSV row formatting |
| `plot_panel.py` | Embedded live matplotlib plots |
| `config.py` | App defaults |
| `requirements.txt` | Python dependencies |
| `arduino/fishfinder_stabilizer_test/` | Arduino Uno firmware |
| `arduino/fishfinder_stabilizer_esp32_s3/` | ESP32-S3 stabilizer firmware |
| `arduino/esp32_s3_smoke_test/` | Minimal ESP32 upload/serial test |
| `arduino/esp32_s3_servo_sweep_test/` | Minimal ESP32 servo PWM test |
| `arduino/esp32_s3_servo_calibrator/` | Interactive ESP32 servo calibration sketch |
| `docs/` | Handoff, GitHub, and hardware notes |

## Windows setup

1. Install Python 3.10 or newer.
2. Install Arduino IDE.
3. In PowerShell, open this project folder and create a virtual environment:

   ```powershell
   py -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install -r requirements.txt
   ```

4. Run the GUI:

   ```powershell
   python main.py
   ```

5. Select the controller COM port, leave baud rate at `115200`, then click **Connect**.

Close Arduino Serial Monitor before using the Python GUI. Only one program can own the COM port at a time.

## GUI modes

The Windows Python app has two views in one program:

- **Simple tester**: friend/tester-facing controls only. Use **LEFT**, **RIGHT**, **LOCK TARGET**, **UNLOCK**, **RESET SERVO**, **ZERO GYRO**, and **RECENTER**.
- **Dev / tuning**: full telemetry, plots, CSV logging, and tuning sliders.

In Simple tester mode, **LEFT** and **RIGHT** are hold-to-move buttons. The firmware keeps moving while the button is held and stops when it receives `JOG STOP` on release.

**RESET SERVO** currently means "return servo command to center", which is 90 degrees for a standard 0-180 degree RC servo signal.

## ESP32 local Wi-Fi web portal

The ESP32-S3 firmware also hosts its own password-protected web portal. This is the recommended friend/field-test control method because it does not require the Python GUI, a router, or internet.

Default network:

```text
SSID: Merman-Stabilizer
Password: merman1234
Address: http://192.168.4.1
```

Use:

1. Power the ESP32 system.
2. Connect phone/tablet/laptop Wi-Fi to `Merman-Stabilizer`.
3. Enter password `merman1234`.
4. Open browser to `http://192.168.4.1`.
5. Use **Simple** mode for normal testing.
6. Use **Dev** mode for telemetry and tuning.

The web portal and serial/Python GUI command protocol control the same firmware state machine.

## Arduino IDE setup

### Arduino Uno

Open:

```text
arduino/fishfinder_stabilizer_test/fishfinder_stabilizer_test.ino
```

Select **Arduino Uno**, select the Uno COM port, and upload.

### ESP32-S3

Install the Espressif ESP32 board package in Arduino IDE.

Open:

```text
arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino
```

Typical board selection:

```text
ESP32S3 Dev Module
```

Useful settings for many ESP32-S3 boards:

```text
Upload Speed: 115200 while debugging
USB CDC On Boot: Enabled
Flash Size: 16MB
PSRAM: Enabled / OPI PSRAM if available
```

If upload or serial is acting strange, upload the smoke test first:

```text
arduino/esp32_s3_smoke_test/esp32_s3_smoke_test.ino
```

## Wiring

### Arduino Uno wiring

| Part | Arduino Uno connection |
| --- | --- |
| MPU6050 SDA | A4 |
| MPU6050 SCL | A5 |
| MPU6050 VCC | 5 V or 3.3 V as required by the breakout board |
| MPU6050 GND | GND |
| Joystick VRx | A0 |
| Joystick VRy | A1, telemetry only |
| Joystick SW | D2, optional |
| Servo signal | D9 |
| Servo power | External 5 V supply |
| Servo ground | External supply ground tied to Arduino GND |

### ESP32-S3 default wiring

The ESP32-S3 pin map is defined near the top of:

```text
arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino
```

Default pin map:

| Part | ESP32-S3 connection |
| --- | --- |
| MPU6050 SDA | GPIO 1 |
| MPU6050 SCL | GPIO 2 |
| MPU6050 VCC | 3.3 V recommended |
| MPU6050 GND | GND |
| Joystick VRx | Not used in final-test firmware |
| Joystick VRy | Not used in final-test firmware |
| Joystick SW | Not used in final-test firmware |
| Servo signal | GPIO 4 |
| Servo power | External 5-6 V servo supply / UBEC |
| Servo ground | Servo supply ground tied to ESP32 GND |

Servo wire colors are usually:

| Servo wire | Function |
| --- | --- |
| Yellow/orange/white | Signal |
| Red | Positive servo power |
| Brown/black | Ground |

## ESP32-S3 power sanity check

For first bench testing:

1. Power the ESP32-S3 from USB.
2. Power the MPU6050 from ESP32 3.3 V and GND.
3. Connect servo signal to ESP32 GPIO 4.
4. Power the DS3245 servo from the UBEC.
5. Tie UBEC negative / servo ground to ESP32 GND.

Recommended battery layout:

| Battery path | Goes to |
| --- | --- |
| 7.4 V battery to UBEC | DS3245 servo power |
| 7.4 V battery to buck converter set to 5.0 V | ESP32 5 V/VIN input, if not using USB |
| All grounds | Common ground together |

## Basic test procedure

1. Upload firmware.
2. Keep the MPU6050 still for about one second after startup while gyro bias is sampled.
3. Run the GUI and connect to the correct COM port.
4. In Simple tester mode, hold **LEFT** or **RIGHT** and confirm the servo command changes.
5. Press **CENTER SERVO** and confirm servo command goes to 90 degrees.
6. In **MANUAL MODE**, use GUI left/right controls and confirm servo command changes.
7. Rotate the MPU6050 by hand and confirm yaw changes.
8. Use **ZERO GYRO** before a deliberate test.
9. Aim the servo/head/laser at a wall target.
10. Press **FIX TARGET**.
11. Rotate the MPU6050/base by hand.
12. Confirm servo command moves opposite the rotation.
13. Confirm **DISENGAGED** triggers at configured angle limits or excessive error.

## Modes and controls

- **MANUAL**: GUI left/right jog commands change the servo command; yaw remains visible.
- **FIXED**: uses `servo = fixed_servo + Kp * (-(yaw - fixed_yaw))`.
- **DISENGAGED**: holds the last servo command after a limit or max-error event.
- **RECENTER**: smoothly drives back to 90 degrees, then enters MANUAL.
- **ZERO GYRO**: resets integrated yaw to zero.
- **CENTER SERVO**: immediately commands 90 degrees.

The tuning sliders send their values when released. The active tuning telemetry field confirms the values currently active in the controller. Tuning values are stored only until the controller resets.

## Tuning reference

| Setting | What it changes | Good first range |
| --- | --- | --- |
| Servo minimum angle | Lowest command the software may send to the servo. Set above any mechanical stop. | 20-30 deg |
| Servo maximum angle | Highest command the software may send to the servo. Set below any mechanical stop. | 150-160 deg |
| Manual speed | Servo travel speed in MANUAL mode while a GUI left/right jog command is held. | 20-40 deg/s |
| Proportional Kp | Degrees of servo correction requested per degree of yaw error in FIXED mode. | 0.5-1.0 |
| Deadband degrees | Small yaw errors ignored in FIXED mode. | 0.3-1.0 deg |
| Max error before disengage | Safety threshold that switches to DISENGAGED. | 30-45 deg |
| Gyro drift correction / trim | Manual yaw-rate offset in degrees/second. | Usually -0.2 to +0.2 deg/s |

Calm first settings:

```text
Manual speed: 25
Kp: 0.7
Deadband: 0.5
Max error: 45
```

## CSV logging

Use **Start / Stop Logging** to choose a CSV file.

CSV logs include:

- timestamp
- yaw
- yaw rate
- servo command
- target/fixed yaw
- error angle
- mode
- joystick values
- limit status
- active tuning values

Telemetry logs are ignored by Git by default.

## Troubleshooting

- If yaw is reversed, change the yaw axis/sign in firmware.
- If the servo moves the same direction as the base in FIXED mode, reverse the correction sign or mechanical linkage.
- If no MPU6050 data appears, check I2C address and wiring. Some modules use `0x69` when AD0 is high.
- If the servo moves randomly, check shared ground first.
- If the servo does nothing with ground connected, run the servo sweep test and measure voltage at the servo.
- If Arduino IDE can upload but the GUI cannot connect, close Serial Monitor and refresh the GUI port list.
