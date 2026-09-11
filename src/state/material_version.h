#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_H

#include <cstdint>

#include "scenes.h"
#include "src/phrase/runtime_synth_events.h"

// 0.9.11 A2: exact accepted-state fingerprint.
//
// This is deliberately NOT MaterialId and NOT lineage. It answers one narrower
// question: "are these the same canonical musical bytes/fields that were
// resolved/prepared?" It is recomputable after reboot and requires no
// per-material resident metadata table.
namespace GroovePuterMaterial {

struct MaterialVersionToken {
  uint32_t low = 0;
  uint32_t high = 0;

  constexpr bool valid() const { return low != 0 || high != 0; }

  friend constexpr bool operator==(MaterialVersionToken lhs,
                                   MaterialVersionToken rhs) {
    return lhs.low == rhs.low && lhs.high == rhs.high;
  }
  friend constexpr bool operator!=(MaterialVersionToken lhs,
                                   MaterialVersionToken rhs) {
    return !(lhs == rhs);
  }
};

static_assert(sizeof(MaterialVersionToken) == 8,
              "MaterialVersionToken must remain exactly eight bytes");

namespace material_version_detail {

constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

inline void feedByte(uint64_t& hash, uint8_t value) {
  hash ^= value;
  hash *= kFnvPrime;
}

inline void feedU16(uint64_t& hash, uint16_t value) {
  feedByte(hash, static_cast<uint8_t>(value & 0xFFu));
  feedByte(hash, static_cast<uint8_t>((value >> 8u) & 0xFFu));
}

inline MaterialVersionToken tokenFromHash(uint64_t hash) {
  // All-zero is reserved for "no trustworthy exact version".
  if (hash == 0) hash = 1;
  return {static_cast<uint32_t>(hash & 0xFFFFFFFFu),
          static_cast<uint32_t>(hash >> 32u)};
}

}  // namespace material_version_detail

inline MaterialVersionToken versionForPattern(const SynthPattern& pattern) {
  using namespace material_version_detail;
  uint64_t hash = kFnvOffset;
  feedByte(hash, static_cast<uint8_t>('P'));
  feedByte(hash, static_cast<uint8_t>(SynthPattern::kSteps));
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    const SynthStep& step = pattern.steps[i];
    feedByte(hash, static_cast<uint8_t>(step.note));
    uint8_t flags = 0;
    if (step.slide) flags |= 1u << 0u;
    if (step.accent) flags |= 1u << 1u;
    if (step.ghost) flags |= 1u << 2u;
    feedByte(hash, flags);
    feedByte(hash, step.velocity);
    feedByte(hash, static_cast<uint8_t>(step.timing));
    feedByte(hash, step.fx);
    feedByte(hash, step.fxParam);
    feedByte(hash, step.probability);
  }
  return tokenFromHash(hash);
}

inline MaterialVersionToken versionForMelody(
    const PhraseRuntime::RuntimeSynthEventBuffer& melody) {
  using namespace material_version_detail;
  uint64_t hash = kFnvOffset;
  feedByte(hash, static_cast<uint8_t>('M'));
  feedU16(hash, melody.count);
  feedU16(hash, melody.lengthTicks);
  const uint16_t count = melody.count <= PhraseRuntime::kMaxSynthEvents
                             ? melody.count
                             : PhraseRuntime::kMaxSynthEvents;
  for (uint16_t i = 0; i < count; ++i) {
    const auto& event = melody.events[i];
    feedU16(hash, event.startTick);
    feedU16(hash, event.durationSubticks);
    feedByte(hash, event.note);
    feedByte(hash, event.velocity);
    feedByte(hash, event.probability);
    feedByte(hash, event.flags);
    feedByte(hash, event.fx);
    feedByte(hash, event.fxParam);
  }
  return tokenFromHash(hash);
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_VERSION_H
