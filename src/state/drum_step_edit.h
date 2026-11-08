#pragma once

#include <stdint.h>

#include "../../scenes.h"
#include "step_setting_marks.h"

// DRUMS STEP SETTINGS (Alt+E on the drum grid): pure field rules, no engine,
// no allocation. The page applies them inside commitDrumPatternMutation, so
// every change is one Undo step.
namespace GroovePuterUndo {
namespace DrumStepEdit {

enum Field : int {
  kFieldProb = 0,
  kFieldTiming = 1,
  kFieldVelocity = 2,
  kFieldOrnament = 3,  // NONE / FLAM / ROLL
  kFieldCount = 4,
};

constexpr int kProbMin = 0;
constexpr int kProbMax = 100;
constexpr int kProbStep = 5;
// Same micro-timing range the scene loader clamps to (scenes.cpp).
constexpr int kTimingMin = -23;
constexpr int kTimingMax = 23;
// Velocity 0 would silence the hit; the drum voices scale by velocity / 100.
constexpr int kVelocityMin = 1;
constexpr int kVelocityMax = 127;
constexpr int kVelocityStep = 5;

// Parameters written with an ornament; both are inside the ranges that
// MiniAcid::setupDrumStepFx_ plays: FLAM gap 1..23 ticks (its own default is
// 2), ROLL 2..12 hits per step (fewer than 2 plays no roll at all).
constexpr uint8_t kFlamGapTicks = 2;
constexpr uint8_t kRollHits = 4;

enum class Ornament : uint8_t { None = 0, Flam = 1, Roll = 2, Other = 3 };

inline int clampValue(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline Ornament ornamentOf(const DrumStep& step) {
  switch (step.fx) {
    case DRUM_FX_NONE: return Ornament::None;
    case DRUM_FX_FLAM: return Ornament::Flam;
    case DRUM_FX_ROLL: return Ornament::Roll;
    default: return Ornament::Other;
  }
}

inline const char* ornamentName(const DrumStep& step) {
  switch (step.fx) {
    case DRUM_FX_NONE: return "OFF";
    case DRUM_FX_FLAM: return "FLAM";
    case DRUM_FX_ROLL: return "ROLL";
    case DRUM_FX_RETRIG: return "RETRIG";
    case DRUM_FX_REVERSE: return "REVERSE";
    default: return "PITCH";
  }
}

inline void setOrnament(DrumStep& step, Ornament ornament) {
  switch (ornament) {
    case Ornament::Flam:
      step.fx = DRUM_FX_FLAM;
      step.fxParam = kFlamGapTicks;
      break;
    case Ornament::Roll:
      step.fx = DRUM_FX_ROLL;
      step.fxParam = kRollHits;
      break;
    default:
      step.fx = DRUM_FX_NONE;
      step.fxParam = 0;
      break;
  }
}

// OFF -> FLAM -> ROLL -> OFF (right), reverse on left. Another step FX
// (RETRIG, REVERSE, PITCH) counts as OFF, so one press replaces it.
inline Ornament nextOrnament(Ornament current, int direction) {
  const int index = current == Ornament::Other ? 0 : static_cast<int>(current);
  const int next = (index + (direction < 0 ? 2 : 1)) % 3;
  return static_cast<Ornament>(next);
}

// Applies one press to `step`. `anchor` is the first step of the area: an
// area ends up uniform, as on the synth STEP SETTINGS window.
inline void applyField(DrumStep& step, const DrumStep& anchor, int field,
                       int direction) {
  const int dir = direction < 0 ? -1 : 1;
  switch (field) {
    case kFieldProb:
      step.probability = static_cast<uint8_t>(StepSettingMarks::stepOnGrid(
          anchor.probability, dir, kProbStep, kProbMin, kProbMax));
      break;
    case kFieldTiming:
      step.timing = static_cast<int8_t>(
          clampValue(anchor.timing + dir, kTimingMin, kTimingMax));
      break;
    case kFieldVelocity:
      step.velocity = static_cast<uint8_t>(StepSettingMarks::stepOnGrid(
          anchor.velocity, dir, kVelocityStep, kVelocityMin, kVelocityMax));
      break;
    case kFieldOrnament:
      setOrnament(step, nextOrnament(ornamentOf(anchor), dir));
      break;
    default:
      break;
  }
}

}  // namespace DrumStepEdit
}  // namespace GroovePuterUndo
