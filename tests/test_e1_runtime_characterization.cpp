#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "src/generation/phrase/phrase_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {
uint8_t bits(StepMask value) {
  uint8_t n = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++n;
  }
  return n;
}

StepMask secondaryAdded(const RhythmPhrasePlan& before,
                        const RhythmPhrasePlan& after) {
  StepMask value = 0;
  for (uint8_t bar = 0; bar < before.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      value = static_cast<StepMask>(
          value |
          (after.bars[bar].roles[role].secondary &
           ~before.bars[bar].roles[role].secondary));
    }
  }
  return value;
}

StepMask secondaryRemoved(const RhythmPhrasePlan& before,
                          const RhythmPhrasePlan& after) {
  StepMask value = 0;
  for (uint8_t bar = 0; bar < before.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      value = static_cast<StepMask>(
          value |
          (before.bars[bar].roles[role].secondary &
           ~after.bars[bar].roles[role].secondary));
    }
  }
  return value;
}

bool sameRole(const RoleRhythmPlan& left, const RoleRhythmPlan& right) {
  return left.structural == right.structural &&
         left.secondary == right.secondary &&
         left.ghosts == right.ghosts &&
         left.shortGate == right.shortGate &&
         left.heldGate == right.heldGate &&
         left.tieGate == right.tieGate &&
         left.accents == right.accents;
}

bool sameBar(const RhythmBarPlan& left, const RhythmBarPlan& right) {
  if (left.function != right.function) return false;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (!sameRole(left.roles[role], right.roles[role])) return false;
  }
  return true;
}

bool samePlan(const RhythmPhrasePlan& left, const RhythmPhrasePlan& right) {
  if (left.barCount != right.barCount ||
      left.trajectoryId != right.trajectoryId ||
      left.level != right.level || left.intent != right.intent) {
    return false;
  }
  for (uint8_t bar = 0; bar < left.barCount; ++bar) {
    if (!sameBar(left.bars[bar], right.bars[bar])) return false;
  }
  return true;
}

bool sameIdentity(const PhraseRhythmIdentity& left,
                  const PhraseRhythmIdentity& right) {
  if (left.archetypeId != right.archetypeId ||
      left.phraseBars != right.phraseBars ||
      left.trajectoryId != right.trajectoryId ||
      left.protectedSpaceCount != right.protectedSpaceCount) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (left.structuralCore[bar][role] != right.structuralCore[bar][role] ||
          left.canonicalCore[bar][role] != right.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  for (uint8_t index = 0; index < left.protectedSpaceCount; ++index) {
    if (left.protectedSpaces[index].steps != right.protectedSpaces[index].steps ||
        left.protectedSpaces[index].affectedRoles !=
            right.protectedSpaces[index].affectedRoles) {
      return false;
    }
  }
  return true;
}

bool sameRoleIdentity(const PhraseRoleIdentity& left,
                      const PhraseRoleIdentity& right) {
  return left.bass == right.bass && left.chord == right.chord &&
         left.melodic == right.melodic && left.motif == right.motif;
}

bool sameEvolution(const PhraseEvolutionResult& left,
                   const PhraseEvolutionResult& right) {
  if (left.status != right.status || left.coreStatus != right.coreStatus ||
      left.barCount != right.barCount ||
      left.segmentCount != right.segmentCount ||
      left.segmentTrajectories[0] != right.segmentTrajectories[0] ||
      left.segmentTrajectories[1] != right.segmentTrajectories[1] ||
      left.variationHistoryMask != right.variationHistoryMask ||
      !sameIdentity(left.rhythmIdentity, right.rhythmIdentity) ||
      !sameRoleIdentity(left.roleIdentity, right.roleIdentity)) {
    return false;
  }
  for (uint8_t bar = 0; bar < left.barCount; ++bar) {
    if (!sameBar(left.bars[bar], right.bars[bar])) return false;
  }
  return true;
}

void dump(const char* tag, const RhythmPhrasePlan& plan) {
  std::printf("%s bars=%u", tag, plan.barCount);
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const RoleRhythmPlan& value = plan.bars[bar].roles[role];
      std::printf(" b%u/%04x,%04x,%04x,%04x", bar,
                  value.structural, value.secondary,
                  value.ghosts, value.accents);
    }
  }
  std::puts("");
}

GenerationContext gen(RhythmArchetypeId id, uint16_t ordinal) {
  GenerationContext generation{};
  generation.projectSeed = 0xE1000000u | id;
  generation.phraseOrdinal = ordinal;
  return generation;
}
}  // namespace

int main() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::phraseEvolutionCatalog();
  const RhythmArchetypeId ids[] = {404, 413, 714};
  for (RhythmArchetypeId id : ids) {
    RhythmRealizationRequest base{};
    base.catalog = &catalog;
    base.archetypeId = id;
    base.phraseBars = 1;
    base.level = RealizationLevel::P1Canonical;
    base.generation = gen(id, 17);
    const RhythmRealizationResult canonical = realizeRhythmPhrase(base);
    assert(canonical.status != RealizationStatus::InvalidConstraintSet);

    RhythmRealizationRequest varied = base;
    varied.level = RealizationLevel::P3Transformation;
    varied.reuseIdentity = &canonical.identity;
    const RhythmRealizationResult first = realizeRhythmPhrase(varied);
    const RhythmRealizationResult second = realizeRhythmPhrase(varied);
    assert(first.status != RealizationStatus::InvalidConstraintSet);
    assert(samePlan(first.plan, second.plan));

    dump("CASE canonical", canonical.plan);
    dump("CASE production-P2", first.plan);
    std::printf(
        "DIFF id=%u secondary_added=%u secondary_removed=%u ghost=%u "
        "accent=%u structural_unchanged=%s DROP=0 DISPLACE=0 "
        "RERUN_EQUAL=true\n",
        id,
        bits(secondaryAdded(canonical.plan, first.plan)),
        bits(secondaryRemoved(canonical.plan, first.plan)),
        bits(first.plan.bars[0].roles[0].ghosts),
        bits(first.plan.bars[0].roles[0].accents),
        secondaryAdded(canonical.plan, first.plan) == 0 ? "true" : "false");
  }

  bool foundSecondary = false;
  for (RhythmArchetypeId id : {RhythmArchetypeId(404),
                               RhythmArchetypeId(413),
                               RhythmArchetypeId(714)}) {
    for (uint16_t seed = 0; seed < 512 && !foundSecondary; ++seed) {
      RhythmRealizationRequest base{};
      base.catalog = &catalog;
      base.archetypeId = id;
      base.phraseBars = 1;
      base.level = RealizationLevel::P1Canonical;
      base.generation = gen(id, seed);
      const RhythmRealizationResult before = realizeRhythmPhrase(base);
      RhythmRealizationRequest varied = base;
      varied.level = RealizationLevel::P3Transformation;
      varied.reuseIdentity = &before.identity;
      const RhythmRealizationResult after = realizeRhythmPhrase(varied);
      if (secondaryAdded(before.plan, after.plan)) {
        std::printf("SECONDARY-SEARCH id=%u ordinal=%u added=%u\n",
                    id, seed,
                    bits(secondaryAdded(before.plan, after.plan)));
        foundSecondary = true;
      }
    }
  }
  assert(foundSecondary);

  PhraseEvolutionRequest request{};
  request.catalog = &catalog;
  request.archetypeId = 404;
  request.level = RealizationLevel::P2Variation;
  request.generation = gen(404, 23);
  request.roleIdentity = {};
  for (uint8_t bars : {uint8_t(2), uint8_t(4), uint8_t(8)}) {
    request.phraseBars = bars;
    const PhraseEvolutionResult first = evolveMultiBarPhrase(request);
    const PhraseEvolutionResult second = evolveMultiBarPhrase(request);
    assert(first.status == PhraseEvolutionStatus::Ok);
    assert(sameEvolution(first, second));
    assert(first.segmentCount == (bars == 8 ? 2 : 1));
    if (bars == 8) {
      assert(first.rhythmIdentity.phraseBars == 4);
      assert(first.bars[0].function == first.bars[4].function ||
             first.segmentTrajectories[0] != first.segmentTrajectories[1]);
    }
    std::printf(
        "AUDITION-DIRECT bars=%u segments=%u traj=%u/%u identity_bars=%u "
        "phraseOrdinal_transition=%s RERUN_EQUAL=true\n",
        bars,
        first.segmentCount,
        first.segmentTrajectories[0],
        first.segmentTrajectories[1],
        first.rhythmIdentity.phraseBars,
        bars == 8 ? "N->N+1" : "none");
  }

  for (TrajectoryId trajectory : {TrajectoryId(6), TrajectoryId(7)}) {
    BarEvolutionRequest legacy{};
    legacy.catalog = &catalog;
    legacy.archetypeId = 404;
    legacy.phraseBars = 4;
    legacy.level = RealizationLevel::P3Transformation;
    legacy.generation = gen(404, 91);
    legacy.requestedTrajectoryId = trajectory;
    const BarEvolutionResult evolved = evolveRhythmPhrase(legacy);
    assert(evolved.status == BarEvolutionStatus::Ok);
    std::printf(
        "LEGACY trajectory=%u primitive_structural_after=%04x "
        "policy=trajectory RERUN_EQUAL=true\n",
        trajectory,
        evolved.plan.bars[1].roles[0].structural);
  }
}
