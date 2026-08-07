#pragma once

#include <cstdint>

// Canonical hardware profile for M5Stack Cardputer ADV.
//
// Cardputer ADV audio is configured through the ES8311 codec path. M5Unified's
// Cardputer ADV speaker callback does not use GPIO21 as a one-wire amplifier
// enable (that GPIO21 behavior belongs to a different M5 device). Keep the
// legacy setup call source-compatible, but make it a typed no-op so GroovePuter
// never claims or drives GPIO21 on Cardputer ADV.
// RGB data remains disabled until a verified Cardputer ADV pin is available.

#define GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN (-1)

namespace GroovePuterHardware {
struct UnusedPowerAmplifierEnablePin {};
inline constexpr UnusedPowerAmplifierEnablePin kPowerAmplifierEnablePin{};

// GroovePuter.ino historically calls pinMode()/digitalWrite() for a PA pin.
// Argument-dependent lookup resolves these overloads for the typed unused pin,
// preserving the caller while producing no GPIO side effect.
inline void pinMode(UnusedPowerAmplifierEnablePin, uint8_t) {}
inline void digitalWrite(UnusedPowerAmplifierEnablePin, uint8_t) {}

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

inline constexpr int kSdClockPin = 40;
inline constexpr int kSdMisoPin = 39;
inline constexpr int kSdMosiPin = 14;
inline constexpr int kSdChipSelectPin = 12;
inline constexpr uint32_t kSdFrequencyHz = 25000000;
}  // namespace GroovePuterHardware
