#include "groove_profile.h"

#include <algorithm>

namespace {
template <typename T>
inline T clampValue(T v, T lo, T hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
}  // namespace

namespace GrooveProfile {

PatternCorridors getCorridors(GrooveboxMode mode, int flavor) {
  const int idx = clampValue(flavor, 0, 4);

  static const PatternCorridors kAcid[5] = {
      {8, 10, 0.28f, 0.18f, 0.00f},
      {9, 11, 0.33f, 0.22f, 0.01f},
      {10, 12, 0.38f, 0.26f, 0.02f},
      {11, 12, 0.43f, 0.30f, 0.03f},
      {12, 13, 0.48f, 0.35f, 0.05f},
  };
  static const PatternCorridors kMinimal[5] = {
      {3, 4, 0.08f, 0.00f, 0.04f},
      {3, 5, 0.10f, 0.02f, 0.06f},
      {4, 5, 0.12f, 0.05f, 0.09f},
      {4, 6, 0.15f, 0.08f, 0.11f},
      {5, 6, 0.18f, 0.10f, 0.14f},
  };
  static const PatternCorridors kBreaks[5] = {
      {5, 6, 0.18f, 0.04f, 0.12f},
      {5, 7, 0.22f, 0.06f, 0.15f},
      {6, 7, 0.26f, 0.08f, 0.18f},
      {6, 8, 0.30f, 0.10f, 0.21f},
      {7, 9, 0.34f, 0.12f, 0.24f},
  };
  static const PatternCorridors kDub[5] = {
      {2, 3, 0.14f, 0.00f, 0.06f},
      {2, 4, 0.18f, 0.02f, 0.08f},
      {3, 4, 0.22f, 0.04f, 0.10f},
      {3, 5, 0.28f, 0.06f, 0.12f},
      {4, 5, 0.34f, 0.08f, 0.14f},
  };
  static const PatternCorridors kElectro[5] = {
      {6, 7, 0.16f, 0.00f, 0.00f},
      {6, 8, 0.20f, 0.01f, 0.01f},
      {7, 8, 0.24f, 0.03f, 0.02f},
      {7, 9, 0.27f, 0.04f, 0.02f},
      {8, 10, 0.30f, 0.06f, 0.03f},
  };

  switch (mode) {
    case GrooveboxMode::Acid: return kAcid[idx];
    case GrooveboxMode::Minimal: return kMinimal[idx];
    case GrooveboxMode::Breaks: return kBreaks[idx];
    case GrooveboxMode::Dub: return kDub[idx];
    case GrooveboxMode::Electro: return kElectro[idx];
    default: return kMinimal[idx];
  }
}

void applyBudgetRules(GrooveboxMode mode, float delayMix, float tapeSpace, PatternCorridors& c) {
  if (mode == GrooveboxMode::Dub) {
    if (delayMix > 0.35f || tapeSpace > 0.60f) {
      c.notesMin = std::max(1, c.notesMin - 2);
      c.notesMax = std::max(c.notesMin, c.notesMax - 3);
      c.accentProbability = std::max(0.05f, c.accentProbability - 0.10f);
    } else if (delayMix > 0.25f || tapeSpace > 0.40f) {
      c.notesMin = std::max(1, c.notesMin - 1);
      c.notesMax = std::max(c.notesMin, c.notesMax - 2);
      c.accentProbability = std::max(0.05f, c.accentProbability - 0.05f);
    }
  } else if (mode == GrooveboxMode::Acid) {
    const int avgNotes = (c.notesMin + c.notesMax) / 2;
    if (avgNotes >= 12) c.slideProbability = std::min(c.slideProbability, 0.32f);
    else if (avgNotes <= 9) c.slideProbability = std::max(c.slideProbability, 0.20f);
  } else if (mode == GrooveboxMode::Electro) {
    c.slideProbability = 0.0f;
    c.swingAmount = std::min(c.swingAmount, 0.02f);
  } else if (mode == GrooveboxMode::Breaks) {
    // Keep synth lane tighter; drums carry most microtiming in breaks.
    c.swingAmount *= 0.30f;
  }
}

PatternMetrics analyzePattern(const SynthPattern& pattern) {
  PatternMetrics m{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    const auto& s = pattern.steps[i];
    if (s.note >= 0) {
      ++m.notes;
      if (s.accent) ++m.accents;
      if (s.slide) ++m.slides;
      if (s.ghost) ++m.ghosts;
    } else {
      ++m.rests;
    }
  }
  return m;
}

}  // namespace GrooveProfile

