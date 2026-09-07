#include "battery_manager.h"

#include "device_config.h"
#include "ip5108_pmic.h"

namespace {
  bool batteryReady = false;
  uint8_t batteryPercent = 0;
  int batteryMillivolts = 0;
  Ip5108Pmic pmic;
}

void initBattery() {
  // The PMIC shares the main I2C bus with the OLED display.
  batteryReady = pmic.begin(DeviceConfig::IP5108_I2C_ADDRESS, &Wire, DeviceConfig::IP5108_INT_PIN);
  if (!batteryReady) return;

  // Keep charging enabled and provide the regulated output to the system.
  pmic.setCharger(true);
  pmic.setBoost(true);
  updateBattery();
}

// Refresh cached battery percentage and voltage values for the UI and BLE.
void updateBattery() {
  if (!batteryReady) return;

  const int8_t percent = pmic.batteryPercentage();
  if (percent >= 0) {
    batteryPercent = static_cast<uint8_t>(constrain(percent, 0, 100));
  }

  const float voltageV = pmic.batteryVoltage();
  if (voltageV > 0.0f) {
    batteryMillivolts = static_cast<int>(voltageV * 1000.0f + 0.5f);
  }
}

// Report whether the PMIC responded successfully during initialization.
bool isBatteryReady() {
  return batteryReady;
}

// Return the last valid battery percentage reported by the PMIC.
uint8_t getBatteryPercent() {
  return batteryPercent;
}

// Return the last valid battery voltage in millivolts.
int getBatteryMillivolts() {
  return batteryMillivolts;
}
