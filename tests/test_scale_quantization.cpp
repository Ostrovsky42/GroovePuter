#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

#include "src/dsp/advanced_pattern_generator.h"

namespace {

constexpr uint16_t pitchClassMask(std::initializer_list<uint8_t> values) {
    uint16_t mask = 0;
    for (uint8_t value : values) {
        mask = static_cast<uint16_t>(mask | static_cast<uint16_t>(1u << value));
    }
    return mask;
}

int generateFixedInputNote(int inputNote, int root, ScaleType scale) {
    GeneratorParams params{};
    params.minNotes = 16;
    params.maxNotes = 16;
    params.minOctave = inputNote;
    params.maxOctave = inputNote;
    params.swingAmount = 0.0f;
    params.velocityRange = 0.0f;
    params.ghostNoteProbability = 0.0f;
    params.microTimingAmount = 0.0f;
    params.preferDownbeats = false;
    params.scaleQuantize = true;
    params.scaleRoot = root;
    params.scale = scale;

    std::srand(static_cast<unsigned>(0x1600 + inputNote * 17 + root * 31 +
                                    static_cast<int>(scale) * 101));

    SynthPattern pattern{};
    AdvancedPatternGenerator::generatePattern(pattern, params);

    int observed = -1;
    for (const SynthStep& step : pattern.steps) {
        if (step.note < 0) continue;
        if (observed < 0) observed = step.note;
        assert(step.note == observed);
    }
    assert(observed >= 0);
    return observed;
}

void assertScalePitchClasses(ScaleType scale, uint16_t expectedMask) {
    uint16_t observedMask = 0;
    for (int note = 60; note < 72; ++note) {
        const int quantized = generateFixedInputNote(note, 0, scale);
        assert(quantized >= 60 && quantized < 72);
        observedMask = static_cast<uint16_t>(
            observedMask | static_cast<uint16_t>(1u << (quantized % 12)));
    }
    assert(observedMask == expectedMask);
}

void testAllScaleTypes() {
    assertScalePitchClasses(MINOR, pitchClassMask({0, 2, 3, 5, 7, 8, 10}));
    assertScalePitchClasses(MAJOR, pitchClassMask({0, 2, 4, 5, 7, 9, 11}));
    assertScalePitchClasses(DORIAN, pitchClassMask({0, 2, 3, 5, 7, 9, 10}));
    assertScalePitchClasses(PHRYGIAN, pitchClassMask({0, 1, 3, 5, 7, 8, 10}));
    assertScalePitchClasses(LYDIAN, pitchClassMask({0, 2, 4, 6, 7, 9, 11}));
    assertScalePitchClasses(MIXOLYDIAN, pitchClassMask({0, 2, 4, 5, 7, 9, 10}));
    assertScalePitchClasses(LOCRIAN, pitchClassMask({0, 1, 3, 5, 6, 8, 10}));
    assertScalePitchClasses(PENTATONIC_MJ, pitchClassMask({0, 2, 4, 7, 9}));
    assertScalePitchClasses(PENTATONIC_MN, pitchClassMask({0, 3, 5, 7, 10}));
    assertScalePitchClasses(CHROMATIC,
                            pitchClassMask({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
}

int legacySevenScaleQuantize(int note, int root, ScaleType scale) {
    static const int intervals[][7] = {
        {0, 2, 3, 5, 7, 8, 10},
        {0, 2, 4, 5, 7, 9, 11},
        {0, 2, 3, 5, 7, 9, 10},
        {0, 1, 3, 5, 7, 8, 10},
        {0, 2, 4, 6, 7, 9, 11},
        {0, 2, 4, 5, 7, 9, 10},
        {0, 1, 3, 5, 6, 8, 10},
    };

    const int* scaleIntervals = intervals[static_cast<int>(scale)];
    const int octave = note / 12;
    const int semitone = note % 12;
    int closest = 0;
    int minDistance = 12;
    for (int i = 0; i < 7; ++i) {
        const int scaleTone = (root + scaleIntervals[i]) % 12;
        const int difference = semitone - scaleTone;
        const int distance = difference < 0 ? -difference : difference;
        if (distance < minDistance) {
            minDistance = distance;
            closest = scaleTone;
        }
    }
    return octave * 12 + closest;
}

void testSevenLegacyScalesRemainExact() {
    for (int scaleValue = static_cast<int>(MINOR);
         scaleValue <= static_cast<int>(LOCRIAN); ++scaleValue) {
        const ScaleType scale = static_cast<ScaleType>(scaleValue);
        for (int root : {0, 2, 11}) {
            for (int note = 36; note <= 95; ++note) {
                const int expected = legacySevenScaleQuantize(note, root, scale);
                const int actual = generateFixedInputNote(note, root, scale);
                assert(actual == expected);
            }
        }
    }
}

void testChromaticIsIdentity() {
    for (int root : {0, 5, 11}) {
        for (int note = 48; note < 84; ++note) {
            assert(generateFixedInputNote(note, root, CHROMATIC) == note);
        }
    }
}

}  // namespace

int main() {
    testAllScaleTypes();
    testSevenLegacyScalesRemainExact();
    testChromaticIsIdentity();
    std::cout << "Scale quantization host matrix: OK\n";
    return 0;
}
