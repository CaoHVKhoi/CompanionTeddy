#include "ip5108_pmic.h"

namespace {
// IP5108 control, status, and ADC register addresses used by this driver.
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

// Approximate LiPo open-circuit-voltage curve. The IP5108 exposes OCV, so
// percentage is estimated from this table instead of using a fuel-gauge IC.
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
      // Interpolate between adjacent calibration points for a smoother value.
      return lo.soc + (uint32_t)(mv - lo.mv) * (hi.soc - lo.soc) / (hi.mv - lo.mv);
    }
  }

  return 100;
}
}

// Configure the PMIC connection and verify that the device is ready.
bool Ip5108Pmic::begin(uint8_t addr, TwoWire *wire, int8_t intPin) {
  _addr = addr;
  _wire = wire;
  _intPin = intPin;

  if (_intPin >= 0) {
    // L3/INT is optional. When connected, a LOW level means the PMIC is not
    // ready for normal I2C access yet.
    pinMode(_intPin, INPUT);
    if (!waitForI2CReady(2000)) {
      return false;
    }
  }

  return ready();
}

// Check whether the PMIC acknowledges its configured I2C address.
bool Ip5108Pmic::ready() const {
  // A zero-length I2C transaction is enough to verify that the PMIC responds.
  _wire->beginTransmission(_addr);
  return (_wire->endTransmission() == 0);
}

// Wait until the optional PMIC interrupt line indicates that I2C access is safe.
bool Ip5108Pmic::waitForI2CReady(uint32_t timeoutMs) {
  if (_intPin < 0) return true;

  const uint32_t start = millis();
  // Do not block boot indefinitely if the optional interrupt line stays low.
  while (digitalRead(_intPin) == LOW) {
    if (millis() - start >= timeoutMs) return false;
    delay(1);
  }
  return true;
}

// Write one byte to an IP5108 register.
bool Ip5108Pmic::writeRegister8(uint8_t reg, uint8_t value) const {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->write(value);
  return (_wire->endTransmission() == 0);
}

// Read one byte from an IP5108 register and optionally report I2C success.
uint8_t Ip5108Pmic::readRegister8(uint8_t reg, bool *ok) const {
  // Keep the bus active between selecting the register and reading its value.
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

// Read one control/status bit from an IP5108 register.
bool Ip5108Pmic::readRegisterBit(uint8_t reg, uint8_t bit) const {
  const uint8_t value = readRegister8(reg);
  if (value == 0xFF) return false;
  return (value >> bit) & 0x01;
}

// Convert the IP5108 battery-voltage ADC bytes into volts.
float Ip5108Pmic::decodeVoltageADC(uint8_t low, uint8_t high) const {
  uint16_t batVol = 0;
  high &= 0x3F;

  // The IP5108 voltage ADC is a signed 14-bit value with a 0.26855 mV step
  // around a 2600 mV offset. The negative branch handles two's-complement data.
  if ((high & 0x20) == 0x20) {
    batVol = 2600 - ((~low) + (~(high & 0x1F)) * 256 + 1) * 0.26855;
  } else {
    batVol = 2600 + (low + high * 256) * 0.26855;
  }

  if (batVol == 4868) batVol = 0;
  return static_cast<float>(batVol) / 1000.0f;
}

// Convert the IP5108 battery-current ADC bytes into amps.
float Ip5108Pmic::decodeCurrentADC(uint8_t low, uint8_t high) const {
  int16_t batCur = 0;
  high &= 0x3F;

  // Battery current uses the same signed ADC layout, with a 0.745985 mA step.
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

// Enable or disable the IP5108 battery charger.
void Ip5108Pmic::setCharger(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  // SYS_CTL0 bit 1 controls battery charging.
  bitWrite(data, 1, enabled);
  writeRegister8(SYS_CTL0, data);
}

// Enable or disable the IP5108 boost converter output.
void Ip5108Pmic::setBoost(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  // SYS_CTL0 bit 2 enables the regulated boost output for the system load.
  bitWrite(data, 2, enabled);
  writeRegister8(SYS_CTL0, data);
}

// Enable or disable the IP5108 flashlight/LED output.
void Ip5108Pmic::setFlashLight(bool enabled) {
  uint8_t data = readRegister8(SYS_CTL0);
  // SYS_CTL0 bit 4 controls the flashlight/LED output.
  bitWrite(data, 4, enabled);
  writeRegister8(SYS_CTL0, data);
}

// Return whether the charger control bit is currently enabled.
bool Ip5108Pmic::chargerEnabled() const {
  return readRegisterBit(SYS_CTL0, 1);
}

// Return whether the boost control bit is currently enabled.
bool Ip5108Pmic::boostEnabled() const {
  return readRegisterBit(SYS_CTL0, 2);
}

// Return true when the PMIC reports an active charging state.
bool Ip5108Pmic::charging() const {
  const uint8_t data = readRegister8(REG_READ0B);
  // Bits 7:5 encode the charging state; values 1-3 represent active charging.
  const uint8_t status = (data >> 5) & 0b00000111;
  return status == 1 || status == 2 || status == 3;
}

// Read the live battery voltage and return it in volts.
float Ip5108Pmic::batteryVoltage() const {
  const uint8_t low = readRegister8(BATVADC_DAT0);
  const uint8_t high = readRegister8(BATVADC_DAT1);
  return decodeVoltageADC(low, high);
}

// Read the battery current and return it in amps.
float Ip5108Pmic::batteryCurrent() const {
  const uint8_t low = readRegister8(BATIADC_DAT0);
  const uint8_t high = readRegister8(BATIADC_DAT1);
  return decodeCurrentADC(low, high);
}

// Convert an OCV value in volts into the configured percentage estimate.
int8_t Ip5108Pmic::calcBatteryPercentage(float ocv) const {
  const uint16_t voltage = static_cast<uint16_t>(ocv * 1000.0f);
  if (voltage == 0) return -1;
  return static_cast<int8_t>(socFromVoltage(voltage));
}

// Read battery OCV and estimate the remaining battery percentage.
int8_t Ip5108Pmic::batteryPercentage() const {
  if (!ready()) return -1;

  // OCV is preferred for the percentage estimate because it is less affected
  // by the instantaneous load than the live battery-voltage ADC value.
  const uint8_t low = readRegister8(BATOCV_DAT0);
  const uint8_t high = readRegister8(BATOCV_DAT1);
  const float ocv = decodeVoltageADC(low, high);
  return calcBatteryPercentage(ocv);
}
