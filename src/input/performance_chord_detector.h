#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_CHORD_DETECTOR_H
#define GROOVEPUTER_PERFORMANCE_CHORD_DETECTOR_H

#include <cstddef>
#include <cstdint>

struct PerformanceChordDetection {
    bool matched{false};
    uint8_t rootPitchClass{0};
    uint8_t inversion{0};
    const char* quality{""};
};

namespace PerformanceChordDetectorDetail {
struct Shape {
    const uint8_t* intervals;
    uint8_t count;
    const char* quality;
};

inline bool containsPitchClass(const uint8_t* notes,
                               std::size_t count,
                               uint8_t pitchClass) {
    for (std::size_t i = 0; i < count; ++i) {
        if ((notes[i] % 12u) == pitchClass) return true;
    }
    return false;
}

inline bool matchesShape(const uint8_t* notes,
                         std::size_t count,
                         uint8_t root,
                         const Shape& shape) {
    if (count != shape.count) return false;
    for (uint8_t i = 0; i < shape.count; ++i) {
        const uint8_t wanted = static_cast<uint8_t>((root + shape.intervals[i]) % 12u);
        if (!containsPitchClass(notes, count, wanted)) return false;
    }
    return true;
}
}  // namespace PerformanceChordDetectorDetail

inline PerformanceChordDetection detectPerformanceChord(const uint8_t* notes,
                                                         std::size_t count) {
    using namespace PerformanceChordDetectorDetail;
    if (notes == nullptr || count < 2 || count > 4) return {};

    static constexpr uint8_t kMajor[] = {0, 4, 7};
    static constexpr uint8_t kMinor[] = {0, 3, 7};
    static constexpr uint8_t kFifth[] = {0, 7};
    static constexpr uint8_t kSus2[] = {0, 2, 7};
    static constexpr uint8_t kSus4[] = {0, 5, 7};
    static constexpr uint8_t kDom7[] = {0, 4, 7, 10};
    static constexpr uint8_t kMaj7[] = {0, 4, 7, 11};
    static constexpr uint8_t kMin7[] = {0, 3, 7, 10};
    static constexpr Shape kShapes[] = {
        {kMaj7, 4, "maj7"},
        {kMin7, 4, "m7"},
        {kDom7, 4, "7"},
        {kMajor, 3, ""},
        {kMinor, 3, "m"},
        {kSus2, 3, "sus2"},
        {kSus4, 3, "sus4"},
        {kFifth, 2, "5"},
    };

    for (uint8_t root = 0; root < 12; ++root) {
        for (const Shape& shape : kShapes) {
            if (!matchesShape(notes, count, root, shape)) continue;
            PerformanceChordDetection result{};
            result.matched = true;
            result.rootPitchClass = root;
            result.quality = shape.quality;
            const uint8_t bassInterval = static_cast<uint8_t>((notes[0] + 12u - root) % 12u);
            result.inversion = 0;
            for (uint8_t i = 0; i < shape.count; ++i) {
                if (shape.intervals[i] == bassInterval) {
                    result.inversion = i;
                    break;
                }
            }
            return result;
        }
    }
    return {};
}

#endif  // GROOVEPUTER_PERFORMANCE_CHORD_DETECTOR_H
