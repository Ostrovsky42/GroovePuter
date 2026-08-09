#include <cstdint>
#include <iostream>

#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

int main() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::catalog();
  std::cout << "FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2\n";

  for (uint8_t definitionIndex = 0;
       definitionIndex < ReferenceVocabulary::definitionCount();
       ++definitionIndex) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(definitionIndex);
    const RhythmArchetype* archetype =
        ReferenceVocabulary::archetypeFor(definition.key);
    if (archetype == nullptr) return 2;

    std::cout << "A\t" << archetype->id << '\t' << definition.name << '\t'
              << static_cast<unsigned>(archetype->family) << '\t'
              << archetype->activeRoles << '\t'
              << static_cast<unsigned>(archetype->density.structuralMin) << '\t'
              << static_cast<unsigned>(archetype->density.structuralPreferred) << '\t'
              << static_cast<unsigned>(archetype->density.structuralMax) << '\t'
              << static_cast<unsigned>(archetype->density.ornamentMax) << '\n';

    for (uint8_t laneIndex = 0; laneIndex < archetype->laneCount; ++laneIndex) {
      const LaneGrammar& lane = archetype->lanes[laneIndex];
      std::cout << "L\t" << archetype->id << '\t'
                << static_cast<unsigned>(lane.role) << '\t'
                << lane.immutableAnchors << '\t'
                << lane.canonicalAnchors << '\t'
                << lane.preferred << '\t'
                << lane.optional << '\t'
                << lane.forbidden << '\t'
                << static_cast<unsigned>(lane.structuralMin) << '\t'
                << static_cast<unsigned>(lane.structuralMax) << '\t'
                << static_cast<unsigned>(lane.ornamentMax) << '\n';
    }

    for (uint8_t spaceIndex = 0;
         spaceIndex < archetype->protectedSpaceCount;
         ++spaceIndex) {
      const ProtectedSpace& space = archetype->protectedSpaces[spaceIndex];
      std::cout << "S\t" << archetype->id << '\t' << space.steps << '\t'
                << space.affectedRoles << '\n';
    }

    for (uint8_t relationshipIndex = 0;
         relationshipIndex < archetype->relationshipCount;
         ++relationshipIndex) {
      const LaneRelationship& relationship =
          archetype->relationships[relationshipIndex];
      std::cout << "R\t" << archetype->id << '\t'
                << static_cast<unsigned>(relationship.source) << '\t'
                << static_cast<unsigned>(relationship.target) << '\t'
                << static_cast<unsigned>(relationship.op) << '\t'
                << static_cast<unsigned>(relationship.strength) << '\t'
                << static_cast<unsigned>(relationship.scope) << '\t'
                << relationship.zoneMask << '\t'
                << static_cast<int>(relationship.minOffset) << '\t'
                << static_cast<int>(relationship.maxOffset) << '\t'
                << static_cast<unsigned>(relationship.minMatches) << '\t'
                << static_cast<unsigned>(relationship.maxMatches) << '\t'
                << static_cast<unsigned>(relationship.minResponsesPerWindow) << '\t'
                << static_cast<unsigned>(relationship.maxResponsesPerWindow) << '\t'
                << static_cast<unsigned>(relationship.weight) << '\n';
    }
  }

  std::cout << "COUNT\t" << catalog.archetypeCount << '\n';
  return catalog.archetypeCount == ReferenceVocabulary::definitionCount() ? 0 : 3;
}
