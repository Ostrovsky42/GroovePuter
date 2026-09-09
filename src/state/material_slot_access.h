#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_ACCESS_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_ACCESS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "scenes.h"
#include "src/state/material_slot.h"

// M1 accessors. Consumers use these rather than the array, so step 3 can
// replace the representation with the resolved runtime authority without
// rewriting every caller.
namespace GroovePuterMaterial {

static_assert(kMaxGlobalPatterns <= 256,
              "MaterialId::globalSlot must cover the global slot space");

inline bool residentSlotInRange(int voice, int slot) {
  return voice >= 0 && voice < Scene::kMaterialVoices &&
         slot >= 0 && slot < Scene::kMaterialSlotsPerVoice;
}

inline bool materialIdInRange(MaterialId id) {
  return static_cast<int>(id.voice) < Scene::kMaterialVoices &&
         static_cast<int>(id.globalSlot) < kMaxGlobalPatterns;
}

inline bool materialIdIsResident(MaterialId id, int activePage) {
  return materialIdInRange(id) && activePage >= 0 && activePage < kMaxPages &&
         songPatternPage(static_cast<int>(id.globalSlot)) == activePage;
}

inline int residentSlotFor(MaterialId id) {
  if (!materialIdInRange(id)) return -1;
  const int globalSlot = static_cast<int>(id.globalSlot);
  return (songPatternBank(globalSlot) * Bank<SynthPattern>::kPatterns) +
         songPatternIndexInBank(globalSlot);
}

// A slot on the resident page. Out of range answers Pattern, because a caller
// asking about a slot that is not there is better served by the representation
// that has always existed than by a confident wrong answer.
inline MaterialKind residentKind(const Scene& scene, int voice, int slot) {
  if (!residentSlotInRange(voice, slot)) return MaterialKind::Pattern;
  return scene.materialSlots[voice][slot].kind;
}

inline bool setResidentKind(Scene& scene, int voice, int slot,
                            MaterialKind kind) {
  if (!residentSlotInRange(voice, slot)) return false;
  scene.materialSlots[voice][slot].kind = kind;
  return true;
}

// The global slot space spans pages, and the Scene holds one page. The caller
// supplies which page it is on rather than this header reaching into the
// paging service: a Scene cannot honestly answer for material it does not
// hold, and pretending otherwise is how a kind drifts away from its pattern.
inline bool globalSlotIsResident(int globalSlot, int activePage) {
  return songPatternPage(globalSlot) == activePage;
}

inline MaterialKind materialKind(const Scene& scene, int voice, int globalSlot,
                                 int activePage) {
  if (!globalSlotIsResident(globalSlot, activePage)) return MaterialKind::Pattern;
  const int slot = (songPatternBank(globalSlot) *
                    Bank<SynthPattern>::kPatterns) +
                   songPatternIndexInBank(globalSlot);
  return residentKind(scene, voice, slot);
}

inline bool setMaterialKind(Scene& scene, int voice, int globalSlot,
                            int activePage, MaterialKind kind) {
  if (!globalSlotIsResident(globalSlot, activePage)) return false;
  const int slot = (songPatternBank(globalSlot) *
                    Bank<SynthPattern>::kPatterns) +
                   songPatternIndexInBank(globalSlot);
  return setResidentKind(scene, voice, slot, kind);
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_SLOT_ACCESS_H
