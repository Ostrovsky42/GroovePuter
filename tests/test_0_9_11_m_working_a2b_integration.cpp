// 0.9.11 unified M-WORKING + A2/A2-B integration RED.
//
// The integration base must not claim address reuse safety until independent
// MaterialId exists in the same tree. This test deliberately compiles before
// the owner exists so RED is semantic, not a missing-header compiler failure.

#include <cstdint>
#include <cstdio>

#if __has_include("src/state/material_identity.h")
#include "src/state/material_identity.h"
#define GROOVEPUTER_HAS_INDEPENDENT_MATERIAL_ID 1
#else
#define GROOVEPUTER_HAS_INDEPENDENT_MATERIAL_ID 0
#endif

int main() {
#if !GROOVEPUTER_HAS_INDEPENDENT_MATERIAL_ID
  std::fprintf(
      stderr,
      "MATERIAL_ID_INDEPENDENCE_RED: unified M-WORKING integration still has address-only material authority; stale M@A cannot be distinguished from replacement N@A\n");
  return 1;
#else
  using namespace GroovePuterMaterial;

  constexpr MaterialAddress address{0, 7};
  constexpr MaterialId idM{0x1001u};
  constexpr MaterialId idN{0x1002u};
  constexpr MaterialReference oldRef{address, idM};
  constexpr MaterialReference newRef{address, idN};

  if (!idM.valid() || !idN.valid() || idM == idN) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: independent identities are invalid\n");
    return 2;
  }
  if (!materialReferenceMatches(oldRef, address, idM)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: live M@A reference does not match itself\n");
    return 2;
  }
  if (materialReferenceMatches(oldRef, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: stale M@A reference aliases replacement N@A\n");
    return 2;
  }
  if (!materialReferenceMatches(newRef, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: live N@A reference does not match replacement\n");
    return 2;
  }

  constexpr MaterialReference invalid{address, MaterialId{}};
  if (invalid.id.valid() ||
      materialReferenceMatches(invalid, address, MaterialId{}) ||
      materialReferenceMatches(invalid, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: id=0 did not fail closed\n");
    return 2;
  }

  constexpr MaterialAddress otherAddress{0, 8};
  if (materialReferenceMatches(oldRef, otherAddress, idM)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_FAIL: reference ignored address component\n");
    return 2;
  }

  std::puts("MATERIAL_ID_INDEPENDENCE_GREEN: stale M@A cannot resolve replacement N@A");
  return 0;
#endif
}
