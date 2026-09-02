#include "phrase_evolution.h"

namespace GroovePuterRhythm {
namespace {

bool validPhraseBars(uint8_t bars) {
  return bars == 1 || bars == 2 || bars == 4 || bars == 8;
}

bool validRoleIdentity(const PhraseRoleIdentity& identity) {
  return isValidBassRhythmId(identity.bass) &&
         isValidChordRhythmId(identity.chord) &&
         isValidMelodicRhythmId(identity.melodic) &&
         isValidMotifShapeId(identity.motif);
}

void copySegment(const RhythmPhrasePlan& source,
                 uint8_t destinationOffset,
                 PhraseEvolutionResult& destination) {
  for (uint8_t bar = 0; bar < source.barCount; ++bar) {
    destination.bars[destinationOffset + bar] = source.bars[bar];
    if (source.bars[bar].function != BarFunction::Statement) {
      destination.variationHistoryMask = static_cast<uint8_t>(
          destination.variationHistoryMask |
          (1u << (destinationOffset + bar)));
    }
  }
}

BarEvolutionResult evolveSegment(const PhraseEvolutionRequest& request,
                                 uint8_t bars,
                                 GenerationContext generation,
                                 const PhraseRhythmIdentity* reuseIdentity) {
  BarEvolutionRequest core{};
  core.catalog = request.catalog;
  core.archetypeId = request.archetypeId;
  core.phraseBars = bars;
  core.level = request.level;
  core.generation = generation;
  core.requestedTrajectoryId = request.requestedTrajectoryId;
  core.reuseIdentity = reuseIdentity;
  return evolveRhythmPhrase(core);
}

}  // namespace

PhraseEvolutionResult evolveMultiBarPhrase(
    const PhraseEvolutionRequest& request) {
  PhraseEvolutionResult result{};
  if (request.catalog == nullptr ||
      request.archetypeId == kNoArchetypeId ||
      !validPhraseBars(request.phraseBars) ||
      static_cast<uint8_t>(request.level) >=
          static_cast<uint8_t>(RealizationLevel::Count) ||
      !validRoleIdentity(request.roleIdentity)) {
    return result;
  }

  const uint8_t segmentBars = request.phraseBars == 8 ? 4 : request.phraseBars;
  const BarEvolutionResult first = evolveSegment(
      request, segmentBars, request.generation, request.reuseIdentity);
  result.coreStatus = first.status;
  if (first.status != BarEvolutionStatus::Ok) {
    result.status = PhraseEvolutionStatus::CoreEvolutionFailed;
    return result;
  }

  PhraseEvolutionResult next{};
  next.coreStatus = BarEvolutionStatus::Ok;
  next.barCount = request.phraseBars;
  next.segmentCount = request.phraseBars == 8 ? 2 : 1;
  next.segmentTrajectories[0] = first.trajectoryId;
  next.rhythmIdentity = first.identity;
  next.roleIdentity = request.roleIdentity;
  copySegment(first.plan, 0, next);

  if (request.phraseBars == 8) {
    GenerationContext secondGeneration = request.generation;
    secondGeneration.phraseOrdinal = static_cast<uint16_t>(
        secondGeneration.phraseOrdinal + 1u);
    const BarEvolutionResult second = evolveSegment(
        request, 4, secondGeneration, &first.identity);
    if (second.status != BarEvolutionStatus::Ok) {
      result.coreStatus = second.status;
      result.status = PhraseEvolutionStatus::CoreEvolutionFailed;
      return result;
    }
    next.segmentTrajectories[1] = second.trajectoryId;
    copySegment(second.plan, 4, next);
  }

  next.status = PhraseEvolutionStatus::Ok;
  return next;
}

}  // namespace GroovePuterRhythm
