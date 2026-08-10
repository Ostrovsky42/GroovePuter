#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/tonal/tonal_projector.h"

using namespace GroovePuterRhythm;

namespace {

TonalProjectionRequest baseRequest() {
  TonalProjectionRequest request{};
  request.rootPitchClass = 0;  // C
  request.scale = MAJOR;
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

}  // namespace

int main() {
  // Exact ScaleType coverage and real cardinalities.
  assert(isValidScaleType(MINOR));
  assert(isValidScaleType(MAJOR));
  assert(isValidScaleType(DORIAN));
  assert(isValidScaleType(PHRYGIAN));
  assert(isValidScaleType(LYDIAN));
  assert(isValidScaleType(MIXOLYDIAN));
  assert(isValidScaleType(LOCRIAN));
  assert(isValidScaleType(PENTATONIC_MJ));
  assert(isValidScaleType(PENTATONIC_MN));
  assert(isValidScaleType(CHROMATIC));
  assert(!isValidScaleType(static_cast<ScaleType>(255)));

  assert(scaleCardinality(MINOR) == 7);
  assert(scaleCardinality(MAJOR) == 7);
  assert(scaleCardinality(DORIAN) == 7);
  assert(scaleCardinality(PHRYGIAN) == 7);
  assert(scaleCardinality(LYDIAN) == 7);
  assert(scaleCardinality(MIXOLYDIAN) == 7);
  assert(scaleCardinality(LOCRIAN) == 7);
  assert(scaleCardinality(PENTATONIC_MJ) == 5);
  assert(scaleCardinality(PENTATONIC_MN) == 5);
  assert(scaleCardinality(CHROMATIC) == 12);
  assert(scaleCardinality(static_cast<ScaleType>(255)) == 0);

  // Empty input is a valid transient result and does not require a root note in
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

  // The root anchor is the root pitch-class occurrence nearest register center.
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

  // Seven-note diatonic scale-degree projection.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 5;
    request.scale = MAJOR;
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
    request.scale = MAJOR;
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
    request.scale = PENTATONIC_MJ;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 4;
    request.tonalOffsets[2] = 5;
    request.tonalOffsets[3] = -1;
    const uint8_t expected[] = {62, 69, 72, 57};
    assertNotes(projectTonalIntent(request), expected, 4);
  }

  // Minor pentatonic and chromatic have independent real cardinalities.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scale = PENTATONIC_MN;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 4;
    request.tonalOffsets[2] = -1;
    const uint8_t expected[] = {63, 70, 58};
    assertNotes(projectTonalIntent(request), expected, 3);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 4;
    request.scale = CHROMATIC;
    request.tonalOffsets[0] = 1;
    request.tonalOffsets[1] = 11;
    request.tonalOffsets[2] = 12;
    request.tonalOffsets[3] = -1;
    const uint8_t expected[] = {61, 71, 72, 59};
    assertNotes(projectTonalIntent(request), expected, 4);
  }

  // Tagged semitone intent bypasses scale-degree interpretation. Locrian degree
  // 4 would be +6, but a tagged fifth remains an exact +7 semitones.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scale = LOCRIAN;
    request.tonalOffsets[0] = 0;
    request.tonalOffsets[1] = 7;
    request.tonalOffsets[2] = 4;
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1);
    const uint8_t expected[] = {60, 67, 66};
    assertNotes(projectTonalIntent(request), expected, 3);
  }

  // Mixed unit input is unambiguous and common leap validation occurs only
  // after every value has become an absolute MIDI note.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scale = PENTATONIC_MJ;
    request.tonalOffsets[0] = 0;   // degree root => C60
    request.tonalOffsets[1] = 7;   // tagged fifth => G67
    request.tonalOffsets[2] = 1;   // degree 1 => D62
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1);
    request.maxAdjacentLeapSemitones = 7;
    const uint8_t expected[] = {60, 67, 62};
    assertNotes(projectTonalIntent(request), expected, 3);

    request.maxAdjacentLeapSemitones = 6;
    const auto rejected = projectTonalIntent(request);
    assert(rejected.status == TonalProjectionStatus::LeapExceeded);
  }

  // Named octave intent is not silently octave-folded to satisfy the register.
  // The only C root in 54..66 is C60, so tagged +12 must reject as C72.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 54;
    request.maxMidi = 66;
    request.tonalOffsets[0] = 12;
    request.semitoneOffsetOrdinals = 1;
    const auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::NoteOutOfRegister);
  }

  // A register without the selected root pitch class has an explicit status.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 61;
    request.maxMidi = 61;
    const auto result = projectTonalIntent(request);
    assert(result.status == TonalProjectionStatus::RootOutOfRegister);
  }

  // Invalid request contracts.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = static_cast<uint8_t>(kStepsPerBar + 1u);
    assert(projectTonalIntent(request).status ==
           TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 2;
    request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 4);
    assert(projectTonalIntent(request).status ==
           TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.rootPitchClass = 12;
    assert(projectTonalIntent(request).status ==
           TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.scale = static_cast<ScaleType>(255);
    assert(projectTonalIntent(request).status ==
           TonalProjectionStatus::InvalidRequest);
  }

  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 1;
    request.minMidi = 80;
    request.maxMidi = 70;
    assert(projectTonalIntent(request).status ==
           TonalProjectionStatus::InvalidRequest);
  }

  // Identical transient input is byte-stable at the semantic field level.
  {
    TonalProjectionRequest request = baseRequest();
    request.onsetCount = 3;
    request.scale = DORIAN;
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
