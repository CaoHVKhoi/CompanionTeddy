#include "audio_manager.h"

#include <I2S.h>

#include "device_config.h"
#include "display_manager.h"
#include "wifi_manager.h"

namespace {
I2SClass audioBus(0, 0, DeviceConfig::I2S_MIC_DATA_PIN,
                  DeviceConfig::I2S_BCLK_PIN, DeviceConfig::I2S_WS_PIN);
int16_t audioChunk[DeviceConfig::AUDIO_CHUNK_SAMPLES];
uint64_t recordedSamples = 0;
bool recording = false;
}

void initAudio() {
  audioBus.setDataOutPin(DeviceConfig::I2S_AMP_DATA_PIN);
  if (!audioBus.setDuplex() ||
      !audioBus.begin(I2S_PHILIPS_MODE, DeviceConfig::SAMPLE_RATE_HZ, 16)) {
    drawStatus("Loi am thanh", "Kiem tra day I2S");
  }
}

void startRecording() {
  if (!isWifiConnected()) {
    drawStatus("Chua co mang", "Quet QR tren app");
    return;
  }
  recordedSamples = 0;
  recording = true;
  drawStatus("Dang nghe...", "Nhan nut de dung");
}

void streamAudioChunk(const int16_t *samples, size_t sampleCount) {
  // TODO: Send this PCM chunk to the Companion voice API.
  (void)samples;
  (void)sampleCount;
}

void captureAudioChunk() {
  if (!recording) return;
  const size_t gotBytes = audioBus.readBytes((char *)audioChunk, DeviceConfig::AUDIO_CHUNK_BYTES);
  const size_t gotSamples = gotBytes / sizeof(int16_t);
  if (gotSamples == 0) return;
  streamAudioChunk(audioChunk, gotSamples);
  recordedSamples += gotSamples;
}

void playTone(uint16_t frequency, uint16_t durationMs) {
  const uint32_t sampleCount = (DeviceConfig::SAMPLE_RATE_HZ * durationMs) / 1000;
  const float phaseStep = 2.0f * PI * frequency / DeviceConfig::SAMPLE_RATE_HZ;
  float phase = 0.0f;
  int16_t samples[128];
  for (uint32_t sent = 0; sent < sampleCount;) {
    const size_t block = min((uint32_t)128, sampleCount - sent);
    for (size_t i = 0; i < block; ++i) {
      samples[i] = (int16_t)(6500.0f * sinf(phase));
      phase += phaseStep;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }
    audioBus.write((uint8_t *)samples, block * sizeof(int16_t));
    sent += block;
  }
}

void stopRecording() {
  if (!recording) return;
  recording = false;
  drawStatus("Da thu am", "Cho API Companion");
  delay(120);
  playTone(660, 120);
  delay(35);
  playTone(880, 180);
  drawStatus(isWifiConnected() ? "San sang de noi" : "Can quet QR tren app",
             isWifiConnected() ? "Nhan nut de hoi" : "De cai Wi-Fi");
}

bool isRecording() {
  return recording;
}
