#pragma once
#include <stdint.h>

// Tape mode (transport control)
enum class TapeMode : uint8_t {
    Stop = 0,
    Rec = 1,
    Dub = 2,
    Play = 3
};

// Tape preset (factory settings)
enum class TapePreset : uint8_t {
    Clean = 0,
    Warm = 1,
    Dust = 2,
    VHS = 3,
    Broken = 4,
    AcidBath = 5,
    Count = 6
};

// Macro parameters (0–100 range for UI, mapped to DSP internally)
struct TapeMacro {
    uint8_t wow = 12;      // 0..100 - motor drift / pitch wobble
    uint8_t age = 20;      // 0..100 - noise + LPF rolloff
    uint8_t sat = 35;      // 0..100 - tape saturation drive
    uint8_t tone = 60;     // 0..100 - brightness (0=dark, 100=bright)
    uint8_t crush = 0;     // 0=off, 1=8bit, 2=6bit, 3=4bit
};
