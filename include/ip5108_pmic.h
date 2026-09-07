#pragma once

#include <Arduino.h>
#include <Wire.h>

class Ip5108Pmic {
public:
  bool begin(uint8_t addr = 0x75, TwoWire *wire = &Wire, int8_t intPin = -1);
  bool ready() const;
  bool waitForI2CReady(uint32_t timeoutMs = 2000);

  void setCharger(bool enabled);
  void setBoost(bool enabled);
  void setFlashLight(bool enabled);

  bool chargerEnabled() const;
  bool boostEnabled() const;
  bool charging() const;

  float batteryVoltage() const;
  float batteryCurrent() const;
  int8_t batteryPercentage() const;

private:
  TwoWire *_wire = &Wire;
  uint8_t _addr = 0x75;
  int8_t _intPin = -1;

  bool writeRegister8(uint8_t reg, uint8_t value) const;
  uint8_t readRegister8(uint8_t reg, bool *ok = nullptr) const;
  bool readRegisterBit(uint8_t reg, uint8_t bit) const;
  float decodeVoltageADC(uint8_t low, uint8_t high) const;
  float decodeCurrentADC(uint8_t low, uint8_t high) const;
  int8_t calcBatteryPercentage(float ocv) const;
};
