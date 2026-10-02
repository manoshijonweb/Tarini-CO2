#pragma once
#include <Arduino.h>
#include <Wire.h>

// Minimal driver for a 16x2 HD44780 LCD on a PCF8574 I2C backpack.
// Backpack wiring: P0=RS P1=RW P2=EN P3=backlight P4..P7=D4..D7
class LcdI2C {
public:
  // addr 0 = auto-detect (PCF8574 0x20-0x27 or PCF8574A 0x38-0x3F; usually 0x27 or 0x3F)
  bool begin(uint8_t addr = 0) {
    _addr = addr ? addr : detect();
    if (!_addr) return false;
    _err = false;
    expanderWrite(0);
    delay(50);
    write4(0x30); delayMicroseconds(4500);
    write4(0x30); delayMicroseconds(4500);
    write4(0x30); delayMicroseconds(150);
    write4(0x20);           // 4-bit mode
    command(0x28);          // 2 lines, 5x8 font
    command(0x0C);          // display on, cursor off
    command(0x06);          // left-to-right entry
    command(0x01);          // clear
    delay(2);
    return !_err;
  }

  uint8_t address() const { return _addr; }

  // Defines custom character `slot` (0-7) from 8 rows of 5 bits. Print it as char `slot`.
  void createChar(uint8_t slot, const uint8_t rows[8]) {
    command(0x40 | ((slot & 7) << 3));
    for (uint8_t i = 0; i < 8; i++) send(rows[i], RS);
  }

  // Writes a full row, padded with spaces to 16 chars. Returns false on an I2C error.
  bool printLine(uint8_t row, const char *text) {
    _err = false;
    command(0x80 | (row ? 0x40 : 0x00));
    for (uint8_t i = 0; i < 16; i++) {
      char c = *text ? *text++ : ' ';
      send(c, RS);
    }
    return !_err;
  }

private:
  uint8_t _addr = 0;
  bool _err = false;
  static const uint8_t BACKLIGHT = 0x08, EN = 0x04, RS = 0x01;

  static uint8_t detect() {
    const uint8_t ranges[2][2] = {{0x20, 0x27}, {0x38, 0x3F}};
    for (auto &r : ranges)
      for (uint8_t a = r[1]; a >= r[0]; a--) {   // 0x27 / 0x3F first
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) return a;
      }
    return 0;
  }

  void expanderWrite(uint8_t v) {
    Wire.beginTransmission(_addr);
    Wire.write(v | BACKLIGHT);
    if (Wire.endTransmission() != 0) _err = true;
  }

  void write4(uint8_t v) {
    expanderWrite(v | EN);
    delayMicroseconds(1);
    expanderWrite(v & ~EN);
    delayMicroseconds(50);
  }

  void send(uint8_t v, uint8_t mode) {
    write4((v & 0xF0) | mode);
    write4(((v << 4) & 0xF0) | mode);
  }

  void command(uint8_t v) { send(v, 0); }
};
