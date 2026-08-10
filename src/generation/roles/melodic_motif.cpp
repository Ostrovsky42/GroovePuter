#include "melodic_motif.h"

#include <initializer_list>

namespace GroovePuterRhythm {
namespace {

constexpr StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps) result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

StepMask anchoredContinuations(StepMask onsets, StepMask continuations) {
  StepMask result = 0;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) != 0) {
      active = true;
    } else if ((continuations & bit) != 0 && active) {
      result = static_cast<StepMask>(result | bit);
    } else {
      active = false;
    }
  }
  return result;
}

struct RhythmCandidates {
  MelodicRhythmId values[5]{};
  uint8_t count = 0;
};

RhythmCandidates rhythmCandidatesFor(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor:
      return {{MelodicRhythmId::TwoNoteHook, MelodicRhythmId::PickupPhrase,
               MelodicRhythmId::SyncopatedMotif,
               MelodicRhythmId::RepeatedCell}, 4};
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Funk16:
      return {{MelodicRhythmId::DelayedAnswer,
               MelodicRhythmId::SyncopatedMotif,
               MelodicRhythmId::BarEndResponse,
               MelodicRhythmId::RepeatedCell}, 4};
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      return {{MelodicRhythmId::SparseCall,
               MelodicRhythmId::DelayedAnswer,
               MelodicRhythmId::PickupPhrase,
               MelodicRhythmId::BarEndResponse}, 4};
    case RhythmFamily::HipHopBackbeat:
      return {{MelodicRhythmId::SparseCall, MelodicRhythmId::TwoNoteHook,
               MelodicRhythmId::RestHeavy,
               MelodicRhythmId::BarEndResponse}, 4};
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      return {{MelodicRhythmId::SparseCall, MelodicRhythmId::LongTone,
               MelodicRhythmId::RestHeavy,
               MelodicRhythmId::DriftPhrase}, 4};
    case RhythmFamily::Count:
      return {};
  }
  return {};
}

MelodicRhythmId selectRhythm(const MelodicMotifRequest& request) {
  if (request.requestedRhythm != MelodicRhythmId::Auto) {
    return request.requestedRhythm;
  }
  const RhythmCandidates candidates = rhythmCandidatesFor(request.family);
  if (candidates.count == 0) return MelodicRhythmId::Auto;
  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MelodicRhythmSelection,
      static_cast<uint32_t>(request.family));
  return candidates.values[deterministicValue(seed, request.barOrdinal) %
                           candidates.count];
}

MotifShapeId selectShape(const MelodicMotifRequest& request,
                         MelodicRhythmId rhythm) {
  if (request.requestedShape != MotifShapeId::Auto) {
    return request.requestedShape;
  }
  constexpr MotifShapeId shapes[] = {
      MotifShapeId::SourceOrder,
      MotifShapeId::TwoNoteCell,
      MotifShapeId::Mirror,
      MotifShapeId::CallResponse,
      MotifShapeId::Pivot,
  };
  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MotifSelection,
      static_cast<uint32_t>(rhythm));
  return shapes[deterministicValue(seed, request.barOrdinal) % 5u];
}

MotifIdentity motifFor(MotifShapeId shape) {
  MotifIdentity motif{};
  motif.shape = shape;
  switch (shape) {
    case MotifShapeId::SourceOrder:
      motif.sourceOrder[0] = 0; motif.sourceOrder[1] = 1;
      motif.sourceOrder[2] = 2; motif.sourceOrder[3] = 3;
      motif.sourceOrderCount = 4;
      break;
    case MotifShapeId::TwoNoteCell:
      motif.sourceOrder[0] = 0; motif.sourceOrder[1] = 1;
      motif.sourceOrder[2] = 0; motif.sourceOrder[3] = 1;
      motif.sourceOrderCount = 4;
      break;
    case MotifShapeId::Mirror:
      motif.sourceOrder[0] = 0; motif.sourceOrder[1] = 1;
      motif.sourceOrder[2] = 2; motif.sourceOrder[3] = 1;
      motif.sourceOrderCount = 4;
      break;
    case MotifShapeId::CallResponse:
      motif.sourceOrder[0] = 0; motif.sourceOrder[1] = 1;
      motif.sourceOrder[2] = 0; motif.sourceOrder[3] = 2;
      motif.sourceOrderCount = 4;
      break;
    case MotifShapeId::Pivot:
      motif.sourceOrder[0] = 1; motif.sourceOrder[1] = 0;
      motif.sourceOrder[2] = 1; motif.sourceOrder[3] = 2;
      motif.sourceOrderCount = 4;
      break;
    case MotifShapeId::Auto:
    case MotifShapeId::Count:
      break;
  }
  return motif;
}

}  // namespace

bool isValidMelodicRhythmId(MelodicRhythmId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(MelodicRhythmId::Count)) return false;
  return allowAuto || id != MelodicRhythmId::Auto;
}

bool isValidMotifShapeId(MotifShapeId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(MotifShapeId::Count)) return false;
  return allowAuto || id != MotifShapeId::Auto;
}

MelodicMotifResult realizeMelodicMotif(const MelodicMotifRequest& request) {
  MelodicMotifResult result{};
  if (request.archetypeId == kNoArchetypeId ||
      static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      !isValidMelodicRhythmId(request.requestedRhythm) ||
      !isValidMotifShapeId(request.requestedShape)) {
    return result;
  }
  const MelodicRhythmId rhythm = selectRhythm(request);
  const MotifShapeId shape = selectShape(request, rhythm);
  if (!isValidMelodicRhythmId(rhythm, false) ||
      !isValidMotifShapeId(shape, false)) {
    return result;
  }

  StepMask onsets = 0;
  StepMask continuations = 0;
  switch (rhythm) {
    case MelodicRhythmId::SparseCall:
      onsets = (request.allowEmptyBar && (request.barOrdinal & 1u) != 0u)
                   ? 0 : stepBit(2);
      break;
    case MelodicRhythmId::DelayedAnswer: onsets = mask({6, 14}); break;
    case MelodicRhythmId::TwoNoteHook: onsets = mask({2, 10}); break;
    case MelodicRhythmId::PickupPhrase: onsets = mask({12, 14, 15}); break;
    case MelodicRhythmId::LongTone:
      onsets = stepBit(3);
      continuations = mask({4, 5, 6, 7, 8, 9, 10, 11});
      break;
    case MelodicRhythmId::RestHeavy:
      onsets = (request.allowEmptyBar && (request.barOrdinal % 4u) != 0u)
                   ? 0 : stepBit(10);
      break;
    case MelodicRhythmId::BarEndResponse: onsets = mask({13, 15}); break;
    case MelodicRhythmId::SyncopatedMotif: onsets = mask({1, 5, 10, 14}); break;
    case MelodicRhythmId::DriftPhrase:
      onsets = mask({3, 11});
      continuations = mask({4, 5, 12, 13});
      break;
    case MelodicRhythmId::RepeatedCell: onsets = mask({0, 4, 8, 12}); break;
    case MelodicRhythmId::Auto:
    case MelodicRhythmId::Count:
      return result;
  }

  const StepMask blocked = static_cast<StepMask>(
      request.protectedSpace | request.bassOnsets | request.chordOnsets);
  onsets = static_cast<StepMask>(onsets & ~blocked);
  continuations = static_cast<StepMask>(
      continuations & ~request.protectedSpace & ~onsets);
  continuations = anchoredContinuations(onsets, continuations);

  result.plan.rhythmId = rhythm;
  result.plan.onsets = onsets;
  result.plan.continuations = continuations;
  result.plan.motif = motifFor(shape);
  result.status = onsets == 0 ? MelodicMotifStatus::ValidButEmpty
                              : MelodicMotifStatus::Ok;
  return result;
}

const char* melodicRhythmName(MelodicRhythmId id) {
  switch (id) {
    case MelodicRhythmId::Auto: return "AUTO";
    case MelodicRhythmId::SparseCall: return "SPARSE CALL";
    case MelodicRhythmId::DelayedAnswer: return "DELAYED ANSWER";
    case MelodicRhythmId::TwoNoteHook: return "TWO-NOTE HOOK";
    case MelodicRhythmId::PickupPhrase: return "PICKUP PHRASE";
    case MelodicRhythmId::LongTone: return "LONG TONE";
    case MelodicRhythmId::RestHeavy: return "REST HEAVY";
    case MelodicRhythmId::BarEndResponse: return "BAR-END RESPONSE";
    case MelodicRhythmId::SyncopatedMotif: return "SYNCOPATED MOTIF";
    case MelodicRhythmId::DriftPhrase: return "DRIFT PHRASE";
    case MelodicRhythmId::RepeatedCell: return "REPEATED CELL";
    case MelodicRhythmId::Count: break;
  }
  return "INVALID";
}

const char* motifShapeName(MotifShapeId id) {
  switch (id) {
    case MotifShapeId::Auto: return "AUTO";
    case MotifShapeId::SourceOrder: return "SOURCE ORDER";
    case MotifShapeId::TwoNoteCell: return "TWO-NOTE CELL";
    case MotifShapeId::Mirror: return "MIRROR";
    case MotifShapeId::CallResponse: return "CALL/RESPONSE";
    case MotifShapeId::Pivot: return "PIVOT";
    case MotifShapeId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
