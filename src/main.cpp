#include <Arduino.h>
#include <Wire.h>

#include "action_manager.h"
#include "audio_manager.h"
#include "battery_manager.h"
#include "ble_manager.h"
#include "device_config.h"
#include "display_manager.h"
#include "wifi_manager.h"

void setup() {
  pinMode(DeviceConfig::RECORD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DeviceConfig::ACTION_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(DeviceConfig::I2C_SDA_PIN, DeviceConfig::I2C_SCL_PIN);
  initDisplay();
  initBattery();
  initAudio();
  initBle();
  connectSavedWifi();
  if (!isProvisioned()) drawStatus("Quet QR tren app", "De cai Wi-Fi");
}

void loop() {
  const bool recordReading = digitalRead(DeviceConfig::RECORD_BUTTON_PIN) == LOW;
  const bool actionPressed = digitalRead(DeviceConfig::ACTION_BUTTON_PIN) == LOW;

  static bool recordWasPressed = false;
  static bool lastRecordReading = false;
  static unsigned long recordChangedAt = 0;
  if (recordReading != lastRecordReading) {
    lastRecordReading = recordReading;
    recordChangedAt = millis();
  }
  if (millis() - recordChangedAt >= 40 && recordWasPressed != lastRecordReading) {
    recordWasPressed = lastRecordReading;
    if (recordWasPressed) {
      if (isRecording()) {
        stopRecording();
      } else {
        startRecording();
      }
    }
  }
  if (isRecording()) captureAudioChunk();

  static bool actionWasPressed = false;
  if (actionPressed && !actionWasPressed && !isRecording()) playLocalReply();
  actionWasPressed = actionPressed;

  static unsigned long lastBatteryUpdate = 0;
  if (millis() - lastBatteryUpdate >= 5000) {
    lastBatteryUpdate = millis();
    updateBattery();
    publishStatus();
  }
}
