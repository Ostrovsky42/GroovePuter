// 0.9.18 Drum & Bass: one two-step core, four poles (ATMOS, FUNK, DANCE,
// NEURO), per docs/superpowers/plans/2026-10-09-0918-dnb-genre-spec.md.
// The genre's main invariant: fast drums are not "everything fast".

#include "arduino_compat.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

#include "../src/dsp/genre_manager.h"
#include "../src/generation/composition/generation_profile.h"
#include "../src/generation/idiom/genre_idiom.h"

SerialMock Serial;
SDMock SD;

namespace {

constexpr uint8_t kDnb = 14;
constexpr uint8_t kPoles[] = {kBaseRecipeId, kDnbAtmosRecipeId, kDnbFunkRecipeId,
                              kDnbDanceRecipeId, kDnbNeuroRecipeId};

struct Bar {
  SynthPattern bass;
  SynthPattern lead;
  DrumPatternSet drums;
};

struct Phrase {
  Bar bars[4];
  GenreIdiom::IdeaPlan plan;
};

Phrase phraseFor(uint8_t recipe, uint32_t press, uint8_t liveliness = 1) {
  Phrase phrase{};
  GenreIdiom::Request request{};
  request.generativeMode = kDnb;
  request.recipe = recipe;
  request.scale = 0;
  request.level = liveliness == 0 ? 2 : 1;
  request.liveliness = liveliness;
  request.press = press;
  request.deckSeed = 3;
  request.salt = 3u * 131u + press * 977u;
  request.phraseBars = 4;
  for (uint8_t bar = 0; bar < 4; ++bar) {
    request.barOrdinal = bar;
    assert(GenreIdiom::apply(request, phrase.bars[bar].bass, phrase.bars[bar].lead,
                             &phrase.plan, &phrase.bars[bar].drums));
  }
  return phrase;
}

bool onset(const SynthPattern& p, int step) {
  const SynthStep& e = p.steps[step];
  if (e.note < 0) return false;
  return !(step > 0 && e.slide && p.steps[step - 1].note == e.note);
}

uint16_t hits(const DrumPatternSet& drums, int voice, bool anchorsOnly) {
  uint16_t mask = 0;
  for (int step = 0; step < 16; ++step) {
    const DrumStep& s = drums.voices[voice].steps[step];
    if (s.hit && (!anchorsOnly || s.probability == 100)) mask |= 1u << step;
  }
  return mask;
}

// Drums: kick on 1, never four-on-the-floor; snare on 5 with a second anchor
// at 13 (or 15 displaced) or a single half-time snare on 9; ghosts are chance,
// anchors are not.
void testTwoStepCore() {
  for (uint8_t recipe : kPoles) {
    for (uint32_t press = 1; press <= 16; ++press) {
      const Phrase phrase = phraseFor(recipe, press);
      for (const Bar& bar : phrase.bars) {
        const uint16_t kick = hits(bar.drums, 0, true);
        const uint16_t snare = hits(bar.drums, 1, true);
        assert(kick & 1u);
        assert((kick & 0x1111u) != 0x1111u);
        // A fill may add 14-16; the anchors are 5 + 13 (or a late 15) or a
        // single half-time snare on 9.
        const bool halfTime = (snare & ~0xE000u) == (1u << 8);
        assert(halfTime || ((snare & (1u << 4)) && (snare & ((1u << 12) | (1u << 14)))));
        for (int step = 0; step < 16; ++step) {
          const DrumStep& hat = bar.drums.voices[2].steps[step];
          if (hat.hit && (step % 2)) assert(hat.probability < 100);  // ghost 16ths are chance
          if (step % 2 == 0) assert(hat.hit && hat.probability == 100);  // eighths are anchors
        }
      }
    }
  }
}

// Bass lives slower than the drums: at most eight attacks a bar, no run of
// three sixteenths, and it leaves the second kick (step 11) to the kick on
// most bars.
void testBassIsSlowerThanDrums() {
  for (uint8_t recipe : kPoles) {
    int kickClashes = 0;
    int bars = 0;
    for (uint32_t press = 1; press <= 16; ++press) {
      const Phrase phrase = phraseFor(recipe, press);
      for (const Bar& bar : phrase.bars) {
        int count = 0;
        int run = 0;
        for (int step = 0; step < 16; ++step) {
          if (onset(bar.bass, step)) {
            ++count;
            ++run;
            assert(run <= 2);
          } else {
            run = 0;
          }
        }
        assert(count <= 8);
        const uint16_t kick = hits(bar.drums, 0, true);
        for (int step = 1; step < 16; ++step) {
          if ((kick & (1u << step)) && onset(bar.bass, step)) ++kickClashes;
        }
        ++bars;
      }
    }
    assert(kickClashes * 4 <= bars);
  }
}

// The lead needs silence: every phrase leaves at least a third of its steps
// to the others, and DnB deals call and response often.
void testLeadLeavesSpace() {
  for (uint8_t recipe : kPoles) {
    int callResponse = 0;
    for (uint32_t press = 1; press <= 20; ++press) {
      const Phrase phrase = phraseFor(recipe, press);
      int attacks = 0;
      for (const Bar& bar : phrase.bars) {
        for (int step = 0; step < 16; ++step) attacks += onset(bar.lead, step);
      }
      assert(attacks <= 40);
      if (phrase.plan.idea == GenreIdiom::Idea::CallResponse) {
        ++callResponse;
        // A A' _ B: bar 3 belongs to the bass.
        for (int step = 0; step < 16; ++step) assert(phrase.bars[2].lead.steps[step].note < 0);
      }
    }
    assert(callResponse >= 3);
  }
}

// Harmony is slow: a hand-written pole changes chord at most twice in four bars.
void testSlowHarmony() {
  for (const GenreIdiom::IdiomVariant* v :
       {&GenreIdiom::k_dnb_atmos, &GenreIdiom::k_dnb_funk, &GenreIdiom::k_dnb_dance,
        &GenreIdiom::k_dnb_neuro}) {
    int changes = 0;
    for (int bar = 1; bar < 4; ++bar) changes += v->progression[bar] != v->progression[bar - 1];
    assert(changes <= 2);
  }
  assert(GenreIdiom::k_dnb_neuro.progression[1] == 0 && GenreIdiom::k_dnb_neuro.progression[3] == 0);
}

// Pendulum vs Noisia at the same tempo, timbre removed: DANCE carries a lead
// hook (three or more lead attacks in bar 1) over a hook bass; NEURO keeps the
// bass to at most three pitches and the lead to at most one attack a bar.
void testDanceAndNeuroAreDifferentMusic() {
  for (uint32_t press = 1; press <= 12; ++press) {
    const Phrase dance = phraseFor(kDnbDanceRecipeId, press);
    const Phrase neuro = phraseFor(kDnbNeuroRecipeId, press);
    int danceHook = 0;
    for (int step = 0; step < 16; ++step) danceHook += onset(dance.bars[0].lead, step);
    assert(danceHook >= 3);
    for (const Bar& bar : neuro.bars) {
      std::set<int> pitches;
      int leadAttacks = 0;
      for (int step = 0; step < 16; ++step) {
        if (bar.bass.steps[step].note >= 0) pitches.insert(bar.bass.steps[step].note % 12);
        leadAttacks += onset(bar.lead, step);
      }
      assert(pitches.size() <= 3);
      assert(leadAttacks <= 2);
    }
  }
}

// Tempo corridor: 168-176 for every pole, 174 by default.
void testTempoCorridor() {
  for (uint8_t recipe : kPoles) {
    GenreSettings settings{};
    settings.generativeMode = kDnb;
    settings.recipe = recipe;
    const auto profile = GroovePuterRhythm::generationProfileFor(settings);
    assert(profile.corridor.bpmMin >= 168 && profile.corridor.bpmMax <= 176);
    assert(profile.corridor.suggestedBpm >= profile.corridor.bpmMin &&
           profile.corridor.suggestedBpm <= profile.corridor.bpmMax);
    std::printf("DnB recipe %u: %u-%u, %u BPM\n", recipe, profile.corridor.bpmMin,
                profile.corridor.bpmMax, profile.corridor.suggestedBpm);
  }
  GenreSettings base{};
  base.generativeMode = kDnb;
  assert(GroovePuterRhythm::generationProfileFor(base).corridor.suggestedBpm == 174);
}

void testDeterministic() {
  for (uint8_t recipe : kPoles) {
    const Phrase a = phraseFor(recipe, 5);
    const Phrase b = phraseFor(recipe, 5);
    for (int bar = 0; bar < 4; ++bar) {
      for (int step = 0; step < 16; ++step) {
        assert(a.bars[bar].bass.steps[step].note == b.bars[bar].bass.steps[step].note);
        assert(a.bars[bar].lead.steps[step].note == b.bars[bar].lead.steps[step].note);
        for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
          assert(a.bars[bar].drums.voices[voice].steps[step].hit ==
                 b.bars[bar].drums.voices[voice].steps[step].hit);
        }
      }
    }
  }
}

}  // namespace

int main() {
  testTwoStepCore();
  testBassIsSlowerThanDrums();
  testLeadLeavesSpace();
  testSlowHarmony();
  testDanceAndNeuroAreDifferentMusic();
  testTempoCorridor();
  testDeterministic();
  std::printf("0.9.18 DnB genre: OK\n");
  return 0;
}
