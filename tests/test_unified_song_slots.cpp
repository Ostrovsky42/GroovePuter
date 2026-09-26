// Unified Song slots: a Song row references a slot, and the slot's kind decides
// whether the row plays a Pattern or a Melody. Key scenario: saved
// Pattern X -> Melody Y -> Pattern Z plays after a cold reload with no manual
// slot switching.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <set>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/audio/pattern_paging.h"
#include "src/state/melody_promotion.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

constexpr int kX = 0;  // bank A slot 1
constexpr int kY = 1;  // bank A slot 2
constexpr int kZ = 2;  // bank A slot 3

Buffer makeMelody(uint8_t firstNote, uint8_t bars) {
  Buffer melody{};
  melody.lengthTicks = static_cast<uint16_t>(PhraseRuntime::kTicksPerBar * bars);
  melody.count = static_cast<uint16_t>(4 * bars);
  for (uint16_t i = 0; i < melody.count; ++i) {
    auto& event = melody.events[i];
    event.startTick = static_cast<uint16_t>(i * 96);
    event.durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    // Each bar gets its own register so the test can tell which bar sounded.
    event.note = static_cast<uint8_t>(firstNote + (i / 4) * 10 + (i % 4) * 2);
    event.velocity = 100;
    event.probability = 100;
  }
  return melody;
}

void writePattern(MiniAcid& engine, int slot, int8_t note) {
  SynthPattern& pattern =
      engine.sceneManager().currentScene().synthABanks[0].patterns[slot];
  for (auto& step : pattern.steps) step = SynthStep{};
  for (int s = 0; s < 16; s += 4) pattern.steps[s].note = note;
}

void acceptMelody(MiniAcid& engine, int slot, const Buffer& melody) {
  if (engine.current303BankIndex(0) != 0) engine.set303BankIndex(0, 0);
  engine.set303PatternIndex(0, slot);
  engine.workingMaterial_[0].storeMelody(melody);
  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(engine.isMelodySlot(0, 0, slot));
}

void writeSong(MiniAcid& engine, const std::vector<int>& rows) {
  for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
    engine.setSongPattern(row, SongTrack::SynthA, static_cast<int16_t>(rows[row]));
  }
  engine.setSongLength(static_cast<int>(rows.size()));
}

// Drives the sequencer tick by tick (the audio thread) and runs the control
// service every 48 ticks (the UI loop). Returns the notes heard in each bar and
// the first note of each bar.
struct BarNotes {
  std::set<int> notes;
  int first = -1;
};

// The UI loop takes the audio guard only when the engine says there is work.
void uiFrame(MiniAcid& engine) {
  if (engine.songMaterialServiceDue()) engine.serviceSongMaterial();
}

std::vector<BarNotes> play(MiniAcid& engine, int bars) {
  std::vector<BarNotes> heard(static_cast<size_t>(bars));
  uiFrame(engine);
  engine.start();
  int lastNote = -1;
  bool lastActive = false;
  for (int tick = 0; tick < bars * 384; ++tick) {
    ++engine.currentTick_;
    engine.advanceTick();
    const auto& state = engine.patternPlaybackState_[0];
    const int bar = tick / 384;
    if (state.active() &&
        (!lastActive || state.activeNote() != lastNote || tick % 96 == 0)) {
      heard[bar].notes.insert(state.activeNote());
      if (heard[bar].first < 0) heard[bar].first = state.activeNote();
    }
    lastActive = state.active();
    lastNote = state.activeNote();
    if (tick % 48 == 47) uiFrame(engine);
  }
  engine.stop();
  return heard;
}

void printBars(const char* label, const std::vector<BarNotes>& bars) {
  std::printf("%s:", label);
  for (const auto& bar : bars) {
    std::printf(" [");
    for (int n : bar.notes) std::printf(" %d", n);
    std::printf(" ]");
  }
  std::printf("\n");
  std::fflush(stdout);
}

void setupProject(const char* dir, const std::string& proj) {
  const auto root = std::filesystem::temp_directory_path() / dir;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);
}

}  // namespace

int main() {
  const std::string proj = "song_slots";
  setupProject("gp_test_unified_song_slots", proj);
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  const Buffer melodyY = makeMelody(60, 1);
  {
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    writePattern(engine, kX, 40);
    writePattern(engine, kZ, 50);
    acceptMelody(engine, kY, melodyY);
    // Leave the voice on a Pattern slot, the way a user finishes editing.
    engine.set303PatternIndex(0, kX);
    engine.releaseSavedWorkingMelody_(0);
    assert(engine.rebuildPatternRuntimeEventBank());
    writeSong(engine, {kX, kY, kZ});
    engine.setSongMode(true);
    engine.setSongPosition(0);
    assert(PatternPagingService::savePage(0, engine.sceneManager().currentScene()));
    assert(engine.saveSceneToStorage());
  }

  // 1. Cold reload: X -> Y -> Z plays with no manual slot switching.
  {
    PatternPagingService::setProjectName(proj);
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    assert(engine.songModeEnabled());
    assert(engine.isMelodySlot(0, 0, kY));
    const auto bars = play(engine, 4);
    printBars("USS-1", bars);
    assert((bars[0].notes == std::set<int>{40}));
    assert(bars[1].first == 60);
    assert((bars[1].notes == std::set<int>{60, 62, 64, 66}));
    assert((bars[2].notes == std::set<int>{50}));
    assert((bars[3].notes == std::set<int>{40}));
    std::puts("USS-1 PASS: saved Pattern X -> Melody Y -> Pattern Z plays after reload");
  }

  // 2. A Melody starts from the row start, not from the global clock. Y is two
  //    bars long and sits on the third row (absolute bar 2, where the global
  //    phase would be Y's second bar); a one-bar row cuts it after bar one.
  {
    PatternPagingService::setProjectName(proj);
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    acceptMelody(engine, kY, makeMelody(60, 2));
    engine.set303PatternIndex(0, kX);
    engine.releaseSavedWorkingMelody_(0);
    writeSong(engine, {kX, kZ, kY});
    engine.setSongMode(true);
    engine.setSongPosition(0);
    const auto bars = play(engine, 4);
    printBars("USS-2", bars);
    assert(bars[2].first == 60);
    assert((bars[2].notes == std::set<int>{60, 62, 64, 66}));
    assert((bars[3].notes == std::set<int>{40}));
    std::puts("USS-2 PASS: a Melody row starts at its first event and is cut at the row end");
  }

  // 3. An unsaved Working Melody holds its voice: Song does not move the slot
  //    and does not drop the edit. DISCARD lets the voice rejoin the Song.
  {
    PatternPagingService::setProjectName(proj);
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    acceptMelody(engine, kY, melodyY);
    engine.set303PatternIndex(0, kX);
    engine.releaseSavedWorkingMelody_(0);
    writeSong(engine, {kY, kZ});
    engine.setSongMode(true);
    engine.setSongPosition(0);
    engine.serviceSongMaterial();
    assert(engine.display303LocalPatternIndex(0) == kY);
    assert(engine.workingMaterial_[0].holdsMelody());
    engine.workingMaterial_[0].melodyIfHeld()->events[0].note = 99;
    assert(engine.hasUnsavedWorkingMelody(0));

    const auto bars = play(engine, 2);
    printBars("USS-3", bars);
    assert(bars[1].first == 99);  // row Z did not replace the edited Y
    assert(engine.display303LocalPatternIndex(0) == kY);
    assert(engine.workingMaterial_[0].melody().events[0].note == 99);

    assert(engine.discardCurrentMaterial(0) == MiniAcid::DiscardResult::Discarded);
    engine.setSongPosition(1);
    engine.serviceSongMaterial();
    assert(engine.display303LocalPatternIndex(0) == kZ);
    assert(!engine.workingMaterial_[0].holdsMelody());
    std::puts("USS-3 PASS: an unsaved Melody holds its voice; DISCARD rejoins the Song");
  }

  // 4. While Song plays, a user NEXT is refused only where the Song needs the
  //    NEXT buffer: the next row brings this voice a different Melody.
  {
    PatternPagingService::setProjectName(proj);
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    acceptMelody(engine, 3, makeMelody(70, 1));
    engine.releaseSavedWorkingMelody_(0);
    writeSong(engine, {kY, kZ});
    engine.setSongMode(true);
    engine.setSongPosition(0);
    uiFrame(engine);
    engine.start();
    auto prepare = [&]() {
      return engine.prepareNextMelody(
          0, makeMelody(80, 1), engine.captureCurrentPreparationBasis(0),
          GroovePuterMaterial::IdeaClassification::Unknown);
    };
    // Next row is a Pattern: the development workflow keeps working in Song.
    assert(prepare() == MiniAcid::NextPrepareResult::Prepared);
    // Next row is another Melody: NEXT belongs to the Song.
    engine.setSongPattern(1, SongTrack::SynthA, 3);
    assert(prepare() == MiniAcid::NextPrepareResult::UnsupportedCurrentState);
    engine.stop();
    std::puts("USS-4 PASS: user NEXT is refused only where Song needs the NEXT buffer");
  }

  // 5. Enabling Song while stopped syncs the voice before START, so the first
  //    event of a Melody on the first row is not lost.
  {
    PatternPagingService::setProjectName(proj);
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    engine.set303PatternIndex(0, kX);
    engine.releaseSavedWorkingMelody_(0);
    writeSong(engine, {kY, kX});
    engine.sceneManager().setSongPosition(0);
    engine.setSongMode(true);
    const auto bars = play(engine, 2);
    printBars("USS-5", bars);
    assert(bars[0].first == 60);
    assert((bars[0].notes == std::set<int>{60, 62, 64, 66}));
    assert((bars[1].notes == std::set<int>{40}));
    std::puts("USS-5 PASS: a Melody on the first row sounds from its first event");
  }

  std::puts("Unified Song slots: PASS");
  return 0;
}
