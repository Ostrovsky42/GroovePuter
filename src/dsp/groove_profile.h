#pragma once

#include "mini_dsp_params.h"
#include "mini_tb303.h"
#include "../../scenes.h"

struct PatternCorridors {
  int notesMin;
  int notesMax;
  float accentProbability;
  float slideProbability;
  float swingAmount;
};

struct PatternMetrics {
  int notes = 0;
  int rests = 0;
  int accents = 0;
  int slides = 0;
  int ghosts = 0;
};

namespace GrooveProfile {

PatternCorridors getCorridors(GrooveboxMode mode, int flavor);
void applyBudgetRules(GrooveboxMode mode, float delayMix, float tapeSpace, PatternCorridors& c);
PatternMetrics analyzePattern(const SynthPattern& pattern);

}  // namespace GrooveProfile
