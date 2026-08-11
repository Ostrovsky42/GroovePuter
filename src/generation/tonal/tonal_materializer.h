#ifndef GROOVEPUTER_GENERATION_TONAL_TONAL_MATERIALIZER_H
#define GROOVEPUTER_GENERATION_TONAL_TONAL_MATERIALIZER_H

#include <cstdint>
#include <type_traits>

#include "tonal_projector.h"
#include "../roles/chord_progression.h"

namespace GroovePuterRhythm {

enum class TonalMaterializationStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  ProjectionFailed,
  Count,
};

struct TonalMaterializationRequest {
  // Timing owners are upstream. Tonal materialization may never add/move an
  // onset or continuation.
  StepMask onsets = 0;
  StepMask continuations = 0;

  // ChordRhythm timing marks the start of harmonic events. ChordProgression
  // supplies degree/chromatic-root content for those events.
  StepMask harmonicEventOnsets = 0;
  ChordProgressionPlan progression{};

  // One value per role-onset ordinal. Clear tag = scale-degree offset relative
  // to the active harmonic event. Set tag = exact semitone offset relative to
  // the active harmonic event root.
  uint16_t semitoneOffsetOrdinals = 0;
  int8_t tonalOffsets[kStepsPerBar]{};

  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;
  uint8_t minMidi = 24;
  uint8_t maxMidi = 71;
  uint8_t maxAdjacentLeapSemitones = 127;
};

struct TonalMaterializationPlan {
  StepMask onsets = 0;
  StepMask continuations = 0;
  uint8_t onsetCount = 0;
  uint8_t onsetSteps[kStepsPerBar]{};
  uint8_t midiNotes[kStepsPerBar]{};
};

struct TonalMaterializationResult {
  TonalMaterializationStatus status =
      TonalMaterializationStatus::InvalidRequest;
  TonalProjectionStatus projectionStatus =
      TonalProjectionStatus::InvalidRequest;
  TonalMaterializationPlan plan{};
};

TonalMaterializationResult materializeTonalIntent(
    const TonalMaterializationRequest& request);

static_assert(std::is_trivially_copyable<TonalMaterializationRequest>::value,
              "TonalMaterializationRequest must remain transient");
static_assert(std::is_trivially_copyable<TonalMaterializationPlan>::value,
              "TonalMaterializationPlan must remain transient");
static_assert(sizeof(TonalMaterializationRequest) <= 64,
              "TonalMaterializationRequest exceeded its command-time budget");
static_assert(sizeof(TonalMaterializationPlan) <= 40,
              "TonalMaterializationPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_TONAL_TONAL_MATERIALIZER_H
