#ifndef GROOVEPUTER_GENERATION_IDIOM_GENRE_IDIOM_H
#define GROOVEPUTER_GENERATION_IDIOM_GENRE_IDIOM_H

#include <cstdint>
#include <cstdlib>

#include "../../../scenes.h"
#include "../tonal/scale_catalog.h"
#include "genre_idiom_tables.h"

// 0.9.18 genre idioms (prototype: Acid, LoFi, HipHop, UK garage, House).
//
// Bass (Synth A) and lead (Synth B) come from one template, so they interlock
// by construction. Synth B plays the chord stabs as one moving top voice (the
// internal synth is mono) with the template's melody notes as its peaks.
// Over a phrase the bars follow A A' A B: A' keeps the rhythm and moves the
// voice to another chord tone, B changes the ending and lands on the tonic.
// Templates are in C minor and are mapped degree by degree onto the project
// key and scale. Research: docs/superpowers/plans/2026-10-08-0918-genre-idioms-research.md
namespace GenreIdiom {

constexpr uint8_t kNoBarOrdinal = 0xFF;

struct Request {
  uint8_t generativeMode = 0;
  uint8_t recipe = 0;
  uint8_t rootPitchClass = 0;
  uint8_t scale = GroovePuterRhythm::kScaleMinor;
  uint8_t level = 1;  // template level 0..2 (P1, P2, P3)
  uint8_t barOrdinal = kNoBarOrdinal;
  uint8_t liveliness = 1;  // 0 CALM, 1 NORMAL, 2 LIVELY
  uint8_t phraseBars = 0;  // 0 = unknown (read as 4); 2 plays A B
  uint32_t salt = 0;
  uint32_t press = 0;     // successive presses walk the idea deck
  uint32_t deckSeed = 0;  // the deck's order (stable for one slot / one Melody)
};

namespace detail {

inline int floorDiv12(int value) {
  return value >= 0 ? value / 12 : -((11 - value) / 12);
}

// A C-minor semitone onto the project scale: same degree, same octave; a
// chromatic note keeps its offset from the degree below it.
inline int mapSemi(int semi, uint8_t scale) {
  static constexpr int8_t kMinorDegree[12] = {0, -1, 1, 2, -1, 3, -1, 4, 5, -1, 6, -1};
  const auto definition = GroovePuterRhythm::scaleDefinitionFor(scale);
  if (definition.intervals == nullptr || definition.count >= 12) return semi;
  const int octave = floorDiv12(semi);
  int pc = semi - octave * 12;
  int chroma = 0;
  while (kMinorDegree[pc] < 0) {
    --pc;
    ++chroma;
  }
  const int degree = kMinorDegree[pc];
  int mapped = 0;
  if (definition.count == 7) {
    mapped = definition.intervals[degree];
  } else {
    // Pentatonic: the scale tone nearest to the minor degree.
    static constexpr int8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    int best = 99;
    for (uint8_t i = 0; i < definition.count; ++i) {
      const int distance = std::abs(definition.intervals[i] - kMinor[degree]);
      if (distance < best) {
        best = distance;
        mapped = definition.intervals[i];
      }
    }
  }
  return octave * 12 + mapped + chroma;
}

// Moves a C-minor template semitone by whole scale degrees (a chromatic note
// keeps its offset from the degree below it), so a bar follows its chord.
inline int transposeDegrees(int semi, int steps) {
  if (steps == 0) return semi;
  static constexpr int8_t kMinorDegree[12] = {0, -1, 1, 2, -1, 3, -1, 4, 5, -1, 6, -1};
  static constexpr int8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
  int octave = floorDiv12(semi);
  int pc = semi - octave * 12;
  int chroma = 0;
  while (kMinorDegree[pc] < 0) {
    --pc;
    ++chroma;
  }
  int degree = kMinorDegree[pc] + steps;
  while (degree < 0) {
    degree += 7;
    --octave;
  }
  octave += degree / 7;
  degree %= 7;
  return octave * 12 + kMinor[degree] + chroma;
}

inline void clear(SynthPattern& pattern) {
  for (auto& step : pattern.steps) step = SynthStep{};
}

inline void place(SynthPattern& pattern, uint8_t step, int pitch, uint8_t len,
                  uint8_t velocity, bool accent) {
  if (step >= SynthPattern::kSteps || pitch < 0 || pitch > 127) return;
  SynthStep note{};
  note.note = static_cast<int8_t>(pitch);
  note.velocity = velocity;
  note.accent = accent;
  pattern.steps[step] = note;
  for (uint8_t k = 1; k < len && step + k < SynthPattern::kSteps; ++k) {
    if (pattern.steps[step + k].note >= 0) break;
    SynthStep tie = note;
    tie.accent = false;
    tie.slide = true;  // a held note: same pitch, slid into
    pattern.steps[step + k] = tie;
  }
}

// The dataset marks a slide on the note it leaves (303); here the slide flag
// sits on the note it reaches, and the gap between them is held.
inline void connectSlides(SynthPattern& pattern, const uint8_t (&slideOut)[16]) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (!slideOut[step] || pattern.steps[step].note < 0) continue;
    int next = step + 1;
    while (next < SynthPattern::kSteps &&
           (pattern.steps[next].note < 0 ||
            (pattern.steps[next].slide && pattern.steps[next].note == pattern.steps[step].note))) {
      ++next;
    }
    if (next >= SynthPattern::kSteps) break;
    for (int gap = step + 1; gap < next; ++gap) {
      SynthStep tie = pattern.steps[step];
      tie.accent = false;
      tie.slide = true;
      pattern.steps[gap] = tie;
    }
    pattern.steps[next].slide = true;
  }
}

}  // namespace detail

// The template for a genre/recipe, or nullptr where the genre keeps the
// regular generator.
inline const IdiomVariant* variantFor(uint8_t generativeMode, uint8_t recipe,
                                      uint32_t salt) {
  switch (generativeMode) {
    case 0:  // Acid
      if (recipe != 0) return nullptr;
      return (salt & 1u) ? &k_acid_rolling : &k_acid_chicago_jack;
    case 1:  // Outrun
      return recipe == 0 ? &k_outrun_drive : nullptr;
    case 2:  // Darksynth
      return recipe == 0 ? &k_darksynth_drive : nullptr;
    case 3:  // Electro
      return recipe == 0 ? &k_electro_machine : nullptr;
    case 7:  // Broken
      return recipe == 0 ? &k_broken_bruk : nullptr;
    case 8:  // Chip
      return recipe == 0 ? &k_chip_arp : nullptr;
    case 10:  // Techno: dub-techno stab and repetition
      if (recipe != 0) return nullptr;
      return (salt & 1u) ? &k_dub_minimal_space : &k_dub_deep_chord;
    case 12:  // FunkSoul
      return recipe == 0 ? &k_funk_pocket : nullptr;
    case 14:  // DnB: four poles on one two-step core; BASE plays the dance pole.
      switch (recipe) {
        case 0: return &k_dnb_dance;
        case 18: return &k_dnb_atmos;
        case 19: return &k_dnb_funk;
        case 20: return &k_dnb_dance;
        case 21: return &k_dnb_neuro;
        default: return nullptr;
      }
    case 9:  // House
      return recipe == 0 ? &k_house_deep_offbeat : nullptr;
    case 11:  // HipHop
      if (recipe == 0 || recipe == 16) return &k_boombap_golden_era;
      if (recipe == 17) return &k_boombap_dusty_jazz;
      return nullptr;
    case 13:  // UK garage
      if (recipe != 0) return nullptr;
      return (salt & 1u) ? &k_ukg_dark_skippy : &k_ukg_classic_2step;
    case 15:  // LoFi
      if (recipe == 0 || recipe == 12) return &k_lofi_classic_chill;
      if (recipe == 13) return &k_lofi_drunken;
      return nullptr;
    default:
      return nullptr;
  }
}

// ---- Phrase ideas -------------------------------------------------------
//
// A press picks one IdeaPlan for the whole phrase before any bar is written;
// bass and lead read the same plan. The plan is a musical strategy over the
// same template (one primary idea, at most one secondary touch), not new
// notes: the reference corpus (docs/midi, Hotline Miami OST) varies a 4-bar
// group by repetition, a lifted or dropped register, shortening and silence,
// and keeps the bass rhythm whole.

enum class Idea : uint8_t {
  Original = 0,       // A A' A B
  LowHighAnswer,      // bar 2 lifted an octave, back down to close
  MotifSubstitution,  // one template degree swapped for another, phrase-wide
  CallResponse,       // bar 3 cut to its first half, bar 4 answers
  RegisterArc,        // a directed rise or dip over the four bars, then home
  DelayedAnswer,      // bar 4 opens with silence, the answer comes late
  Turnaround,         // bar 4 turns back into bar 1
  ReducedMotif,       // bar 3 thinned to its strong steps, bar 4 minimal
  Count,
};

enum class Ending : uint8_t {
  Tonic = 0,   // rings on the tonic
  Fifth,       // rings on the fifth
  RunUp,       // scale run into the next bar's first note
  Late,        // silence, then the tonic late in the bar
  Minimal,     // a single tonic, bass on the downbeat only
};

enum class Secondary : uint8_t {
  None = 0,
  LeadStart,     // the line starts from another chord tone
  PrimeDown,     // A' moves the line down instead of up
  FifthEnding,   // the answer lands on the fifth
};

struct IdeaPlan {
  Idea idea = Idea::Original;
  Secondary secondary = Secondary::None;
  uint8_t leadStart = 0;
  int8_t primeShift = 1;
  Ending ending = Ending::Tonic;
  bool arcRises = true;
  int8_t substituteFrom = -1;  // pitch class (C-minor template space)
  int8_t substituteTo = -1;
};

inline const char* ideaName(Idea idea) {
  switch (idea) {
    case Idea::Original: return "ORIGINAL";
    case Idea::LowHighAnswer: return "LOW/HIGH ANSWER";
    case Idea::MotifSubstitution: return "MOTIF SUBSTITUTION";
    case Idea::CallResponse: return "CALL/RESPONSE";
    case Idea::RegisterArc: return "REGISTER ARC";
    case Idea::DelayedAnswer: return "DELAYED ANSWER";
    case Idea::Turnaround: return "TURNAROUND";
    case Idea::ReducedMotif: return "REDUCED MOTIF";
    case Idea::Count: break;
  }
  return "?";
}

inline uint32_t mix(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7FEB352Du;
  value ^= value >> 15;
  value *= 0x846CA68Bu;
  value ^= value >> 16;
  return value;
}

namespace detail {

inline uint32_t field(uint32_t salt, uint32_t index) {
  return mix(salt * 0x9E3779B1u + index * 0x85EBCA77u + 1u);
}

// Idea weights by genre character and LIVELY (CALM leans to space and
// reduction, LIVELY to motion and turnarounds; NORMAL is even). Acid keeps its
// running line: a delayed answer is rare there; UKG, LoFi and HipHop favour
// space; House avoids register arcs that would turn stabs into a moving lead.
inline uint8_t ideaWeight(uint8_t generativeMode, uint8_t liveliness, Idea idea) {
  static constexpr uint8_t kBase[3][8] = {
      // ORIG LOWHI SUBST CALL ARC DELAY TURN REDUCE
      {3, 2, 3, 4, 2, 5, 2, 6},   // CALM
      {4, 4, 4, 4, 4, 4, 4, 4},   // NORMAL
      {3, 6, 4, 3, 6, 2, 6, 2},   // LIVELY
  };
  uint8_t weight = kBase[liveliness > 2 ? 2 : liveliness][static_cast<uint8_t>(idea)];
  const bool spacious = generativeMode == 11 || generativeMode == 13 || generativeMode == 15;
  if (generativeMode == 0 && idea == Idea::DelayedAnswer) weight = 1;
  if (spacious && (idea == Idea::DelayedAnswer || idea == Idea::CallResponse)) weight += 2;
  if (generativeMode == 9 && idea == Idea::RegisterArc) weight = weight > 2 ? weight - 2 : 1;
  // DnB: the lead needs silence, so call and response leads the deck.
  if (generativeMode == 14 && idea == Idea::CallResponse) weight += 4;
  if (generativeMode == 14 && idea == Idea::DelayedAnswer) weight += 2;
  return weight;
}

}  // namespace detail

// Pitch classes (C-minor template space) the level actually uses: a motif
// substitution only swaps one of them for another, so it never leaves the
// template's material.
inline uint16_t templatePitchClasses(const IdiomLevel& level, bool bassPart) {
  uint16_t mask = 0;
  const IdiomNote* notes = bassPart ? level.bass : level.melody;
  const uint8_t count = bassPart ? level.bassCount : level.melodyCount;
  for (uint8_t i = 0; i < count; ++i) mask |= 1u << (((notes[i].semi % 12) + 12) % 12);
  return mask;
}

// Successive presses deal from a deck of ten: every idea once plus two
// extra picks by weight (so CALM/LIVELY and the genre lean the deck), in a
// weighted order that never deals the same idea twice in a row. Independent
// draws repeated one idea three times in eight presses.
inline Idea ideaFromDeck(uint8_t generativeMode, uint8_t liveliness, uint32_t deckSeed,
                         uint32_t press, bool* repeatOut = nullptr) {
  constexpr uint8_t kIdeas = static_cast<uint8_t>(Idea::Count);
  constexpr uint8_t kDeck = kIdeas + 2;
  const uint32_t cycle = press / kDeck;
  const uint32_t position = press % kDeck;
  uint32_t seed = mix(deckSeed * 0x9E3779B1u + cycle * 0x85EBCA77u +
                      generativeMode * 0xC2B2AE3Du + liveliness * 0x27D4EB2Fu + 7u);
  auto next = [&]() {
    seed = mix(seed + 0x9E3779B9u);
    return seed;
  };
  auto weightOf = [&](uint8_t idea) {
    return detail::ideaWeight(generativeMode, liveliness, static_cast<Idea>(idea));
  };
  uint8_t items[kDeck];
  for (uint8_t i = 0; i < kIdeas; ++i) items[i] = i;
  uint32_t total = 0;
  for (uint8_t i = 0; i < kIdeas; ++i) total += weightOf(i);
  for (uint8_t extra = kIdeas; extra < kDeck; ++extra) {
    uint32_t pick = next() % total;
    uint8_t idea = 0;
    while (pick >= weightOf(idea)) pick -= weightOf(idea++);
    items[extra] = idea;
  }
  uint8_t order[kDeck];
  bool used[kDeck] = {};
  int last = -1;
  for (uint8_t slot = 0; slot < kDeck; ++slot) {
    uint32_t sum = 0;
    bool alternative = false;
    for (uint8_t i = 0; i < kDeck; ++i) {
      if (!used[i] && items[i] != last) alternative = true;
    }
    for (uint8_t i = 0; i < kDeck; ++i) {
      if (!used[i] && (!alternative || items[i] != last)) sum += weightOf(items[i]);
    }
    uint32_t pick = next() % sum;
    for (uint8_t i = 0; i < kDeck; ++i) {
      if (used[i] || (alternative && items[i] == last)) continue;
      if (pick < weightOf(items[i])) {
        used[i] = true;
        order[slot] = items[i];
        last = items[i];
        break;
      }
      pick -= weightOf(items[i]);
    }
  }
  if (repeatOut != nullptr) {
    *repeatOut = false;
    for (uint32_t earlier = 0; earlier < position; ++earlier) {
      if (order[earlier] == order[position]) *repeatOut = true;
    }
  }
  return static_cast<Idea>(order[position]);
}

inline IdeaPlan ideaPlanFor(const Request& request, const IdiomLevel& level) {
  IdeaPlan plan{};
  bool repeat = false;
  plan.idea = ideaFromDeck(request.generativeMode, request.liveliness, request.deckSeed,
                           request.press, &repeat);
  plan.arcRises = (detail::field(request.salt, 2) % 2u) == 0;

  // At most one secondary touch.
  const uint32_t secondary = detail::field(request.salt, 3) % 6u;
  plan.secondary = secondary < 3 ? static_cast<Secondary>(secondary + 1) : Secondary::None;
  if (plan.secondary == Secondary::LeadStart) {
    plan.leadStart = static_cast<uint8_t>(1 + detail::field(request.salt, 4) % 3u);
  }
  if (plan.secondary == Secondary::PrimeDown) plan.primeShift = -1;

  switch (plan.idea) {
    case Idea::DelayedAnswer: plan.ending = Ending::Late; break;
    case Idea::Turnaround: plan.ending = Ending::RunUp; break;
    // A running 303 line is never cut to one note: it reduces bar 3 only.
    case Idea::ReducedMotif:
      plan.ending = level.bassCount >= 10 ? Ending::Tonic : Ending::Minimal;
      break;
    default:
      plan.ending = plan.secondary == Secondary::FifthEnding ? Ending::Fifth : Ending::Tonic;
      break;
  }

  // A fifth ending means nothing to an idea with its own ending: it starts
  // the line from another chord tone instead.
  if (plan.secondary == Secondary::FifthEnding && plan.ending != Ending::Tonic &&
      plan.ending != Ending::Fifth) {
    plan.secondary = Secondary::LeadStart;
    plan.leadStart = static_cast<uint8_t>(1 + detail::field(request.salt, 4) % 3u);
  }

  // The second deal of an idea in one deck plays it the other way round.
  if (repeat) {
    plan.primeShift = static_cast<int8_t>(-plan.primeShift);
    plan.leadStart = static_cast<uint8_t>((plan.leadStart + 2u) & 3u);
    plan.arcRises = !plan.arcRises;
  }

  if (plan.idea == Idea::MotifSubstitution) {
    // Swap one non-root degree of the bass for another degree the template
    // already uses (root stays the anchor).
    const uint16_t used = templatePitchClasses(level, true) | templatePitchClasses(level, false);
    int8_t from[12];
    int fromCount = 0;
    for (int pc = 1; pc < 12; ++pc) {
      if (templatePitchClasses(level, true) & (1u << pc)) from[fromCount++] = static_cast<int8_t>(pc);
    }
    int8_t to[12];
    int toCount = 0;
    for (int pc = 1; pc < 12; ++pc) {
      if (used & (1u << pc)) to[toCount++] = static_cast<int8_t>(pc);
    }
    if (fromCount > 0 && toCount > 1) {
      plan.substituteFrom = from[detail::field(request.salt, 5) % fromCount];
      for (int attempt = 0; attempt < toCount; ++attempt) {
        const int8_t candidate = to[(detail::field(request.salt, 6) + attempt) % toCount];
        if (candidate != plan.substituteFrom) {
          plan.substituteTo = candidate;
          break;
        }
      }
    }
    if (plan.substituteTo < 0) plan.idea = Idea::Original;
  }
  return plan;
}

// Per-bar shape of an idea: octave offsets, chord-tone rank shift of the
// lead, how much of the bar plays, and whether this is the answer bar.
struct BarShape {
  int bassOctave = 0;
  int leadRank = 0;
  uint8_t keepFrom = 0;   // first step that plays
  uint8_t keepTo = 16;    // steps from here on are silent (before the ending)
  bool strongOnly = false;
  bool answer = false;
  bool leadSilent = false;  // protected silence: the bass owns this bar
};

inline BarShape barShapeFor(const IdeaPlan& plan, int role, int bassBase) {
  BarShape shape{};
  shape.answer = role == 3;
  const int prime = plan.primeShift;
  switch (plan.idea) {
    case Idea::Original:
    case Idea::MotifSubstitution:
    case Idea::Turnaround:
    case Idea::DelayedAnswer:
      if (role == 1) shape.leadRank = prime;
      break;
    case Idea::LowHighAnswer:
      if (role == 1) {
        shape.bassOctave = 12;
        shape.leadRank = prime > 0 ? 2 : 1;
      }
      break;
    case Idea::CallResponse:
      if (role == 1) shape.leadRank = prime;
      if (role == 2) shape.keepTo = 8;
      break;
    case Idea::RegisterArc:
      if (plan.arcRises) {
        static constexpr int kRank[4] = {-1, 0, 1, 0};
        shape.leadRank = kRank[role];
        if (role == 2) shape.bassOctave = 12;
      } else {
        static constexpr int kRank[4] = {0, -1, 0, 0};
        shape.leadRank = kRank[role];
        if (role == 1 && bassBase - 12 >= 28) shape.bassOctave = -12;
      }
      break;
    case Idea::ReducedMotif:
      if (role == 1) shape.leadRank = prime;
      if (role == 2) shape.strongOnly = true;
      break;
    case Idea::Count:
      break;
  }
  return shape;
}

// Writes Synth A (bass) and Synth B (lead) for one bar of the phrase. False
// (and nothing written) when the genre has no idiom.
inline void applyDnbDrums(const Request& request, const IdeaPlan& plan, int role,
                          bool singleBar, DrumPatternSet& drums);

inline bool apply(const Request& request, SynthPattern& bass, SynthPattern& lead,
                  IdeaPlan* planOut = nullptr, DrumPatternSet* drums = nullptr) {
  const IdiomVariant* variant =
      variantFor(request.generativeMode, request.recipe, mix(request.salt) >> 7);
  if (variant == nullptr) return false;
  const IdiomLevel& level = variant->levels[request.level > 2 ? 2 : request.level];
  if (level.bass == nullptr) return false;
  const IdeaPlan plan = ideaPlanFor(request, level);
  if (planOut != nullptr) *planOut = plan;

  // A single looping bar (G on STEPS) has no phrase: the idea picks which of
  // its bars that loop plays, and no answer ending (it would loop).
  const bool singleBar = request.barOrdinal == kNoBarOrdinal;
  int role = singleBar ? 0 : request.barOrdinal % 4;
  // A two-bar phrase is statement and answer: A B.
  if (!singleBar && request.phraseBars == 2) role = (request.barOrdinal % 2) ? 3 : 0;
  if (singleBar) {
    switch (plan.idea) {
      case Idea::LowHighAnswer:
      case Idea::Turnaround: role = 1; break;
      case Idea::CallResponse:
      case Idea::ReducedMotif: role = 2; break;
      case Idea::RegisterArc: role = plan.arcRises ? 2 : 1; break;
      default: role = 0; break;
    }
  }
  const int root = request.rootPitchClass % 12;
  const int shift = root >= 6 ? root - 12 : root;  // keep registers near C
  const int bassBase = 36 + shift;
  const int leadBase = 60 + shift;
  // This bar's chord: a degree shift from the variant's progression, folded so
  // the line moves at most three degrees either way.
  int degreeShift = singleBar ? 0 : variant->progression[role];
  if (degreeShift > 3) degreeShift -= 7;
  if (degreeShift < -3) degreeShift += 7;
  auto map = [&](int semi) {
    return detail::mapSemi(detail::transposeDegrees(semi, degreeShift), request.scale);
  };
  auto mapHome = [&](int semi) { return detail::mapSemi(semi, request.scale); };
  auto substitute = [&](int semi) {
    if (plan.substituteFrom < 0) return semi;
    const int pc = ((semi % 12) + 12) % 12;
    return pc == plan.substituteFrom ? semi - pc + plan.substituteTo : semi;
  };
  BarShape shape = barShapeFor(plan, role, bassBase);
  if (singleBar) {
    shape.answer = false;
    // Delayed answer as a loop: the lead enters a beat late.
    if (plan.idea == Idea::DelayedAnswer) shape.keepFrom = 4;
  }
  const bool acid = request.generativeMode == 0;
  const bool dnb = request.generativeMode == 14;
  // DnB: fast drums, never a sixteenth bass run, even in an answer.
  const bool runningBass = level.bassCount >= 8 && !dnb;
  // DnB call and response: A A' _ B -- the lead leaves bar 3 to the bass whole.
  if (dnb && plan.idea == Idea::CallResponse && role == 2 && !singleBar) {
    shape.keepTo = 16;
    shape.leadSilent = true;
  }
  // keepFrom delays only the lead; the bass keeps its downbeat.
  auto plays = [&](uint8_t step, uint8_t flags, bool leadPart = false) {
    if ((leadPart && step < shape.keepFrom) || step >= shape.keepTo) return false;
    if (shape.strongOnly && (flags & kAccent) == 0) {
      // Thinned to the beats; a running line keeps its eighths.
      if (step % (runningBass ? 2 : 4) != 0) return false;
    }
    return true;
  };

  // How bar 4 ends decides how much of it the template still plays.
  uint8_t answerFrom = 16;
  if (shape.answer) {
    switch (plan.ending) {
      case Ending::Tonic:
      case Ending::Fifth: answerFrom = 12; break;
      case Ending::RunUp: answerFrom = 12; break;
      case Ending::Late: answerFrom = acid ? 12 : 8; break;
      case Ending::Minimal: answerFrom = 4; break;
    }
  }

  // ---- Synth A
  detail::clear(bass);
  uint8_t slideOut[16] = {};
  for (uint8_t i = 0; i < level.bassCount; ++i) {
    const IdiomNote& note = level.bass[i];
    if (note.step >= answerFrom && !(plan.ending == Ending::Minimal && note.step == 0)) continue;
    if (plan.ending == Ending::Minimal && shape.answer && note.step != 0) continue;
    if (!plays(note.step, note.flags)) continue;
    const int semi = substitute(note.semi) + shape.bassOctave;
    detail::place(bass, note.step, bassBase + mapHome(semi), note.len, note.velocity,
                  (note.flags & kAccent) != 0);
    if (note.flags & kSlideOut) slideOut[note.step] = 1;
  }
  if (shape.answer) {
    switch (plan.ending) {
      case Ending::RunUp:
        if (runningBass) {
          // Octave run with a glide back into the downbeat.
          detail::place(bass, 12, bassBase + mapHome(0), 1, 104, true);
          detail::place(bass, 13, bassBase + mapHome(12), 1, 84, false);
          detail::place(bass, 14, bassBase + mapHome(0), 1, 92, false);
          detail::place(bass, 15, bassBase + mapHome(12), 1, 96, false);
          slideOut[14] = 1;
        } else {
          // Fifth, then b7 stepping up into the root of bar 1.
          detail::place(bass, 12, bassBase + mapHome(-5), 2, 90, false);
          detail::place(bass, 14, bassBase + mapHome(-2), 2, 86, false);
        }
        break;
      case Ending::Minimal:
        detail::place(bass, 14, bassBase + mapHome(-2), 2, 80, false);
        break;
      case Ending::Late:
        if (acid) {
          detail::place(bass, 12, bassBase + mapHome(0), 1, 100, true);
          detail::place(bass, 14, bassBase + mapHome(-2), 2, 88, false);
        } else {
          detail::place(bass, 12, bassBase + mapHome(0), 2, 92, false);
          detail::place(bass, 15, bassBase + mapHome(-2), 1, 80, false);
        }
        break;
      default:
        if (runningBass) {
          detail::place(bass, 12, bassBase + mapHome(0), 1, 104, true);
          detail::place(bass, 14, bassBase + mapHome(12), 1, 92, false);
          slideOut[12] = 1;
        } else {
          detail::place(bass, 14, bassBase + mapHome(plan.ending == Ending::Fifth ? 7 : -2), 2, 88, false);
        }
        break;
    }
  }
  detail::connectSlides(bass, slideOut);

  // ---- Synth B: stabs as one moving top voice, melody notes as its peaks.
  detail::clear(lead);
  static constexpr int kTargets[4] = {7, 10, 3, 12};
  // `previous` follows the unshifted line (as in A); a shifted bar plays that
  // same line one chord tone up or down, so its contour survives.
  int previous = leadBase + kTargets[plan.leadStart & 3u];
  int sounding = previous;
  const int lo = leadBase - 3;
  const int hi = leadBase + 19;
  bool inScale[12] = {};
  {
    static constexpr int8_t kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    for (int degree : kMinor) inScale[((map(degree) % 12) + 12) % 12] = true;
  }
  auto scaleStep = [&](int pitch, int direction) {
    if (direction == 0) return pitch;
    for (int next = pitch + direction; std::abs(next - pitch) <= 4; next += direction) {
      if (inScale[((next - leadBase) % 12 + 12) % 12]) return next;
    }
    return pitch;
  };
  int8_t melodyAt[16];
  for (auto& m : melodyAt) m = -1;
  for (uint8_t i = 0; i < level.melodyCount; ++i) melodyAt[level.melody[i].step] = static_cast<int8_t>(i);
  int firstPitch = -1;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (step >= answerFrom || shape.leadSilent) break;
    int placed = -1;
    if (melodyAt[step] >= 0) {
      const IdiomNote& note = level.melody[melodyAt[step]];
      if (!plays(step, note.flags, true)) continue;
      int pitch = leadBase + map(substitute(note.semi));
      for (int r = 0; r < std::abs(shape.leadRank); ++r) pitch = scaleStep(pitch, shape.leadRank > 0 ? 1 : -1);
      detail::place(lead, step, pitch, note.len, note.velocity, (note.flags & kAccent) != 0);
      placed = leadBase + map(substitute(note.semi));
      sounding = pitch;
    } else {
      for (uint8_t i = 0; i < level.stabCount; ++i) {
        const IdiomStab& stab = level.stabs[i];
        if (stab.step != step) continue;
        // Reduced bars keep the stabs on the beat grid only.
        if (shape.strongOnly && (step % 4) != 0 && step != level.stabs[0].step) continue;
        if (!plays(step, 0, true) && !shape.strongOnly) continue;
        bool tone[12] = {};
        for (int interval = 0; interval < 24; ++interval) {
          if ((stab.intervalMask & (1u << interval)) == 0) continue;
          tone[((map(stab.root + interval) % 12) + 12) % 12] = true;
        }
        int candidates[24];
        int count = 0;
        for (int pitch = lo; pitch <= hi && count < 24; ++pitch) {
          if (tone[((pitch - leadBase) % 12 + 12) % 12]) candidates[count++] = pitch;
        }
        int bestIndex = -1;
        int bestCost = 1 << 30;
        for (int c = 0; c < count; ++c) {
          const int cost = std::abs(candidates[c] - previous) + (candidates[c] == previous ? 3 : 0);
          if (cost < bestCost) {
            bestCost = cost;
            bestIndex = c;
          }
        }
        if (bestIndex >= 0) {
          int index = bestIndex + shape.leadRank;
          if (index >= count) index = count - 1;
          if (index < 0) index = 0;
          detail::place(lead, step, candidates[index], stab.len, stab.velocity, false);
          placed = candidates[bestIndex];
          sounding = candidates[index];
        }
      }
    }
    if (placed >= 0) {
      previous = placed;
      if (firstPitch < 0) firstPitch = placed;
    }
    (void)sounding;
  }
  if (shape.answer) {
    int tonic = leadBase;
    while (tonic + 12 <= previous + 6) tonic += 12;
    int fifth = tonic + mapHome(7);
    if (fifth > previous + 7) fifth -= 12;
    switch (plan.ending) {
      case Ending::Tonic: detail::place(lead, 12, tonic, 4, 92, true); break;
      case Ending::Fifth: detail::place(lead, 12, fifth, 4, 90, true); break;
      case Ending::Late: detail::place(lead, acid ? 13 : 11, tonic, acid ? 3 : 5, 94, true); break;
      case Ending::Minimal: detail::place(lead, 12, tonic, 4, 84, false); break;
      case Ending::RunUp: {
        // Neuro keeps the lead a rare stab: one pickup note, the bass turns.
        if (dnb && request.recipe == 21) {
          detail::place(lead, 14, tonic, 2, 90, true);
          break;
        }
        // A scale run that ends one step under bar 1's first note, so the
        // phrase turns back into its own beginning.
        const int target = firstPitch >= 0 ? firstPitch : tonic;
        int pitch = scaleStep(scaleStep(scaleStep(scaleStep(target, -1), -1), -1), -1);
        for (uint8_t step = 12; step < 16; ++step) {
          detail::place(lead, step, pitch, 1, static_cast<uint8_t>(74 + (step - 12) * 6), step == 15);
          pitch = scaleStep(pitch, 1);
        }
        break;
      }
    }
  }
  if (dnb && drums != nullptr) applyDnbDrums(request, plan, role, singleBar, *drums);
  return true;
}

// DnB drums: deterministic two-step anchors (kick 1 + 11, snare 5 + 13),
// eighth hats, and probabilistic ghosts on top -- anchors fixed, ghosts
// chance, never the reverse. The idea shapes the bar: a neuro push, a late
// second snare in A', a half-time bar 3 in a reduced phrase, a snare fill
// into bar 1.
inline void applyDnbDrums(const Request& request, const IdeaPlan& plan, int role,
                          bool singleBar, DrumPatternSet& drums) {
  // Voice order of DrumPatternSet (8 voices; the kit has no cymbal lane).
  enum : int { kKick = 0, kSnare, kClosedHat, kOpenHat, kMidTom, kHighTom, kRim, kClap };
  const bool neuro = request.recipe == 21;
  const bool atmos = request.recipe == 18;
  const bool funk = request.recipe == 19;
  const bool dance = !neuro && !atmos && !funk;
  for (auto& voice : drums.voices) {
    for (auto& step : voice.steps) step = DrumStep{};
  }
  auto hit = [&](int voice, int step, uint8_t velocity, uint8_t probability = 100,
                 bool accent = false) {
    if (voice >= DrumPatternSet::kVoices || step < 0 || step >= DrumPattern::kSteps) return;
    DrumStep& s = drums.voices[voice].steps[step];
    s.hit = 1;
    s.accent = accent;
    s.velocity = velocity;
    s.probability = probability;
  };
  const bool halfTime = plan.idea == Idea::ReducedMotif && role == 2;
  const bool fill = !singleBar && role == 3 &&
                    (plan.idea == Idea::Turnaround || plan.idea == Idea::Original ||
                     plan.idea == Idea::LowHighAnswer);
  const bool lateSnare = (dance || funk) && role == 1 &&
                         (plan.idea == Idea::LowHighAnswer || plan.idea == Idea::RegisterArc);

  // Anchors.
  hit(kKick, 0, 118, 100, true);
  if (!halfTime) hit(kKick, neuro ? 7 : 10, 108);
  if (halfTime) {
    hit(kSnare, 8, 116, 100, true);
  } else {
    hit(kSnare, 4, 116, 100, true);
    hit(kSnare, lateSnare ? 14 : 12, 116, 100, true);
  }
  if (fill) {
    // Snare roll into the next bar's downbeat.
    hit(kSnare, 13, 62);
    hit(kSnare, 14, 78);
    hit(kSnare, 15, 94);
  }

  // Hats: eighths, then quiet sixteenths by chance (more with LIVELY).
  const uint8_t ghostChance = request.liveliness == 0 ? 0 : request.liveliness == 2 ? 60 : 35;
  for (int step = 0; step < 16; step += 2) hit(kClosedHat, step, (step % 4) == 2 ? 92 : 70);
  if (ghostChance > 0 && !atmos) {
    for (int step = 1; step < 16; step += 2) {
      if (fill && step >= 13) continue;
      hit(kClosedHat, step, 40, ghostChance);
    }
  }
  // Ghost snares between the anchors.
  if (funk) {
    hit(kRim, 7, 46, 70);
    hit(kRim, 15, 42, 60);
  } else if (dance) {
    hit(kRim, 15, 40, 50);
    hit(kOpenHat, 14, 70);
    if (!singleBar && role == 0) hit(kOpenHat, 0, 96, 100, true);  // opens the phrase
  } else if (atmos) {
    hit(kRim, 7, 36, 40);
  }
}

}  // namespace GenreIdiom

#endif  // GROOVEPUTER_GENERATION_IDIOM_GENRE_IDIOM_H
