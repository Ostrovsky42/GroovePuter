#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_H

#include <cstdint>

// M1: what kind of musical material a slot holds.
//
// GroovePuter already has three levels -- MATERIAL, PHRASE (a 1/2/4/8-bar
// section referencing materials) and SONG. What was missing is that a material
// slot can be expressed two ways: as a Pattern on a step grid, or as a Melody
// of events with real durations. This names that, and nothing else: no
// storage, no playback, no key handling.
//
// One canonical representation per slot, and promotion is one-way. Holding a
// Pattern and a Melody for the same slot would immediately ask which one Song
// plays, which one the generator rewrites, and what Undo means -- two owners
// again, which is the failure this whole line of work exists to remove.
//
// Consumers must go through the accessors rather than touch the storage. The
// storage is deliberately the smallest thing that works today; step 3 replaces
// it with the resolved runtime authority without rewriting every caller.
namespace GroovePuterMaterial {

// 0.9.11 A0/A1: project-relative musical address shared across workstreams.
//
// A Scene only owns the currently resident 16-slot page, so a page-local slot
// cannot identify material across persistence, Song or edit targeting. The
// global slot already names the arrangement coordinate across all pages; pair
// it with voice. Project namespace belongs only to persistence and is therefore
// deliberately absent from this compact value.
struct MaterialAddress {
  uint8_t voice = 0;
  uint8_t globalSlot = 0;
};

static_assert(sizeof(MaterialAddress) == 2,
              "MaterialAddress must remain a two-byte embedded value");

enum class MaterialKind : uint8_t {
  Pattern = 0,
  Melody = 1,
};

struct MaterialSlotDescriptor {
  MaterialKind kind = MaterialKind::Pattern;
};

inline bool validKindValue(int value) {
  return value == static_cast<int>(MaterialKind::Pattern) ||
         value == static_cast<int>(MaterialKind::Melody);
}

// An unknown or absent value decodes as Pattern. Scenes written before this
// existed carry no kinds at all, and a slot whose kind cannot be trusted must
// read as the representation that has always been there.
inline MaterialKind kindFromPersistedValue(int value) {
  return validKindValue(value) ? static_cast<MaterialKind>(value)
                               : MaterialKind::Pattern;
}

inline const char* kindName(MaterialKind kind) {
  return kind == MaterialKind::Melody ? "MELODY" : "PATTERN";
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_H
