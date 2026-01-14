#pragma once
#include "../../scenes.h"

// Preset macro tables for 6 tape characters
// Each preset defines starting values for WOW/AGE/SAT/TONE/CRUSH

inline constexpr TapeMacro kTapePresets[static_cast<int>(TapePreset::Count)] = {
    // CLEAN: subtle warmth, almost transparent
    { .wow = 5,  .age = 0,  .sat = 10, .tone = 80, .crush = 0 },
    
    // WARM: classic tape character
    { .wow = 12, .age = 20, .sat = 35, .tone = 60, .crush = 0 },
    
    // DUST: aged cassette with noise
    { .wow = 15, .age = 55, .sat = 30, .tone = 45, .crush = 0 },
    
    // VHS: video tape vibe with artifacts
    { .wow = 25, .age = 65, .sat = 40, .tone = 35, .crush = 1 },
    
    // BROKEN: destroyed tape, heavy degradation
    { .wow = 70, .age = 80, .sat = 55, .tone = 30, .crush = 2 },
    
    // ACID_BATH: 303 character, saturated but present
    { .wow = 35, .age = 35, .sat = 80, .tone = 55, .crush = 1 },
};

inline const char* tapePresetName(TapePreset preset) {
    static constexpr const char* names[] = {
        "CLEAN", "WARM", "DUST", "VHS", "BROKEN", "ACIDBTH"
    };
    int idx = static_cast<int>(preset);
    if (idx < 0 || idx >= static_cast<int>(TapePreset::Count)) idx = 0;
    return names[idx];
}

inline const char* tapeModeName(TapeMode mode) {
    static constexpr const char* names[] = {
        "STOP", "REC", "DUB", "PLAY"
    };
    int idx = static_cast<int>(mode);
    if (idx < 0 || idx > 3) idx = 0;
    return names[idx];
}

inline const char* tapeSpeedName(uint8_t speed) {
    static constexpr const char* names[] = {
        "0.5x", "1.0x", "2.0x"
    };
    if (speed > 2) speed = 1;
    return names[speed];
}

inline float tapeSpeedMultiplier(uint8_t speed) {
    static constexpr float multipliers[] = { 0.5f, 1.0f, 2.0f };
    if (speed > 2) speed = 1;
    return multipliers[speed];
}

// Load a preset into a TapeMacro
inline void loadTapePreset(TapePreset preset, TapeMacro& macro) {
    int idx = static_cast<int>(preset);
    if (idx >= 0 && idx < static_cast<int>(TapePreset::Count)) {
        macro = kTapePresets[idx];
    }
}

// Cycle to next preset
inline TapePreset nextTapePreset(TapePreset current) {
    int next = (static_cast<int>(current) + 1) % static_cast<int>(TapePreset::Count);
    return static_cast<TapePreset>(next);
}

// Cycle to next mode (STOP -> REC -> DUB -> PLAY -> STOP)
inline TapeMode nextTapeMode(TapeMode current) {
    int next = (static_cast<int>(current) + 1) % 4;
    return static_cast<TapeMode>(next);
}
