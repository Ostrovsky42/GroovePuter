#pragma once
#include <stdint.h>
#include "mini_dsp_params.h"
#include "tape_defs.h"

// === MODE-SPECIFIC PRESETS ===

// TB-303 presets for each mode
struct TB303ModePreset {
    float cutoff;           // Hz
    float resonance;        // 0-1
    float envAmount;        // 0-1000
    float decay;            // 0-1 (normalized for our engine's 20-2200 range)
    bool distortion;
    bool delay;
    const char* name;
};

// Acid presets (bright, resonant)
const TB303ModePreset kAcidPresets[] = {
    {800.0f, 0.65f, 450.0f, 0.45f, false, false, "CLASSIC"},
    {1200.0f, 0.75f, 600.0f, 0.55f, true, false, "SCREAM"},
    {600.0f, 0.55f, 350.0f, 0.65f, false, true, "DEEP"},
    {1000.0f, 0.70f, 500.0f, 0.35f, true, true, "DIRTY"}
};

// Minimal presets (dark, fat)
const TB303ModePreset kMinimalPresets[] = {
    {400.0f, 0.25f, 150.0f, 0.35f, true, false, "DEEP"},
    {550.0f, 0.30f, 200.0f, 0.25f, true, true, "DUSTY"},
    {500.0f, 0.20f, 80.0f, 0.75f, false, true, "WARM"},
    {480.0f, 0.35f, 180.0f, 0.30f, true, false, "GRIT"}
};

// Tape FX presets for each mode
struct TapeModePreset {
    TapeMacro macro;
    const char* name;
};

// Acid tape (wow, movement)
const TapeModePreset kAcidTapePresets[] = {
    {{3, 5, 8, 85, 0}, "CLEAN"},    // transparent
    {{8, 15, 12, 70, 0}, "WARM"},   // vinyl
    {{12, 8, 10, 75, 0}, "SPACE"},  // Space Echo
    {{15, 12, 15, 68, 0}, "ANALOG"} // Juno
};

// Minimal tape (dub techno, depth)
const TapeModePreset kMinimalTapePresets[] = {
    {{12, 15, 10, 68, 0}, "DUB"},      // deep
    {{8, 18, 12, 65, 0}, "WAREHOUSE"}, // raw but clean
    {{25, 12, 10, 70, 0}, "HYPNOTIC"}, // moving
    {{15, 20, 12, 60, 0}, "BEDROOM"}   // warm lofi
};

// === MODE CONFIGURATION ===
struct ModeConfig {
    // Pattern generation parameters
    struct {
        int minNotes;                    // min notes in pattern
        int maxNotes;                    // max notes
        int minOctave;                   // 24=C1, 36=C2, 48=C3
        int maxOctave;     
        float slideProbability;          // 0-1
        float accentProbability;         // 0-1
        float chromaticProbability;      // 0-1, chromatic passing tones
        float rootNoteBias;              // 0-1, probability of playing root
        float ghostProbability;          // 0-1, ghost note probability
        float swingAmount;               // 0-1, swing intensity
        int velocityMin;                 // min velocity (without accents)
        int velocityMax;                 // max velocity (without accents)
        int ghostVelocityMin;            // ghost note velocity min
        int ghostVelocityMax;            // ghost note velocity max
    } pattern;
    
    // Drum pattern parameters
    struct {
        bool sparseKick;      // false = four-on-floor, true = minimal
        bool sparseHats;      // less hats
        bool noAccents;       // remove accents
        float fillProbability; // probability of fills
    } drums;
    
    // DSP parameters
    struct {
        bool lofiDrums;       // bitcrush + vinyl
        bool subOscillator;   // add sub
        float noiseAmount;    // 0-1 (built-in noise)
    } dsp;
    
    // UI
    uint32_t accentColor;     // mode accent color in UI (RGB888)
    const char* displayName;
};

// ============================================================
// ACID CONFIGURATION
// Character: Aggressive, melodic, contrasting, grid-tight
// ============================================================
const ModeConfig kAcidConfig = {
    .pattern = {
        .minNotes = 8,
        .maxNotes = 16,
        .minOctave = 36,                  // C2
        .maxOctave = 72,                  // C5
        .slideProbability = 0.4f,
        .accentProbability = 0.5f,
        .chromaticProbability = 0.15f,    // 15% chromatic passing tones
        .rootNoteBias = 0.25f,            // 25% root (variety!)
        .ghostProbability = 0.10f,        // few ghosts (clean attack)
        .swingAmount = 0.0f,              // grid-tight, no swing
        .velocityMin = 85,                // wide dynamic range
        .velocityMax = 120,               // loud notes
        .ghostVelocityMin = 35,           // ghosts still punchy
        .ghostVelocityMax = 55
    },
    .drums = {
        .sparseKick = false,
        .sparseHats = false,
        .noAccents = false,
        .fillProbability = 0.6f
    },
    .dsp = {
        .lofiDrums = false,
        .subOscillator = false,
        .noiseAmount = 0.0f
    },
    .accentColor = 0xF59E0B,  // warn orange
    .displayName = "ACID"
};

// ============================================================
// MINIMAL CONFIGURATION
// Character: Hypnotic, deep, textural, shuffled
// ============================================================
const ModeConfig kMinimalConfig = {
    .pattern = {
        .minNotes = 2,
        .maxNotes = 5,
        .minOctave = 12,                  // C0 (deep!)
        .maxOctave = 48,                  // C3
        .slideProbability = 0.08f,        // rare slides
        .accentProbability = 0.15f,       // subtle accents
        .chromaticProbability = 0.0f,     // stay in scale (hypnotic)
        .rootNoteBias = 0.70f,            // 70% root (hypnosis)
        .ghostProbability = 0.35f,        // many ghosts (texture)
        .swingAmount = 0.22f,             // noticeable shuffle
        .velocityMin = 70,                // narrow, consistent
        .velocityMax = 90,                // flat dynamics
        .ghostVelocityMin = 20,           // very quiet ghosts
        .ghostVelocityMax = 40            // textural, not melodic
    },
    .drums = {
        .sparseKick = true,
        .sparseHats = true,
        .noAccents = true,
        .fillProbability = 0.2f
    },
    .dsp = {
        .lofiDrums = true,
        .subOscillator = true,
        .noiseAmount = 0.02f
    },
    .accentColor = 0x22C55E,  // accent green
    .displayName = "MINIMAL"
};
