#include <cstdio>

#include "scenes.h"
#include "src/state/material_slot_access.h"

int main() {
  using GroovePuterMaterial::MaterialAddress;
  using GroovePuterMaterial::MaterialKind;

  static_assert(kMaxGlobalPatterns > 17,
                "A2 characterization requires a non-resident global slot");

  Scene scene{};
  constexpr int kActivePage = 0;
  constexpr MaterialAddress kNonResident{0, 17};

  if (!GroovePuterMaterial::materialAddressInRange(kNonResident)) {
    std::fprintf(stderr, "A2 characterization setup is invalid\n");
    return 2;
  }
  if (GroovePuterMaterial::materialAddressIsResident(kNonResident,
                                                      kActivePage)) {
    std::fprintf(stderr, "A2 characterization expected slot 17 off page 0\n");
    return 2;
  }

  const MaterialKind legacy = GroovePuterMaterial::materialKind(
      scene, kNonResident.voice, kNonResident.globalSlot, kActivePage);
  if (legacy != MaterialKind::Pattern) {
    std::fprintf(stderr,
                 "A2 characterization changed: non-resident no longer collapses to Pattern\n");
    return 2;
  }

  std::printf("A2 characterization: NOT_RESIDENT currently collapses to PATTERN\n");
  return 0;
}
