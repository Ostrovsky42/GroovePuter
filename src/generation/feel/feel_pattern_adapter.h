#ifndef GROOVEPUTER_GENERATION_FEEL_FEEL_PATTERN_ADAPTER_H
#define GROOVEPUTER_GENERATION_FEEL_FEEL_PATTERN_ADAPTER_H

#include "feel_interpreter.h"
#include "../materialization/pattern_materializer.h"

namespace GroovePuterRhythm {

enum class FeelPatternApplyStatus : uint8_t {
  Ok = 0,
  InvalidPlan,
  InvalidBinding,
  InterpretFailed,
  Count,
};

// Applies Feel only to material already emitted from the semantic rhythm plan.
// Physical binding stays explicit; no Genre or transport knowledge enters the
// interpreter. The destination remains unchanged on failure.
FeelPatternApplyStatus applyFeelToMaterializedPattern(
    const RhythmPhrasePlan& plan,
    const PatternMaterializerBinding& binding,
    FeelProfileId profile,
    uint8_t amount,
    const GenerationContext& generation,
    MaterializedPatterns& destination);

// Apply the same interpreter to the drum hits after an idiom has replaced the
// rhythm material. This keeps FEEL tied to the final onset topology.
FeelPatternApplyStatus applyFeelToMaterializedDrums(
    const PatternMaterializerBinding& binding,
    FeelProfileId profile,
    uint8_t amount,
    const GenerationContext& generation,
    DrumPatternSet& destination);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_FEEL_FEEL_PATTERN_ADAPTER_H
