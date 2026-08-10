#ifndef GROOVEPUTER_GENERATION_FEEL_FEEL_INTERPRETER_H
#define GROOVEPUTER_GENERATION_FEEL_FEEL_INTERPRETER_H

#include "../generation_context.h"
#include "feel_types.h"

namespace GroovePuterRhythm {

struct FeelInterpretRequest {
  const FeelPhrase* phrase = nullptr;
  FeelProfileId profile = FeelProfileId::Straight;
  uint8_t amount = 0;  // 0..100
  uint16_t gridIntervalTicks = kFeelTicksPerStep;
  GenerationContext generation{};
};

// Converts ideal musical events to absolute ticks. The input/output order is
// preserved, onsets remain inside their bar, and offsets are always derived
// from the ideal coordinate (never from the preceding interpreted event).
FeelInterpretStatus interpretFeelPhrase(const FeelInterpretRequest& request,
                                        TimedFeelPhrase& destination);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_FEEL_FEEL_INTERPRETER_H
