#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_PULSE_H
#define GROOVEPUTER_PERFORMANCE_PULSE_H

#include <cstdint>

// ProjectTransportTimeline expresses time in sixteenth-note project steps.
// Keep rates rational until the final conversion to the existing double
// timeline coordinate so triplets never accumulate rounded durations.
enum class PerformanceRate : uint8_t {
    Quarter = 0,
    Eighth,
    EighthTriplet,
    Sixteenth,
    SixteenthTriplet,
    ThirtySecond,
    Count,
};

struct PerformancePulseRatio {
    uint8_t numerator{1};
    uint8_t denominator{1};
};

constexpr PerformancePulseRatio performancePulseRatio(PerformanceRate rate) {
    switch (rate) {
        case PerformanceRate::Quarter: return {4, 1};
        case PerformanceRate::Eighth: return {2, 1};
        case PerformanceRate::EighthTriplet: return {4, 3};
        case PerformanceRate::Sixteenth: return {1, 1};
        case PerformanceRate::SixteenthTriplet: return {2, 3};
        case PerformanceRate::ThirtySecond: return {1, 2};
        case PerformanceRate::Count: break;
    }
    return {1, 1};
}

constexpr double performancePulseSteps(PerformanceRate rate) {
    const PerformancePulseRatio ratio = performancePulseRatio(rate);
    return static_cast<double>(ratio.numerator) /
           static_cast<double>(ratio.denominator);
}

constexpr const char* performanceRateName(PerformanceRate rate) {
    switch (rate) {
        case PerformanceRate::Quarter: return "1/4";
        case PerformanceRate::Eighth: return "1/8";
        case PerformanceRate::EighthTriplet: return "1/8T";
        case PerformanceRate::Sixteenth: return "1/16";
        case PerformanceRate::SixteenthTriplet: return "1/16T";
        case PerformanceRate::ThirtySecond: return "1/32";
        case PerformanceRate::Count: break;
    }
    return "1/16";
}

// Rate/gate are musician-facing values. The remaining clocked settings are
// kept here so all NEXT_STEP changes can be copied atomically at one pulse
// boundary; a ratchet group can never be half old / half new.
struct PerformanceClockedConfig {
    PerformanceRate rate{PerformanceRate::Sixteenth};
    uint8_t gatePercent{60};
    uint8_t ratchetCount{1};
    uint8_t euclideanLength{16};
    uint8_t euclideanPulses{0};
    uint8_t euclideanRotation{0};
    uint8_t arpOctaves{1};
};

constexpr bool performanceClockedConfigEqual(const PerformanceClockedConfig& a,
                                             const PerformanceClockedConfig& b) {
    return a.rate == b.rate &&
           a.gatePercent == b.gatePercent &&
           a.ratchetCount == b.ratchetCount &&
           a.euclideanLength == b.euclideanLength &&
           a.euclideanPulses == b.euclideanPulses &&
           a.euclideanRotation == b.euclideanRotation &&
           a.arpOctaves == b.arpOctaves;
}

// Small deterministic PRNG for future/pulse mutation. Its state lives with the
// performance session; TAKE persistence stores the seed, not a random outcome.
struct PerformanceMutationConfig {
    uint32_t seed{0x47505631u};  // "GPV1"
    uint8_t skipPercent{0};
    uint8_t octaveJumpPercent{0};
    uint8_t deviatePercent{0};
};

class PerformanceMutationRng {
public:
    void reset(uint32_t seed) { state_ = seed == 0 ? 0x47505631u : seed; }
    uint32_t next() {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x == 0 ? 0x47505631u : x;
        return state_;
    }
    bool chance(uint8_t percent) {
        if (percent == 0) return false;
        if (percent >= 100) return true;
        return (next() % 100u) < percent;
    }
private:
    uint32_t state_{0x47505631u};
};

#endif  // GROOVEPUTER_PERFORMANCE_PULSE_H
