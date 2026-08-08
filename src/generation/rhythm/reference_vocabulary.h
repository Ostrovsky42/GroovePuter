#pragma once

#include <cstdint>

#include "rhythm_catalog.h"

namespace GroovePuterRhythm {
namespace ReferenceVocabulary {

enum class Archetype : uint8_t {
  StraightDrive = 0,
  OffbeatOpenHat,
  HypnoticSparse,
  BrokenTechno,
  StraightAcid,
  RollingAcid,
  SyncopatedAcid,
  SparseAcid,
  OneDropSpace,
  Steppers,
  SparseSkank,
  ChordResponse,
  TwoStepRoll,
  GhostedRoll,
  SparseFastBreak,
  HalftimeSwitch,
  ClassicTwoStep,
  SkippyTwoStep,
  ShuffledFourFour,
  MachineSyncopation,
  Count,
};

struct Definition {
  Archetype key = Archetype::StraightDrive;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  const char* name = "straight_drive";
  RhythmFamily family = RhythmFamily::FourFloor;
  uint16_t suggestedBpmMin = 120;
  uint16_t suggestedBpmMax = 132;
};

const RhythmCatalogView& catalog();
uint8_t definitionCount();
const Definition& definition(uint8_t index);
const Definition* definitionFor(Archetype key);
const RhythmArchetype* archetypeFor(Archetype key);

}  // namespace ReferenceVocabulary
}  // namespace GroovePuterRhythm
