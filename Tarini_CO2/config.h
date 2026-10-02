#pragma once

// ---------------- Wi-Fi ----------------
// Your router. Leave WIFI_SSID empty to run on the device's own hotspot only.
// You can also set or change Wi-Fi later from the web app (saved on the device,
// and that saved value takes priority over what is written here).
#define WIFI_SSID      ""
#define WIFI_PASSWORD  ""

// Hotspot the ESP32 always creates, so the web app is reachable even without a router.
#define AP_SSID        "Tarini-CO2"
#define AP_PASSWORD    "tarini123"     // at least 8 characters
#define MDNS_NAME      "tarini"        // web app at http://tarini.local

// ---------------- Pins ----------------
#define PIN_CO2_RX         16   // ESP32 RX2  <- MH-Z19E TX
#define PIN_CO2_TX         17   // ESP32 TX2  -> MH-Z19E RX
#define PIN_I2C_SDA        21   // LCD SDA
#define PIN_I2C_SCL        22   // LCD SCL
#define PIN_RELAY_INTAKE   27   // relay IN2
#define PIN_RELAY_EXHAUST  26   // relay IN1
#define RELAY_ACTIVE_LOW   true // most relay modules switch on when IN is LOW
#define FAN_STAGGER_MS     1500 // gap between switching the two fans on (limits supply dips)

// ---------------- Behaviour ----------------
#define SENSOR_WARMUP_MS     15000UL   // wait before readings are trusted (datasheet preheat is longer)
#define SENSOR_READ_MS       2000UL
#define SENSOR_FAULT_MS      10000UL   // no valid reply for this long = sensor fault
#define DEFAULT_ON_PPM       1000      // auto mode: fans on at or above this
#define DEFAULT_OFF_PPM      800       // auto mode: fans off at or below this
#define DEFAULT_ABC          true      // MH-Z19E automatic baseline calibration
#define HISTORY_INTERVAL_MS  10000UL   // one chart point every 10 s
#define HISTORY_SIZE         360       // 360 x 10 s = 1 hour
