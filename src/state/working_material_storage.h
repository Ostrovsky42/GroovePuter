#pragma once

#ifndef GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
#define GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "../../scenes.h"
#include "../phrase/runtime_synth_events.h"

namespace GroovePuterMaterial {

// One physical session-owned payload for the voice's mutable material.
//
// Pattern and Melody reuse the same already-paid 1284-byte footprint.  The
// physical C++ lifetime tag is intentionally not ActiveMaterial.kind: playback
// source may toggle Pattern <-> Melody without replacing retained Working
// material.  When Pattern is active, its much smaller object leaves tail bytes
// unused.  We keep the Pattern target binding there and mark the final four
// bytes with a state that a valid Melody can never have
// (count=0xffff,lengthTicks=0xffff).  storeMelody() overwrites that tail as part
// of the Melody value, so no sidecar allocation or second musical owner exists.
class WorkingMaterialStorage {
 public:
  using MelodyBuffer = PhraseRuntime::RuntimeSynthEventBuffer;

  WorkingMaterialStorage() { storeMelody(MelodyBuffer{}); }

  void storePattern(const SynthPattern& value) {
    storePattern(value, kUnbound, kUnbound, kUnbound);
  }

  void storePattern(const SynthPattern& value,
                    int page,
                    int bank,
                    int pattern) {
    static_assert(std::is_trivially_destructible<SynthPattern>::value,
                  "Working Pattern must remain trivially destructible");
    new (&payload_.pattern) SynthPattern(value);
    uint8_t* bytes = rawBytes();
    bytes[kPageOffset] = encodeBinding(page);
    bytes[kBankOffset] = encodeBinding(bank);
    bytes[kPatternOffset] = encodeBinding(pattern);
    for (size_t i = kTagOffset; i < sizeof(Payload); ++i) bytes[i] = 0xffu;
  }

  void storeMelody(const MelodyBuffer& value) {
    static_assert(std::is_trivially_destructible<MelodyBuffer>::value,
                  "Working Melody must remain trivially destructible");
    new (&payload_.melody) MelodyBuffer(value);
  }

  bool holdsPattern() const {
    const uint8_t* bytes = rawBytes();
    for (size_t i = kTagOffset; i < sizeof(Payload); ++i) {
      if (bytes[i] != 0xffu) return false;
    }
    return true;
  }

  bool holdsMelody() const { return !holdsPattern(); }

  bool patternMatches(int page, int bank, int pattern) const {
    if (!holdsPattern()) return false;
    const uint8_t* bytes = rawBytes();
    return bytes[kPageOffset] == encodeBinding(page) &&
           bytes[kBankOffset] == encodeBinding(bank) &&
           bytes[kPatternOffset] == encodeBinding(pattern);
  }

  int patternPage() const {
    return holdsPattern() ? decodeBinding(rawBytes()[kPageOffset]) : kUnbound;
  }
  int patternBank() const {
    return holdsPattern() ? decodeBinding(rawBytes()[kBankOffset]) : kUnbound;
  }
  int patternIndex() const {
    return holdsPattern() ? decodeBinding(rawBytes()[kPatternOffset]) : kUnbound;
  }

  SynthPattern& pattern() { return payload_.pattern; }
  const SynthPattern& pattern() const { return payload_.pattern; }

  MelodyBuffer& melody() { return payload_.melody; }
  const MelodyBuffer& melody() const { return payload_.melody; }

 private:
  static constexpr int kUnbound = -1;
  static constexpr size_t kBindingBytes = 3u;
  static constexpr size_t kTagBytes = 4u;

  union Payload {
    SynthPattern pattern;
    MelodyBuffer melody;

    Payload() {}
    ~Payload() {}
  } payload_;

  static constexpr size_t kPageOffset =
      sizeof(Payload) - kTagBytes - kBindingBytes;
  static constexpr size_t kBankOffset = kPageOffset + 1u;
  static constexpr size_t kPatternOffset = kPageOffset + 2u;
  static constexpr size_t kTagOffset = sizeof(Payload) - kTagBytes;

  static uint8_t encodeBinding(int value) {
    return value < 0 ? 0xffu : static_cast<uint8_t>(value);
  }
  static int decodeBinding(uint8_t value) {
    return value == 0xffu ? kUnbound : static_cast<int>(value);
  }

  uint8_t* rawBytes() {
    return reinterpret_cast<uint8_t*>(&payload_);
  }
  const uint8_t* rawBytes() const {
    return reinterpret_cast<const uint8_t*>(&payload_);
  }

  static_assert(sizeof(SynthPattern) + kBindingBytes + kTagBytes <=
                    sizeof(MelodyBuffer),
                "Working Pattern no longer leaves room for in-place lifetime binding");
};

static_assert(sizeof(SynthPattern) <= sizeof(WorkingMaterialStorage::MelodyBuffer),
              "Working Pattern no longer fits in the already-paid Melody footprint");
static_assert(sizeof(WorkingMaterialStorage) <=
                  sizeof(WorkingMaterialStorage::MelodyBuffer),
              "M-WORKING storage may not exceed one Melody buffer");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_STATE_WORKING_MATERIAL_STORAGE_H
