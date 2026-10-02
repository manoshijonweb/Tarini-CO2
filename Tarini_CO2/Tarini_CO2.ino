// Tarini CO2 ventilation controller
// ESP32 + MH-Z19E CO2 sensor + 2-channel relay (intake & exhaust fans) + 16x2 I2C LCD
// Web app: http://tarini.local, the IP shown on the LCD, or http://192.168.4.1 on the
// "Tarini-CO2" hotspot.

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>

#include "config.h"
#include "mhz19.h"
#include "lcd_i2c.h"
#include "web_page.h"

HardwareSerial co2Serial(2);
MHZ19 co2;
LcdI2C lcd;
WebServer server(80);
DNSServer dns;
Preferences prefs;

// ---------------- State ----------------
struct {
  int ppm = -1;
  int tempC = 0;
  uint32_t lastOkAt = 0;   // millis() of last valid reading
  uint32_t errors = 0;
  bool abcPending = true;  // apply ABC setting after the sensor first answers
} sensor;

struct {
  bool autoMode = true;
  bool intake = false;
  bool exhaust = false;
  int onPpm = DEFAULT_ON_PPM;
  int offPpm = DEFAULT_OFF_PPM;
  bool abc = DEFAULT_ABC;
} ctl;

struct Sample {
  uint16_t ppm;   // 0 = no valid reading
  uint8_t fans;   // bit0 intake, bit1 exhaust
};
Sample history[HISTORY_SIZE];
int histHead = 0, histCount = 0;

bool lcdOk = false;
uint32_t lcdRetryAt = 0;
char lcdShown[2][17] = {"", ""};

String staSsid, staPass;
volatile bool staTrying = false;
bool staWasUp = false;
uint32_t staAttemptAt = 0;
volatile uint8_t staDisconnectReason = 0;  // set by the Wi-Fi event handler during an attempt
uint8_t staLastFail = 0;                   // reason of the last failed attempt (255 = timeout)

// ---------------- Sensor & fans ----------------
bool sensorWarm() { return millis() >= SENSOR_WARMUP_MS; }
bool sensorOk() { return millis() - sensor.lastOkAt < SENSOR_FAULT_MS; }
bool readingValid() { return sensorWarm() && sensorOk() && sensor.ppm > 0; }
unsigned long warmupLeft() { return sensorWarm() ? 0 : (SENSOR_WARMUP_MS - millis() + 999) / 1000; }

const char *levelOf(int ppm) {
  if (ppm < 800) return "good";
  if (ppm < 1000) return "fair";
  if (ppm < 1500) return "poor";
  return "bad";
}

const int RELAY_PINS[2] = {PIN_RELAY_INTAKE, PIN_RELAY_EXHAUST};
bool relayOn[2] = {false, false};
uint32_t lastRelayOnAt = 0;

void setRelay(int pin, bool on) {
  digitalWrite(pin, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// Moves the relays towards the requested fan state. Fans switch on one at a time,
// FAN_STAGGER_MS apart, so their start-up currents don't add up and dip the supply.
void fanTick() {
  const bool want[2] = {ctl.intake, ctl.exhaust};
  for (int i = 0; i < 2; i++) {
    if (want[i] == relayOn[i]) continue;
    if (want[i] && millis() - lastRelayOnAt < FAN_STAGGER_MS) continue;
    setRelay(RELAY_PINS[i], want[i]);
    relayOn[i] = want[i];
    if (want[i]) {
      lastRelayOnAt = millis();
      Serial.printf("Relay %s ON (GPIO%d)\n", i ? "exhaust" : "intake", RELAY_PINS[i]);
    }
  }
}

void setFans(bool intake, bool exhaust, const char *why) {
  if (intake != ctl.intake || exhaust != ctl.exhaust)
    Serial.printf("Fans -> intake %s, exhaust %s (%s)\n",
                  intake ? "ON" : "OFF", exhaust ? "ON" : "OFF", why);
  ctl.intake = intake;
  ctl.exhaust = exhaust;
  fanTick();
}

const char *resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "Power on";
    case ESP_RST_EXT:      return "Reset button";
    case ESP_RST_SW:       return "Software restart";
    case ESP_RST_PANIC:    return "Crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "Watchdog (froze)";
    case ESP_RST_BROWNOUT: return "Brownout (power dipped)";
    default:               return "Unknown";
  }
}
uint32_t bootCount = 0;

void autoControl() {
  if (!ctl.autoMode) return;
  if (!sensorWarm()) { setFans(false, false, "warming up"); return; }
  if (!sensorOk())   { setFans(true, true, "sensor fault, fail-safe ventilation"); return; }
  if (sensor.ppm >= ctl.onPpm)       setFans(true, true, "CO2 high");
  else if (sensor.ppm <= ctl.offPpm) setFans(false, false, "CO2 back to normal");
  // between the two thresholds: keep the current state (hysteresis)
}

void readSensor() {
  int ppm, t;
  int r = co2.read(ppm, t);
  if (r != MHZ19::OK) {
    sensor.errors++;
    Serial.printf("Sensor read error %d (check TX/RX wiring and 5V)\n", r);
    return;
  }
  sensor.ppm = ppm;
  sensor.tempC = t;
  sensor.lastOkAt = millis();
  if (sensor.abcPending) {
    co2.setABC(ctl.abc);
    sensor.abcPending = false;
  }
  if (sensorWarm()) Serial.printf("CO2: %d ppm\n", ppm);
  else Serial.printf("Warming up, %lus left (raw %d ppm)\n", warmupLeft(), ppm);
}

void pushHistory() {
  history[histHead] = {(uint16_t)(readingValid() ? sensor.ppm : 0),
                       (uint8_t)((ctl.intake ? 1 : 0) | (ctl.exhaust ? 2 : 0))};
  histHead = (histHead + 1) % HISTORY_SIZE;
  if (histCount < HISTORY_SIZE) histCount++;
}

// ---------------- Wi-Fi state ----------------
const char *wifiState() {
  if (!staSsid.length()) return "off";
  if (WiFi.status() == WL_CONNECTED) return "connected";
  return staTrying ? "connecting" : "failed";
}

// Reason codes from esp_wifi_types.h (wifi_err_reason_t).
const char *wifiFailText(bool shortText) {
  switch (staLastFail) {
    case 201: case 210: case 211: case 212:   // NO_AP_FOUND (+ security / auth-mode variants)
      return shortText ? "No network found" : "Network not found. It must be a 2.4 GHz network";
    case 2: case 15: case 202: case 204:      // AUTH_EXPIRE, 4WAY_HANDSHAKE_TIMEOUT, AUTH_FAIL, HANDSHAKE_TIMEOUT
      return "Wrong password";
    default:
      return shortText ? "Retrying soon" : "The network did not respond";
  }
}

// ---------------- LCD ----------------
const uint8_t GLYPH_SUB2[8] = {0x00, 0x00, 0x00, 0x0C, 0x12, 0x04, 0x08, 0x1E};  // subscript 2
#define LCD_SUB2 "\x01"

bool lcdInit() {
  if (!lcd.begin()) return false;
  lcd.createChar(1, GLYPH_SUB2);
  lcdShown[0][0] = lcdShown[1][0] = '\0';
  Serial.printf("LCD found at 0x%02X\n", lcd.address());
  return true;
}

// Main screen: sensor status, or CO2 reading with air quality and fan state.
void lcdStatusScreen(char *l0, char *l1) {
  if (!sensorOk()) {
    snprintf(l0, 17, "Sensor error");
    snprintf(l1, 17, "Check wiring");
  } else if (!sensorWarm()) {
    snprintf(l0, 17, "Sensor warming");
    snprintf(l1, 17, "up, ready in %ds", (int)warmupLeft());
  } else {
    static const char *names[] = {"Good", "Fair", "Poor", "Bad"};
    int lv = sensor.ppm < 800 ? 0 : sensor.ppm < 1000 ? 1 : sensor.ppm < 1500 ? 2 : 3;
    const char *fans = ctl.intake && ctl.exhaust ? "Fans ON"
                     : ctl.intake               ? "Intake ON"
                     : ctl.exhaust              ? "Exhaust ON"
                                                : "Fans OFF";
    snprintf(l0, 17, "CO" LCD_SUB2 "%9d ppm", sensor.ppm);
    snprintf(l1, 17, "%-6s%10s", names[lv], fans);
  }
}

// Network screen: how to reach the web app.
void lcdNetworkScreen(char *l0, char *l1) {
  const char *st = wifiState();
  if (!strcmp(st, "connected")) {
    snprintf(l0, 17, "Wi-Fi connected");
    snprintf(l1, 17, "%s", WiFi.localIP().toString().c_str());
  } else if (!strcmp(st, "connecting")) {
    static const char *dots[] = {".", "..", "..."};
    snprintf(l0, 17, "Connecting to");
    snprintf(l1, 17, "Wi-Fi%s", dots[(millis() / 1000) % 3]);
  } else if (!strcmp(st, "failed")) {
    snprintf(l0, 17, "Wi-Fi failed");
    snprintf(l1, 17, "%s", wifiFailText(true));
  } else {
    snprintf(l0, 17, "Join %s", AP_SSID);
    snprintf(l1, 17, "%s", WiFi.softAPIP().toString().c_str());
  }
}

void updateLcd() {
  if (!lcdOk) {
    if (millis() < lcdRetryAt) return;
    lcdRetryAt = millis() + 10000;
    lcdOk = lcdInit();
    if (!lcdOk) return;
  }

  char l0[17], l1[17];
  if ((millis() / 4000) % 2 == 0) lcdStatusScreen(l0, l1);   // screens alternate every 4 s
  else lcdNetworkScreen(l0, l1);

  char *lines[2] = {l0, l1};
  for (int row = 0; row < 2; row++) {
    if (strcmp(lines[row], lcdShown[row]) == 0) continue;
    if (!lcd.printLine(row, lines[row])) {   // LCD unplugged: re-detect later
      lcdOk = false;
      Serial.println("LCD not responding");
      return;
    }
    strcpy(lcdShown[row], lines[row]);
  }
}

// ---------------- Wi-Fi ----------------
void connectSta() {
  Serial.printf("Wi-Fi: connecting to \"%s\"\n", staSsid.c_str());
  staDisconnectReason = 0;
  staTrying = true;
  staAttemptAt = millis();
  WiFi.begin(staSsid.c_str(), staPass.c_str());
}

void startWiFi() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);   // reconnects are handled in wifiTick()
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
    if (staTrying) staDisconnectReason = info.wifi_sta_disconnected.reason;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dns.start(53, "*", WiFi.softAPIP());   // captive portal on the hotspot
  if (staSsid.length()) connectSta();
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
  Serial.printf("Hotspot \"%s\" (password %s) at http://%s\n", AP_SSID, AP_PASSWORD,
                WiFi.softAPIP().toString().c_str());
}

// A failing station connection keeps the radio scanning, which makes the hotspot
// unreliable, so attempts are limited to 20 s every 2 minutes.
void wifiTick() {
  if (!staSsid.length()) return;
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    staTrying = false;
    if (!staWasUp) {
      staWasUp = true;
      Serial.printf("Wi-Fi: connected, web app at http://%s or http://%s.local\n",
                    WiFi.localIP().toString().c_str(), MDNS_NAME);
    }
    return;
  }
  if (staWasUp) {
    staWasUp = false;
    staAttemptAt = now - 120000;   // retry straight away after a drop
    Serial.println("Wi-Fi: connection lost");
  }
  // An attempt ends on a reported disconnect reason (e.g. wrong password) or after 20 s.
  if (staTrying && (staDisconnectReason || now - staAttemptAt > 20000)) {
    staLastFail = staDisconnectReason ? staDisconnectReason : 255;
    staTrying = false;              // before disconnect(), so its own event is not recorded
    WiFi.disconnect();
    staAttemptAt = now;
    Serial.printf("Wi-Fi: could not connect (%s, reason %d), retrying in 2 min\n",
                  wifiFailText(false), staLastFail);
  } else if (!staTrying && now - staAttemptAt > 120000) {
    connectSta();
  }
}

// Applies Wi-Fi settings saved from the web app, without a restart.
void applyWifiChange() {
  staTrying = false;
  WiFi.disconnect();
  staWasUp = false;
  staLastFail = 0;
  if (staSsid.length()) connectSta();
  else Serial.println("Wi-Fi: cleared, hotspot only");
}

// ---------------- Web API ----------------
struct Json {
  String s = "{";
  void key(const char *k) {
    if (s.length() > 1) s += ',';
    s += '"'; s += k; s += "\":";
  }
  void num(const char *k, long v) { key(k); s += v; }
  void flag(const char *k, bool v) { key(k); s += v ? "true" : "false"; }
  void str(const char *k, const String &v) {
    key(k);
    s += '"';
    for (char c : v) {
      if (c == '"' || c == '\\') { s += '\\'; s += c; }
      else if ((uint8_t)c < 0x20) s += ' ';
      else s += c;
    }
    s += '"';
  }
  String done() { return s + '}'; }
};

void sendJson(int code, const String &body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}
void sendError(int code, const char *msg) {
  Json j; j.str("error", msg); sendJson(code, j.done());
}
void sendOk() { sendJson(200, "{\"ok\":true}"); }

void handleStatus() {
  bool sta = WiFi.status() == WL_CONNECTED;
  Json j;
  j.num("ppm", sensor.ppm);
  j.num("temp", sensor.tempC);
  j.str("level", sensor.ppm > 0 ? levelOf(sensor.ppm) : "unknown");
  j.flag("warm", sensorWarm());
  j.num("warmupLeft", warmupLeft());
  j.flag("sensorOk", sensorOk());
  j.num("errors", sensor.errors);
  j.flag("intake", ctl.intake);
  j.flag("exhaust", ctl.exhaust);
  j.str("mode", ctl.autoMode ? "auto" : "manual");
  j.num("onPpm", ctl.onPpm);
  j.num("offPpm", ctl.offPpm);
  j.flag("abc", ctl.abc);
  j.num("uptime", millis() / 1000);
  j.flag("staConnected", sta);
  j.str("wifi", wifiState());
  j.str("wifiError", !strcmp(wifiState(), "failed") ? wifiFailText(false) : "");
  j.str("ssid", staSsid);
  j.str("ip", sta ? WiFi.localIP().toString() : "");
  j.num("rssi", sta ? WiFi.RSSI() : 0);
  j.str("apSsid", AP_SSID);
  j.str("apIp", WiFi.softAPIP().toString());
  j.str("mdns", MDNS_NAME);
  j.num("heap", ESP.getFreeHeap());
  j.flag("lcd", lcdOk);
  j.str("resetReason", resetReasonText());
  j.num("boots", bootCount);
  sendJson(200, j.done());
}

void handleHistory() {
  String ppm, fans;
  ppm.reserve(histCount * 5);
  fans.reserve(histCount * 2);
  for (int i = 0; i < histCount; i++) {
    const Sample &s = history[(histHead - histCount + i + HISTORY_SIZE) % HISTORY_SIZE];
    if (i) { ppm += ','; fans += ','; }
    ppm += s.ppm;
    fans += s.fans;
  }
  sendJson(200, "{\"interval\":" + String(HISTORY_INTERVAL_MS / 1000) +
                ",\"ppm\":[" + ppm + "],\"fan\":[" + fans + "]}");
}

void handleMode() {
  String m = server.arg("mode");
  if (m != "auto" && m != "manual") return sendError(400, "mode must be auto or manual");
  ctl.autoMode = (m == "auto");
  Serial.printf("Mode -> %s\n", m.c_str());
  autoControl();
  sendOk();
}

void handleFan() {
  String fan = server.arg("fan"), state = server.arg("state");
  if (state != "on" && state != "off") return sendError(400, "state must be on or off");
  bool on = (state == "on");
  bool intake = ctl.intake, exhaust = ctl.exhaust;
  if (fan == "intake") intake = on;
  else if (fan == "exhaust") exhaust = on;
  else if (fan == "both") intake = exhaust = on;
  else return sendError(400, "fan must be intake, exhaust or both");
  if (ctl.autoMode) Serial.println("Mode -> manual");
  ctl.autoMode = false;
  setFans(intake, exhaust, "manual");
  sendOk();
}

void handleSettings() {
  int on = server.arg("onPpm").toInt(), off = server.arg("offPpm").toInt();
  if (on < 450 || on > 5000) return sendError(400, "Fans-on level must be 450-5000 ppm");
  if (off < 400 || off > on - 50) return sendError(400, "Fans-off level must be at least 50 ppm below fans-on");
  bool abc = server.arg("abc") == "1";
  ctl.onPpm = on;
  ctl.offPpm = off;
  if (abc != ctl.abc) co2.setABC(abc);
  ctl.abc = abc;
  prefs.putInt("on", on);
  prefs.putInt("off", off);
  prefs.putBool("abc", abc);
  Serial.printf("Settings -> on %d ppm, off %d ppm, ABC %s\n", on, off, abc ? "on" : "off");
  autoControl();
  sendOk();
}

void handleCalibrate() {
  if (!sensorWarm() || !sensorOk()) return sendError(409, "Sensor is not ready yet");
  co2.calibrateZero();
  Serial.println("Sensor zero-point calibration sent (current air = 400 ppm)");
  sendOk();
}

void handleWifi() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() > 32) return sendError(400, "Wi-Fi name is too long");
  if (pass.length() && (pass.length() < 8 || pass.length() > 63))
    return sendError(400, "Wi-Fi password must be 8-63 characters");
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  staSsid = ssid;
  staPass = pass;
  Serial.printf("Wi-Fi settings saved (\"%s\")\n", ssid.c_str());
  sendOk();
  applyWifiChange();   // the browser is on the hotspot, so dropping the station link is safe
}

void handleNotFound() {
  // On the hotspot, send phones' connectivity checks to the dashboard (captive portal).
  if (server.client().localIP() == WiFi.softAPIP()) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void setupServer() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/fan", HTTP_POST, handleFan);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound(handleNotFound);
  server.begin();
}

// ---------------- Main ----------------
void setup() {
  Serial.begin(115200);
  for (int pin : RELAY_PINS) {
    pinMode(pin, OUTPUT);
    setRelay(pin, false);
  }
  Serial.println("\nTarini CO2 controller starting");

  prefs.begin("tarini", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  Serial.printf("Last reset: %s (boot #%lu)\n", resetReasonText(), (unsigned long)bootCount);
  ctl.onPpm = prefs.getInt("on", DEFAULT_ON_PPM);
  ctl.offPpm = prefs.getInt("off", DEFAULT_OFF_PPM);
  ctl.abc = prefs.getBool("abc", DEFAULT_ABC);
  staSsid = prefs.getString("ssid", WIFI_SSID);
  staPass = prefs.getString("pass", WIFI_PASSWORD);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcdOk = lcdInit();
  if (lcdOk) {
    lcd.printLine(0, "Tarini CO" LCD_SUB2);
    lcd.printLine(1, "Starting...");
  } else {
    Serial.println("LCD not found (check SDA/SCL and 5V), will keep retrying");
    lcdRetryAt = millis() + 10000;
  }

  co2.begin(co2Serial, PIN_CO2_RX, PIN_CO2_TX);
  startWiFi();
  setupServer();
  Serial.printf("Auto mode: fans on >= %d ppm, off <= %d ppm\n", ctl.onPpm, ctl.offPpm);
}

void loop() {
  server.handleClient();
  dns.processNextRequest();
  wifiTick();
  fanTick();

  static uint32_t lastRead = 0, lastHist = 0, lastLcd = 0;
  uint32_t now = millis();
  if (now - lastRead >= SENSOR_READ_MS) {
    lastRead = now;
    readSensor();
    autoControl();
  }
  if (sensorWarm() && now - lastHist >= HISTORY_INTERVAL_MS) {
    lastHist = now;
    pushHistory();
  }
  if (now - lastLcd >= 1000) {
    lastLcd = now;
    updateLcd();
  }
  delay(2);
}
