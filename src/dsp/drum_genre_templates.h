#pragma once

#include <stdint.h>
#include "genre_manager.h"

// Bitmasks use step 0 -> bit 15, step 15 -> bit 0.
struct DrumGenreTemplate {
    uint16_t kickMask;
    uint16_t snareMask;
    uint16_t hatMask;
    uint16_t openHatMask;
    float kickGhostProb;
    float snareGhostProb;
    float hatVariation;
    uint8_t kickVelBase;
    uint8_t snareVelBase;
    uint8_t hatVelBase;
    bool useRim;
    bool useClap;
};

static constexpr DrumGenreTemplate kDrumTemplates[kGenerativeModeCount] = {
    // ACID
    {0x8888, 0x0808, 0xFFFF, 0x2222, 0.15f, 0.10f, 0.35f, 116, 109, 96, true,  true},
    // OUTRUN (Minimal)
    {0x8080, 0x0808, 0xAAAA, 0x0202, 0.10f, 0.07f, 0.25f, 106, 96, 78, true,  false},
    // DARKSYNTH (Techno)
    {0x8888, 0x0808, 0xFFFF, 0x2222, 0.04f, 0.04f, 0.12f, 124, 114, 104, false, true},
    // ELECTRO
    {0x90A0, 0x0808, 0xAAAA, 0x0200, 0.10f, 0.10f, 0.30f, 112, 108, 86, false, false},
    // RAVE
    {0x8888, 0x0808, 0xFFFF, 0x2222, 0.05f, 0.06f, 0.22f, 124, 120, 108, false, true},
    // REGGAE
    {0x8000, 0x0202, 0xAAAA, 0x0000, 0.15f, 0.10f, 0.20f, 94, 86, 72, true,  false},
    // TRIPHOP
    {0x8080, 0x0820, 0xA8A8, 0x0200, 0.12f, 0.18f, 0.40f, 96, 106, 74, false, false},
    // BROKEN
    {0x8484, 0x0840, 0xF5A5, 0x0200, 0.08f, 0.12f, 0.46f, 104, 108, 82, false, true},
    // CHIP
    {0x8888, 0x0808, 0xFFFF, 0x0000, 0.02f, 0.02f, 0.05f, 112, 106, 100, false, false},
};

