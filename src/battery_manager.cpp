#include "battery_manager.h"

#include <SparkFunBQ27441.h>

namespace {
bool batteryReady = false;
uint8_t batteryPercent = 0;
int batteryMillivolts = 0;
}

void initBattery() {
  batteryReady = lipo.begin();
  if (!batteryReady) return;
  lipo.setCapacity(1000);
  updateBattery();
}

void updateBattery() {
  if (!batteryReady) return;
  batteryPercent = constrain((int)lipo.soc(), 0, 100);
  batteryMillivolts = lipo.voltage();
}

bool isBatteryReady() {
  return batteryReady;
}

uint8_t getBatteryPercent() {
  return batteryPercent;
}

int getBatteryMillivolts() {
  return batteryMillivolts;
}
