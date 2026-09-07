#pragma once

#include <Arduino.h>
#include <Wire.h>

// Small IP5108 driver that keeps register access and conversion rules out of
// the application-level battery manager.
class Ip5108Pmic {
public:
  // Start using the selected I2C bus and optionally wait for L3/INT readiness.
  bool begin(uint8_t addr = 0x75, TwoWire *wire = &Wire, int8_t intPin = -1);
  // Check or wait for communication readiness.
  bool ready() const;
  bool waitForI2CReady(uint32_t timeoutMs = 2000);

  // Power-path controls exposed by the firmware.
  void setCharger(bool enabled);
  void setBoost(bool enabled);
  void setFlashLight(bool enabled);

  // Read current power-path state from the PMIC.
  bool chargerEnabled() const;
  bool boostEnabled() const;
  bool charging() const;

  // Measurements use volts, amps, and a percentage in the range 0-100.
  // A negative percentage indicates that the PMIC could not be read.
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
