// Generator probe (analysis, not a test): what does plain G on STEPS produce?
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include <map>

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
  const auto root = std::filesystem::temp_directory_path() / "gp_addrprobe";
  std::error_code ec; std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root); std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("addrprobe", 0);
  PatternPagingService::setProjectName("addrprobe");
  SceneStorageSdl storage; storage.setCurrentSceneName("default");
  MiniAcid engine(44100, &storage); engine.init(); engine.setSongMode(false);
  auto& scene = engine.sceneManager().currentScene();
  std::map<std::string,int> why;
  std::printf("genre\trecipe\temptyA/256\temptyB/256\tfirstEmptyB\n");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
   for (int recipe = 0; recipe < 16; ++recipe) {
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(static_cast<GenreRecipeId>(recipe));
    if (static_cast<int>(engine.genreManager().recipe()) != recipe) continue;
    const GenreSettings g = scene.genre;
    GenerativeParams params = GenreCatalog::compiledGenerativeParams(g);
    GenreBehavior beh = GenreCatalog::behavior(g);
    GrooveboxModeManager scratch(engine);
    scratch.setModeLocal(GenreCatalog::grooveboxModeForRecipe(g.recipe, static_cast<GenerativeMode>(g.generativeMode)));
    scratch.setFlavorLocal(engine.modeManager().flavor());
    scratch.setGenerationSeed(engine.modeManager().generationSeed());
    SynthPattern a0{}, b0{}; DrumPatternSet d0{};
    scratch.generatePattern(a0, engine.bpm(), params, beh, 0);
    scratch.generatePattern(b0, engine.bpm(), params, beh, 1);
    scratch.generateDrumPattern(d0, params, beh);
    int ea=0, eb=0, first=-1;
    for (int addr = 0; addr < kMaxGlobalPatterns; ++addr) {
      SynthPattern a=a0, b=b0; DrumPatternSet d=d0;
      auto ctx = R::QuantizedGenerationDetail::migrationContextFor(scene, R::QuantizedGenerationDetail::captureGenerationActivationTarget(engine.sceneManager()));
      ctx.patternAddress = static_cast<int16_t>(addr);
      const auto res = R::migrateStrongRhythmMaterial(g, ctx, d, a, b);
      if (measure(a).notes == 0) ++ea;
      if (measure(b).notes == 0) { ++eb; if (first < 0) first = addr;
        char k[96]; std::snprintf(k, sizeof k, "%s chord=%s mel=%s cs=%d proj=%d ton=%d tp=%d ad=%d feel=%d", kGenre[mode], R::chordRhythmName(res.chordRhythmId), R::melodicRhythmName(res.melodicRhythmId), (int)res.chordRhythmStatus, (int)res.chordProjectionStatus, (int)res.chordTonalStatus, (int)res.chordTonalProjectionStatus, (int)res.chordTonalAdaptStatus, (int)res.chordFeelStatus); ++why[k]; }
    }
    if (ea || eb) std::printf("%s\t%d\t%d\t%d\t%d\n", kGenre[mode], recipe, ea, eb, first);
   }
  }
  for (auto& [k,v] : why) std::printf("WHY %4d %s\n", v, k.c_str());
  return 0;
}
