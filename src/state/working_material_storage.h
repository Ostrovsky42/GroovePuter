#pragma once

#ifndef GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
#define GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H

#include <new>
#include <type_traits>

#include "../../scenes.h"
#include "../phrase/runtime_synth_events.h"

namespace GroovePuterMaterial {

// One physical session-owned payload for the voice's mutable material.
//
// The active representation is deliberately NOT stored here.  Existing
// ActiveMaterial.kind / sequenced-source authority owns that fact.  This keeps
// the M-WORKING layer from growing a second representation owner or a dirty bit.
// Both supported payload types are trivially destructible fixed values, so
// placement-new may reuse the same union storage when representation changes.
class WorkingMaterialStorage {
 public:
  using MelodyBuffer = PhraseRuntime::RuntimeSynthEventBuffer;

  WorkingMaterialStorage() { storeMelody(MelodyBuffer{}); }

  void storePattern(const SynthPattern& value) {
    static_assert(std::is_trivially_destructible<SynthPattern>::value,
                  "Working Pattern must remain trivially destructible");
    new (&payload_.pattern) SynthPattern(value);
  }

  void storeMelody(const MelodyBuffer& value) {
    static_assert(std::is_trivially_destructible<MelodyBuffer>::value,
                  "Working Melody must remain trivially destructible");
    new (&payload_.melody) MelodyBuffer(value);
  }

  SynthPattern& pattern() { return payload_.pattern; }
  const SynthPattern& pattern() const { return payload_.pattern; }

  MelodyBuffer& melody() { return payload_.melody; }
  const MelodyBuffer& melody() const { return payload_.melody; }

 private:
  union Payload {
    SynthPattern pattern;
    MelodyBuffer melody;

    Payload() {}
    ~Payload() {}
  } payload_;
};

static_assert(sizeof(SynthPattern) <= sizeof(WorkingMaterialStorage::MelodyBuffer),
              "Working Pattern no longer fits in the already-paid Melody footprint");
static_assert(sizeof(WorkingMaterialStorage) <=
                  sizeof(WorkingMaterialStorage::MelodyBuffer),
              "M-WORKING storage may not exceed one Melody buffer");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
