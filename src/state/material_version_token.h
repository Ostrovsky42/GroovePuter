#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_TOKEN_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_TOKEN_H

#include <cstdint>

namespace GroovePuterMaterial {

// Exact accepted-state fingerprint, kept separate from the pattern and phrase
// hashing helpers so lightweight metadata owners do not include scenes.h.
struct MaterialVersionToken {
  uint32_t low = 0;
  uint32_t high = 0;

  constexpr bool valid() const { return low != 0 || high != 0; }

  friend constexpr bool operator==(MaterialVersionToken lhs,
                                   MaterialVersionToken rhs) {
    return lhs.low == rhs.low && lhs.high == rhs.high;
  }
  friend constexpr bool operator!=(MaterialVersionToken lhs,
                                   MaterialVersionToken rhs) {
    return !(lhs == rhs);
  }
};

static_assert(sizeof(MaterialVersionToken) == 8,
              "MaterialVersionToken must remain exactly eight bytes");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_TOKEN_H
