#ifndef GROOVEPUTER_GENERATION_TONAL_TONAL_PROJECTOR_H
#define GROOVEPUTER_GENERATION_TONAL_TONAL_PROJECTOR_H

#include <cstdint>
#include <type_traits>

#include "src/generation/rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

// Compact transient ABI value of the existing global ScaleType enum in
// scenes.h. We intentionally do not define a second scale enum here: the future
// integration adapter passes static_cast<uint8_t>(ScaleType), while source
// regressions pin the existing enum order so ABI drift fails loudly.
using ScaleTypeValue = uint8_t;
constexpr ScaleTypeValue kDefaultScaleTypeValue = 2u;  // DORIAN

enum class TonalProjectionStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  RootOutOfRegister,
  NoteOutOfRegister,
  LeapExceeded,
  Count,
};

struct TonalProjectionRequest {
  uint8_t onsetCount = 0;

  // One value per onset ordinal. If bit N in semitoneOffsetOrdinals is set,
  // tonalOffsets[N] is a chromatic semitone offset from the harmonic root.
  // Otherwise it is a scale-degree offset in the selected ScaleType value.
  int8_t tonalOffsets[kStepsPerBar]{};
  uint16_t semitoneOffsetOrdinals = 0;

  uint8_t rootPitchClass = 0;  // 0=C ... 11=B
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;

  // Inclusive absolute-MIDI register corridor. The projector chooses the root
  // pitch-class occurrence nearest the corridor midpoint as its deterministic
  // harmonic anchor, then applies every tonal offset exactly. It never silently
  // octave-folds an out-of-register result because that could destroy a tagged
  // fifth/octave relation.
  uint8_t minMidi = 36;
  uint8_t maxMidi = 84;

  // Common musical leap guard evaluated only after both tagged/untagged values
  // have been converted to absolute MIDI notes. 127 effectively disables it.
  uint8_t maxAdjacentLeapSemitones = 127;
};

struct TonalProjectionResult {
  TonalProjectionStatus status = TonalProjectionStatus::InvalidRequest;
  uint8_t noteCount = 0;
  uint8_t rootAnchorMidi = 0;
  uint8_t midiNotes[kStepsPerBar]{};
};

TonalProjectionResult projectTonalIntent(const TonalProjectionRequest& request);

bool isValidScaleTypeValue(ScaleTypeValue scaleTypeValue);
uint8_t scaleCardinality(ScaleTypeValue scaleTypeValue);

static_assert(std::is_trivially_copyable<TonalProjectionRequest>::value,
              "TonalProjectionRequest must remain transient/fixed-capacity");
static_assert(std::is_trivially_copyable<TonalProjectionResult>::value,
              "TonalProjectionResult must remain transient/fixed-capacity");
static_assert(sizeof(TonalProjectionRequest) <= 24,
              "TonalProjectionRequest exceeded its command-time budget");
static_assert(sizeof(TonalProjectionResult) <= 20,
              "TonalProjectionResult exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_TONAL_TONAL_PROJECTOR_H
