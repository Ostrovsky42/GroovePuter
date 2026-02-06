#include "pattern_generator.h"
#include <stdlib.h>
#include <Arduino.h> // For random(), millis() if needed, or just standard rand()

// Use standard C rand() for predictable portable behavior if needed, 
// or Arduino random(). We'll use rand() to match the user's snippet style
// but assume seed is managed.

SmartPatternGenerator::SmartPatternGenerator() : seed_(0) {
    // Initial seed
    srand(12345);
}

void SmartPatternGenerator::setSeed(uint32_t seed) {
    seed_ = seed;
    srand(seed_);
}

uint32_t SmartPatternGenerator::generatePattern(Mode mode, GenerativeMode genre, 
                                          uint8_t track_id, 
                                          uint32_t current_pattern) {
    switch (mode) {
        case PG_RANDOM:
            return generateRandom(track_id);
        case PG_GENRE:
            return generateGenreBased(genre, track_id);
        case PG_EVOL:
            return generateEvolution(current_pattern, track_id);
        default:
            return current_pattern;
    }
}

uint32_t SmartPatternGenerator::generateRandom(uint8_t track_id) {
    // 8 patterns available (0-7)
    return rand() % 8;
}

uint32_t SmartPatternGenerator::generateGenreBased(GenerativeMode genre, uint8_t track_id) {
    // For 303 tracks (0, 1), we look at specific style preferences
    // For Drum track (2), we look at drum pattern preferences
    
    // Map track_id to simple index: 0=TB1, 1=TB2, 2=Drums, 3=Other
    uint8_t simple_track_id = track_id;
    if (simple_track_id > 3) simple_track_id = 3; 

    // Reuse logic from getWeightedPatternForGenre
    return getWeightedPatternForGenre(genre, simple_track_id);
}

uint32_t SmartPatternGenerator::generateEvolution(uint32_t current_pattern, uint8_t track_id) {
    // Current pattern is the starting point
    // We assume pattern indices are 0-7
    // If current_pattern is > 7 (empty/invalid), pick random
    if (current_pattern > 7) {
        return generateRandom(track_id);
    }
    
    return mutatePattern((uint8_t)current_pattern);
}

uint8_t SmartPatternGenerator::getWeightedPatternForGenre(GenerativeMode genre, uint8_t track_id) {
    // Genre mapping to GenerativeMode:
    // Acid = 0
    // Minimal (Outrun) = 1
    // Techno (Darksynth) = 2
    // Electro = 3
    // Rave = 4
    // Reggae = 5
    // TripHop = 6
    // Broken = 7
    // Chip = 8
    
    // Track mapping: 0=303A, 1=303B, 2=Drums, 3=Other
    
    // Bitmasks for preferred patterns (0-7)
    // Each byte represents valid patterns for a track type in a specific genre
    
    static const uint8_t genre_masks[kGenerativeModeCount][4] PROGMEM = {
        // Acid: TB1 pref, TB2 pref, Drums pref, Other
        // Acid patterns usually use 0,1,4,5,6
        { 0b01110011, 0b01110011, 0b11111111, 0b00001111 }, 
        
        // Minimal (Outrun/Synthwave): Simple, driving
        // Prefers 0,1,2
        { 0b00000111, 0b00000111, 0b00001111, 0b00000001 },
        
        // Techno (Darksynth): Aggressive
        // Prefers 4,5,6,7
        { 0b11110000, 0b11110000, 0b11111111, 0b11110000 },
        
        // Electro: Syncopated
        // Prefers 2,3,6,7
        { 0b11001100, 0b11001100, 0b11111111, 0b00001111 },
        
        // Rave: High energy, any can work but prefers dense
        // Prefers 0, 4, 5, 7
        { 0b10110001, 0b10110001, 0b11111111, 0b11111111 },

        // Reggae: Sparse, offbeat-friendly
        { 0b00010101, 0b00010101, 0b00011101, 0b00000101 },

        // TripHop: Slow, roomy, simple phrases
        { 0b00111001, 0b00111001, 0b00111101, 0b00001101 },

        // Broken: Syncopated, lopsided grooves
        { 0b11001100, 0b11001100, 0b11111111, 0b00011111 },

        // Chip: very regular clocked motifs
        { 0b11111111, 0b11111111, 0b10101010, 0b00001111 }
    };
    
    int genre_idx = static_cast<int>(genre);
    if (genre_idx >= kGenerativeModeCount) genre_idx = 0; // Fallback
    
    uint8_t mask = pgm_read_byte(&genre_masks[genre_idx][track_id]);
    
    // If mask is 0 (shouldn't happen with proper config), fallback to all
    if (mask == 0) mask = 0xFF;
    
    // Collect valid patterns
    uint8_t valid_patterns[8];
    uint8_t valid_count = 0;
    
    for (int i = 0; i < 8; i++) {
        if (mask & (1 << i)) {
            valid_patterns[valid_count++] = i;
        }
    }
    
    if (valid_count == 0) return rand() % 8;
    
    return valid_patterns[rand() % valid_count];
}

uint8_t SmartPatternGenerator::mutatePattern(uint8_t pattern_idx) {
    // 60% - small mutation (±1)
    // 30% - medium mutation (±2)
    // 10% - random
    
    int r = rand() % 100;
    
    if (r < 60) {
        // ±1 with 50/50
        int delta = (rand() % 2 == 0) ? 1 : -1;
        return (pattern_idx + delta + 8) % 8;
    } 
    else if (r < 90) {
        // ±2
        int delta = (rand() % 2 == 0) ? 2 : -2;
        return (pattern_idx + delta + 8) % 8;
    }
    else {
        // Random
        return rand() % 8;
    }
}
