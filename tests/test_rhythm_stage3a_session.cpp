#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/audition/rhythm_audition_session.h"

using namespace GroovePuterRhythm;

namespace {

bool synthEqual(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& x = a.steps[step];
    const SynthStep& y = b.steps[step];
    if (x.note != y.note || x.slide != y.slide || x.accent != y.accent ||
        x.ghost != y.ghost || x.velocity != y.velocity ||
        x.timing != y.timing || x.fx != y.fx || x.fxParam != y.fxParam ||
        x.probability != y.probability) {
      return false;
    }
  }
  return true;
}

bool drumsEqual(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& x = a.voices[voice].steps[step];
      const DrumStep& y = b.voices[voice].steps[step];
      if (x.hit != y.hit || x.accent != y.accent ||
          x.velocity != y.velocity || x.timing != y.timing ||
          x.fx != y.fx || x.fxParam != y.fxParam ||
          x.probability != y.probability) {
        return false;
      }
    }
  }
  return a.groove.swing == b.groove.swing &&
         a.groove.humanize == b.groove.humanize;
}

void seedExistingPatterns(DrumPatternSet& drums,
                          SynthPattern& synthA,
                          SynthPattern& synthB) {
  drums.voices[KICK].steps[1].hit = true;
  drums.voices[KICK].steps[1].velocity = 77;
  drums.voices[SNARE].steps[9].hit = true;
  drums.groove.swing = 0.61f;
  drums.groove.humanize = 0.22f;

  synthA.steps[2].note = 49;
  synthA.steps[2].slide = true;
  synthA.steps[2].velocity = 91;
  synthB.steps[5].note = 62;
  synthB.steps[5].accent = true;
  synthB.steps[5].velocity = 103;
}

void testActivationIsEphemeralAndRestoresExactly() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  seedExistingPatterns(drums, synthA, synthB);
  const DrumPatternSet originalDrums = drums;
  const SynthPattern originalA = synthA;
  const SynthPattern originalB = synthB;

  Audition::Session session;
  assert(!session.active());
  assert(session.activate(drums, synthA, synthB));
  assert(session.active());
  assert(!drumsEqual(drums, originalDrums));

  session.deactivate(drums, synthA, synthB);
  assert(!session.active());
  assert(drumsEqual(drums, originalDrums));
  assert(synthEqual(synthA, originalA));
  assert(synthEqual(synthB, originalB));
}

void testAuditionChangesNeverReplaceBackup() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  seedExistingPatterns(drums, synthA, synthB);
  const DrumPatternSet originalDrums = drums;
  const SynthPattern originalA = synthA;
  const SynthPattern originalB = synthB;

  Audition::Session session;
  assert(session.activate(drums, synthA, synthB));
  assert(session.selectDefinition(
      static_cast<uint8_t>(Audition::Archetype::ClassicTwoStep),
      drums, synthA, synthB));
  assert(session.shiftSeed(4, drums, synthA, synthB));
  assert(session.cycleLevel(drums, synthA, synthB));
  assert(session.toggleBass(drums, synthA, synthB));
  assert(session.rerender(drums, synthA, synthB));

  session.deactivate(drums, synthA, synthB);
  assert(drumsEqual(drums, originalDrums));
  assert(synthEqual(synthA, originalA));
  assert(synthEqual(synthB, originalB));
}

void testInactiveMutationsAreRejected() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Audition::Session session;

  assert(!session.selectDefinition(1, drums, synthA, synthB));
  assert(!session.shiftSeed(1, drums, synthA, synthB));
  assert(!session.cycleLevel(drums, synthA, synthB));
  assert(!session.toggleBass(drums, synthA, synthB));
  assert(!session.rerender(drums, synthA, synthB));
}

void testStatusIsBoundedAndUseful() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Audition::Session session;
  assert(session.activate(drums, synthA, synthB));
  char status[64]{};
  session.formatStatus(status, sizeof(status));
  assert(std::strstr(status, "AUD straight_drive") != nullptr);
  assert(std::strstr(status, "S1") != nullptr);
  assert(std::strstr(status, "P1") != nullptr);
  assert(std::strstr(status, "BOFF") != nullptr);
}

}  // namespace

int main() {
  testActivationIsEphemeralAndRestoresExactly();
  testAuditionChangesNeverReplaceBackup();
  testInactiveMutationsAreRejected();
  testStatusIsBoundedAndUseful();
  return 0;
}
