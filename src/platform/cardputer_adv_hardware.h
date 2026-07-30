#pragma once

// Canonical hardware profile for M5Stack Cardputer ADV.
//
// GPIO21 is the power-amplifier enable line on the ADV audio path. It must
// never be driven with WS2812 timing. The RGB data pin remains disabled until
// it is verified against the actual Cardputer ADV schematic and hardware.

#define GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN 21
#define GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN (-1)

namespace GroovePuterHardware {
inline constexpr int kPowerAmplifierEnablePin = GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN;
inline constexpr int kRgbLedDataPin = GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN;
inline constexpr bool kRgbLedEnabled = kRgbLedDataPin >= 0;
}  // namespace GroovePuterHardware
