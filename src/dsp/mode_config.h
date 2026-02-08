#pragma once
#include <stdint.h>
#include "mini_dsp_params.h"
#include "src/dsp/tape_defs.h"

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

// ACID flavors (5)
const TB303ModePreset kAcidPresets[] = {
    {820.0f, 0.66f, 460.0f, 0.45f, false, false, "CLASSIC"},
    {1300.0f, 0.78f, 620.0f, 0.32f, true, false, "SHARP"},
    {620.0f, 0.55f, 360.0f, 0.70f, false, true, "DEEP"},
    {780.0f, 0.60f, 520.0f, 0.62f, false, false, "RUBBER"},
    {1080.0f, 0.74f, 580.0f, 0.28f, true, true, "RAVE"}
};

// MINIMAL flavors (5)
const TB303ModePreset kMinimalPresets[] = {
    {460.0f, 0.24f, 170.0f, 0.26f, true, false, "TIGHT"},
    {520.0f, 0.28f, 190.0f, 0.42f, true, true, "WARM"},
    {580.0f, 0.20f, 120.0f, 0.65f, false, true, "AIRY"},
    {420.0f, 0.22f, 130.0f, 0.22f, false, false, "DRY"},
    {500.0f, 0.26f, 210.0f, 0.55f, false, true, "HYPNO"}
};

// BREAKS flavors (5)
const TB303ModePreset kBreaksPresets[] = {
    {920.0f, 0.48f, 300.0f, 0.30f, false, false, "NUSKOOL"},
    {880.0f, 0.44f, 340.0f, 0.24f, true, false, "SKITTER"},
    {760.0f, 0.42f, 260.0f, 0.40f, false, true, "ROLLER"},
    {980.0f, 0.52f, 420.0f, 0.22f, true, true, "CRUNCH"},
    {700.0f, 0.38f, 220.0f, 0.52f, false, true, "LIQUID"}
};

// DUB flavors (5)
const TB303ModePreset kDubPresets[] = {
    {520.0f, 0.30f, 170.0f, 0.58f, false, true, "HEAVY"},
    {460.0f, 0.24f, 120.0f, 0.70f, false, true, "SPACE"},
    {560.0f, 0.32f, 200.0f, 0.44f, false, false, "STEPPERS"},
    {500.0f, 0.28f, 170.0f, 0.62f, true, true, "TAPE"},
    {430.0f, 0.22f, 100.0f, 0.78f, false, true, "FOG"}
};

// ELECTRO flavors (5)
const TB303ModePreset kElectroPresets[] = {
    {900.0f, 0.46f, 280.0f, 0.24f, false, false, "ROBOT"},
    {1120.0f, 0.58f, 420.0f, 0.20f, true, false, "ZAP"},
    {820.0f, 0.50f, 300.0f, 0.34f, true, false, "BOING"},
    {760.0f, 0.44f, 260.0f, 0.38f, false, true, "MIAMI"},
    {980.0f, 0.62f, 460.0f, 0.22f, true, true, "INDUS"}
};

// Tape FX presets for each mode
struct TapeModePreset {
    TapeMacro macro;
    const char* name;
};

// ACID tape flavors (5)
const TapeModePreset kAcidTapePresets[] = {
    {{3, 5, 8, 85, 0}, "CLASSIC"},
    {{9, 8, 10, 78, 0}, "SHARP"},
    {{14, 14, 12, 68, 0}, "DEEP"},
    {{10, 18, 9, 72, 0}, "RUBBER"},
    {{16, 16, 14, 66, 0}, "RAVE"}
};

// MINIMAL tape flavors (5)
const TapeModePreset kMinimalTapePresets[] = {
    {{10, 12, 8, 72, 0}, "TIGHT"},
    {{14, 16, 10, 66, 0}, "WARM"},
    {{20, 12, 9, 74, 0}, "AIRY"},
    {{8, 10, 7, 78, 0}, "DRY"},
    {{18, 22, 12, 62, 0}, "HYPNO"}
};

const TapeModePreset kBreaksTapePresets[] = {
    {{9, 15, 10, 70, 0}, "NUSKOOL"},
    {{7, 20, 14, 64, 0}, "SKITTER"},
    {{12, 14, 11, 68, 0}, "ROLLER"},
    {{10, 18, 15, 60, 0}, "CRUNCH"},
    {{16, 16, 8, 72, 0}, "LIQUID"}
};

const TapeModePreset kDubTapePresets[] = {
    {{20, 18, 12, 64, 0}, "HEAVY"},
    {{26, 22, 14, 58, 0}, "SPACE"},
    {{14, 14, 10, 66, 0}, "STEPPERS"},
    {{22, 20, 12, 60, 0}, "TAPE"},
    {{30, 24, 15, 52, 0}, "FOG"}
};

const TapeModePreset kElectroTapePresets[] = {
    {{6, 8, 7, 78, 0}, "ROBOT"},
    {{8, 14, 10, 72, 0}, "ZAP"},
    {{10, 12, 9, 74, 0}, "BOING"},
    {{12, 16, 11, 68, 0}, "MIAMI"},
    {{7, 10, 13, 70, 0}, "INDUS"}
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
        
        // Corridor boundaries (for the 0..4 Flavor system)
        struct {
            int notesMin[2];        // [0]=min, [1]=max
            int restsMin[2];
            float accentProb[2];
            float slideProb[2];
            float swingRange[2];
        } corridor;
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
        .ghostVelocityMax = 55,
        .corridor = {
            .notesMin = {8, 14},
            .restsMin = {2, 6},
            .accentProb = {0.25f, 0.45f},
            .slideProb = {0.20f, 0.40f},
            .swingRange = {0.0f, 0.06f}
        }
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
        .ghostVelocityMax = 40,           // textural, not melodic
        .corridor = {
            .notesMin = {3, 7},
            .restsMin = {9, 13},
            .accentProb = {0.08f, 0.20f},
            .slideProb = {0.05f, 0.15f},
            .swingRange = {0.12f, 0.24f}
        }
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

const ModeConfig kBreaksConfig = {
    .pattern = {
        .minNotes = 4, .maxNotes = 9, .minOctave = 24, .maxOctave = 60,
        .slideProbability = 0.16f, .accentProbability = 0.34f, .chromaticProbability = 0.06f,
        .rootNoteBias = 0.38f, .ghostProbability = 0.22f, .swingAmount = 0.24f,
        .velocityMin = 74, .velocityMax = 112, .ghostVelocityMin = 28, .ghostVelocityMax = 52,
        .corridor = {
            .notesMin = {5, 10},
            .restsMin = {6, 11},
            .accentProb = {0.20f, 0.38f},
            .slideProb = {0.08f, 0.18f},
            .swingRange = {0.16f, 0.30f}
        }
    },
    .drums = {.sparseKick = false, .sparseHats = false, .noAccents = false, .fillProbability = 0.70f},
    .dsp = {.lofiDrums = false, .subOscillator = false, .noiseAmount = 0.01f},
    .accentColor = 0x38BDF8,
    .displayName = "BREAKS"
};

const ModeConfig kDubConfig = {
    .pattern = {
        .minNotes = 2, .maxNotes = 6, .minOctave = 12, .maxOctave = 48,
        .slideProbability = 0.10f, .accentProbability = 0.18f, .chromaticProbability = 0.00f,
        .rootNoteBias = 0.78f, .ghostProbability = 0.30f, .swingAmount = 0.18f,
        .velocityMin = 66, .velocityMax = 92, .ghostVelocityMin = 18, .ghostVelocityMax = 38,
        .corridor = {
            .notesMin = {2, 6},
            .restsMin = {10, 14},
            .accentProb = {0.10f, 0.24f},
            .slideProb = {0.04f, 0.12f},
            .swingRange = {0.10f, 0.20f}
        }
    },
    .drums = {.sparseKick = true, .sparseHats = true, .noAccents = false, .fillProbability = 0.24f},
    .dsp = {.lofiDrums = true, .subOscillator = true, .noiseAmount = 0.015f},
    .accentColor = 0xA3E635,
    .displayName = "DUB"
};

const ModeConfig kElectroConfig = {
    .pattern = {
        .minNotes = 6, .maxNotes = 11, .minOctave = 24, .maxOctave = 67,
        .slideProbability = 0.12f, .accentProbability = 0.26f, .chromaticProbability = 0.08f,
        .rootNoteBias = 0.42f, .ghostProbability = 0.12f, .swingAmount = 0.04f,
        .velocityMin = 82, .velocityMax = 116, .ghostVelocityMin = 30, .ghostVelocityMax = 48,
        .corridor = {
            .notesMin = {6, 11},
            .restsMin = {5, 10},
            .accentProb = {0.18f, 0.35f},
            .slideProb = {0.00f, 0.10f},
            .swingRange = {0.00f, 0.04f}
        }
    },
    .drums = {.sparseKick = false, .sparseHats = false, .noAccents = true, .fillProbability = 0.40f},
    .dsp = {.lofiDrums = false, .subOscillator = false, .noiseAmount = 0.0f},
    .accentColor = 0xF472B6,
    .displayName = "ELECTRO"
};
