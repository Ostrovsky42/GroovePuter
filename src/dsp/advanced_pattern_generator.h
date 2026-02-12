#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "../../scenes.h" // For pattern structs
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

struct DrumGenreTemplate;


class AdvancedPatternGenerator {
public:
    static void generatePattern(SynthPattern& pattern, const GeneratorParams& params);

private:
    static std::vector<int> generatePositions(int count, const GeneratorParams& params);
    static int generateNote(int step, const GeneratorParams& params);
    static uint8_t generateVelocity(int step, const GeneratorParams& params);
    static int8_t generateTiming(int step, const GeneratorParams& params);
    static void applySwing(SynthPattern& pattern, float amount);
    static int quantizeToScale(int note, int root, ScaleType scale);
    static bool isDownbeat(int step);
};

class DrumPatternGenerator {
public:
    static void generateDrumPattern(DrumPatternSet& patternSet, 
                                    const GenerativeParams& params,
                                    GenerativeMode mode,
                                    const DrumGenreTemplate* templateOverride = nullptr);
    
private:
    static void applyDrumSwing(DrumPatternSet& ps, float amount);
};
