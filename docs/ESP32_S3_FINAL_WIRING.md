# ESP32-S3 final test wiring

This wiring is for the ESP32-S3 N16R8 final-test setup, where manual movement is controlled from the GUI instead of the joystick.

## Important board-label note

ESP32-S3 N16R8 development boards are not all laid out the same. The GPIO numbers in firmware are stable, but the printed power pin labels can vary by board.

Common power labels:

| Board label | Meaning | Use |
| --- | --- | --- |
| `GND` | Ground | Tie all grounds together |
| `3V3` or `3.3V` | Regulated 3.3 V output | MPU6050 VCC |
| `5V`, `VIN`, or `VBUS` | 5 V board input/output depending on board design | Use only if your board documentation/silkscreen confirms it |

Safest bench setup: power the ESP32-S3 from USB and power the DS3245 servo from the UBEC. Tie UBEC negative to ESP32 GND.

Do not connect 7.4 V battery directly to the ESP32. Do not connect 5 V to a `3V3` pin.

## Firmware pin map

These are the defaults in `arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino`.

| Function | ESP32-S3 GPIO | Notes |
| --- | --- | --- |
| MPU6050 SDA | GPIO 8 | I2C data |
| MPU6050 SCL | GPIO 9 | I2C clock |
| Servo signal | GPIO 4 | PWM signal only |
| Joystick X | Not used | Reserved in firmware only |
| Joystick Y | Not used | Reserved in firmware only |
| Joystick switch | Not used | Reserved in firmware only |

Manual left/right movement now comes from GUI serial commands:

```text
JOG LEFT
JOG RIGHT
JOG STOP
NUDGE LEFT
NUDGE RIGHT
```

## MPU6050 wiring

| MPU6050 pin | ESP32-S3 connection |
| --- | --- |
| VCC | `3V3` / `3.3V` |
| GND | `GND` |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

Some MPU6050 breakout boards tolerate 5 V VCC, but for ESP32-S3 use 3.3 V unless the module documentation says otherwise.

## DS3245 servo wiring

| Servo wire | Usually colored | Connects to |
| --- | --- | --- |
| Signal | Yellow/orange/white | ESP32 GPIO 4 |
| Positive power | Red | UBEC positive, 5-6 V |
| Ground | Brown/black | UBEC negative |

Also connect UBEC negative to ESP32 GND.

The servo signal wire alone is not enough. The ESP32 and servo supply must share ground or the servo will move randomly or ignore commands.

## Recommended power diagram

```text
7.4 V RC battery
  |
  +--> UBEC 5-6 V output
  |       |
  |       +--> DS3245 red wire
  |       +--> DS3245 brown/black wire
  |
  +--> optional buck converter set to 5.0 V
          |
          +--> ESP32 5V/VIN/VBUS only if your board confirms that input

ESP32 USB from laptop can replace the buck converter during bench testing.

Grounds:
  UBEC negative ---- ESP32 GND ---- MPU6050 GND
```

## Simple connection diagram

```text
ESP32-S3                         MPU6050
--------                         -------
3V3    ------------------------> VCC
GND    ------------------------> GND
GPIO8  ------------------------> SDA
GPIO9  ------------------------> SCL

ESP32-S3                         DS3245 Servo
--------                         ------------
GPIO4  ------------------------> Signal, yellow/orange/white
GND    ----+
          |
UBEC - ---+--------------------> Ground, brown/black
UBEC + ------------------------> Power, red
```

## Bring-up order

1. Upload `arduino/esp32_s3_smoke_test/esp32_s3_smoke_test.ino`.
2. Confirm Serial Monitor prints at 115200 baud.
3. Upload `arduino/esp32_s3_servo_sweep_test/esp32_s3_servo_sweep_test.ino`.
4. Confirm servo moves between 1000 us, 1500 us, and 2000 us.
5. Upload `arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino`.
6. Run `python main.py`.
7. Connect to the ESP32 COM port.
8. Use Simple tester mode: LEFT, RIGHT, LOCK TARGET, UNLOCK, RESET SERVO.
