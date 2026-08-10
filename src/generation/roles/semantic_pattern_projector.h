#pragma once

#include <cstdint>

#include "../../../scenes.h"
#include "../feel/feel_interpreter.h"
#include "../rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

enum class SemanticPatternProjectStatus : uint8_t {
  Ok = 0,
  InvalidPlan,
  MissingPitchSource,
  Count,
};

// Projects an already-generated pitch phrase onto semantic rhythm sites.
// onsetMask owns NoteOn topology. continuationMask owns tied continuation
// cells and may never add a new pitch choice. Source pitch, velocity, accent
// and engine-independent articulation remain authoritative.
SemanticPatternProjectStatus projectLegacyPitchPattern(
    const SynthPattern& source,
    StepMask onsetMask,
    StepMask continuationMask,
    SynthPattern& destination);

SemanticPatternProjectStatus projectLegacyPitchPatternWithOrder(
    const SynthPattern& source,
    StepMask onsetMask,
    StepMask continuationMask,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount,
    SynthPattern& destination);

FeelInterpretStatus applyFeelToSemanticPattern(
    RhythmRole role,
    StepMask onsetMask,
    FeelProfileId profile,
    uint8_t amount,
    const GenerationContext& generation,
    SynthPattern& destination);

}  // namespace GroovePuterRhythm
