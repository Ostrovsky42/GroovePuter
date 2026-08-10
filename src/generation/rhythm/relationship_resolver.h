#ifndef GROOVEPUTER_GENERATION_RHYTHM_RELATIONSHIP_RESOLVER_H
#define GROOVEPUTER_GENERATION_RHYTHM_RELATIONSHIP_RESOLVER_H

#include <cstdint>

#include "rhythm_types.h"

namespace GroovePuterRhythm {

struct PhraseOccupancy {
  uint8_t barCount = 0;
  StepMask roleMasks[kMaxPhraseBars][kRhythmRoleCount]{};
};

bool relationshipSatisfied(const LaneRelationship& relationship,
                           const PhraseOccupancy& occupancy);

bool hardRelationshipsSatisfied(const RhythmArchetype& archetype,
                                const PhraseOccupancy& occupancy);

// Returns false only for an immediately irreversible hard conflict caused by
// adding this event to the current occupancy. Unsatisfied positive obligations
// (Coincide/Respond minima) are handled by the bounded realization passes and
// final hard validation.
bool hardCandidateAdditionAllowed(const RhythmArchetype& archetype,
                                  const PhraseOccupancy& occupancy,
                                  uint8_t barIndex,
                                  RhythmRole role,
                                  uint8_t step);

// Deterministic preference score for soft relationships. Positive values make
// the candidate more desirable; zero means no relationship preference.
int16_t softRelationshipCandidateScore(const RhythmArchetype& archetype,
                                       const PhraseOccupancy& occupancy,
                                       uint8_t barIndex,
                                       RhythmRole role,
                                       uint8_t step);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_RELATIONSHIP_RESOLVER_H
