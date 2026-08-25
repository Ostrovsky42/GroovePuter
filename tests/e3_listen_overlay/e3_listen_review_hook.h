#pragma once

#include <cstdint>

#include "../../generation/composition/generation_profile.h"
#include "../../generation/rhythm/rhythm_realizer.h"
#include "../../generation/roles/bass_rhythm.h"

namespace GroovePuterRhythm {

enum class E3ListenVariant : uint8_t {
  Canonical = 0,
  Before,
  After,
  Count,
};

// Disposable review-build hook. This header is copied only into the staged
// sketch tree by prepare_0_9_9_e3_listen_sketch.py. No normal build contains
// this API.
void configureE3ListenReview(uint8_t caseIndex, E3ListenVariant variant);
void disableE3ListenReview();
bool e3ListenReviewActive();

// Narrow review-only handoffs used by the staged migration source.
void e3ListenOverrideComposition(GenerationCompositionResult& composition);
void e3ListenOverrideRhythmPlan(RhythmPhrasePlan& plan);
void e3ListenOverrideBassPlan(BassRhythmPlan& plan);

}  // namespace GroovePuterRhythm
