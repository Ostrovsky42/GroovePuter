#pragma once

#include <algorithm>

#include "../../scenes.h"

namespace GroovePuterUndo {
namespace PatternEdit {

constexpr int kMin303Note = 24;
constexpr int kMax303Note = 71;

inline int clampStep(int step) {
  if (step < 0) return 0;
  if (step >= SynthPattern::kSteps) return SynthPattern::kSteps - 1;
  return step;
}

inline bool sameStep(const SynthStep& lhs, const SynthStep& rhs) {
  return lhs.note == rhs.note &&
         lhs.slide == rhs.slide &&
         lhs.accent == rhs.accent &&
         lhs.ghost == rhs.ghost &&
         lhs.velocity == rhs.velocity &&
         lhs.timing == rhs.timing &&
         lhs.fx == rhs.fx &&
         lhs.fxParam == rhs.fxParam &&
         lhs.probability == rhs.probability;
}

inline bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (!sameStep(lhs.steps[step], rhs.steps[step])) return false;
  }
  return true;
}

inline void clearStep(SynthPattern& pattern, int stepIndex) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  // Match MiniAcid::clear303Step exactly. Velocity and timing are retained.
  step.note = -1;
  step.slide = false;
  step.accent = false;
  step.ghost = false;
  step.probability = 100;
  step.fx = 0;
  step.fxParam = 0;
}

inline void adjustNote(SynthPattern& pattern, int stepIndex, int semitoneDelta) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  int note = step.note;

  if (note == -2) {
    if (semitoneDelta > 0) step.note = -1;
    return;
  }
  if (note == -1) {
    if (semitoneDelta > 0) step.note = kMin303Note;
    else if (semitoneDelta < 0) step.note = -2;
    return;
  }

  note += semitoneDelta;
  if (note < kMin303Note) {
    step.note = -1;
    return;
  }
  if (note > kMax303Note) note = kMax303Note;
  step.note = static_cast<int8_t>(note);
}

inline void adjustOctave(SynthPattern& pattern, int stepIndex, int octaveDelta) {
  adjustNote(pattern, stepIndex, octaveDelta * 12);
}

inline void toggleAccent(SynthPattern& pattern, int stepIndex) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  step.accent = !step.accent;
}

inline void setAccent(SynthPattern& pattern, int stepIndex, bool accent) {
  pattern.steps[clampStep(stepIndex)].accent = accent;
}

inline void toggleSlide(SynthPattern& pattern, int stepIndex) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  step.slide = !step.slide;
}

inline void setSlide(SynthPattern& pattern, int stepIndex, bool slide) {
  pattern.steps[clampStep(stepIndex)].slide = slide;
}

inline void cycleFx(SynthPattern& pattern, int stepIndex) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  uint8_t current = step.fx;
  if (current == static_cast<uint8_t>(StepFx::None)) {
    current = static_cast<uint8_t>(StepFx::Retrig);
  } else if (current == static_cast<uint8_t>(StepFx::Retrig)) {
    current = static_cast<uint8_t>(StepFx::Reverse);
  } else {
    current = static_cast<uint8_t>(StepFx::None);
  }
  step.fx = current;
}

inline void adjustFxParam(SynthPattern& pattern, int stepIndex, int delta) {
  SynthStep& step = pattern.steps[clampStep(stepIndex)];
  int value = static_cast<int>(step.fxParam) + delta;
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  step.fxParam = static_cast<uint8_t>(value);
}

inline void rotate(SynthPattern& pattern, int steps) {
  if (steps == 0) return;
  int shift = steps % SynthPattern::kSteps;
  if (shift < 0) shift += SynthPattern::kSteps;
  if (shift == 0) return;
  std::rotate(pattern.steps,
              pattern.steps + (SynthPattern::kSteps - shift),
              pattern.steps + SynthPattern::kSteps);
}

}  // namespace PatternEdit
}  // namespace GroovePuterUndo
