#pragma once

#include <Arduino.h>

namespace DeviceConfig {
constexpr char DEVICE_ID[] = "TEDDY-PROTO-001";
constexpr char BLE_SERVICE_UUID[] = "8d9e0c10-8c53-4d08-a0c0-7a7eb45fc001";
constexpr char PROVISIONING_UUID[] = "8d9e0c10-8c53-4d08-a0c0-7a7eb45fc002";
constexpr char STATUS_UUID[] = "8d9e0c10-8c53-4d08-a0c0-7a7eb45fc003";

constexpr int I2S_BCLK_PIN = 4;
constexpr int I2S_WS_PIN = 5;
constexpr int I2S_MIC_DATA_PIN = 6;
constexpr int I2S_AMP_DATA_PIN = 7;
constexpr int I2C_SDA_PIN = 8;
constexpr int I2C_SCL_PIN = 9;
constexpr int8_t IP5108_INT_PIN = -1;  // set to GPIO assigned to L3/INT if wired
constexpr uint8_t IP5108_I2C_ADDRESS = 0x75;
constexpr int RECORD_BUTTON_PIN = 14;
constexpr int ACTION_BUTTON_PIN = 15;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint16_t SCREEN_WIDTH = 128;
constexpr uint16_t SCREEN_HEIGHT = 64;
constexpr uint32_t SAMPLE_RATE_HZ = 16000;
constexpr size_t AUDIO_CHUNK_BYTES = 512;
constexpr size_t AUDIO_CHUNK_SAMPLES = AUDIO_CHUNK_BYTES / sizeof(int16_t);
}
