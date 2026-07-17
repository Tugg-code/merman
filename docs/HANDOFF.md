# Project handoff: Fish Finder Stabilizer Test POC

This handoff is written for a future Codex session, a future maintainer, or a tired version of us standing at the bench with wires everywhere.

## Current project goal

Build and test a proof-of-concept gyro-stabilized fish finder/sonar head. The head can be manually aimed from the GUI, then locked to a fixed target direction. When the base rotates, the servo rotates in the opposite direction to approximately hold the target.

This is intentionally a simple bench prototype. It does not use GPS, compass, magnetometer, PID, brushless motors, or Raspberry Pi direct hardware control yet.

## Current architecture

```text
Python tkinter GUI on laptop
        |
        | USB serial, 115200 baud
        v
Microcontroller firmware
        |
        | I2C / PWM
        v
MPU6050 + servo
```

The Python GUI does not know or care whether the controller is an Arduino Uno or ESP32-S3 as long as the firmware uses the same serial protocol.

## Important files

| Path | Purpose |
| --- | --- |
| `main.py` | Tkinter GUI, controls, telemetry display, tuning sliders, CSV logging |
| `serial_interface.py` | Threaded serial reader/writer using pyserial |
| `telemetry.py` | JSON telemetry parser and CSV row formatting |
| `plot_panel.py` | Embedded matplotlib live plots |
| `config.py` | GUI defaults |
| `requirements.txt` | Python dependencies |
| `arduino/fishfinder_stabilizer_test/fishfinder_stabilizer_test.ino` | Arduino Uno firmware |
| `arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino` | ESP32-S3 firmware |
| `arduino/esp32_s3_smoke_test/esp32_s3_smoke_test.ino` | Minimal ESP32 upload/serial test |
| `arduino/esp32_s3_servo_sweep_test/esp32_s3_servo_sweep_test.ino` | Minimal ESP32 servo PWM test |
| `docs/ESP32_S3_FINAL_WIRING.md` | Final-test ESP32-S3 wiring and pinout |
| `README.md` | Main user setup and operating instructions |

## Serial telemetry protocol

Firmware sends newline-delimited JSON at about 20 Hz:

```json
{"yaw":12.4,"yaw_rate":-3.2,"joy_x":512,"joy_y":510,"servo":88,"target":15.0,"error":-2.6,"mode":"FIXED","limit":false,"kp":0.7,"deadband":0.5,"manual_speed":25.0,"gyro_trim":0.03}
```

The GUI sends plain text commands ending in newline:

```text
FIX
MANUAL
DISENGAGE
RECENTER
ZERO
CENTER
SET KP 0.7
SET DEADBAND 0.5
SET LIMITS 20 160
SET MANUAL_SPEED 25
SET MAX_ERROR 45
SET GYRO_TRIM 0.05
JOG LEFT
JOG RIGHT
JOG STOP
NUDGE LEFT
NUDGE RIGHT
```

## Control behavior

Modes:

- `MANUAL`: GUI left/right jog commands change servo angle.
- `FIXED`: stores current yaw and servo angle, then applies opposite servo correction from gyro-integrated yaw change.
- `DISENGAGED`: holds last servo command after a limit or max-error event.
- `RECENTER`: moves back toward 90 degrees, then returns to manual.

FIXED equation:

```text
yaw_delta = yaw - fixed_yaw
error = -yaw_delta
servo_request = fixed_servo + Kp * error
```

The firmware applies deadband, servo min/max limits, and a correction rate limit.

## Known hardware state from testing

- Original Uno + SG90-style proof-of-concept worked after fixing a loose servo ground.
- Manual mode originally looked dead because manual speed was too low; GUI slider now allows up to 60 deg/s.
- GUI/firmware active tuning telemetry was added because the Uno initially was not parsing float commands correctly.
- ESP32-S3 N16R8 arrived and was added as a supported controller.
- DS3245 / 45 kg waterproof servo arrived.
- User has:
  - ESP32-S3 N16R8
  - DS3245 45 kg waterproof servo
  - UBEC rated 8 A, 16 A peak
  - 7.4 V RC car battery
  - Buck converter for stepping battery down to 5 V
  - MPU6050
  - Analog joystick, now disabled for final-test control

## Known ESP32 notes

- Use Arduino C++ in Arduino IDE.
- Board package: `esp32 by Espressif Systems`.
- Likely board selection: `ESP32S3 Dev Module`.
- For many ESP32-S3 boards, enable USB CDC on boot if using native USB serial.
- Some boards expose changing COM ports depending on bootloader/application mode.
- A CH343/CH340-style serial device may appear as `USB-Enhanced-SERIAL CH343`.

## Current unresolved/active bench issue

The ESP32 telemetry connects and looks good in the GUI. The final-test direction disables joystick control; manual movement now comes from GUI commands.

Servo behavior is still under debug:

- With servo signal ground not shared, servo moves randomly. This is expected and confirms the signal is floating.
- With the servo ground properly tied to the servo supply and ESP32 GND, the servo does not move.

Next diagnostic should be the ESP32 servo-only sweep test, with nothing else in the loop:

`arduino/esp32_s3_servo_sweep_test/esp32_s3_servo_sweep_test.ino`

If the sweep test does not move the servo:

1. Measure voltage directly at servo red/brown while connected.
2. Confirm servo signal is on the actual GPIO number configured in the sketch.
3. Confirm UBEC negative is tied to ESP32 GND.
4. Try a different ESP32 GPIO and update `SERVO_PIN`.
5. Use the UBEC instead of the old 5 V 1 A supply for the DS3245.
6. Confirm the DS3245 signal wire is yellow/orange/white, red is positive, brown/black is ground.

## Safe next development steps

1. Verify ESP32 servo sweep test.
2. Once servo moves, return to the ESP32 stabilizer sketch.
3. Confirm Simple tester mode can command left/right motion from the GUI.
4. Confirm `CENTER` moves servo to center.
5. Confirm `MANUAL` moves servo.
6. Confirm `FIX` causes opposite movement when rotating the MPU6050.
7. Only then mount hardware mechanically.

## Things not to add yet

- Magnetometer/compass
- GPS
- PID loop
- Brushless motor controller
- Raspberry Pi direct servo/I2C control
- Automatic target tracking

Those are later architecture decisions. The current job is to make the simple gyro-rate, servo, and GUI-commanded manual-control loop reliable.
