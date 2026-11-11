#pragma once

#include <stdint.h>

#include "../../scenes.h"
#include "../phrase/runtime_synth_events.h"

// STEP SETTINGS (Alt+E, synth STEPS and DRUMS grid): which values differ from
// a plain step, and how a +/- press moves a value. Pure rules, no engine, no
// allocation; the windows draw a mark on every field this reports as custom.
namespace GroovePuterUndo {
namespace StepSettingMarks {

// The defaults are the ones the SynthStep / DrumStep structs start with.
constexpr int kDefaultProbability = 100;
constexpr int kDefaultTiming = 0;
constexpr int kDefaultVelocity = 100;

// Moves `value` one press in `direction`, landing on a multiple of `step`
// (100 is one, so the default stays reachable from 1 or 127 and from an
// imported 97), clamped to [lo, hi].
inline int stepOnGrid(int value, int direction, int step, int lo, int hi) {
  int next;
  if (direction < 0) {
    next = value - 1;
    next -= ((next % step) + step) % step;  // down to a multiple of step
  } else {
    next = value + 1;
    const int rem = ((next % step) + step) % step;
    if (rem != 0) next += step - rem;  // up to a multiple of step
  }
  return next < lo ? lo : (next > hi ? hi : next);
}

// Micro-timing mark on the step grid: -1 early (TIMING < 0), +1 late, 0 none.
inline int timingMarkSide(int timing) {
  return timing < kDefaultTiming ? -1 : (timing > kDefaultTiming ? 1 : 0);
}

// Synth fields: 0 PROB, 1 TIMING, 2 VEL, 3 CHANCE (the ghost flag: the note
// plays with a ~80% chance).
inline bool synthFieldCustom(const SynthStep& s, int field) {
  switch (field) {
    case 0: return s.probability != kDefaultProbability;
    case 1: return s.timing != kDefaultTiming;
    case 2: return s.velocity != kDefaultVelocity;
    case 3: return s.ghost != 0;
    default: return false;
  }
}

// Drum fields: 0 PROB, 1 TIMING, 2 VEL, 3 FLAM/ROLL (any step FX counts).
inline bool drumFieldCustom(const DrumStep& s, int field) {
  switch (field) {
    case 0: return s.probability != kDefaultProbability;
    case 1: return s.timing != kDefaultTiming;
    case 2: return s.velocity != kDefaultVelocity;
    case 3: return s.fx != DRUM_FX_NONE;
    default: return false;
  }
}

// MIXED: true when two steps of the selected area differ in `field`. The
// windows show MIXED instead of the first cell's value then.
inline bool synthFieldDiffers(const SynthStep& a, const SynthStep& b, int field) {
  switch (field) {
    case 0: return a.probability != b.probability;
    case 1: return a.timing != b.timing;
    case 2: return a.velocity != b.velocity;
    case 3: return (a.ghost != 0) != (b.ghost != 0);
    default: return false;
  }
}

inline bool drumFieldDiffers(const DrumStep& a, const DrumStep& b, int field) {
  switch (field) {
    case 0: return a.probability != b.probability;
    case 1: return a.timing != b.timing;
    case 2: return a.velocity != b.velocity;
    case 3: return a.fx != b.fx;
    default: return false;
  }
}

// One hint line for both STEP SETTINGS windows (SYNTH and DRUMS), 36 chars.
constexpr const char* kStepSettingsHint = "UP/DN FIELD  L/R VALUE  ENT/ESC CLOSE";

// Synth grid mark: any of PROB / VEL / CHANCE differs from a plain step. TIMING
// is left out on purpose: it has its own corner mark on the cell.
inline bool synthStepHasNonTimingSettings(const SynthStep& s) {
  return synthFieldCustom(s, 0) || synthFieldCustom(s, 2) || synthFieldCustom(s, 3);
}

// CHANCE and PROB multiply in the engine (ghost = 80%, then PROB). The
// resulting percentage of the note actually playing, for display only.
inline int effectiveChancePercent(int probability, bool ghost) {
  return ghost ? (80 * probability) / 100 : probability;
}

// Melody note fields (Alt+E on MELODY): 0 VEL, 1 PROB, 2 SLIDE, 3 CHANCE.
inline bool melodyFieldCustom(const PhraseRuntime::RuntimeSynthEvent& e, int field) {
  switch (field) {
    case 0: return e.velocity != kDefaultVelocity;
    case 1: return e.probability != kDefaultProbability;
    case 2: return (e.flags & PhraseRuntime::kEventSlide) != 0;
    case 3: return (e.flags & PhraseRuntime::kEventGhost) != 0;
    default: return false;
  }
}

inline bool melodyFieldDiffers(const PhraseRuntime::RuntimeSynthEvent& a,
                               const PhraseRuntime::RuntimeSynthEvent& b,
                               int field) {
  switch (field) {
    case 0: return a.velocity != b.velocity;
    case 1: return a.probability != b.probability;
    case 2: return ((a.flags & PhraseRuntime::kEventSlide) != 0) !=
                   ((b.flags & PhraseRuntime::kEventSlide) != 0);
    case 3: return ((a.flags & PhraseRuntime::kEventGhost) != 0) !=
                   ((b.flags & PhraseRuntime::kEventGhost) != 0);
    default: return false;
  }
}

inline bool melodyNoteHasCustomSettings(const PhraseRuntime::RuntimeSynthEvent& e) {
  return melodyFieldCustom(e, 0) || melodyFieldCustom(e, 1) || melodyFieldCustom(e, 3);
}

}  // namespace StepSettingMarks
}  // namespace GroovePuterUndo
