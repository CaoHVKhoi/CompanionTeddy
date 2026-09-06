#pragma once

#include <Arduino.h>

void initAudio();
void startRecording();
void captureAudioChunk();
void stopRecording();
void playTone(uint16_t frequency, uint16_t durationMs);
bool isRecording();
