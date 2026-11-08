// P06: genre reachability matrix (baseline measurement, no production change).
//
// For every (mode, recipe) pair the production profile table offers (BASE for
// each of the 16 modes included) this prints one row describing which owner
// actually produces the material. It asserts only what is true today:
// drum template rows 0..8 are non-zero. Rows 9..15 are recorded, not asserted
// (that is P01's job); later waves tighten the allow-list.
//
// Pair enumeration uses availableRecipeAt(), the production view of kProfiles
// (kProfiles itself is file-local to generation_profile.cpp).

#include "arduino_compat.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "../src/dsp/atlas_runtime.h"
#include "../src/dsp/drum_genre_templates.h"
#include "../src/dsp/genre_manager.h"
#include "../src/generation/composition/generation_profile.h"
#include "../src/generation/harmony/genre_harmonic_policy.h"
#include "../src/generation/idiom/genre_idiom.h"
#include "../src/generation/migration/strong_rhythm_migration.h"

SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

int g_failures = 0;
void check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

const char* routeName(StrongRhythmRoute route) {
  switch (route) {
    case StrongRhythmRoute::Legacy: return "Legacy";
    case StrongRhythmRoute::AcidBase: return "AcidBase";
    case StrongRhythmRoute::TechnoBase: return "TechnoBase";
    case StrongRhythmRoute::RaveBase: return "RaveBase";
    case StrongRhythmRoute::DrumAndBass: return "DrumAndBass";
    case StrongRhythmRoute::DubTechno: return "DubTechno";
    case StrongRhythmRoute::ChicagoJack: return "ChicagoJack";
    case StrongRhythmRoute::RollingAcid: return "RollingAcid";
    case StrongRhythmRoute::DeepChord: return "DeepChord";
    case StrongRhythmRoute::Stage7Composition: return "Stage7";
    default: return "?";
  }
}

const char* harmonyName(GroovePuterHarmony::HarmonyStatus s) {
  using GroovePuterHarmony::HarmonyStatus;
  switch (s) {
    case HarmonyStatus::NoHarmony: return "NoHarmony";
    case HarmonyStatus::Ready: return "Ready";
    case HarmonyStatus::Unsupported: return "Unsupported";
    case HarmonyStatus::Stale: return "Stale";
    case HarmonyStatus::Forbidden: return "Forbidden";
    case HarmonyStatus::Invalid: return "Invalid";
    default: return "Other";
  }
}

const char* grooveboxName(GrooveboxMode mode) {
  switch (mode) {
    case GrooveboxMode::Acid: return "Acid";
    case GrooveboxMode::Minimal: return "Minimal";
    case GrooveboxMode::Breaks: return "Breaks";
    case GrooveboxMode::Dub: return "Dub";
    case GrooveboxMode::Electro: return "Electro";
    default: return "?";
  }
}

bool rowIsZero(const DrumGenreTemplate& t) {
  return (t.kickMask | t.snareMask | t.hatMask | t.openHatMask) == 0;
}

uint32_t fnv(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

uint32_t barHash(const DrumPatternSet& drums, const SynthPattern& a,
                 const SynthPattern& b) {
  uint32_t h = 2166136261u;
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      const DrumStep& e = drums.voices[v].steps[s];
      h = fnv(h, e.hit);
      h = fnv(h, e.accent);
      h = fnv(h, e.velocity);
    }
  }
  for (const SynthPattern* p : {&a, &b}) {
    for (int s = 0; s < SynthPattern::kSteps; ++s) {
      const SynthStep& e = p->steps[s];
      h = fnv(h, static_cast<uint8_t>(e.note));
      h = fnv(h, e.slide);
      h = fnv(h, e.accent);
      h = fnv(h, e.velocity);
    }
  }
  return h;
}

// Index of the lowest mode whose compiled preset equals this mode's preset:
// modes that print the same slot share one GenerativeParams.
int presetSlot(uint8_t mode) {
  for (int i = 0; i <= mode; ++i) {
    if (std::memcmp(&kGenerativePresets[i], &kGenerativePresets[mode],
                    sizeof(GenerativeParams)) == 0) {
      return i;
    }
  }
  return mode;
}

}  // namespace

int main() {
  std::printf(
      "mode,recipe,route,harmony,idiom,atlas,drumOwner,presetSlot,groovebox,"
      "distinctBars32\n");

  std::vector<int> zeroRows;
  for (int m = 0; m < kGenerativeModeCount; ++m) {
    if (rowIsZero(kDrumTemplates[m])) zeroRows.push_back(m);
  }

  bool baseSeen[kGenerativeModeCount] = {};
  int rows = 0;

  for (int m = 0; m < kGenerativeModeCount; ++m) {
    const GenerativeMode mode = static_cast<GenerativeMode>(m);
    const uint8_t count = availableRecipeCount(mode);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!availableRecipeAt(mode, ordinal, recipe)) continue;
      if (recipe == kBaseRecipeId) baseSeen[m] = true;

      GenreSettings settings{};
      settings.generativeMode = static_cast<uint8_t>(m);
      settings.recipe = recipe;
      settings.morphTarget = kBaseRecipeId;
      settings.morphAmount = 0;

      const StrongRhythmRoute route = selectStrongRhythmRoute(settings);

      GroovePuterHarmony::HarmonicStyleId style{};
      const GroovePuterHarmony::HarmonyStatus hs =
          GroovePuterHarmony::harmonicStyleFor(settings, style);
      char harmony[40];
      if (hs == GroovePuterHarmony::HarmonyStatus::Ready) {
        std::snprintf(harmony, sizeof harmony, "Ready:%u",
                      static_cast<unsigned>(style));
      } else {
        std::snprintf(harmony, sizeof harmony, "%s", harmonyName(hs));
      }

      const GenreIdiom::IdiomVariant* v0 = GenreIdiom::variantFor(m, recipe, 0);
      const GenreIdiom::IdiomVariant* v1 = GenreIdiom::variantFor(m, recipe, 1);
      std::string idiom = "none";
      if (v0) {
        idiom = v0->name;
        if (v1 && std::strcmp(v0->name, v1->name) != 0) {
          idiom += "|";
          idiom += v1->name;
        }
      }

      const bool atlas = AtlasRuntime::hasRecipe(recipe);

      std::string owner = rowIsZero(kDrumTemplates[m]) ? "tmpl-ZERO" : "tmpl";
      if (GenreCatalog::drumTemplateOverride(settings)) owner += "+override";

      const GrooveboxMode gbox = GenreCatalog::grooveboxModeForRecipe(recipe, mode);

      char distinct[24] = "n/a(Legacy)";
      if (route != StrongRhythmRoute::Legacy) {
        std::set<uint32_t> bars;
        bool applied = true;
        unsigned lastStatus = 0;
        for (uint32_t press = 1; press <= 32; ++press) {
          StrongRhythmMigrationContext context{};
          context.patternAddress = 7;
          context.level = RealizationLevel::P2Variation;
          context.feelProfile = FeelProfileId::Straight;
          context.generationAttemptOrdinal = press;
          context.ideaPress = press;
          context.ideaPhraseBars = 4;
          // Same context the UKG interaction case uses: Stage 15 tonal
          // materialization on, so empty source synth patterns are valid.
          context.tonalMaterializationEnabled = true;
          context.scaleTypeValue = kScaleDorian;
          DrumPatternSet drums{};
          SynthPattern a{};
          SynthPattern b{};
          const StrongRhythmMigrationResult r =
              migrateStrongRhythmMaterial(settings, context, drums, a, b);
          if (r.status != StrongRhythmMigrationStatus::Applied) {
            applied = false;
            lastStatus = static_cast<unsigned>(r.status);
            break;
          }
          bars.insert(barHash(drums, a, b));
        }
        if (applied) {
          std::snprintf(distinct, sizeof distinct, "%u",
                        static_cast<unsigned>(bars.size()));
        } else {
          std::snprintf(distinct, sizeof distinct, "n/a(status%u)",
                        lastStatus);
        }
      }

      std::printf("%s,%u,%s,%s,%s,%s,%s,%d,%s,%s\n",
                  GenreCatalog::generativeModeName(mode),
                  static_cast<unsigned>(recipe), routeName(route), harmony,
                  idiom.c_str(), atlas ? "yes" : "no", owner.c_str(),
                  presetSlot(static_cast<uint8_t>(m)), grooveboxName(gbox),
                  distinct);
      ++rows;
    }
  }

  std::printf("rows=%d\n", rows);
  std::printf("zero drum template rows (recorded, not asserted for 9..15):");
  for (int m : zeroRows) std::printf(" %d", m);
  std::printf("\n");

  // Baseline invariants true today.
  for (int m = 0; m < kGenerativeModeCount; ++m) {
    check(baseSeen[m], "every mode has a BASE row in the profile table");
  }
  for (int m = 0; m <= 8; ++m) {
    check(!rowIsZero(kDrumTemplates[m]),
          "drum template rows 0..8 are non-zero");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("test_genre_reachability_matrix: PASS\n");
  return 0;
}
