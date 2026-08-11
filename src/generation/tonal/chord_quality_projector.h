#ifndef GROOVEPUTER_GENERATION_TONAL_CHORD_QUALITY_PROJECTOR_H
#define GROOVEPUTER_GENERATION_TONAL_CHORD_QUALITY_PROJECTOR_H

#include <cstdint>
#include <type_traits>

#include "tonal_projector.h"
#include "../roles/chord_progression.h"

namespace GroovePuterRhythm {

constexpr uint8_t kMaxChordQualityTones = 4;

enum class TriadPolarity : uint8_t {
  Major = 0,
  Minor,
  Count,
};

enum class ChordQualityProjectionStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  UnsupportedQuality,
  ProjectionFailed,
  Count,
};

// P1 feasibility request. This API deliberately has no StepMask, onset,
// continuation, retrigger or pattern fields: a chord-quality projection may
// change only the pitch-set attached to one already-owned harmonic event.
struct ChordQualityProjectionRequest {
  HarmonicEvent event{};
  TriadPolarity triadPolarity = TriadPolarity::Major;
  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;
  uint8_t minMidi = 36;
  uint8_t maxMidi = 84;
};

struct ChordQualityProjectionPlan {
  uint8_t toneCount = 0;
  uint8_t rootAnchorMidi = 0;
  uint8_t midiNotes[kMaxChordQualityTones]{};
};

struct ChordQualityProjectionResult {
  ChordQualityProjectionStatus status =
      ChordQualityProjectionStatus::InvalidRequest;
  TonalProjectionStatus projectionStatus =
      TonalProjectionStatus::InvalidRequest;
  ChordQualityProjectionPlan plan{};
};

ChordQualityProjectionResult projectChordQualityPitchSet(
    const ChordQualityProjectionRequest& request);

static_assert(std::is_trivially_copyable<ChordQualityProjectionRequest>::value,
              "ChordQualityProjectionRequest must stay fixed-capacity");
static_assert(std::is_trivially_copyable<ChordQualityProjectionPlan>::value,
              "ChordQualityProjectionPlan must stay fixed-capacity");
static_assert(sizeof(ChordQualityProjectionRequest) <= 12,
              "ChordQualityProjectionRequest exceeded P1 command budget");
static_assert(sizeof(ChordQualityProjectionPlan) <= 8,
              "ChordQualityProjectionPlan exceeded P1 result budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_TONAL_CHORD_QUALITY_PROJECTOR_H
