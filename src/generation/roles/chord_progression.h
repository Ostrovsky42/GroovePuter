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
constexpr int8_t kMaxRootOffsetSemitones = 2;

struct HarmonicEvent {
  uint8_t degree = 0;
  ChordQuality quality = ChordQuality::Triad;
  int8_t rootOffsetSemitones = 0;
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

ChordProgressionResult realizeChordProgression(
    const ChordProgressionRequest& request);
const char* chordProgressionName(ProgressionId id);
bool isValidProgressionId(ProgressionId id, bool allowAuto = true);

static_assert(std::is_trivially_copyable<ChordProgressionPlan>::value,
              "ChordProgressionPlan must remain fixed-capacity");
static_assert(sizeof(ChordProgressionPlan) <= 32,
              "ChordProgressionPlan exceeded its command-time budget");
static_assert(std::is_trivially_copyable<ChordProgressionRequest>::value,
              "ChordProgressionRequest must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_CHORD_PROGRESSION_H
