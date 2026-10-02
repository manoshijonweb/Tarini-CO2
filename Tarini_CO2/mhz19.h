#pragma once
#include <Arduino.h>

// Minimal MH-Z19E driver over UART (9600 8N1).
class MHZ19 {
public:
  enum Result { OK = 0, TIMEOUT = -1, BAD_HEADER = -2, BAD_CHECKSUM = -3 };

  void begin(HardwareSerial &serial, int rxPin, int txPin) {
    _s = &serial;
    _s->begin(9600, SERIAL_8N1, rxPin, txPin);
    _s->setTimeout(300);
  }

  // tempC is the sensor's internal temperature (approximate, undocumented byte).
  int read(int &ppm, int &tempC) {
    uint8_t r[9];
    send(0x86, 0x00);
    if (_s->readBytes(r, 9) != 9) return TIMEOUT;
    if (r[0] != 0xFF || r[1] != 0x86) return BAD_HEADER;
    if (r[8] != checksum(r)) return BAD_CHECKSUM;
    ppm = (r[2] << 8) | r[3];
    tempC = (int)r[4] - 40;
    return OK;
  }

  void setABC(bool on) { send(0x79, on ? 0xA0 : 0x00); }

  // Sets the current reading as 400 ppm. Only use in fresh outdoor air.
  void calibrateZero() { send(0x87, 0x00); }

private:
  HardwareSerial *_s = nullptr;

  static uint8_t checksum(const uint8_t *p) {
    uint8_t sum = 0;
    for (int i = 1; i < 8; i++) sum += p[i];
    return 0xFF - sum + 1;
  }

  void send(uint8_t cmd, uint8_t arg) {
    uint8_t p[9] = {0xFF, 0x01, cmd, arg, 0, 0, 0, 0, 0};
    p[8] = checksum(p);
    while (_s->available()) _s->read();  // drop stale bytes / earlier command acks
    _s->write(p, 9);
  }
};
