#include "../scenes.h"
#include "../src/state/material_identity.h"
#include "../src/state/material_slot_access.h"

#include <cstdio>

namespace {
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::materialReferenceMatches;

int failures = 0;
void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "C1A_IDENTITY_FAIL: %s\n", message);
  ++failures;
}
}  // namespace

int main() {
  const MaterialAddress address{0, 3};
  const MaterialId first{41};
  const MaterialId replacement{42};

  expect(first.valid(), "non-zero MaterialId must be valid");
  expect(!MaterialId{}.valid(), "zero MaterialId must remain unassigned");

  const MaterialReference stale{address, first};
  expect(materialReferenceMatches(stale, address, first),
         "exact resident identity did not resolve");
  expect(!materialReferenceMatches(stale, address, replacement),
         "address reuse resolved a different MaterialId");
  expect(!materialReferenceMatches(MaterialReference{address, {}}, address, {}),
         "legacy id=0 reference fell back to address-only identity");

  Scene scene{};
  scene.materialSlots[0][3].id = first;
  expect(GroovePuterMaterial::residentId(scene, 0, 3) == first,
         "residentId did not expose descriptor identity");
  expect(!GroovePuterMaterial::residentId(scene, 2, 3).valid(),
         "out-of-range residentId did not fail closed");

  expect(GroovePuterMaterial::materialAddressInRange(address),
         "valid material address rejected");
  expect(GroovePuterMaterial::materialAddressIsResident(address, 0),
         "resident page address rejected");
  expect(!GroovePuterMaterial::materialAddressIsResident(address, 1),
         "off-page address reported resident");

  if (failures != 0) return 1;
  std::puts("C1A stable material identity foundation: PASS");
  return 0;
}
