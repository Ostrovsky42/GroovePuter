#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_INSTRUMENT_TYPES_H
#define GROOVEPUTER_PERFORMANCE_INSTRUMENT_TYPES_H

#include <cstdint>

enum class PerformanceScale : uint8_t {
    Chromatic = 0,
    Major,
    NaturalMinor,
    MinorPentatonic,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    MajorPentatonic,
    Count,
};

enum class PerformanceChordMode : uint8_t {
    Off = 0,
    Major,
    Minor,
    Fifth,
    Minor7,
    Memory,
    Sus2,
    Sus4,
    Dominant7,
    Major7,
    ScaleTriad,
    ScaleSeventh,
    Count,
};

enum class PerformanceArpDirection : uint8_t {
    Up = 0,
    Down,
    UpDown,
    DownUp,
    AsPlayed,
    Random,
    Count,
};

enum class PerformanceVoiceMode : uint8_t { Mono = 0, Poly, Count };
enum class PerformanceSpread : uint8_t { Close = 0, Wide, Count };
enum class PerformanceVoiceLeading : uint8_t { Off = 0, Nearest, Count };
enum class PerformanceStrumDirection : uint8_t { LowToHigh = 0, HighToLow, AsPlayed, Count };

#endif  // GROOVEPUTER_PERFORMANCE_INSTRUMENT_TYPES_H
