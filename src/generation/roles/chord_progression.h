#ifndef GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H
#define GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H

#include <cstdint>
#include <type_traits>

#include "../harmony/harmonic_quality.h"
#include "../generation_context.h"

namespace GroovePuterRhythm {

// Append-only. Auto remains zero because the numeric value participates in
// deterministic selection and progression fingerprints.
enum class ProgressionId : uint8_t {
  Auto = 0,
  StaticModal,
  PedalDrone,
  PopCycle,
  TwoFiveOne,
  ParallelShift,
  MinorFall,
  BorrowedLift,
  Count,
};

enum class ChordQuality : uint8_t {
  Triad = 0,
  Minor7,
  Major7,
  Dominant7,
  Sus4,
  Minor9,
  Major9,
  Diminished,
  Count,
};

// Legacy ordered/compound voicing view over the shared root-independent
// harmonic vocabulary. Pitch-set consumers use the same definition directly.
inline bool chordQualityIntervals(ChordQuality quality,
                                 uint8_t* out,
                                 uint8_t capacity,
                                 uint8_t& count) {
  GroovePuterHarmony::QualityId id = 0;
  if (!GroovePuterHarmony::legacyQualityToId(quality, id)) return false;
  const auto* definition = GroovePuterHarmony::qualityDefinition(id);
  if (definition == nullptr) return false;
  uint8_t values[12]{};
  uint8_t size = 0;
  for (uint8_t interval = 0; interval < 12; ++interval) {
    if ((definition->intervalMask & static_cast<uint16_t>(1u << interval)) == 0) continue;
    values[size++] = interval;
  }
  // Legacy 9th voicings spell pitch class 2 as the compound ninth (14).
  if (id == GroovePuterHarmony::kQualityMinor9 ||
      id == GroovePuterHarmony::kQualityMajor9) {
    for (uint8_t i = 1; i < size; ++i) {
      if (values[i] == 2) values[i] = 14;
    }
  }
  for (uint8_t i = 1; i < size; ++i) {
    const uint8_t value = values[i];
    uint8_t j = i;
    while (j > 0 && values[j - 1] > value) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = value;
  }
  if (out == nullptr || capacity < size) return false;
  for (uint8_t i = 0; i < size; ++i) out[i] = values[i];
  count = size;
  return true;
}

enum class ChordProgressionStatus : uint8_t {
  Ok = 0,
  ValidButStatic,
  InvalidRequest,
  Count,
};

constexpr uint8_t kMaxHarmonicEvents = 8;
constexpr uint8_t kMaxChordProgressionSourceEvents = 4;
constexpr int8_t kMaxRootOffsetSemitones = 2;

struct HarmonicEvent {
  uint8_t degree = 0;
  ChordQuality quality = ChordQuality::Triad;
  int8_t rootOffsetSemitones = 0;
};

// Complete selected WHAT source. harmonicEventCount deliberately does not
// participate: cardinality belongs to HarmonicRhythm WHEN, not source identity.
struct ChordProgressionSourceRequest {
  ProgressionId requestedId = ProgressionId::Auto;
  RhythmFamily family = RhythmFamily::FourFloor;
  GenerationContext generation{};
  uint8_t phraseBars = 1;
};

struct ChordProgressionSource {
  ProgressionId id = ProgressionId::Auto;
  uint8_t period = 0;
  HarmonicEvent events[kMaxChordProgressionSourceEvents]{};
};

struct ChordProgressionSourceResult {
  ChordProgressionStatus status = ChordProgressionStatus::InvalidRequest;
  ChordProgressionSource source{};
};

struct ChordProgressionEventResult {
  ChordProgressionStatus status = ChordProgressionStatus::InvalidRequest;
  HarmonicEvent event{};
};

struct ChordProgressionRequest {
  ProgressionId requestedId = ProgressionId::Auto;
  RhythmFamily family = RhythmFamily::FourFloor;
  GenerationContext generation{};
  uint8_t harmonicEventCount = 0;
  uint8_t phraseBars = 1;
};

struct ChordProgressionPlan {
  ProgressionId id = ProgressionId::Auto;
  uint8_t eventCount = 0;
  HarmonicEvent events[kMaxHarmonicEvents]{};
};

struct ChordProgressionResult {
  ChordProgressionStatus status = ChordProgressionStatus::InvalidRequest;
  ChordProgressionPlan plan{};
};

ChordProgressionSourceResult realizeChordProgressionSource(
    const ChordProgressionSourceRequest& request);
ChordProgressionEventResult chordProgressionEventAt(
    const ChordProgressionSource& source,
    uint32_t globalHarmonicOrdinal);
ChordProgressionResult realizeChordProgression(
    const ChordProgressionRequest& request);
const char* chordProgressionName(ProgressionId id);
bool isValidProgressionId(ProgressionId id, bool allowAuto = true);

static_assert(kMaxHarmonicEvents == 8,
              "ChordProgressionPlan finite carrier capacity changed");
static_assert(kMaxChordProgressionSourceEvents == 4,
              "ChordProgressionSource intrinsic grammar capacity changed");
static_assert(std::is_trivially_copyable<HarmonicEvent>::value,
              "HarmonicEvent must remain fixed-capacity");
static_assert(sizeof(HarmonicEvent) <= 4,
              "HarmonicEvent exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionSource>::value,
              "ChordProgressionSource must remain fixed-capacity");
static_assert(sizeof(ChordProgressionSource) <= 16,
              "ChordProgressionSource exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionSourceRequest>::value,
              "ChordProgressionSourceRequest must remain fixed-capacity");
static_assert(sizeof(ChordProgressionSourceRequest) <= 16,
              "ChordProgressionSourceRequest exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionSourceResult>::value,
              "ChordProgressionSourceResult must remain fixed-capacity");
static_assert(sizeof(ChordProgressionSourceResult) <= 20,
              "ChordProgressionSourceResult exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionEventResult>::value,
              "ChordProgressionEventResult must remain fixed-capacity");
static_assert(sizeof(ChordProgressionEventResult) <= 8,
              "ChordProgressionEventResult exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionPlan>::value,
              "ChordProgressionPlan must remain fixed-capacity");
static_assert(sizeof(ChordProgressionPlan) <= 32,
              "ChordProgressionPlan exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionRequest>::value,
              "ChordProgressionRequest must remain fixed-capacity");

}  // namespace GroovePuterRhythm

inline bool GroovePuterHarmony::legacyQualityToId(
    GroovePuterRhythm::ChordQuality quality,
    GroovePuterHarmony::QualityId& out) {
  using GroovePuterHarmony::kQualityDiminished;
  using GroovePuterHarmony::kQualityDominant7;
  using GroovePuterHarmony::kQualityMajor7;
  using GroovePuterHarmony::kQualityMajor9;
  using GroovePuterHarmony::kQualityMinor7;
  using GroovePuterHarmony::kQualityMinor9;
  using GroovePuterHarmony::kQualitySus4;
  using GroovePuterHarmony::kQualityMajor;
  switch (quality) {
    case GroovePuterRhythm::ChordQuality::Triad: out = kQualityMajor; return true;
    case GroovePuterRhythm::ChordQuality::Minor7: out = kQualityMinor7; return true;
    case GroovePuterRhythm::ChordQuality::Major7: out = kQualityMajor7; return true;
    case GroovePuterRhythm::ChordQuality::Dominant7: out = kQualityDominant7; return true;
    case GroovePuterRhythm::ChordQuality::Sus4: out = kQualitySus4; return true;
    case GroovePuterRhythm::ChordQuality::Minor9: out = kQualityMinor9; return true;
    case GroovePuterRhythm::ChordQuality::Major9: out = kQualityMajor9; return true;
    case GroovePuterRhythm::ChordQuality::Diminished:
      out = kQualityDiminished;
      return true;
    case GroovePuterRhythm::ChordQuality::Count: return false;
  }
  return false;
}

#endif  // GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H
