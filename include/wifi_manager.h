#pragma once

#include <Arduino.h>

void connectSavedWifi();
void saveProvisioning(const String &payload);
bool isWifiConnected();
bool isProvisioned();
const String &getAccessToken();
