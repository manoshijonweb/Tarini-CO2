# Tarini CO2 ventilation controller

ESP32 firmware that reads an MH-Z19E CO2 sensor, switches an intake fan and an exhaust fan through a 2-channel relay, shows the readings on a 16x2 I2C LCD, and serves a web dashboard.

## Wiring

| Part | Pin | ESP32 |
|---|---|---|
| MH-Z19E | Vin / GND | 5V / GND |
| | TX / RX | GPIO16 / GPIO17 |
| 16x2 I2C LCD | VCC / GND | 5V / GND |
| | SDA / SCL | GPIO21 / GPIO22 |
| 2-ch relay | VCC / GND | 5V / GND |
| | IN1 (exhaust) / IN2 (intake) | GPIO26 / GPIO27 |

Fans: 12V (+) to relay COM, relay NO to fan (+), fan (−) to 12V (−). Connect all grounds together.

Pins and default settings are in `Tarini_CO2/config.h`.

## Flashing

1. Open `Tarini_CO2/Tarini_CO2.ino` in the Arduino IDE.
2. Board: **ESP32 Dev Module** (esp32 by Espressif). No extra libraries are needed.
3. Optionally put your Wi-Fi name and password in `config.h`, or set them later in the web app under **Setup → Wi-Fi** (2.4 GHz networks only).
4. Upload, then open the Serial Monitor at 115200 baud. If the upload fails with "Wrong boot mode", hold the **BOOT** button while it connects.

## Opening the web app

- **Your Wi-Fi:** use the IP address shown on the LCD's Wi-Fi screen, or http://tarini.local.
- **No router:** join the `Tarini-CO2` hotspot (password `tarini123`) and open http://192.168.4.1. Most phones open it automatically.

## LCD screens

The LCD alternates every 4 seconds between a status screen and a Wi-Fi screen.

| Status | Wi-Fi |
|---|---|
| `Sensor warming` / `up, ready in 12s` | `Connecting to` / `Wi-Fi...` |
| `CO₂      1209 ppm` / `Poor     Fans ON` | `Wi-Fi connected` / `192.168.1.42` |
| `Sensor error` / `Check wiring` | `Wi-Fi failed` / `Wrong password` |
| | `Join Tarini-CO2` / `192.168.4.1` (no Wi-Fi set up) |

The dashboard shows live CO2, air-quality level, both fans, a one-hour history chart and device status. You can also switch between Auto and Manual, turn each fan on or off, change the on/off thresholds, calibrate the sensor and set up Wi-Fi.

## How the fans behave (Auto mode)

- Both fans turn on at or above the fans-on level (default 1000 ppm) and off at or below the fans-off level (default 800 ppm). Between the two they keep their current state.
- During the 15-second sensor warm-up the fans stay off.
- If the sensor stops responding for 10 seconds, both fans turn on as a fail-safe.
- Turning a fan on or off from the web app switches to Manual mode. Manual mode resets to Auto after a reboot.
