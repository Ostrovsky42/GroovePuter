#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_H

#include <cstdint>
#include "material_identity.h"

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
// Identity is co-located with kind so a page cannot move one without the other.
// MaterialId is not the slot address: replacing a material at the same address
// assigns a new id while ordinary edits preserve the existing id.
namespace GroovePuterMaterial {

enum class MaterialKind : uint8_t {
  Pattern = 0,
  Melody = 1,
};

struct MaterialSlotDescriptor {
  MaterialKind kind = MaterialKind::Pattern;
  MaterialId id{};
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
