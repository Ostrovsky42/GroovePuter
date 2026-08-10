#include <cassert>
#include <cstddef>
#include <cstdlib>

#include "src/dsp/advanced_pattern_generator.h"

namespace {

int quantizedGeneratedNote(ScaleType scale, int sourceNote, int root = 0) {
    GeneratorParams params{};
    params.minNotes = 1;
    params.maxNotes = 1;
    params.minOctave = sourceNote;
    params.maxOctave = sourceNote;
    params.swingAmount = 0.0f;
    params.velocityRange = 0.0f;
    params.ghostNoteProbability = 0.0f;
    params.microTimingAmount = 0.0f;
    params.scaleQuantize = true;
    params.scaleRoot = root;
    params.scale = scale;

    std::srand(1);
    SynthPattern pattern{};
    AdvancedPatternGenerator::generatePattern(pattern, params);

    int result = -1;
    int noteCount = 0;
    for (int step = 0; step < SynthPattern::kSteps; ++step) {
        if (pattern.steps[step].note < 0) continue;
        result = pattern.steps[step].note;
        ++noteCount;
    }

    assert(noteCount == 1);
    return result;
}

bool containsPitchClass(int pitchClass, const int* values, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (values[i] == pitchClass) return true;
    }
    return false;
}

void assertScaleOutputsStayInSet(
    ScaleType scale,
    const int* pitchClasses,
    std::size_t pitchClassCount) {
    for (int sourcePitchClass = 0; sourcePitchClass < 12; ++sourcePitchClass) {
        const int result = quantizedGeneratedNote(scale, 48 + sourcePitchClass);
        const int resultPitchClass = result % 12;
        assert(containsPitchClass(resultPitchClass, pitchClasses, pitchClassCount));
    }
}

}  // namespace

int main() {
    static constexpr int kMinor[] = {0, 2, 3, 5, 7, 8, 10};
    static constexpr int kMajor[] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int kDorian[] = {0, 2, 3, 5, 7, 9, 10};
    static constexpr int kPhrygian[] = {0, 1, 3, 5, 7, 8, 10};
    static constexpr int kLydian[] = {0, 2, 4, 6, 7, 9, 11};
    static constexpr int kMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
    static constexpr int kLocrian[] = {0, 1, 3, 5, 6, 8, 10};
    static constexpr int kMajorPentatonic[] = {0, 2, 4, 7, 9};
    static constexpr int kMinorPentatonic[] = {0, 3, 5, 7, 10};
    static constexpr int kChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    assertScaleOutputsStayInSet(MINOR, kMinor, 7);
    assertScaleOutputsStayInSet(MAJOR, kMajor, 7);
    assertScaleOutputsStayInSet(DORIAN, kDorian, 7);
    assertScaleOutputsStayInSet(PHRYGIAN, kPhrygian, 7);
    assertScaleOutputsStayInSet(LYDIAN, kLydian, 7);
    assertScaleOutputsStayInSet(MIXOLYDIAN, kMixolydian, 7);
    assertScaleOutputsStayInSet(LOCRIAN, kLocrian, 7);
    assertScaleOutputsStayInSet(PENTATONIC_MJ, kMajorPentatonic, 5);
    assertScaleOutputsStayInSet(PENTATONIC_MN, kMinorPentatonic, 5);
    assertScaleOutputsStayInSet(CHROMATIC, kChromatic, 12);

    // These three probes specifically fail with the inherited `scale % 7`
    // aliasing: major pentatonic->minor, minor pentatonic->major,
    // chromatic->dorian.
    assert(quantizedGeneratedNote(PENTATONIC_MJ, 51) == 50);
    assert(quantizedGeneratedNote(PENTATONIC_MN, 51) == 51);
    assert(quantizedGeneratedNote(CHROMATIC, 49) == 49);

    // Root transposition must still use the selected scale rather than an
    // enum-index alias. D major pentatonic quantizes F (53) to E (52).
    assert(quantizedGeneratedNote(PENTATONIC_MJ, 53, 2) == 52);

    return 0;
}
