// Generator probe (analysis, not a test): what does plain G on STEPS produce?
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/migration/quantized_generation_commit.h"
#include "src/generation/migration/quantized_generation_commit_impl.h"
#include "src/generation/migration/quantized_generation_undo_owner_impl.h"
#include "src/platform/cardputer_material_publication_session.h"

SerialMock Serial;
SDMock SD;

namespace R = GroovePuterRhythm;

static const char* kGenre[] = {"Acid", "Outrun", "Darksynth", "Electro", "Rave",
                               "Reggae", "TripHop", "Broken", "Chip", "House",
                               "Techno", "HipHop", "FunkSoul", "UkGarage", "DnB", "LoFi"};

static std::string name(int n) {
  static const char* k[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (n < 0) return ".";
  return std::string(k[n % 12]) + std::to_string(n / 12 - 1);
}

struct Stats {
  int notes = 0, distinct = 0, range = 0, repeats = 0, steps = 0, skips = 0, leaps = 0;
  int slides = 0, accents = 0, longestSame = 0;
};

static Stats measure(const SynthPattern& p) {
  Stats s;
  std::set<int> pitches;
  int lo = 999, hi = -1, prev = -1, run = 0;
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    const auto& st = p.steps[i];
    if (st.note < 0) continue;
    ++s.notes;
    pitches.insert(st.note);
    lo = std::min<int>(lo, st.note);
    hi = std::max<int>(hi, st.note);
    if (st.slide) ++s.slides;
    if (st.accent) ++s.accents;
    if (prev >= 0) {
      const int d = std::abs(st.note - prev);
      if (d == 0) { ++s.repeats; ++run; }
      else { run = 0; if (d <= 2) ++s.steps; else if (d <= 4) ++s.skips; else ++s.leaps; }
      s.longestSame = std::max(s.longestSame, run + 1);
    }
    prev = st.note;
  }
  s.distinct = static_cast<int>(pitches.size());
  s.range = s.notes ? hi - lo : 0;
  return s;
}

static int diff(const SynthPattern& a, const SynthPattern& b) {
  int d = 0;
  for (int i = 0; i < SynthPattern::kSteps; ++i) d += a.steps[i].note != b.steps[i].note;
  return d;
}

int main(int argc, char** argv) {
  const int presses = argc > 1 ? std::atoi(argv[1]) : 8;
  const auto root = std::filesystem::temp_directory_path() / "gp_genprobe";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("genprobe", 0);
  PatternPagingService::setProjectName("genprobe");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100, &storage);
  engine.init();
  engine.setSongMode(false);
  auto& scene = engine.sceneManager().currentScene();

  const int level = argc > 3 ? std::atoi(argv[3]) : -1;
  if (level >= 0) GroovePuterState::setGenerationLevel(static_cast<R::RealizationLevel>(level));
  std::printf("# level=%u\n", static_cast<unsigned>(GroovePuterState::currentGenerationLevel()));
  std::printf("genre\tvoice\tnotes\tdistinct\trange\trepeat%%\tstep%%\tskip%%\tleap%%\tslide\taccent\tmaxSame\tchangeVsPrev\tuniqueOfN\n");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(0);
    scene.genre.rhythmSelectionMode = 0;
    scene.genre.rhythmArchetypeId = 0;
    if (level >= 0) GroovePuterState::setGenerationLevel(static_cast<R::RealizationLevel>(level));
    if (mode == 0) std::printf("# level after genre set: %u\n", static_cast<unsigned>(GroovePuterState::currentGenerationLevel()));
    {
      // Legacy generator output before the strong-rhythm migration.
      const GenreSettings g = scene.genre;
      GenerativeParams params = GenreCatalog::compiledGenerativeParams(g);
      GenreBehavior beh = GenreCatalog::behavior(g);
      GrooveboxModeManager scratch(engine);
      scratch.setModeLocal(GenreCatalog::grooveboxModeForRecipe(g.recipe, static_cast<GenerativeMode>(g.generativeMode)));
      scratch.setFlavorLocal(engine.modeManager().flavor());
      scratch.setGenerationSeed(engine.modeManager().generationSeed());
      SynthPattern a{}, b{};
      DrumPatternSet dr{};
      scratch.generatePattern(a, engine.bpm(), params, beh, 0);
      scratch.generatePattern(b, engine.bpm(), params, beh, 1);
      scratch.generateDrumPattern(dr, params, beh);
      const Stats sa = measure(a), sb = measure(b);
      auto ctx = R::QuantizedGenerationDetail::migrationContextFor(scene, R::QuantizedGenerationDetail::captureGenerationActivationTarget(engine.sceneManager()));
      ctx.level = GroovePuterState::currentGenerationLevel();
      R::migrateStrongRhythmMaterial(g, ctx, dr, a, b);
      const Stats ma = measure(a), mb = measure(b);
      std::printf("#MIG %s  A legacy %d notes/%d pitches -> migrated %d/%d   B legacy %d notes/%d pitches range %d -> migrated %d/%d range %d\n",
                  kGenre[mode], sa.notes, sa.distinct, ma.notes, ma.distinct,
                  sb.notes, sb.distinct, sb.range, mb.notes, mb.distinct, mb.range);
    }
    for (int voice = 0; voice < 2; ++voice) {
      std::vector<SynthPattern> got;
      double n = 0, dis = 0, rng = 0, rep = 0, stp = 0, skp = 0, lp = 0, sl = 0, ac = 0, ms = 0, ch = 0;
      int intervals = 0;
      for (int k = 0; k < presses; ++k) {
        if (R::regenerateSynthWithQuantizedCommit(engine, voice) !=
            R::QuantizedGenerationResult::CommittedNow) {
          std::printf("# %s v%d press %d: not committed\n", kGenre[mode], voice, k);
          continue;
        }
        const int bank = engine.current303BankIndex(voice);
        const int slot = engine.current303PatternIndex(voice);
        const auto& p = voice == 0 ? scene.synthABanks[bank].patterns[slot]
                                   : scene.synthBBanks[bank].patterns[slot];
        const Stats s = measure(p);
        n += s.notes; dis += s.distinct; rng += s.range; sl += s.slides; ac += s.accents;
        ms += s.longestSame;
        rep += s.repeats; stp += s.steps; skp += s.skips; lp += s.leaps;
        intervals += s.repeats + s.steps + s.skips + s.leaps;
        if (!got.empty()) ch += diff(got.back(), p);
        got.push_back(p);
        if (k < 2 && argc > 2) {
          std::printf("#  %s %s #%d:", kGenre[mode], voice ? "B" : "A", k);
          for (int i = 0; i < SynthPattern::kSteps; ++i)
            std::printf(" %s%s", name(p.steps[i].note).c_str(), p.steps[i].slide ? "~" : "");
          std::printf("\n");
        }
      }
      int unique = 0;
      for (size_t i = 0; i < got.size(); ++i) {
        bool same = false;
        for (size_t j = 0; j < i && !same; ++j) same = diff(got[i], got[j]) == 0;
        unique += !same;
      }
      const double c = got.empty() ? 1 : got.size();
      const double it = intervals ? intervals : 1;
      std::printf("%s\t%s\t%.1f\t%.1f\t%.1f\t%.0f\t%.0f\t%.0f\t%.0f\t%.1f\t%.1f\t%.1f\t%.1f\t%d/%zu\n",
                  kGenre[mode], voice ? "B" : "A", n / c, dis / c, rng / c,
                  100 * rep / it, 100 * stp / it, 100 * skp / it, 100 * lp / it,
                  sl / c, ac / c, ms / c, got.size() > 1 ? ch / (got.size() - 1) : 0.0,
                  unique, got.size());
    }
  }
  return 0;
}
