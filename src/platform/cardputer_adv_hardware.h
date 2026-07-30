#pragma once

#include <cstdint>

// Canonical hardware profile for M5Stack Cardputer ADV.
//
// GPIO21 is the power-amplifier enable line on the ADV audio path. It must
// never be driven with WS2812 timing. The RGB data pin remains disabled until
// it is verified against the actual Cardputer ADV schematic and hardware.

#define GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN 21
#define GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN (-1)

namespace GroovePuterHardware {
inline constexpr int kPowerAmplifierEnablePin =
    GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN;
inline constexpr int kRgbLedDataPin =
    GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN;
inline constexpr bool kRgbLedEnabled = kRgbLedDataPin >= 0;

inline constexpr int kCodecI2cSdaPin = 8;
inline constexpr int kCodecI2cSclPin = 9;
inline constexpr uint8_t kEs8311I2cAddress = 0x18;

inline constexpr int kI2sBitClockPin = 41;
inline constexpr int kI2sWordSelectPin = 43;
inline constexpr int kI2sDataOutPin = 42;
inline constexpr int kI2sMasterClockPin = 0;
}  // namespace GroovePuterHardware
