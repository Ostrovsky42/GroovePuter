// 0.9.14 P0 product path: DEVELOP + BREAK cycle for the kept generated phrase.
// Real engine, real GeneratedPhraseSong transaction, real Undo owner.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/state/generation_request_state.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {

class CycleGfx : public IGfx {
 public:
  std::vector<std::string> labels;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override { labels.emplace_back(value ? value : ""); }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int, int, int, int, IGfxColor) override {}
  void fillCircle(int, int, int, IGfxColor) override {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, IGfxColor) override {}
  void setRotation(int) override {}
  void setTextColor(IGfxColor) override {}
  void setTextColor(uint16_t) override {}
  void setFont(GfxFont) override {}
  void startWrite() override {}
  void endWrite() override {}
  void flush() override {}
  int textWidth(const char* value) const override { return value ? std::strlen(value) : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
  bool shows(const char* phrase) const {
    for (const auto& label : labels) if (label.find(phrase) != std::string::npos) return true;
    return false;
  }
};

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
    engine.setBpm(120.0f);
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

bool naturalLawIs(Fixture& f, R::PhraseEvolutionLawId law) {
  const auto* recipe = f.engine.generatedPhraseRecipe();
  CHECK(recipe != nullptr);
  R::PreparedPhraseExecution execution{};
  R::PhraseExecutionScratch scratch{};
  CHECK(R::preparePhraseExecution(recipe->genre, recipe->materialization,
                                  recipe->phraseGenerationIdentity, recipe->bars, scratch,
                                  execution) == R::PhraseExecutionStatus::Ready);
  return execution.selection.composition.phraseLaw == law;
}


// For tests that need the full 8-bar cycle: regenerate the kept phrase (Undo of a fresh phrase is
// exact) until its own law is not DevelopReturn.
bool generateKeptFullCycle(Fixture& f) {
  for (int attempt = 0; attempt < 16; ++attempt) {
    if (!f.generateKept()) return false;
    if (!naturalLawIs(f, R::PhraseEvolutionLawId::DevelopReturn)) return true;
    CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
          GroovePuterUndo::UndoResult::Restored);
  }
  return false;
}

void testNoRecipe() {
  Fixture f("p0-cycle-norecipe", R::RealizationLevel::P3Transformation);
  CHECK(f.engine.generatedPhraseRecipe() == nullptr);
  CHECK(f.cycle() == CycleStatus::NoRecipe);
  std::puts("P0 cycle: no recipe -> typed NoRecipe: PASS");
}

void testPublishUndoRepeat() {
  Fixture f("p0-cycle-publish", R::RealizationLevel::P3Transformation);
  CHECK(generateKeptFullCycle(f));
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
  // Acid and House: excluded at scenario level. Each must reach a kept phrase WITH a recipe
  // (otherwise NoRecipe would mask the exclusion) and then be refused as NotAdmitted, with
  // nothing published.
  struct Excluded { GenerativeMode mode; GenreRecipeId recipe; const char* project; };
  for (const Excluded& e : {Excluded{GenerativeMode::Acid, 6, "p0-cycle-acid"},
                            Excluded{GenerativeMode::Acid, 7, "p0-cycle-acid-rolling"},
                            Excluded{GenerativeMode::House, 0, "p0-cycle-house"}}) {
    Fixture f(e.project, R::RealizationLevel::P3Transformation);
    Scene& s = f.scene();
    s.genre.generativeMode = static_cast<uint8_t>(e.mode);
    s.genre.recipe = e.recipe;
    s.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Auto);
    s.genre.rhythmArchetypeId = 0;
    f.engine.genreManager().setGenerativeMode(e.mode);
    f.engine.genreManager().setRecipe(e.recipe);
    CHECK(f.generateKept());
    CHECK(f.engine.generatedPhraseRecipe() != nullptr);
    CHECK(f.cycle() == CycleStatus::NotAdmitted);
    CHECK(f.scene().songs[0].length == 4);
    CHECK(f.engine.generatedPhraseRecipe()->cycleSongStart == -1);
  }
  std::puts("P0 cycle: typed refusals (P2, edited, context, genre, bpm, rows, undo, Acid, AcidRolling, House): PASS");
}


// Live path: the cycle is requested while the kept phrase is playing.
size_t renderBars(MiniAcid& engine, double bars, int* minRow, int* maxRow) {
  const size_t perBar = static_cast<size_t>(4.0 * 60.0 / engine.bpm() * kSampleRate);
  const size_t total = static_cast<size_t>(perBar * bars);
  std::vector<int16_t> pcm(128);
  for (size_t at = 0; at < total; at += 128) {
    engine.generateAudioBuffer(pcm.data(), 128);
    const int row = engine.currentSongPosition();
    if (minRow && row < *minRow) *minRow = row;
    if (maxRow && row > *maxRow) *maxRow = row;
  }
  return total;
}

void startPlaying(Fixture& f) {
  CHECK(f.engine.rebuildPatternRuntimeEventBank());
  f.engine.setSongMode(true);
  f.engine.setSongPlaybackSlot(0);
  f.engine.setSongPosition(0);
  f.engine.start();
  int lo = 99, hi = -1;
  renderBars(f.engine, 0.4, &lo, &hi);
  CHECK(f.engine.isPlaying() && hi == 0);
}

void testLivePendingActivation() {
  Fixture f("p0-cycle-live", R::RealizationLevel::P3Transformation);
  CHECK(generateKeptFullCycle(f));
  startPlaying(f);
  CHECK(f.cycle() == CycleStatus::PendingNextBar);
  // Rows are published atomically by the commit: all eight are present at once, never partial.
  for (int b = 0; b < 8; ++b) {
    CHECK(f.scene().songs[0].positions[4 + b].patterns[static_cast<int>(SongTrack::Drums)] >= 0);
  }
  const uint32_t revision = GroovePuterUndo::undoOwner().committedRevision();
  CHECK(R::PhraseLiveArrangementDetail::hasPendingPhraseActivationForRevision(f.engine, revision));
  int lo = 99, hi = -1;
  renderBars(f.engine, 4.0, &lo, &hi);
  CHECK(hi >= 4);   // activation moved playback into the cycle
  CHECK(!R::PhraseLiveArrangementDetail::hasPendingPhraseActivationForRevision(f.engine, revision));
  f.engine.stop();
  // resources are free again: a second phrase generation is not Busy
  CHECK(GeneratedPhraseSong::generate(f.engine, 4, 12, kGuard).status !=
        LifecycleStatus::Busy);
  std::puts("P0 cycle live: PendingNextBar activates at the bar boundary, nothing left pending: PASS");
}

void testLiveStopAndUndoWhilePending() {
  {
    Fixture f("p0-cycle-live-stop", R::RealizationLevel::P3Transformation);
    CHECK(generateKeptFullCycle(f));
    startPlaying(f);
    CHECK(f.cycle() == CycleStatus::PendingNextBar);
    f.engine.stop();
    CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
          GroovePuterUndo::UndoResult::Restored);
    const uint32_t revision = GroovePuterUndo::undoOwner().committedRevision();
    CHECK(!R::PhraseLiveArrangementDetail::hasPendingPhraseActivationForRevision(f.engine, revision));
    CHECK(f.scene().songs[0].positions[4].patterns[static_cast<int>(SongTrack::Drums)] < 0);
    CHECK(f.cycle() == CycleStatus::CommittedNow);   // no leaked lease or pending state
  }
  {
    Fixture f("p0-cycle-live-undo", R::RealizationLevel::P3Transformation);
    CHECK(generateKeptFullCycle(f));
    startPlaying(f);
    CHECK(f.cycle() == CycleStatus::PendingNextBar);
    CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
          GroovePuterUndo::UndoResult::Restored);
    int lo = 99, hi = -1;
    renderBars(f.engine, 8.0, &lo, &hi);
    CHECK(hi < 4);   // playback never entered the undone cycle rows
    f.engine.stop();
    CHECK(f.cycle() == CycleStatus::CommittedNow);
  }
  std::puts("P0 cycle live: stop / Undo while pending leaves no state and no partial cycle: PASS");
}

// The kept phrase's own law is not Loop. When it already is DevelopReturn, DEVELOP would repeat
// it, so only BREAK is published (four bars) and the result says so.
void testDevelopEqualsKeptIsSkipped() {
  bool sawSkip = false, sawFull = false;
  for (int attempt = 0; attempt < 16 && !(sawSkip && sawFull); ++attempt) {
    // Fresh project per attempt (the Undo owner holds one receipt); the identity counter is
    // process-wide, so every attempt gets a new identity.
    char project[40];
    std::snprintf(project, sizeof(project), "p0-cycle-skip-%d", attempt);
    Fixture f(project, R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    const bool developNatural = naturalLawIs(f, R::PhraseEvolutionLawId::DevelopReturn);
    uint64_t kept[4];
    for (int b = 0; b < 4; ++b) kept[b] = f.rowHash(b);
    const auto result = GeneratedPhraseSong::generateCycle(f.engine, kGuard);
    CHECK(result.status == CycleStatus::CommittedNow);
    if (developNatural) {
      sawSkip = true;
      CHECK(result.developSkipped && !result.breakSkipped && result.bars == 4);
      CHECK(f.scene().songs[0].length == 8);   // kept 4 + BREAK 4
      // the published section is not a copy of the kept phrase
      int differing = 0;
      for (int b = 0; b < 4; ++b) differing += f.rowHash(4 + b) != kept[b];
      CHECK(differing >= 1);
      CHECK(f.scene().songs[0].positions[8].patterns[static_cast<int>(SongTrack::Drums)] < 0);
      const auto* origin = f.engine.generatedSynthAOrigin();
      CHECK(origin != nullptr && origin->valid() && origin->common.barCount == 4);
    } else {
      sawFull = true;
      CHECK(!result.developSkipped && result.bars == 8);
    }
    // One Undo restores the state before the cycle whichever sections were published.
    CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
          GroovePuterUndo::UndoResult::Restored);
    CHECK(f.scene().songs[0].length == 4);
  }
  CHECK(sawSkip && sawFull);
  std::puts("P0 cycle: DEVELOP equal to the kept phrase is skipped, BREAK published alone; other identities keep 8 bars: PASS");
}

void testPhrasePageCycleGesture() {
  {
    Fixture f("p0-ui-short", R::RealizationLevel::P3Transformation);
    CHECK(GeneratedPhraseSong::generate(f.engine, 2, 0, kGuard).status ==
          LifecycleStatus::CommittedNow);
    CycleGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    UIEvent key{};
    key.event_type = GROOVEPUTER_KEY_DOWN;
    key.key = 'd';
    CHECK(page.handleEvent(key));
    CHECK(f.scene().songs[0].length == 2);
    UI::drawToast(gfx);
    CHECK(gfx.shows("4B TAKE"));
  }
  {
    Fixture f("p0-ui-p2", R::RealizationLevel::P2Variation);
    CHECK(f.generateKept());
    CycleGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    UIEvent key{};
    key.event_type = GROOVEPUTER_KEY_DOWN;
    key.key = 'd';
    CHECK(page.handleEvent(key));
    CHECK(f.scene().songs[0].length == 4);
    UI::drawToast(gfx);
    CHECK(gfx.shows("REWORK"));
  }
  {
    Fixture f("p0-ui-p3", R::RealizationLevel::P3Transformation);
    CHECK(f.generateKept());
    CycleGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    UIEvent key{};
    key.event_type = GROOVEPUTER_KEY_DOWN;
    key.key = 'd';
    CHECK(page.handleEvent(key));
    const auto* recipe = f.engine.generatedPhraseRecipe();
    CHECK(recipe != nullptr);
    CHECK(f.scene().songs[0].length == 8 || f.scene().songs[0].length == 12);
    UI::drawToast(gfx);
    CHECK(gfx.shows("BREAK") || gfx.shows("DEVELOP"));
    gfx.labels.clear();
    page.draw(gfx);
    CHECK(gfx.shows("GROW 8B") || gfx.shows("BREAK ONLY 4B"));
    UIEvent undo{};
    undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    CHECK(page.handleEvent(undo));
    CHECK(f.scene().songs[0].length == 4);
    gfx.labels.clear();
    page.draw(gfx);
    CHECK(!gfx.shows("GROW 8B") && !gfx.shows("BREAK ONLY 4B"));
  }
  std::puts("P0 cycle: PHRASE D gesture, P2 guidance and one-step Undo: PASS");
}

// Song playback reads Synth events from the derived runtime bank. After generate, the cycle and
// Undo the bank must equal the Patterns (regression: it kept the slot's previous events).
int noteCount(const SynthPattern& p) {
  int n = 0;
  for (int i = 0; i < SynthPattern::kSteps; ++i) n += p.steps[i].note >= 0;
  return n;
}

void expectBankMatchesRows(Fixture& f, int rows, bool expectEmptyAfter) {
  f.engine.setSongMode(true);
  f.engine.setSongPlaybackSlot(0);
  for (int row = 0; row < rows; ++row) {
    f.engine.setSongPosition(row);
    for (int synth = 0; synth < 2; ++synth) {
      const int pattern = f.scene().songs[0].positions[row].patterns[
          synth == 0 ? static_cast<int>(SongTrack::SynthA) : static_cast<int>(SongTrack::SynthB)];
      if (pattern < 0) continue;
      const int local = pattern % kPatternsPerPage;
      const int bank = local / Bank<SynthPattern>::kPatterns;
      const int index = local % Bank<SynthPattern>::kPatterns;
      const SynthPattern& source = synth == 0 ? f.scene().synthABanks[bank].patterns[index]
                                              : f.scene().synthBBanks[bank].patterns[index];
      // The bank projects notes to events (a slide/hold can merge steps): compare against a
      // fresh projection, which is exactly what a rebuild would publish.
      const uint32_t before = static_cast<uint32_t>(f.engine.activePatternRuntimeEvents(synth).count);
      CHECK(f.engine.rebuildPatternRuntimeEventBank());
      const uint32_t rebuilt = static_cast<uint32_t>(f.engine.activePatternRuntimeEvents(synth).count);
      CHECK(before == rebuilt);
      if (!expectEmptyAfter) CHECK(noteCount(source) == 0 || rebuilt > 0);
    }
  }
}

void testRuntimeBankFollowsPublication() {
  Fixture f("p0-cycle-bank", R::RealizationLevel::P3Transformation);
  CHECK(generateKeptFullCycle(f));
  expectBankMatchesRows(f, 4, false);          // kept phrase
  CHECK(f.cycle() == CycleStatus::CommittedNow);
  expectBankMatchesRows(f, 12, false);         // kept + DEVELOP + BREAK
  CHECK(GeneratedPhraseSong::undoLastGeneratedPhrase(f.engine, kGuard) ==
        GroovePuterUndo::UndoResult::Restored);
  expectBankMatchesRows(f, 4, false);          // cycle rows are gone; kept rows unchanged
  for (int row = 4; row < 12; ++row) {
    f.engine.setSongPosition(row);
  }
  std::puts("P0 cycle: runtime event bank equals a fresh rebuild after generate, cycle and Undo: PASS");
}

}  // namespace

int main() {
  testNoRecipe();
  testPublishUndoRepeat();
  testRefusals();
  testRuntimeBankFollowsPublication();
  testDevelopEqualsKeptIsSkipped();
  testPhrasePageCycleGesture();
  testLivePendingActivation();
  testLiveStopAndUndoWhilePending();
  std::puts("0.9.14 P0 cycle: PASS");
  return 0;
}
