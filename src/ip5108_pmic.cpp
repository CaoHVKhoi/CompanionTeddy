#include "ip5108_pmic.h"

namespace {
constexpr uint8_t SYS_CTL0 = 0x01;
constexpr uint8_t SYS_CTL1 = 0x02;
constexpr uint8_t REG_READ0B = 0x71;
constexpr uint8_t REG_READ1 = 0x72;
constexpr uint8_t BATVADC_DAT0 = 0xA2;
constexpr uint8_t BATVADC_DAT1 = 0xA3;
constexpr uint8_t BATIADC_DAT0 = 0xA4;
constexpr uint8_t BATIADC_DAT1 = 0xA5;
constexpr uint8_t BATOCV_DAT0 = 0xA8;
constexpr uint8_t BATOCV_DAT1 = 0xA9;

struct SocPoint {
  uint16_t mv;
  uint8_t soc;
};

const SocPoint SOC_CURVE[] = {
    {3000, 0}, {3300, 3}, {3500, 6}, {3610, 10}, {3650, 13}, {3690, 17},
    {3710, 20}, {3730, 25}, {3750, 30}, {3770, 35}, {3790, 40}, {3800, 45},
    {3820, 50}, {3840, 55}, {3850, 60}, {3870, 65}, {3910, 70}, {3950, 75},
    {3980, 80}, {4020, 85}, {4080, 90}, {4110, 95}, {4200, 100},
};

uint8_t socFromVoltage(uint16_t mv) {
  if (mv <= SOC_CURVE[0].mv) return SOC_CURVE[0].soc;
  if (mv >= SOC_CURVE[sizeof(SOC_CURVE) / sizeof(SOC_CURVE[0]) - 1].mv) {
    return SOC_CURVE[sizeof(SOC_CURVE) / sizeof(SOC_CURVE[0]) - 1].soc;
  }

  for (uint8_t i = 1; i < sizeof(SOC_CURVE) / sizeof(SOC_CURVE[0]); ++i) {
    if (mv <= SOC_CURVE[i].mv) {
      const SocPoint &lo = SOC_CURVE[i - 1];
      const SocPoint &hi = SOC_CURVE[i];
      return lo.soc + (uint32_t)(mv - lo.mv) * (hi.soc - lo.soc) / (hi.mv - lo.mv);
    }
  }

  return 100;
}
}

bool Ip5108Pmic::begin(uint8_t addr, TwoWire *wire, int8_t intPin) {
  _addr = addr;
  _wire = wire;
  _intPin = intPin;

  if (_intPin >= 0) {
    pinMode(_intPin, INPUT);
    if (!waitForI2CReady(2000)) {
      return false;
    }
  }

  return ready();
}

bool Ip5108Pmic::ready() const {
  _wire->beginTransmission(_addr);
  return (_wire->endTransmission() == 0);
}

bool Ip5108Pmic::waitForI2CReady(uint32_t timeoutMs) {
  if (_intPin < 0) return true;

  const uint32_t start = millis();
  while (digitalRead(_intPin) == LOW) {
    if (millis() - start >= timeoutMs) return false;
    delay(1);
  }
  return true;
}

bool Ip5108Pmic::writeRegister8(uint8_t reg, uint8_t value) const {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->write(value);
  return (_wire->endTransmission() == 0);
}

uint8_t Ip5108Pmic::readRegister8(uint8_t reg, bool *ok) const {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) {
    if (ok) *ok = false;
    return 0xFF;
  }

  if (_wire->requestFrom(_addr, (uint8_t)1) != 1) {
    if (ok) *ok = false;
    return 0xFF;
  }

  if (ok) *ok = true;
  return _wire->read();
}

bool Ip5108Pmic::readRegisterBit(uint8_t reg, uint8_t bit) const {
  const uint8_t value = readRegister8(reg);
  if (value == 0xFF) return false;
  return (value >> bit) & 0x01;
}

float Ip5108Pmic::decodeVoltageADC(uint8_t low, uint8_t high) const {
  uint16_t batVol = 0;
  high &= 0x3F;

  if ((high & 0x20) == 0x20) {
    batVol = 2600 - ((~low) + (~(high & 0x1F)) * 256 + 1) * 0.26855;
  } else {
    batVol = 2600 + (low + high * 256) * 0.26855;
  }

  if (batVol == 4868) batVol = 0;
  return static_cast<float>(batVol) / 1000.0f;
}

float Ip5108Pmic::decodeCurrentADC(uint8_t low, uint8_t high) const {
  int16_t batCur = 0;
  high &= 0x3F;

  if ((high & 0x20) == 0x20) {
    const uint8_t a = static_cast<uint8_t>(~low);
    const uint8_t b = static_cast<uint8_t>(~(high & 0x1F) & 0x1F);
    const int c = static_cast<int>(b) * 256 + static_cast<int>(a) + 1;
    batCur = static_cast<int16_t>(-(static_cast<int32_t>(c * 0.745985)));
  } else {
    batCur = static_cast<int16_t>((high * 256 + low) * 0.745985f);
  }

  return static_cast<float>(batCur) / 1000.0f;
}

void Ip5108Pmic::setCharger(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  bitWrite(data, 1, enabled);
  writeRegister8(SYS_CTL0, data);
}

void Ip5108Pmic::setBoost(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  bitWrite(data, 2, enabled);
  writeRegister8(SYS_CTL0, data);
}

void Ip5108Pmic::setFlashLight(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  bitWrite(data, 4, enabled);
  writeRegister8(SYS_CTL0, data);
}

bool Ip5108Pmic::chargerEnabled() const {
  return readRegisterBit(SYS_CTL0, 1);
}

bool Ip5108Pmic::boostEnabled() const {
  return readRegisterBit(SYS_CTL0, 2);
}

bool Ip5108Pmic::charging() const {
  const uint8_t data = readRegister8(REG_READ0B);
  const uint8_t status = (data >> 5) & 0b00000111;
  return status == 1 || status == 2 || status == 3;
}

float Ip5108Pmic::batteryVoltage() const {
  const uint8_t low = readRegister8(BATVADC_DAT0);
  const uint8_t high = readRegister8(BATVADC_DAT1);
  return decodeVoltageADC(low, high);
}

float Ip5108Pmic::batteryCurrent() const {
  const uint8_t low = readRegister8(BATIADC_DAT0);
  const uint8_t high = readRegister8(BATIADC_DAT1);
  return decodeCurrentADC(low, high);
}

int8_t Ip5108Pmic::calcBatteryPercentage(float ocv) const {
  const uint16_t voltage = static_cast<uint16_t>(ocv * 1000.0f);
  if (voltage == 0) return -1;
  return static_cast<int8_t>(socFromVoltage(voltage));
}

int8_t Ip5108Pmic::batteryPercentage() const {
  if (!ready()) return -1;

  const uint8_t low = readRegister8(BATOCV_DAT0);
  const uint8_t high = readRegister8(BATOCV_DAT1);
  const float ocv = decodeVoltageADC(low, high);
  return calcBatteryPercentage(ocv);
}
