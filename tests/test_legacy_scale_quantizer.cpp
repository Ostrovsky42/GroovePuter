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

void assertExactPitchClassMap(ScaleType scale, const int (&expected)[12]) {
    for (int sourcePitchClass = 0; sourcePitchClass < 12; ++sourcePitchClass) {
        const int result = quantizedGeneratedNote(scale, 48 + sourcePitchClass);
        assert(result == 48 + expected[sourcePitchClass]);
    }
}

}  // namespace

int main() {
    // Exact legacy nearest-tone behavior for a C-root scale. These maps protect
    // both scale membership and the existing first-match tie-breaking rule.
    static constexpr int kMinorMap[12] =
        {0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 10, 10};
    static constexpr int kMajorMap[12] =
        {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11};
    static constexpr int kDorianMap[12] =
        {0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 10, 10};
    static constexpr int kPhrygianMap[12] =
        {0, 1, 1, 3, 3, 5, 5, 7, 8, 8, 10, 10};
    static constexpr int kLydianMap[12] =
        {0, 0, 2, 2, 4, 4, 6, 7, 7, 9, 9, 11};
    static constexpr int kMixolydianMap[12] =
        {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 10, 10};
    static constexpr int kLocrianMap[12] =
        {0, 1, 1, 3, 3, 5, 6, 6, 8, 8, 10, 10};
    static constexpr int kMajorPentatonicMap[12] =
        {0, 0, 2, 2, 4, 4, 7, 7, 7, 9, 9, 9};
    static constexpr int kMinorPentatonicMap[12] =
        {0, 0, 3, 3, 3, 5, 5, 7, 7, 10, 10, 10};
    static constexpr int kChromaticMap[12] =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    assertExactPitchClassMap(MINOR, kMinorMap);
    assertExactPitchClassMap(MAJOR, kMajorMap);
    assertExactPitchClassMap(DORIAN, kDorianMap);
    assertExactPitchClassMap(PHRYGIAN, kPhrygianMap);
    assertExactPitchClassMap(LYDIAN, kLydianMap);
    assertExactPitchClassMap(MIXOLYDIAN, kMixolydianMap);
    assertExactPitchClassMap(LOCRIAN, kLocrianMap);
    assertExactPitchClassMap(PENTATONIC_MJ, kMajorPentatonicMap);
    assertExactPitchClassMap(PENTATONIC_MN, kMinorPentatonicMap);
    assertExactPitchClassMap(CHROMATIC, kChromaticMap);

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
