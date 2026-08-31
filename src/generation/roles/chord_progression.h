#ifndef GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H
#define GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H

#include <cstdint>
#include <type_traits>

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

#endif  // GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H