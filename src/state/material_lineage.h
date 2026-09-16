#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_LINEAGE_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_LINEAGE_H

#include <cstdint>
#include "material_identity.h"
#include "material_slot.h"
#include "material_version.h"

namespace GroovePuterMaterial {

// M1: Idea Classification for musical development.
enum class IdeaClassification : uint8_t {
  Unknown = 0,
  Preserved,
  Variation,
  NewIdea,
};

// M0: Causal basis representing the exact runtime CURRENT from which a candidate
// was derived. Captured before private preparation begins and checked at publication.
struct PreparationBasis {
  MaterialReference reference{};
  MaterialKind kind = MaterialKind::Pattern;
  MaterialVersionToken version{};

  constexpr bool valid() const {
    return reference.id.valid() && version.valid();
  }

  friend inline bool operator==(const PreparationBasis& lhs,
                                const PreparationBasis& rhs) {
    return lhs.reference.address == rhs.reference.address &&
           lhs.reference.id == rhs.reference.id &&
           lhs.kind == rhs.kind &&
           lhs.version == rhs.version;
  }

  friend inline bool operator!=(const PreparationBasis& lhs,
                                const PreparationBasis& rhs) {
    return !(lhs == rhs);
  }
};

// M1: Lineage tracking for musical development.
struct DevelopmentLineage {
  PreparationBasis sourceAnchorBasis{};
  PreparationBasis predecessorBasis{};
};

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_LINEAGE_H
