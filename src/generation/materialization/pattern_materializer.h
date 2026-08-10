#ifndef GROOVEPUTER_GENERATION_MATERIALIZATION_PATTERN_MATERIALIZER_H
#define GROOVEPUTER_GENERATION_MATERIALIZATION_PATTERN_MATERIALIZER_H

#include <cstdint>

#include "../../dsp/mini_drumvoices.h"
#include "../rhythm/rhythm_realizer.h"
#include "../../../scenes.h"

namespace GroovePuterRhythm {

enum class SynthPatternTarget : uint8_t {
  None = 0,
  SynthA,
  SynthB,
  Count,
};

enum class PatternMaterializeStatus : uint8_t {
  Ok = 0,
  InvalidPlan,
  InvalidBinding,
  UnboundRole,
  Count,
};

struct SynthRhythmBinding {
  SynthPatternTarget target = SynthPatternTarget::None;
  int8_t note = -1;
};

struct PatternMaterializerBinding {
  // -1 means no drum destination for the role. Physical synth destinations are
  // never inferred: Stage 4 must not introduce Synth A == Bass ownership.
  int8_t drumVoiceByRole[kRhythmRoleCount] = {-1, -1, -1, -1,
                                              -1, -1, -1, -1};
  SynthRhythmBinding synthByRole[kRhythmRoleCount]{};

  // A role with realized onsets must either have exactly one binding or be
  // explicitly ignored by the caller. This makes deferred Bass/Chord/Melodic
  // ownership visible instead of silently dropping musical material.
  RhythmRoleMask ignoredRoles = 0;
};

struct PatternMaterializationDiagnostics {
  uint16_t structuralEvents = 0;
  uint16_t secondaryEvents = 0;
  uint16_t ghostEvents = 0;
  uint16_t accentEvents = 0;
  uint16_t ignoredEvents = 0;

  // SynthPattern has no generic GateClass representation. Stage 4 records how
  // many Short/Held/Tie intents were intentionally deferred rather than
  // translating them into engine-specific slide behavior.
  uint16_t deferredGateEvents = 0;

  RhythmRoleMask emittedRoles = 0;
  RhythmRoleMask ignoredRoles = 0;
};

struct MaterializedPatterns {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

// Convenience adapter for the existing physical drum layout only. Synth roles
// remain unbound unless the caller explicitly binds or ignores them.
PatternMaterializerBinding standardDrumPatternBinding(
    RhythmRoleMask ignoredRoles = 0);

// Stage 4 accepts the current Stage 2/3 one-bar Statement surface. Stage 6 owns
// BarEvolution and multi-bar destination arrays. The destination and optional
// diagnostics remain byte-for-byte unchanged on failure.
PatternMaterializeStatus materializeRhythmPattern(
    const RhythmPhrasePlan& plan,
    const PatternMaterializerBinding& binding,
    MaterializedPatterns& destination,
    PatternMaterializationDiagnostics* diagnostics = nullptr);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MATERIALIZATION_PATTERN_MATERIALIZER_H
