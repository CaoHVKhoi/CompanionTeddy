#pragma once

#include <Arduino.h>

void initBattery();
void updateBattery();
bool isBatteryReady();
uint8_t getBatteryPercent();
int getBatteryMillivolts();
