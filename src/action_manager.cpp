#include "action_manager.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "wifi_manager.h"

void playLocalReply() {
  drawStatus("Phan hoi thu tai cho", "Cho API Companion");
  playTone(660, 120);
  delay(35);
  playTone(880, 180);
  drawStatus(isWifiConnected() ? "San sang de noi" : "Can quet QR tren app",
             isWifiConnected() ? "Nhan nut de hoi" : "De cai Wi-Fi");
}
