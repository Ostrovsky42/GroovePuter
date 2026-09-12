#pragma once

#ifndef GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
#define GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "../../scenes.h"
#include "../phrase/runtime_synth_events.h"
#include "material_identity.h"

namespace GroovePuterMaterial {

// One physical session-owned payload for the voice's mutable material.
//
// Pattern and Melody reuse the same already-paid 1284-byte footprint. The
// physical C++ lifetime state is intentionally not ActiveMaterial.kind:
// playback source may toggle Pattern <-> Melody without replacing retained
// Working material.
//
// Pattern is much smaller than Melody, so its active object leaves tail bytes
// unused. The tail stores the stable MaterialReference plus an impossible
// Melody footer. A second impossible footer represents EMPTY. Valid Melody
// count is bounded to 128, so neither 0xffffffff nor 0xfefefefe can ever be a
// valid (count,lengthTicks) footer. No sidecar allocation or second musical
// owner is needed.
class WorkingMaterialStorage {
 public:
  using MelodyBuffer = PhraseRuntime::RuntimeSynthEventBuffer;

  WorkingMaterialStorage() {
    new (&payload_.melody) MelodyBuffer{};
    markEmpty();
  }

  // Structural/storage tests may still construct an unbound Pattern. It is a
  // physical Pattern value but can never match a live material identity.
  void storePattern(const SynthPattern& value) {
    storePattern(value, MaterialReference{});
  }

  void storePattern(const SynthPattern& value,
                    const MaterialReference& reference) {
    static_assert(std::is_trivially_destructible<SynthPattern>::value,
                  "Working Pattern must remain trivially destructible");
    new (&payload_.pattern) SynthPattern(value);
    uint8_t* bytes = rawBytes();
    bytes[kVoiceOffset] = reference.address.voice;
    bytes[kGlobalSlotOffset] = reference.address.globalSlot;
    writeU32(bytes + kMaterialIdOffset, reference.id.value);
    fillTag(kPatternTagByte);
  }

  void storeMelody(const MelodyBuffer& value) {
    static_assert(std::is_trivially_destructible<MelodyBuffer>::value,
                  "Working Melody must remain trivially destructible");
    new (&payload_.melody) MelodyBuffer(value);
  }

  bool empty() const { return tagIs(kEmptyTagByte); }
  bool holdsPattern() const { return tagIs(kPatternTagByte); }
  bool holdsMelody() const { return !empty() && !holdsPattern(); }

  MaterialReference patternReference() const {
    if (!holdsPattern()) return {};
    const uint8_t* bytes = rawBytes();
    MaterialReference reference{};
    reference.address.voice = bytes[kVoiceOffset];
    reference.address.globalSlot = bytes[kGlobalSlotOffset];
    reference.id.value = readU32(bytes + kMaterialIdOffset);
    return reference;
  }

  bool patternMatches(const MaterialReference& actual) const {
    if (!holdsPattern()) return false;
    const MaterialReference stored = patternReference();
    return materialReferenceMatches(stored, actual.address, actual.id);
  }

  SynthPattern& pattern() { return payload_.pattern; }
  const SynthPattern& pattern() const { return payload_.pattern; }

  MelodyBuffer& melody() { return payload_.melody; }
  const MelodyBuffer& melody() const { return payload_.melody; }

 private:
  static constexpr size_t kReferenceBytes = 6u;
  static constexpr size_t kTagBytes = 4u;
  static constexpr uint8_t kPatternTagByte = 0xffu;
  static constexpr uint8_t kEmptyTagByte = 0xfeu;

  union Payload {
    SynthPattern pattern;
    MelodyBuffer melody;

    Payload() {}
    ~Payload() {}
  } payload_;

  static constexpr size_t kVoiceOffset =
      sizeof(Payload) - kTagBytes - kReferenceBytes;
  static constexpr size_t kGlobalSlotOffset = kVoiceOffset + 1u;
  static constexpr size_t kMaterialIdOffset = kVoiceOffset + 2u;
  static constexpr size_t kTagOffset = sizeof(Payload) - kTagBytes;

  static void writeU32(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xffu);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xffu);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xffu);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xffu);
  }

  static uint32_t readU32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
  }

  void markEmpty() { fillTag(kEmptyTagByte); }

  void fillTag(uint8_t value) {
    uint8_t* bytes = rawBytes();
    for (size_t i = kTagOffset; i < sizeof(Payload); ++i) bytes[i] = value;
  }

  bool tagIs(uint8_t value) const {
    const uint8_t* bytes = rawBytes();
    for (size_t i = kTagOffset; i < sizeof(Payload); ++i) {
      if (bytes[i] != value) return false;
    }
    return true;
  }

  uint8_t* rawBytes() {
    return reinterpret_cast<uint8_t*>(&payload_);
  }
  const uint8_t* rawBytes() const {
    return reinterpret_cast<const uint8_t*>(&payload_);
  }

  static_assert(sizeof(SynthPattern) + kReferenceBytes + kTagBytes <=
                    sizeof(MelodyBuffer),
                "Working Pattern no longer leaves room for MaterialReference binding");
};

static_assert(sizeof(SynthPattern) <= sizeof(WorkingMaterialStorage::MelodyBuffer),
              "Working Pattern no longer fits in the already-paid Melody footprint");
static_assert(sizeof(WorkingMaterialStorage) <=
                  sizeof(WorkingMaterialStorage::MelodyBuffer),
              "M-WORKING storage may not exceed one Melody buffer");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
