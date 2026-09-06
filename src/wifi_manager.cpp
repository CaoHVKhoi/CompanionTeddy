#include "wifi_manager.h"

#include <Preferences.h>
#include <WiFi.h>

#include "display_manager.h"

namespace {
Preferences preferences;
bool wifiConnected = false;
bool provisioned = false;
String accessToken;
}

void saveProvisioning(const String &payload) {
  const int first = payload.indexOf('|');
  const int second = payload.indexOf('|', first + 1);
  if (first < 1 || second <= first + 1 || second >= (int)payload.length() - 1) {
    drawStatus("Ghep noi khong dung", "Quet QR va thu lai");
    return;
  }

  const String ssid = payload.substring(0, first);
  const String password = payload.substring(first + 1, second);
  accessToken = payload.substring(second + 1);
  preferences.begin("companion", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("token", accessToken);
  preferences.end();
  provisioned = true;
  drawStatus("Da nhan cau hinh", "Dang ket noi Wi-Fi");
  connectSavedWifi();
}

void connectSavedWifi() {
  preferences.begin("companion", true);
  const String ssid = preferences.getString("ssid", "");
  const String password = preferences.getString("password", "");
  accessToken = preferences.getString("token", "");
  preferences.end();
  provisioned = ssid.length() > 0;
  if (!provisioned) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 10000) delay(100);
  wifiConnected = WiFi.status() == WL_CONNECTED;
  drawStatus(wifiConnected ? "San sang de noi" : "Khong vao duoc Wi-Fi",
             wifiConnected ? "Nhan nut de hoi" : "Quet QR tren app");
}

bool isWifiConnected() {
  return wifiConnected;
}

bool isProvisioned() {
  return provisioned;
}

const String &getAccessToken() {
  return accessToken;
}
