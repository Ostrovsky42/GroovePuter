// 0.9.11 MATERIAL — independent identity adversarial contract.
//
// This test deliberately compiles on the pre-MaterialId W0 root so the RED is
// a semantic runtime verdict rather than an include/compiler failure. Once the
// production owner exists, the same binary exercises the address-reuse case.

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
      "MATERIAL_ID_INDEPENDENCE_RED: independent MaterialId owner is missing; "
      "the current model can identify a slot but cannot distinguish M@A from "
      "N@A after address reuse\n");
  return 1;
#else
  using namespace GroovePuterMaterial;

  // Same physical/logical location, two different material incarnations.
  const MaterialAddress address{0, 7};
  const MaterialId idM{0x1001u};
  const MaterialId idN{0x1002u};

  if (!idM.valid() || !idN.valid() || idM == idN) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: MaterialId cannot represent "
                 "two distinct non-zero identities at one address\n");
    return 1;
  }

  const MaterialReference oldRef{address, idM};
  const MaterialReference newRef{address, idN};

  if (!materialReferenceMatches(oldRef, address, idM)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: a live reference does not "
                 "match its own address and identity\n");
    return 1;
  }

  // Critical witness: A was reused. Address still matches but identity does
  // not. The old reference must not silently bind to the new material.
  if (materialReferenceMatches(oldRef, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: stale reference alias — old "
                 "M@A reference resolved new N@A after address reuse\n");
    return 1;
  }
  if (!materialReferenceMatches(newRef, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: new N@A reference cannot "
                 "resolve the new identity\n");
    return 1;
  }

  // ID 0 is legacy/unassigned and must never fall back to address-only truth.
  const MaterialReference unassigned{address, MaterialId{0u}};
  if (unassigned.id.valid() ||
      materialReferenceMatches(unassigned, address, MaterialId{0u}) ||
      materialReferenceMatches(unassigned, address, idN)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: invalid/unassigned identity "
                 "does not fail closed\n");
    return 1;
  }

  // Identity is independent of location: moving the actual material without
  // explicitly updating the reference does not make the old address valid.
  const MaterialAddress otherAddress{0, 8};
  if (materialReferenceMatches(oldRef, otherAddress, idM)) {
    std::fprintf(stderr,
                 "MATERIAL_ID_INDEPENDENCE_RED: reference ignored its address "
                 "component\n");
    return 1;
  }

  std::printf("MaterialId independence: PASS\n");
  return 0;
#endif
}
