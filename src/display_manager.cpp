#include "display_manager.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "battery_manager.h"
#include "device_config.h"

namespace {
Adafruit_SSD1306 display(DeviceConfig::SCREEN_WIDTH, DeviceConfig::SCREEN_HEIGHT, &Wire, -1);
bool displayReady = false;
}

void initDisplay() {
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, DeviceConfig::OLED_ADDRESS);
}

void drawStatus(const char *line1, const char *line2) {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Companion Teddy");
  display.drawFastHLine(0, 10, DeviceConfig::SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(0, 16);
  display.println(line1);
  display.setCursor(0, 28);
  display.println(line2);
  display.setCursor(0, 50);
  display.print("Pin: ");
  if (isBatteryReady()) {
    display.print(getBatteryPercent());
    display.print("% ");
    display.print(getBatteryMillivolts() / 1000.0f, 2);
    display.print("V");
  } else {
    display.print("dang do...");
  }
  display.display();
}
