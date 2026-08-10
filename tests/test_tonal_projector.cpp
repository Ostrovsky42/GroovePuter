#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/tonal/tonal_projector.h"

using namespace GroovePuterRhythm;

namespace {

// These numeric values are the existing global ScaleType ABI. The source
// regression independently parses scenes.h and pins the exact enum order.
constexpr ScaleTypeValue kMinorValue = 0;
constexpr ScaleTypeValue kMajorValue = 1;
constexpr ScaleTypeValue kDorianValue = 2;
constexpr ScaleTypeValue kPhrygianValue = 3;
constexpr ScaleTypeValue kLydianValue = 4;
constexpr ScaleTypeValue kMixolydianValue = 5;
constexpr ScaleTypeValue kLocrianValue = 6;
constexpr ScaleTypeValue kMajorPentatonicValue = 7;
constexpr ScaleTypeValue kMinorPentatonicValue = 8;
constexpr ScaleTypeValue kChromaticValue = 9;

TonalProjectionRequest baseRequest() {
  TonalProjectionRequest request{};
  request.rootPitchClass = 0;  // C
  request.scaleTypeValue = kMajorValue;
  request.minMidi = 48;
  request.maxMidi = 72;
  request.maxAdjacentLeapSemitones = 127;
  return request;
}

void assertNotes(const TonalProjectionResult& result,
                 const uint8_t* expected,
                 uint8_t count) {
  assert(result.status == TonalProjectionStatus::Ok);
  assert(result.noteCount == count);
  for (uint8_t i = 0; i < count; ++i)
    assert(result.midiNotes[i] == expected[i]);
}

void assertAtomicFailure(const TonalProjectionResult& result,
                         TonalProjectionStatus expectedStatus) {
  assert(result.status == expectedStatus);
  assert(result.noteCount == 0);
  assert(result.rootAnchorMidi == 0);
  for (uint8_t i = 0; i < kStepsPerBar; ++i)
    assert(result.midiNotes[i] == 0);
}

}  // namespace

int main() {
  assert(kDefaultScaleTypeValue == kDorianValue);

  // Exact ScaleType ABI coverage and real cardinalities.
  assert(isValidScaleTypeValue(kMinorValue));
  assert(isValidScaleTypeValue(kMajorValue));
  assert(isValidScaleTypeValue(kDorianValue));
  assert(isValidScaleTypeValue(kPhrygianValue));
  assert(isValidScaleTypeValue(kLydianValue));
  assert(isValidScaleTypeValue(kMixolydianValue));
  assert(isValidScaleTypeValue(kLocrianValue));
  assert(isValidScaleTypeValue(kMajorPentatonicValue));
  assert(isValidScaleTypeValue(kMinorPentatonicValue));
  assert(isValidScaleTypeValue(kChromaticValue));
  assert(!isValidScaleTypeValue(255));

  assert(scaleCardinality(kMinorValue) == 7);
  assert(scaleCardinality(kMajorValue) == 7);
  assert(scaleCardinality(kDorianValue) == 7);
  assert(scaleCardinality(kPhrygianValue) == 7);
  assert(scaleCardinality(kLydianValue) == 7);
  assert(scaleCardinality(kMixolydianValue) == 7);
  assert(scaleCardinality(kLocrianValue) == 7);
  assert(scaleCardinality(kMajorPentatonicValue) == 5);
  assert(scaleCardinality(kMinorPentatonicValue) == 5);
  assert(scaleCardinality(kChromaticValue) == 12);
  assert(scaleCardinality(255) == 0);

  // The ordinal tag mask is exactly 16 bits. Pin the full-width boundary and
  // prove bit 15 still selects semitone units: in Locrian, untagged degree 7 is
  // +12, while the tagged value below must remain the exact +7 fifth.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = kStepsPerBar;
    request.scaleTypeValue = kLocrianValue;
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 15);
    for (uint8_t i = 0; i < kStepsPerBar; ++i)
      request.tonalOffsets[i] = 0;
    request.tonalOffsets[15] = 7;
    const auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::Ok);
    assert(result.noteCount == kStepsPerBar);
    assert(result.rootAnchorMidi == 60);
    for (uint8_t i = 0; i < 15; ++i)
      assert(result.midiNotes[i] == 60);
    assert(result.midiNotes[15] == 67);
  }

  // Empty input is valid and does not require the root pitch class to occur in
  // the register because no note is materialized.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 0;
    request.minMidi = 61;
    request.maxMidi = 61;
    const auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::ValidButEmpty);
    assert(result.noteCount == 0);
  }

  // Root anchor is the feasible selected root pitch-class occurrence nearest
  // register center; ties resolve downward because the scan is ascending.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.tonalOffsets[0] = 0;
    auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::Ok);
    assert(result.rootAnchorMidi == 60);
    assert(result.midiNotes[0] == 60);

    request.rootPitchClass = 2;  // D, candidates 50/62 in 48..72.
    result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::Ok);
    assert(result.rootAnchorMidi == 62);
    assert(result.midiNotes[0] == 62);
  }

  // If the center-nearest root cannot fit the complete intent, choose the
  // nearest feasible root rather than returning a false register failure.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 48;
    request.maxMidi = 66;
    request.tonalOffsets[0] = 12;
    request.semitoneOffsetOrdinals = 1;
    const auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::Ok);
    assert(result.rootAnchorMidi == 48);
    assert(result.midiNotes[0] == 60);
  }

  // Seven-note diatonic scale-degree projection.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 5;
    request.scaleTypeValue = kMajorValue;
    request.tonalOffsets[0] = 0;
    request.tonalOffsets[1] = 1;
    request.tonalOffsets[2] = 2;
    request.tonalOffsets[3] = 6;
    request.tonalOffsets[4] = 7;
    const uint8_t expected[] = {60, 62, 64, 71, 72};
    assertNotes(projectTonalIntent(request), expected, 5);
  }

  // Negative degrees use floor division, not truncating C++ division.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 4;
    request.scaleTypeValue = kMajorValue;
    request.tonalOffsets[0] = -1;  // B below C
    request.tonalOffsets[1] = -2;  // A below C
    request.tonalOffsets[2] = -7;  // C one octave below
    request.tonalOffsets[3] = 0;
    const uint8_t expected[] = {59, 57, 48, 60};
    assertNotes(projectTonalIntent(request), expected, 4);
  }

  // Major pentatonic uses five degrees per octave: degree 4=A, degree 5=C.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 4;
    request.scaleTypeValue = kMajorPentatonicValue;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 4;
    request.tonalOffsets[2] = 5;
    request.tonalOffsets[3] = -1;
    const uint8_t expected[] = {62, 69, 72, 57};
    assertNotes(projectTonalIntent(request), expected, 4);
  }

  // Minor pentatonic and chromatic use their own real cardinalities.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scaleTypeValue = kMinorPentatonicValue;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 4;
    request.tonalOffsets[2] = -1;
    const uint8_t expected[] = {63, 70, 58};
    assertNotes(projectTonalIntent(request), expected, 3);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 4;
    request.scaleTypeValue = kChromaticValue;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 11;
    request.tonalOffsets[2] = 12;
    request.tonalOffsets[3] = -1;
    const uint8_t expected[] = {61, 71, 72, 59};
    assertNotes(projectTonalIntent(request), expected, 4);
  }

  // Tagged semitone intent bypasses scale-degree interpretation. Locrian degree
  // 4 would be +6, but a tagged fifth stays exact +7 semitones.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scaleTypeValue = kLocrianValue;
    request.tonalOffsets[0] = 0;
    request.tonalOffsets[1] = 7;
    request.tonalOffsets[2] = 4;
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1);
    const uint8_t expected[] = {60, 67, 66};
    assertNotes(projectTonalIntent(request), expected, 3);
  }

  // Mixed units are compared for common leap only after absolute projection.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scaleTypeValue = kMajorPentatonicValue;
    request.tonalOffsets[0] = 0;   // degree root => C60
    request.tonalOffsets[1] = 7;   // tagged fifth => G67
    request.tonalOffsets[2] = 1;   // degree 1 => D62
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1);
    request.maxAdjacentLeapSemitones = 7;
    const uint8_t expected[] = {60, 67, 62};
    assertNotes(projectTonalIntent(request), expected, 3);

    request.maxAdjacentLeapSemitones = 6;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::LeapExceeded);
  }

  // Named octave intent is not silently octave-folded. Only C60 exists as C
  // root in 54..66, so tagged +12 cannot be materialized in the corridor.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 54;
    request.maxMidi = 66;
    request.tonalOffsets[0] = 12;
    request.semitoneOffsetOrdinals = 1;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::NoteOutOfRegister);
  }

  // Register without selected root pitch class has an explicit atomic status.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 61;
    request.maxMidi = 61;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::RootOutOfRegister);
  }

  // Invalid request contracts are atomic too.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = static_cast<uint8_t>(kStepsPerBar + 1u);
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 2;
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 4);
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.rootPitchClass = 12;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.scaleTypeValue = 255;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 80;
    request.maxMidi = 70;
    assertAtomicFailure(projectTonalIntent(request),
                        TonalProjectionStatus::InvalidRequest);
  }

  // Identical transient input is field-stable.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scaleTypeValue = kDorianValue;
    request.tonalOffsets[0] = 0;
    request.tonalOffsets[1] = 2;
    request.tonalOffsets[2] = -1;
    const auto a = projectTonalIntent(request);
    const auto b = projectTonalIntent(request);
    assert(a.status == b.status);
    assert(a.noteCount == b.noteCount);
    assert(a.rootAnchorMidi == b.rootAnchorMidi);
    assert(std::memcmp(a.midiNotes, b.midiNotes, sizeof(a.midiNotes)) == 0);
  }

  return 0;
}
