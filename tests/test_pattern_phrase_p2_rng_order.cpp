#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "src/phrase/runtime_pattern_event_bank.h"

namespace {

using namespace PhraseRuntime;

enum class Role : uint8_t {
  SynthA = 0,
  SynthB = 1,
  Drum0 = 2,
  Drum1 = 3,
};

struct Decision {
  uint8_t step = 0;
  Role role = Role::SynthA;
  bool accepted = false;
};

struct Trace {
  Decision values[64]{};
  uint8_t count = 0;

  void push(uint8_t step, Role role, bool accepted) {
    assert(count < 64);
    values[count++] = Decision{step, role, accepted};
  }
};

SynthPattern emptySynthPattern() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
  }
  return pattern;
}

PatternProjectionSettings settingsFor(uint8_t synthIndex) {
  PatternProjectionSettings settings{};
  settings.synthIndex = synthIndex;
  settings.swingPercent = 50;
  settings.swingEnabled = false;
  settings.gateLengthRatio = 0.5f;
  return settings;
}

bool acceptLegacySynth(const SynthStep& step) {
  assert(step.note >= 0);
  if (step.ghost && (std::rand() % 100 >= 80)) return false;
  if (step.probability < 100 &&
      (std::rand() % 100 >= step.probability)) {
    return false;
  }
  return true;
}

bool acceptRuntimeSynth(const RuntimeSynthEvent& event) {
  const bool ghost = (event.flags & kEventGhost) != 0;
  if (ghost && (std::rand() % 100 >= 80)) return false;
  if (event.probability < 100 &&
      (std::rand() % 100 >= event.probability)) {
    return false;
  }
  return true;
}

bool acceptDrum(const DrumStep& step) {
  assert(step.hit);
  if (step.probability < 100 &&
      (std::rand() % 100 >= step.probability)) {
    return false;
  }
  return true;
}

Trace runLegacyTrace(const SynthPattern& synthA,
                     const SynthPattern& synthB,
                     const DrumPatternSet& drums) {
  Trace trace{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (synthA.steps[step].note >= 0) {
      trace.push(step, Role::SynthA, acceptLegacySynth(synthA.steps[step]));
    }
    if (synthB.steps[step].note >= 0) {
      trace.push(step, Role::SynthB, acceptLegacySynth(synthB.steps[step]));
    }
    if (drums.voices[0].steps[step].hit) {
      trace.push(step, Role::Drum0, acceptDrum(drums.voices[0].steps[step]));
    }
    if (drums.voices[1].steps[step].hit) {
      trace.push(step, Role::Drum1, acceptDrum(drums.voices[1].steps[step]));
    }
  }
  return trace;
}

Trace runP2Trace(const RuntimePatternEventBuffer& synthA,
                 const RuntimePatternEventBuffer& synthB,
                 const DrumPatternSet& drums) {
  Trace trace{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (const RuntimeSynthEvent* event = synthA.eventForSourceStep(step)) {
      trace.push(step, Role::SynthA, acceptRuntimeSynth(*event));
    }
    if (const RuntimeSynthEvent* event = synthB.eventForSourceStep(step)) {
      trace.push(step, Role::SynthB, acceptRuntimeSynth(*event));
    }
    if (drums.voices[0].steps[step].hit) {
      trace.push(step, Role::Drum0, acceptDrum(drums.voices[0].steps[step]));
    }
    if (drums.voices[1].steps[step].hit) {
      trace.push(step, Role::Drum1, acceptDrum(drums.voices[1].steps[step]));
    }
  }
  return trace;
}

void assertSameObservableTrace(const Trace& legacy, const Trace& p2) {
  assert(legacy.count == p2.count);
  int accepted = 0;
  int rejected = 0;
  for (uint8_t i = 0; i < legacy.count; ++i) {
    assert(legacy.values[i].step == p2.values[i].step);
    assert(legacy.values[i].role == p2.values[i].role);
    assert(legacy.values[i].accepted == p2.values[i].accepted);
    if (legacy.values[i].accepted) {
      ++accepted;
    } else {
      ++rejected;
    }
  }
  assert(accepted > 0);
  assert(rejected > 0);
}

void testFixedSeedObservableRngOrderingMatchesLegacy() {
  SynthPattern synthA = emptySynthPattern();
  SynthPattern synthB = emptySynthPattern();
  DrumPatternSet drums{};

  for (uint8_t step = 0; step < 8; ++step) {
    SynthStep& a = synthA.steps[step];
    a.note = static_cast<int8_t>(48 + step);
    a.ghost = (step % 3) == 0;
    a.probability = static_cast<uint8_t>(31 + step * 7);

    SynthStep& b = synthB.steps[step];
    b.note = static_cast<int8_t>(60 + step);
    b.ghost = (step % 2) != 0;
    b.probability = static_cast<uint8_t>(73 - step * 5);

    DrumStep& d0 = drums.voices[0].steps[step];
    d0.hit = true;
    d0.probability = static_cast<uint8_t>(43 + step * 6);

    if ((step % 2) != 0) {
      DrumStep& d1 = drums.voices[1].steps[step];
      d1.hit = true;
      d1.probability = static_cast<uint8_t>(29 + step * 4);
    }
  }

  RuntimePatternEventBank bank{};
  assert(bank.refresh(0, 0, 0, synthA, settingsFor(0)) ==
         PatternBankRefreshStatus::Ready);
  assert(bank.refresh(1, 0, 0, synthB, settingsFor(1)) ==
         PatternBankRefreshStatus::Ready);

  constexpr unsigned kSeed = 0x00C0FFEEu;
  std::srand(kSeed);
  const Trace legacy = runLegacyTrace(synthA, synthB, drums);
  std::srand(kSeed);
  const Trace p2 = runP2Trace(bank.select(0, 0, 0),
                              bank.select(1, 0, 0),
                              drums);

  assertSameObservableTrace(legacy, p2);
}

}  // namespace

int main() {
  testFixedSeedObservableRngOrderingMatchesLegacy();
  std::puts("PATTERN/PHRASE P2 fixed-seed observable RNG ordering: PASS");
  return 0;
}
