// 0.9.18 phrase idea variability: G x 8 per genre must give different phrase
// ideas (not different random notes) while every result keeps the genre's
// template, key and register. Drives GenreIdiom directly with the salts the
// migration derives from the generation stream (pattern address + attempt).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

#include "../src/generation/idiom/genre_idiom.h"

bool SynthPattern::isEmpty() const {
  for (const auto& step : steps) {
    if (step.note >= 0) return false;
  }
  return true;
}

namespace {

using GenreIdiom::Ending;
using GenreIdiom::Idea;
using GenreIdiom::IdeaPlan;

constexpr int kAddress = 7;
constexpr uint8_t kDorian = 2;
constexpr uint8_t kRoot = 0;

struct Genre {
  uint8_t mode;
  const char* name;
};
constexpr Genre kGenres[] = {
    {0, "Acid"},      {9, "House"},    {13, "UKG"},    {11, "HipHop"},
    {15, "LoFi"},     {1, "Outrun"},   {2, "Darksynth"}, {10, "Techno"},
    {12, "FunkSoul"}, {3, "Electro"},  {7, "Broken"},  {14, "DnB"},
    {8, "Chip"}};

struct Phrase {
  SynthPattern bass[4];
  SynthPattern lead[4];
  IdeaPlan plan;
};

uint32_t saltFor(uint32_t press) {
  // As strong_rhythm_migration.cpp: patternAddress * 131 + attempt * 977.
  return static_cast<uint32_t>(kAddress) * 131u + press * 977u;
}

GenreIdiom::Request requestFor(uint8_t mode, uint32_t press, uint8_t liveliness) {
  GenreIdiom::Request request{};
  request.generativeMode = mode;
  request.recipe = 0;
  request.rootPitchClass = kRoot;
  request.scale = kDorian;
  // As the migration: CALM takes the sparse P3, otherwise P2 (default STYLE).
  request.level = liveliness == 0 ? 2 : 1;
  request.liveliness = liveliness;
  request.salt = saltFor(press);
  request.press = press;
  request.deckSeed = kAddress;
  return request;
}

Phrase phraseFor(uint8_t mode, uint32_t press, uint8_t liveliness = 1) {
  Phrase phrase{};
  GenreIdiom::Request request = requestFor(mode, press, liveliness);
  for (uint8_t bar = 0; bar < 4; ++bar) {
    request.barOrdinal = bar;
    IdeaPlan plan{};
    assert(GenreIdiom::apply(request, phrase.bass[bar], phrase.lead[bar], &plan));
    if (bar == 0) {
      phrase.plan = plan;
    } else {
      // T2: one plan for the whole phrase, whatever the bar.
      assert(plan.idea == phrase.plan.idea &&
             plan.interactionShape == phrase.plan.interactionShape &&
             plan.ending == phrase.plan.ending &&
             plan.substituteFrom == phrase.plan.substituteFrom);
    }
  }
  return phrase;
}

// Onsets only: a held note (same pitch, slid into) is not a new attack.
bool isOnset(const SynthPattern& pattern, int step) {
  const SynthStep& event = pattern.steps[step];
  if (event.note < 0) return false;
  if (step > 0 && event.slide && pattern.steps[step - 1].note == event.note) return false;
  return true;
}

// Structural signature of one part over the phrase: onset mask per bar,
// pitch contour (up/down/same between onsets) and register (mean pitch, in
// thirds) per bar. Velocity, accents and slides are left out on purpose.
std::string partSignature(const SynthPattern (&bars)[4]) {
  std::string signature;
  int previous = -1;
  for (const auto& bar : bars) {
    uint16_t mask = 0;
    int sum = 0;
    int count = 0;
    std::string contour;
    for (int step = 0; step < 16; ++step) {
      if (!isOnset(bar, step)) continue;
      mask |= 1u << step;
      const int pitch = bar.steps[step].note;
      sum += pitch;
      ++count;
      if (previous >= 0) contour += pitch > previous ? '+' : pitch < previous ? '-' : '=';
      previous = pitch;
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%04X/%d/", mask, count ? sum / count / 4 : -1);
    signature += buffer + contour + "|";
  }
  return signature;
}

std::string signatureFor(const Phrase& phrase) {
  return std::string(GenreIdiom::ideaName(phrase.plan.idea)) + "#" +
         std::to_string(static_cast<int>(phrase.plan.ending)) + "#" +
         partSignature(phrase.bass) + "#" + partSignature(phrase.lead);
}

int onsetCount(const Phrase& phrase) {
  int count = 0;
  for (int bar = 0; bar < 4; ++bar) {
    for (int step = 0; step < 16; ++step) {
      count += isOnset(phrase.bass[bar], step) + isOnset(phrase.lead[bar], step);
    }
  }
  return count;
}

uint16_t onsetMask(const SynthPattern& pattern) {
  uint16_t mask = 0;
  for (int step = 0; step < 16; ++step) {
    if (isOnset(pattern, step)) mask |= static_cast<uint16_t>(1u << step);
  }
  return mask;
}

// Characterization: the existing REPEAT topology is A | A' | A | B.
// This catches a production change that moves the return away from bar 3,
// changes the paired A rhythm, or stops bar 4 from closing the phrase.
void testOriginalPhraseTopologyBaseline() {
  // Characterize the existing repeat topology across the five target genres.
  for (std::size_t genreIndex = 0; genreIndex < 5; ++genreIndex) {
    const Genre& genre = kGenres[genreIndex];
    uint32_t originalPress = 0;
    for (uint32_t press = 1; press <= 32; ++press) {
      if (GenreIdiom::ideaFromDeck(genre.mode, 1, kAddress, press) == Idea::Original) {
        originalPress = press;
        break;
      }
    }
    assert(originalPress != 0);
    const Phrase phrase = phraseFor(genre.mode, originalPress);
    assert(phrase.plan.idea == Idea::Original);

    // Bass and upper voice share the same A / A' / A placement decision.
    assert(onsetMask(phrase.bass[0]) == onsetMask(phrase.bass[2]));
    assert(onsetMask(phrase.lead[0]) == onsetMask(phrase.lead[2]));
    assert(onsetMask(phrase.bass[0]) == onsetMask(phrase.bass[1]));
    assert(onsetMask(phrase.lead[0]) == onsetMask(phrase.lead[1]));

    // B carries the genre-aware cadence; the paired bass remains active.
    assert(phrase.lead[3].steps[12].note >= 0);
    assert(onsetMask(phrase.bass[3]) != 0);
    std::printf("%s A A' A B baseline: OK\n", genre.name);
  }
}

// Regression target for the new CALL_RESPONSE shape: bar 3 is an owned
// structural rest. This catches the old half-bar cut and any later filler.
void testCallResponseProtectsTheWholeThirdBar() {
  uint32_t callResponsePress = 0;
  for (uint32_t press = 1; press <= 32; ++press) {
    if (GenreIdiom::ideaFromDeck(13, 1, kAddress, press) == Idea::CallResponse) {
      callResponsePress = press;
      break;
    }
  }
  assert(callResponsePress != 0);
  GenreIdiom::Request request = requestFor(13, callResponsePress, 1);
  request.phraseBars = 4;
  request.barOrdinal = 2;
  SynthPattern bass{};
  SynthPattern lead{};
  assert(GenreIdiom::apply(request, bass, lead));
  assert(bass.isEmpty());
  assert(lead.isEmpty());
}

void testShortCallAndDelayedResponseOwnTheirSpace() {
  uint32_t shortPress = 0;
  uint32_t delayedPress = 0;
  for (uint32_t press = 1; press <= 32; ++press) {
    const Idea idea = GenreIdiom::ideaFromDeck(13, 1, kAddress, press);
    if (idea == Idea::ReducedMotif && shortPress == 0) shortPress = press;
    if (idea == Idea::DelayedAnswer && delayedPress == 0) delayedPress = press;
  }
  assert(shortPress != 0 && delayedPress != 0);

  GenreIdiom::Request request = requestFor(13, shortPress, 1);
  request.phraseBars = 4;
  request.barOrdinal = 1;
  SynthPattern bass{};
  SynthPattern lead{};
  GenreIdiom::IdeaPlan plan{};
  assert(GenreIdiom::apply(request, bass, lead, &plan));
  assert(plan.interactionShape == GenreIdiom::InteractionShape::ShortCall);
  assert(bass.isEmpty() && lead.isEmpty());
  request.barOrdinal = 2;
  assert(GenreIdiom::apply(request, bass, lead));
  assert(!bass.isEmpty() || !lead.isEmpty());

  request = requestFor(13, delayedPress, 1);
  request.phraseBars = 4;
  request.barOrdinal = 2;
  assert(GenreIdiom::apply(request, bass, lead, &plan));
  assert(plan.interactionShape == GenreIdiom::InteractionShape::DelayedResponse);
  bool lateMaterial = false;
  for (int step = 0; step < 16; ++step) {
    if (step < 8) {
      assert(bass.steps[step].note < 0 && lead.steps[step].note < 0);
    } else if (bass.steps[step].note >= 0 || lead.steps[step].note >= 0) {
      lateMaterial = true;
    }
  }
  assert(lateMaterial);  // The allowed pickup region still uses template notes.
}

std::string noteName(int note) {
  static const char* kNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return std::string(kNames[note % 12]) + std::to_string(note / 12 - 1);
}

void printPhrase(const char* genre, uint32_t press, const Phrase& phrase) {
  std::printf("%s G%u %s ending=%d", genre, press, GenreIdiom::ideaName(phrase.plan.idea),
              static_cast<int>(phrase.plan.ending));
  if (phrase.plan.secondary != GenreIdiom::Secondary::None) {
    std::printf(" secondary=%d", static_cast<int>(phrase.plan.secondary));
  }
  std::printf("\n");
  for (int part = 0; part < 2; ++part) {
    std::printf("   %s", part ? "B" : "A");
    for (int bar = 0; bar < 4; ++bar) {
      const SynthPattern& pattern = part ? phrase.lead[bar] : phrase.bass[bar];
      std::printf(" |");
      for (int step = 0; step < 16; ++step) {
        if (isOnset(pattern, step)) std::printf(" %d:%s", step, noteName(pattern.steps[step].note).c_str());
      }
    }
    std::printf("\n");
  }
}

// T1 + acceptance: 8 presses, >= 6 structurally distinct phrases per genre.
void testEightPressesGiveDistinctIdeas() {
  int total = 0;
  for (const Genre& genre : kGenres) {
    std::set<std::string> signatures;
    for (uint32_t press = 1; press <= 8; ++press) {
      const Phrase phrase = phraseFor(genre.mode, press);
      const Phrase again = phraseFor(genre.mode, press);
      assert(signatureFor(phrase) == signatureFor(again));  // T1
      for (int bar = 0; bar < 4; ++bar) {
        for (int step = 0; step < 16; ++step) {
          assert(phrase.bass[bar].steps[step].note == again.bass[bar].steps[step].note);
          assert(phrase.lead[bar].steps[step].note == again.lead[bar].steps[step].note);
        }
      }
      signatures.insert(signatureFor(phrase));
      printPhrase(genre.name, press, phrase);
    }
    std::printf("%s distinct signatures: %zu/8\n", genre.name, signatures.size());
    assert(signatures.size() >= 6);
    total += static_cast<int>(signatures.size());
  }
  std::printf("DISTINCT_SIGNATURE_COUNT total %d/%zu\n", total,
              8 * (sizeof(kGenres) / sizeof(kGenres[0])));
}

// T3 + T6: every note is in the project scale or is a template note mapped
// onto it, and in range after the key/mode projection.
void testPitchSetAndRange() {
  bool inScale[12] = {};
  const auto definition = GroovePuterRhythm::scaleDefinitionFor(kDorian);
  for (uint8_t i = 0; i < definition.count; ++i) inScale[(kRoot + definition.intervals[i]) % 12] = true;
  for (const Genre& genre : kGenres) {
    for (uint32_t press = 1; press <= 32; ++press) {
      for (uint8_t liveliness = 0; liveliness < 3; ++liveliness) {
        GenreIdiom::Request request = requestFor(genre.mode, press, liveliness);
        const GenreIdiom::IdiomVariant* variant =
            GenreIdiom::variantFor(genre.mode, 0, GenreIdiom::mix(request.salt) >> 7);
        const GenreIdiom::IdiomLevel& level = variant->levels[request.level];
        bool allowed[12] = {};
        for (int pc = 0; pc < 12; ++pc) allowed[pc] = inScale[pc];
        auto allow = [&](int semi) {
          for (int shift = -3; shift <= 3; ++shift) {
            const int moved = GenreIdiom::detail::transposeDegrees(semi, shift);
            allowed[(((GenreIdiom::detail::mapSemi(moved, kDorian) + kRoot) % 12) + 12) % 12] = true;
          }
        };
        for (uint8_t i = 0; i < level.bassCount; ++i) allow(level.bass[i].semi);
        for (uint8_t i = 0; i < level.melodyCount; ++i) allow(level.melody[i].semi);
        for (uint8_t i = 0; i < level.stabCount; ++i) {
          for (int interval = 0; interval < 24; ++interval) {
            if (level.stabs[i].intervalMask & (1u << interval)) allow(level.stabs[i].root + interval);
          }
        }
        const Phrase phrase = phraseFor(genre.mode, press, liveliness);
        for (int bar = 0; bar < 4; ++bar) {
          for (const SynthPattern* pattern : {&phrase.bass[bar], &phrase.lead[bar]}) {
            for (const SynthStep& step : pattern->steps) {
              if (step.note < 0) continue;
              assert(step.note >= 24 && step.note <= 96);
              assert(allowed[step.note % 12]);
            }
          }
          for (const SynthStep& step : phrase.bass[bar].steps) {
            if (step.note >= 0) assert(step.note <= 64);  // the bass stays a bass
          }
        }
      }
    }
  }
}

// T4: Acid stays a monophonic 303 line whose slides always come from a
// sounding note.
void testAcidSlideTopology() {
  for (uint32_t press = 1; press <= 32; ++press) {
    const Phrase phrase = phraseFor(0, press);
    for (int bar = 0; bar < 4; ++bar) {
      int notes = 0;
      for (int step = 0; step < 16; ++step) {
        const SynthStep& event = phrase.bass[bar].steps[step];
        if (event.note < 0) continue;
        ++notes;
        if (event.slide) assert(step > 0 && phrase.bass[bar].steps[step - 1].note >= 0);
      }
      assert(notes >= 6);  // the running line survives every idea
    }
  }
}

// T5: CALM does not systematically play more than LIVELY.
void testCalmIsNotBusierThanLively() {
  for (const Genre& genre : kGenres) {
    int calm = 0;
    int lively = 0;
    for (uint32_t press = 1; press <= 16; ++press) {
      calm += onsetCount(phraseFor(genre.mode, press, 0));
      lively += onsetCount(phraseFor(genre.mode, press, 2));
    }
    std::printf("%s onsets over 16 presses: CALM %d LIVELY %d\n", genre.name, calm, lively);
    assert(calm <= lively);
  }
}

// T7: bar 4 answers: the lead ends on the tonic or the fifth, or turns back
// into bar 1 (RUN UP).
void testBarFourAnswers() {
  for (const Genre& genre : kGenres) {
    for (uint32_t press = 1; press <= 32; ++press) {
      const Phrase phrase = phraseFor(genre.mode, press);
      int last = -1;
      for (int step = 0; step < 16; ++step) {
        if (phrase.lead[3].steps[step].note >= 0) last = phrase.lead[3].steps[step].note;
      }
      assert(last >= 0);
      const int degree = ((last - kRoot) % 12 + 12) % 12;
      const bool home = degree == 0 || degree == 7;
      assert(home || phrase.plan.ending == Ending::RunUp);
    }
  }
}

// T8: no idea dominates: over 64 presses every genre uses at least six ideas,
// none more than 40% of the time, and 8 presses never repeat one idea 5 times.
void testNoDominantIdea() {
  for (const Genre& genre : kGenres) {
    for (uint8_t liveliness = 0; liveliness < 3; ++liveliness) {
      std::map<int, int> histogram;
      for (uint32_t press = 1; press <= 64; ++press) {
        ++histogram[static_cast<int>(phraseFor(genre.mode, press, liveliness).plan.idea)];
      }
      assert(histogram.size() >= 6);
      for (const auto& entry : histogram) assert(entry.second <= 64 * 40 / 100);
      std::map<int, int> eight;
      for (uint32_t press = 1; press <= 8; ++press) {
        ++eight[static_cast<int>(phraseFor(genre.mode, press, liveliness).plan.idea)];
      }
      for (const auto& entry : eight) assert(entry.second <= 4);
    }
  }
}

// G on STEPS writes one looping bar: the idea still shows there (a lifted,
// thinned, halved or late bar), so repeated presses are not the same loop.
void testSingleBarPressesDiffer() {
  for (const Genre& genre : kGenres) {
    std::set<std::string> loops;
    for (uint32_t press = 1; press <= 8; ++press) {
      GenreIdiom::Request request = requestFor(genre.mode, press, 1);
      SynthPattern bass[4]{};
      SynthPattern lead[4]{};
      assert(GenreIdiom::apply(request, bass[0], lead[0]));
      bass[3] = bass[2] = bass[1] = bass[0];
      lead[3] = lead[2] = lead[1] = lead[0];
      loops.insert(partSignature(bass) + "#" + partSignature(lead));
    }
    std::printf("%s single-bar loops: %zu/8\n", genre.name, loops.size());
    assert(loops.size() >= 5);
  }
}

}  // namespace

int main() {
  testOriginalPhraseTopologyBaseline();
  testCallResponseProtectsTheWholeThirdBar();
  testShortCallAndDelayedResponseOwnTheirSpace();
  testEightPressesGiveDistinctIdeas();
  testPitchSetAndRange();
  testAcidSlideTopology();
  testCalmIsNotBusierThanLively();
  testBarFourAnswers();
  testNoDominantIdea();
  testSingleBarPressesDiffer();
  std::printf("0.9.18 idea variability: OK\n");
  return 0;
}
