// 0.9.14 P0 product path: DEVELOP + BREAK cycle for the kept generated phrase.
// Real engine, real GeneratedPhraseSong transaction, real Undo owner.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/state/generation_request_state.h"

SerialMock Serial;
SDMock SD;

namespace {

namespace R = GroovePuterRhythm;
using GeneratedPhraseSong::CycleStatus;
using GeneratedPhraseSong::LifecycleStatus;
using GeneratedPhraseP1R::canonicalBarHash;

constexpr float kSampleRate = 44100.0f;
const auto kGuard = [](auto&& body) { body(); };

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                   #cond);                                                   \
      std::abort();                                                          \
    }                                                                        \
  } while (0)

struct Fixture {
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  explicit Fixture(const char* project, R::RealizationLevel level) {
    CHECK(PatternPagingService::setProjectName(project));
    CHECK(PatternPagingService::clearProjectPages());
    engine.init();
    engine.setSongMode(false);
    Scene& scene = engine.sceneManager().currentScene();
    scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
    scene.genre.recipe = 0;
    scene.genre.morphTarget = 0;
    scene.genre.morphAmount = 0;
    scene.genre.regenerateOnApply = false;
    scene.genre.applyTempoOnApply = false;
    scene.genre.rhythmSelectionMode =
        static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
    scene.genre.rhythmArchetypeId = 404;  // broken_techno: admitted at P3
    scene.activeSongSlot = 0;
    scene.songs[0] = Song{};
    scene.songs[1] = Song{};
    scene.feel.patternBars = 1;
    for (int b = 0; b < kBankCount; ++b) {
      for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
        scene.synthABanks[b].patterns[i] = SynthPattern{};
        scene.synthBBanks[b].patterns[i] = SynthPattern{};
        scene.drumBanks[b].patterns[i] = DrumPatternSet{};
      }
    }
    for (int v = 0; v < Scene::kMaterialVoices; ++v) {
      for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
        scene.materialSlots[v][slot] = GroovePuterMaterial::MaterialSlotDescriptor{};
      }
    }
    engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
    engine.genreManager().setRecipe(0);
    GroovePuterState::setGenerationLevel(level);
  }
  ~Fixture() {
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation);
  }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  bool generateKept() {
    return GeneratedPhraseSong::generate(engine, 4, 0, kGuard).status ==
           LifecycleStatus::CommittedNow;
  }
  uint64_t rowHash(int row) {
    const Song& song = scene().songs[0];
    const int pattern = song.positions[row].patterns[static_cast<int>(SongTrack::SynthA)];
    const int local = pattern % kPatternsPerPage;
    const int bank = local / Bank<SynthPattern>::kPatterns;
    const int index = local % Bank<SynthPattern>::kPatterns;
    return canonicalBarHash(scene().drumBanks[bank].patterns[index],
                            scene().synthABanks[bank].patterns[index],
                            scene().synthBBanks[bank].patterns[index]);
  }
  CycleStatus cycle() { return GeneratedPhraseSong::generateCycle(engine, kGuard).status; }
};

void testNoRecipe() {
  Fixture f("p0-cycle-norecipe", R::RealizationLevel::P3Transformation);
  CHECK(f.engine.generatedPhraseRecipe() == nullptr);
  CHECK(f.cycle() == CycleStatus::NoRecipe);
  std::puts("P0 cycle: no recipe -> typed NoRecipe: PASS");
}

void testPublishUndoRepeat() {
  Fixture f("p0-cycle-publish", R::RealizationLevel::P3Transformation);
  CHECK(f.generateKept());
  const auto* recipe = f.engine.generatedPhraseRecipe();
  CHECK(recipe != nullptr);
  CHECK(recipe->songStart == 0 && recipe->bars == 4 && recipe->cycleSongStart == -1);
  const GroovePuterMaterial::GeneratedPhraseRecipe keptRecipe = *recipe;
  std::printf("P0 recipe size: %zu B\n", sizeof(GroovePuterMaterial::GeneratedPhraseRecipe));

  uint64_t keptHash[4];
  for (int b = 0; b < 4; ++b) keptHash[b] = f.rowHash(b);
  const Song songBefore = f.scene().songs[0];

  {
    const CycleStatus st = f.cycle();
    if (st != CycleStatus::CommittedNow) std::fprintf(stderr, "cycle status=%d\n", static_cast<int>(st));
    CHECK(st == CycleStatus::CommittedNow);
  }

  // kept phrase untouched
  for (int b = 0; b < 4; ++b) CHECK(f.rowHash(b) == keptHash[b]);
  CHECK(std::memcmp(&songBefore.positions[0], &f.scene().songs[0].positions[0],
                    4 * sizeof(SongPosition)) == 0);

  // eight new rows, distinct patterns, eight distinct valid canonical ids
  const auto* origin = f.engine.generatedSynthAOrigin();
  CHECK(origin != nullptr && origin->valid() && origin->common.barCount == 8);
  std::set<uint32_t> ids;
  std::set<int> patterns;
  for (int b = 0; b < 8; ++b) {
    const int pattern =
        f.scene().songs[0].positions[4 + b].patterns[static_cast<int>(SongTrack::SynthA)];
    CHECK(pattern >= 0);
    patterns.insert(pattern);
    CHECK(origin->bars[b].material.id.valid());
    ids.insert(static_cast<uint32_t>(origin->bars[b].material.id.value));
  }
  CHECK(patterns.size() == 8 && ids.size() == 8);
  CHECK(f.engine.generatedPhraseRecipe()->cycleSongStart == 4);

  // the developed material really is a different arc (not a copy of the kept bars)
  int differing = 0;
  for (int b = 0; b < 8; ++b) differing += f.rowHash(4 + b) != keptHash[b % 4];
  CHECK(differing >= 2);

  // differential: each section equals an independent rebuild from the same recipe
  for (int section = 0; section < 2; ++section) {
    R::PreparedPhraseExecution execution{};
    R::PhraseExecutionScratch scratch{};
    CHECK(R::preparePhraseExecution(keptRecipe.genre, keptRecipe.materialization,
                                    keptRecipe.phraseGenerationIdentity, 4, scratch,
                                    execution) == R::PhraseExecutionStatus::Ready);
    CHECK(R::applyPhraseLawToExecution(
              execution, section == 0 ? R::PhraseEvolutionLawId::DevelopReturn
                                      : R::PhraseEvolutionLawId::SparseDrift) ==
          R::PhraseLawApplyStatus::Applied);
    for (int b = 0; b < 4; ++b) {
      PhraseGenerator::PhraseBar bar{};
      CHECK(GeneratedPhraseP1R::materializeOneBar(f.engine, execution, static_cast<uint8_t>(b),
                                                  0, bar));
      CHECK(canonicalBarHash(bar.drums, bar.synthA, bar.synthB) ==
            f.rowHash(4 + section * 4 + b));
    }
  }

  // one call only
  CHECK(f.cycle() == CycleStatus::CycleAlreadyPublished);

  // one Undo removes the whole cycle, keeps the kept phrase and its recipe
  const auto undo = GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard);
  CHECK(undo == GroovePuterUndo::UndoResult::Restored);
  for (int b = 0; b < 4; ++b) CHECK(f.rowHash(b) == keptHash[b]);
  for (int b = 0; b < 8; ++b) {
    CHECK(f.scene().songs[0].positions[4 + b].patterns[static_cast<int>(SongTrack::SynthA)] ==
          songBefore.positions[4 + b].patterns[static_cast<int>(SongTrack::SynthA)]);
  }
  CHECK(f.engine.generatedSynthAOrigin() == nullptr);
  const auto* kept = f.engine.generatedPhraseRecipe();
  CHECK(kept != nullptr && kept->cycleSongStart == -1 && kept->songStart == 0);

  // repeatable after Undo, with fresh MaterialIds
  CHECK(f.cycle() == CycleStatus::CommittedNow);
  const auto* again = f.engine.generatedSynthAOrigin();
  CHECK(again != nullptr && again->valid());
  for (int b = 0; b < 8; ++b) {
    CHECK(ids.count(static_cast<uint32_t>(again->bars[b].material.id.value)) == 0);
  }

  // Undo the cycle, then Undo of the kept phrase drops the recipe
  CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
        GroovePuterUndo::UndoResult::Restored);
  std::puts("P0 cycle: publish, kept phrase untouched, sections equal rebuild, one Undo, repeat: PASS");
}

void testRefusals() {
  {
    Fixture f("p0-cycle-p2", R::RealizationLevel::P2Variation);
    CHECK(f.generateKept());
    CHECK(f.cycle() == CycleStatus::DepthNotP3);
  }
  {
    Fixture f("p0-cycle-edit", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    const int pattern =
        f.scene().songs[0].positions[1].patterns[static_cast<int>(SongTrack::SynthA)];
    const int local = pattern % kPatternsPerPage;
    SynthPattern& p = f.scene().synthABanks[local / Bank<SynthPattern>::kPatterns]
                          .patterns[local % Bank<SynthPattern>::kPatterns];
    p.steps[3].velocity = static_cast<uint8_t>(p.steps[3].velocity ^ 0x11);
    CHECK(f.cycle() == CycleStatus::EditedSinceGeneration);
    CHECK(f.engine.generatedPhraseRecipe() != nullptr);
  }
  {
    Fixture f("p0-cycle-context", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    f.scene().generatorParams.scaleRoot = (f.scene().generatorParams.scaleRoot + 1) % 12;
    CHECK(f.cycle() == CycleStatus::ContextChanged);
  }
  {
    Fixture f("p0-cycle-genre", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    f.scene().genre.rhythmArchetypeId = 420;
    CHECK(f.cycle() == CycleStatus::ContextChanged);
  }
  {
    Fixture f("p0-cycle-bpm", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    f.engine.setBpm(f.engine.bpm() + 7.0f);
    CHECK(f.cycle() == CycleStatus::ContextChanged);
  }
  {
    Fixture f("p0-cycle-rows", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    f.scene().songs[0].positions[5].patterns[static_cast<int>(SongTrack::Drums)] = 0;
    CHECK(f.cycle() == CycleStatus::RowsOccupied);
  }
  {
    Fixture f("p0-cycle-undo-kept", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
          GroovePuterUndo::UndoResult::Restored);
    CHECK(f.engine.generatedPhraseRecipe() == nullptr);
    CHECK(f.cycle() == CycleStatus::NoRecipe);
  }
  {
    // Acid is excluded at scenario level: typed refusal, nothing published.
    Fixture f("p0-cycle-acid", R::RealizationLevel::P3Transformation);
    Scene& s = f.scene();
    s.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
    s.genre.recipe = 6;
    s.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Auto);
    s.genre.rhythmArchetypeId = 0;
    f.engine.genreManager().setGenerativeMode(GenerativeMode::Acid);
    f.engine.genreManager().setRecipe(6);
    if (f.generateKept() && f.engine.generatedPhraseRecipe() != nullptr) {
      CHECK(f.cycle() == CycleStatus::NotAdmitted);
      CHECK(f.scene().songs[0].length == 4);
    } else {
      CHECK(f.cycle() == CycleStatus::NoRecipe);
    }
  }
  std::puts("P0 cycle: typed refusals (P2, edited, context, genre, bpm, rows, undo, Acid): PASS");
}

}  // namespace

int main() {
  testNoRecipe();
  testPublishUndoRepeat();
  testRefusals();
  std::puts("0.9.14 P0 cycle: PASS");
  return 0;
}
