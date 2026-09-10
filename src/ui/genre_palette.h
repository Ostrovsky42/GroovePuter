#pragma once
#ifndef GROOVEPUTER_SRC_UI_GENRE_PALETTE_H
#define GROOVEPUTER_SRC_UI_GENRE_PALETTE_H

#include "src/dsp/genre_manager.h"
#include "ui_core.h"

namespace UI {

struct GenreThemeProfile {
    const char* name;
    const char* tag;
    IGfxColor primary;    // Dominant neon/accent color
    IGfxColor secondary;  // Complementary accent
    const char* description;
};

inline GenreThemeProfile genreProfile(GenerativeMode mode) {
    switch (mode) {
        case GenerativeMode::Acid:
            return {"ACID", "303", IGfxColor(0x39FF14), IGfxColor(0x00F0FF), "Acid 303 squelch & resonant drive"};
        case GenerativeMode::Outrun:
            return {"OUTRUN", "SYNTH", IGfxColor(0xFF2A6D), IGfxColor(0x05D9E8), "Neon 80s retrowave & night drive"};
        case GenerativeMode::Darksynth:
            return {"DARKSYNTH", "DARK", IGfxColor(0xFF0055), IGfxColor(0x7209B7), "Aggressive cyberpunk & distorted bass"};
        case GenerativeMode::Electro:
            return {"ELECTRO", "ELEC", IGfxColor(0x00F0FF), IGfxColor(0x7000FF), "Robotic vocoder funk & 808 breaks"};
        case GenerativeMode::Rave:
            return {"RAVE", "RAVE", IGfxColor(0xFFE600), IGfxColor(0xFF0055), "Oldschool 90s hardcore & hoover stabs"};
        case GenerativeMode::Reggae:
            return {"REGGAE", "DUB", IGfxColor(0x2EC4B6), IGfxColor(0xE71D36), "Dub delay, spring reverb & offbeat skank"};
        case GenerativeMode::TripHop:
            return {"TRIP HOP", "TRIP", IGfxColor(0x9D4EDD), IGfxColor(0x3A0CA3), "Bristol down-tempo & smoky vinyl Rhodes"};
        case GenerativeMode::Broken:
            return {"BROKEN", "BRK", IGfxColor(0xFF9E00), IGfxColor(0x240046), "Syncopated broken beat & West London groove"};
        case GenerativeMode::Chip:
            return {"CHIPTUNE", "8BIT", IGfxColor(0x8BAC0F), IGfxColor(0x9BBC0F), "GameBoy DMG 8-bit square wave pulse"};
        case GenerativeMode::House:
            return {"HOUSE", "4x4", IGfxColor(0x4CC9F0), IGfxColor(0xF72585), "Deep 909 four-on-the-floor & organ groove"};
        case GenerativeMode::Techno:
            return {"TECHNO", "MOD", IGfxColor(0x90E0EF), IGfxColor(0x0077B6), "Industrial hypnotic modular warehouse rumble"};
        case GenerativeMode::HipHop:
            return {"HIP HOP", "BOOM", IGfxColor(0xF77F00), IGfxColor(0xFCBF49), "Golden era boom-bap & SP-1200 swing"};
        case GenerativeMode::FunkSoul:
            return {"FUNK SOUL", "FUNK", IGfxColor(0xFCBF49), IGfxColor(0xD62828), "Tight live slap bass & warm brass stabs"};
        case GenerativeMode::UkGarage:
            return {"UK GARAGE", "2STEP", IGfxColor(0x06D6A0), IGfxColor(0x118AB2), "2-step shuffle & vocal chop skipping hats"};
        case GenerativeMode::DrumAndBass:
            return {"D&B", "JUNG", IGfxColor(0xFFD60A), IGfxColor(0x003566), "Amen chop, reese bassline & rolling 174"};
        case GenerativeMode::LoFi:
            return {"LO-FI", "CHILL", IGfxColor(0xDDA15E), IGfxColor(0xBC6C25), "Tape wow, flutter, dusty jazz & SP warmth"};
        default:
            return {"GENRE", "GEN", IGfxColor(0x00F0FF), IGfxColor(0xFF007F), "Generative groove machine"};
    }
}

inline IGfxColor genreAccentColor(GenerativeMode mode) {
    return genreProfile(mode).primary;
}

} // namespace UI

#endif // GROOVEPUTER_SRC_UI_GENRE_PALETTE_H
