#pragma once
#include <stdint.h>
#include "genre_manager.h"

class SmartPatternGenerator {
public:
    enum Mode {
        PG_RANDOM = 0,
        PG_GENRE,
        PG_EVOL,
        COUNT
    };
    
    SmartPatternGenerator();
    
    // Generate pattern index for a specific track
    uint32_t generatePattern(Mode mode, GenerativeMode genre, 
                            uint8_t track_id, 
                            uint32_t current_pattern = 0);
    
    // Set seed for reproducibility
    void setSeed(uint32_t seed);
    
private:
    uint32_t seed_;
    
    // Generators
    uint32_t generateRandom(uint8_t track_id);
    uint32_t generateGenreBased(GenerativeMode genre, uint8_t track_id);
    uint32_t generateEvolution(uint32_t current_pattern, uint8_t track_id);
    
    // Helpers
    // track_id: 0=303A, 1=303B, 2=Drums, >2=Voice/Other (treated as random for now)
    uint8_t getWeightedPatternForGenre(GenerativeMode genre, uint8_t track_id);
    uint8_t mutatePattern(uint8_t pattern_idx);
};
