#pragma once

#include <cstdint>

#include "../rhythm/rhythm_catalog.h"

namespace GroovePuterRhythm {
namespace Audition {

enum class Archetype : uint8_t {
  StraightDrive = 0,
  RollingAcid,
  ClassicTwoStep,
  TwoStepRoll,
  SparseSkank,
  Count,
};

struct Definition {
  Archetype key = Archetype::StraightDrive;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  const char* name = "straight_drive";
  uint16_t suggestedBpm = 120;
};

const RhythmCatalogView& catalog();
uint8_t definitionCount();
const Definition& definition(uint8_t index);
const Definition* definitionFor(Archetype key);

}  // namespace Audition
}  // namespace GroovePuterRhythm
