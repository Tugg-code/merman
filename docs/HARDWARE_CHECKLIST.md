# Hardware checklist

## ESP32-S3 default pin map

These are the defaults in `arduino/fishfinder_stabilizer_esp32_s3/fishfinder_stabilizer_esp32_s3.ino`.

| Function | ESP32-S3 GPIO |
| --- | --- |
| MPU6050 SDA | GPIO 8 |
| MPU6050 SCL | GPIO 9 |
| Joystick VRx | GPIO 1 |
| Joystick VRy | GPIO 2 |
| Joystick SW | GPIO 5 |
| Servo signal | GPIO 4 |

Change these constants at the top of the sketch if the physical board labels make another pin more convenient.

## DS3245 servo wiring

| Servo wire | Function | Connects to |
| --- | --- | --- |
| Yellow/orange/white | Signal | ESP32 GPIO 4 by default |
| Red | Servo positive | UBEC positive, 5-6 V |
| Brown/black | Servo ground | UBEC negative |

Also connect UBEC negative to ESP32 GND. Without this shared ground, the servo signal floats and the servo may move randomly.

## Recommended power layout

```text
7.4 V battery
  |-- UBEC 5-6 V output --> DS3245 red/brown power wires
  |
  |-- Buck converter set to 5.0 V --> ESP32 5V/VIN, if not using USB

UBEC ground, buck ground, ESP32 ground, joystick ground, and MPU6050 ground are common.
```

For early bench testing, use USB for ESP32 power and the UBEC only for the servo. This keeps servo power noise away from the ESP32 until the servo is known-good.

## Voltage rules

- Do not feed 7.4 V directly into the ESP32.
- Do not feed 5 V into the ESP32 `3V3` pin.
- Power joystick from ESP32 3.3 V so its analog outputs do not exceed ESP32 ADC limits.
- Prefer powering MPU6050 from 3.3 V when connected to ESP32 I2C.

## Bring-up order

1. ESP32 alone: upload `esp32_s3_smoke_test.ino`.
2. ESP32 + Serial Monitor: confirm text prints at 115200 baud.
3. Servo only: upload `esp32_s3_servo_sweep_test.ino`.
4. Add MPU6050 and joystick.
5. Upload full stabilizer sketch.
6. Connect Python GUI.
7. Test `CENTER`, then `MANUAL`, then `FIXED`.

