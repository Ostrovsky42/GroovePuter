#include "chord_progression.h"

namespace GroovePuterRhythm {
namespace {

struct Grammar {
  HarmonicEvent events[4]{};
  uint8_t count = 0;
};

struct GrammarSet {
  Grammar variants[2]{};
  uint8_t count = 0;
};

struct ProgressionCandidates {
  ProgressionId values[4]{};
  uint8_t count = 0;
};

constexpr HarmonicEvent event(uint8_t degree,
                              ChordQuality quality,
                              int8_t rootOffsetSemitones = 0) {
  return HarmonicEvent{degree, quality, rootOffsetSemitones};
}

constexpr Grammar grammar(HarmonicEvent a) {
  return Grammar{{a, {}, {}, {}}, 1};
}

constexpr Grammar grammar(HarmonicEvent a, HarmonicEvent b, HarmonicEvent c) {
  return Grammar{{a, b, c, {}}, 3};
}

constexpr Grammar grammar(HarmonicEvent a,
                          HarmonicEvent b,
                          HarmonicEvent c,
                          HarmonicEvent d) {
  return Grammar{{a, b, c, d}, 4};
}

constexpr GrammarSet kStaticModal = {
    {grammar(event(0, ChordQuality::Triad)), {}}, 1};
constexpr GrammarSet kPedalDrone = {
    {grammar(event(0, ChordQuality::Sus4)), {}}, 1};
constexpr GrammarSet kPopCycle = {
    {grammar(event(0, ChordQuality::Triad),
             event(4, ChordQuality::Triad),
             event(5, ChordQuality::Minor7),
             event(3, ChordQuality::Major7)),
     grammar(event(0, ChordQuality::Triad),
             event(5, ChordQuality::Minor7),
             event(3, ChordQuality::Major7),
             event(4, ChordQuality::Dominant7))},
    2};
constexpr GrammarSet kTwoFiveOne = {
    {grammar(event(1, ChordQuality::Minor7),
             event(4, ChordQuality::Dominant7),
             event(0, ChordQuality::Major7)),
     grammar(event(1, ChordQuality::Minor9),
             event(4, ChordQuality::Dominant7),
             event(0, ChordQuality::Major9))},
    2};
constexpr GrammarSet kParallelShift = {
    {grammar(event(0, ChordQuality::Minor9),
             event(0, ChordQuality::Minor9, 1),
             event(0, ChordQuality::Minor9),
             event(0, ChordQuality::Minor9, -1)),
     grammar(event(0, ChordQuality::Minor7),
             event(0, ChordQuality::Minor7, -2),
             event(0, ChordQuality::Minor7),
             event(0, ChordQuality::Minor7, 2))},
    2};
constexpr GrammarSet kMinorFall = {
    {grammar(event(0, ChordQuality::Minor7),
             event(5, ChordQuality::Major7),
             event(2, ChordQuality::Major7),
             event(6, ChordQuality::Major7)),
     grammar(event(0, ChordQuality::Minor7),
             event(5, ChordQuality::Triad),
             event(2, ChordQuality::Triad),
             event(6, ChordQuality::Triad))},
    2};
constexpr GrammarSet kBorrowedLift = {
    {grammar(event(0, ChordQuality::Minor7),
             event(3, ChordQuality::Major7),
             event(4, ChordQuality::Dominant7),
             event(3, ChordQuality::Major7, 1)),
     grammar(event(0, ChordQuality::Triad),
             event(4, ChordQuality::Dominant7),
             event(3, ChordQuality::Major7, 1),
             event(0, ChordQuality::Triad))},
    2};

bool validPhraseBars(uint8_t bars) {
  return bars == 1 || bars == 2 || bars == 4 || bars == 8;
}

bool validQuality(ChordQuality quality) {
  return static_cast<uint8_t>(quality) <
         static_cast<uint8_t>(ChordQuality::Count);
}

bool allowsChromaticRootOffset(ProgressionId id) {
  return id == ProgressionId::ParallelShift ||
         id == ProgressionId::BorrowedLift;
}

const GrammarSet* grammarSetFor(ProgressionId id) {
  switch (id) {
    case ProgressionId::StaticModal: return &kStaticModal;
    case ProgressionId::PedalDrone: return &kPedalDrone;
    case ProgressionId::PopCycle: return &kPopCycle;
    case ProgressionId::TwoFiveOne: return &kTwoFiveOne;
    case ProgressionId::ParallelShift: return &kParallelShift;
    case ProgressionId::MinorFall: return &kMinorFall;
    case ProgressionId::BorrowedLift: return &kBorrowedLift;
    case ProgressionId::Auto:
    case ProgressionId::Count:
      return nullptr;
  }
  return nullptr;
}

ProgressionCandidates candidatesFor(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor:
      return {{ProgressionId::StaticModal, ProgressionId::PopCycle,
               ProgressionId::MinorFall, ProgressionId::BorrowedLift}, 4};
    case RhythmFamily::MachineSyncopation:
      return {{ProgressionId::StaticModal, ProgressionId::MinorFall,
               ProgressionId::BorrowedLift, ProgressionId::ParallelShift}, 4};
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      return {{ProgressionId::StaticModal, ProgressionId::TwoFiveOne,
               ProgressionId::ParallelShift, ProgressionId::BorrowedLift}, 4};
    case RhythmFamily::HipHopBackbeat:
      return {{ProgressionId::TwoFiveOne, ProgressionId::ParallelShift,
               ProgressionId::PedalDrone, ProgressionId::BorrowedLift}, 4};
    case RhythmFamily::DubPulse:
      return {{ProgressionId::PedalDrone, ProgressionId::StaticModal,
               ProgressionId::BorrowedLift}, 3};
    case RhythmFamily::SparsePulse:
      return {{ProgressionId::PedalDrone, ProgressionId::ParallelShift,
               ProgressionId::TwoFiveOne}, 3};
    case RhythmFamily::Funk16:
      return {{ProgressionId::TwoFiveOne, ProgressionId::PopCycle,
               ProgressionId::BorrowedLift, ProgressionId::StaticModal}, 4};
    case RhythmFamily::Count:
      return {};
  }
  return {};
}

ProgressionId selectId(const ChordProgressionRequest& request) {
  if (request.requestedId != ProgressionId::Auto) return request.requestedId;
  const ProgressionCandidates candidates = candidatesFor(request.family);
  if (candidates.count == 0) return ProgressionId::Auto;
  // ProgressionId is append-only: this numeric value is the stable ChordPitch
  // salt, so renumbering an existing id would change the deterministic corpus.
  const uint32_t seed = deriveGenerationSeed(
      request.generation, kNoArchetypeId, GenerationDomain::ChordPitch,
      static_cast<uint8_t>(ProgressionId::Auto));
  const uint32_t coordinate =
      static_cast<uint32_t>(static_cast<uint8_t>(request.family)) |
      (static_cast<uint32_t>(request.phraseBars) << 8u);
  return candidates.values[deterministicValue(seed, coordinate) %
                           candidates.count];
}

const Grammar* selectGrammar(const ChordProgressionRequest& request,
                             ProgressionId id) {
  const GrammarSet* set = grammarSetFor(id);
  if (set == nullptr || set->count == 0) return nullptr;
  // The concrete progression id is the salt inside the existing ChordPitch
  // domain. Keep existing enum values stable; append new values before Count.
  const uint32_t seed = deriveGenerationSeed(
      request.generation, kNoArchetypeId, GenerationDomain::ChordPitch,
      static_cast<uint8_t>(id));
  const uint32_t coordinate = static_cast<uint32_t>(request.phraseBars);
  return &set->variants[deterministicValue(seed, coordinate) % set->count];
}

bool validEvent(const HarmonicEvent& value, ProgressionId id) {
  if (value.degree > 6 || !validQuality(value.quality) ||
      value.rootOffsetSemitones < -kMaxRootOffsetSemitones ||
      value.rootOffsetSemitones > kMaxRootOffsetSemitones) {
    return false;
  }
  return value.rootOffsetSemitones == 0 || allowsChromaticRootOffset(id);
}

}  // namespace

bool isValidProgressionId(ProgressionId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(ProgressionId::Count)) return false;
  return allowAuto || id != ProgressionId::Auto;
}

ChordProgressionResult realizeChordProgression(
    const ChordProgressionRequest& request) {
  ChordProgressionResult result{};
  if (!isValidProgressionId(request.requestedId) ||
      static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      request.harmonicEventCount > kMaxHarmonicEvents ||
      !validPhraseBars(request.phraseBars)) {
    return result;
  }

  const ProgressionId id = selectId(request);
  if (!isValidProgressionId(id, false)) return result;
  result.plan.id = id;

  if (request.harmonicEventCount == 0) {
    result.status = ChordProgressionStatus::Ok;
    return result;
  }

  const Grammar* selected = selectGrammar(request, id);
  if (selected == nullptr || selected->count == 0) return result;

  const bool isStatic =
      id == ProgressionId::StaticModal || id == ProgressionId::PedalDrone;
  result.plan.eventCount =
      isStatic ? 1 : request.harmonicEventCount;
  for (uint8_t index = 0; index < result.plan.eventCount; ++index) {
    const HarmonicEvent value = selected->events[index % selected->count];
    if (!validEvent(value, id)) return ChordProgressionResult{};
    result.plan.events[index] = value;
  }

  result.status = isStatic ? ChordProgressionStatus::ValidButStatic
                           : ChordProgressionStatus::Ok;
  return result;
}

const char* chordProgressionName(ProgressionId id) {
  switch (id) {
    case ProgressionId::Auto: return "AUTO";
    case ProgressionId::StaticModal: return "STATIC MODAL";
    case ProgressionId::PedalDrone: return "PEDAL DRONE";
    case ProgressionId::PopCycle: return "POP CYCLE";
    case ProgressionId::TwoFiveOne: return "II-V-I";
    case ProgressionId::ParallelShift: return "PARALLEL SHIFT";
    case ProgressionId::MinorFall: return "MINOR FALL";
    case ProgressionId::BorrowedLift: return "BORROWED LIFT";
    case ProgressionId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
