#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/project_page.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/state/melody_promotion.h"
#include "src/audio/pattern_paging.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;
class RecordingGfx final : public IGfx {
 public:
  std::vector<std::string> texts;
  int playheadRects = 0;
  bool contains(const std::string& s) const {
    for (const auto& text : texts) if (text.find(s) != std::string::npos) return true;
    return false;
  }
  void reset() { texts.clear(); playheadRects = 0; }
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* text) override { if (text) texts.emplace_back(text); }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor color) override {
    if (color.color24() == COLOR_STEP_HILIGHT.color24()) ++playheadRects;
  }
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
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) * 6 : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
Buffer makeMelody(int voice) {
  Buffer out{};
  out.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  out.count = 8;
  for (int i = 0; i < 8; ++i) {
    auto& e = out.events[i];
    e.startTick = i * PhraseRuntime::kTicksPerBar + 24;
    e.durationSubticks = 48 * PhraseRuntime::kSubticksPerTick;
    e.note = 48 + voice * 12 + i;
    e.velocity = 110;
    e.probability = 100;
  }
  return out;
}
int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() / "gp-project-melody-reload";
  fs::remove_all(root); fs::create_directories(root);
  fs::current_path(root); SD.setRoot(root);
  int failures = 0;
  {
    SceneStorageSdl storage;
    assert(storage.setCurrentSceneName("reload-original"));
    MiniAcid engine{44100.0f, &storage};
    engine.init(); engine.setSongMode(false);
    for (int v = 0; v < 2; ++v) {
      engine.set303PatternIndex(v, 0);
      engine.workingMaterial_[v].storeMelody(makeMelody(v));
      assert(engine.acceptMaterialWorking(v) == MiniAcid::AcceptResult::Accepted);
    }
    assert(engine.saveSceneToStorage());
    // Canonical edits made after ACCEPT must not be replaced by an older page.
    engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note = 73;
    engine.sceneManager().currentScene().drumBanks[0].patterns[0].voices[0].steps[1].hit = true;
    assert(engine.saveSceneToStorage());
    engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note = 76;
    assert(engine.autoSaveSceneRecovery());
    assert(engine.loadSceneByName("reload-original"));
    assert(engine.lastSceneLoadRecoveredAutosave());
    if (engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note != 76) {
      std::fprintf(stderr, "RECOVERY FAIL: stale page overrode autosaved canonical edits\n"); ++failures;
    }
    engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note = 73;
    assert(engine.saveSceneToStorage());
    for (int v = 0; v < 2; ++v) {
      auto unrelated = makeMelody(v);
      unrelated.lengthTicks = PhraseRuntime::kTicksPerBar; unrelated.count = 1;
      unrelated.events[0].note = 99;
      engine.workingMaterial_[v].storeMelody(unrelated);
    }
    assert(engine.loadSceneByName("reload-original"));
    for (int v = 0; v < 2; ++v) {
      const auto* restored = engine.readableWorkingMelody(v);
      if (!restored || !MelodyPromotion::sameMelody(*restored, makeMelody(v))) {
        std::fprintf(stderr, "RELOAD FAIL: voice %d did not hydrate saved 8-bar payload\n", v);
        ++failures;
      }
    }
    if (engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note != 73 ||
        !engine.sceneManager().currentScene().drumBanks[0].patterns[0].voices[0].steps[1].hit) {
      std::fprintf(stderr, "RELOAD FAIL: canonical Pattern/drum edits lost\n"); ++failures;
    }
    // Save As over an existing name must copy nonresident pages as well.
    Scene extra{};
    extra.synthABanks[0].patterns[2].steps[0].note = 81;
    assert(PatternPagingService::savePage(3, extra));
    assert(PatternPagingService::setProjectName("reload-existing"));
    extra.synthABanks[0].patterns[2].steps[0].note = 99;
    assert(PatternPagingService::savePage(3, extra));
    { std::ofstream existing("reload-existing.json"); existing << "{}"; }
    assert(PatternPagingService::setProjectName("reload-original"));
    assert(PatternPagingService::loadPage(0, engine.sceneManager().currentScene()));
    assert(engine.saveSceneAs("reload-existing"));
    assert(PatternPagingService::loadPage(3, extra));
    if (extra.synthABanks[0].patterns[2].steps[0].note != 81) {
      std::fprintf(stderr, "SAVE AS FAIL: existing target kept foreign nonresident pages\n"); ++failures;
    }
    assert(PatternPagingService::loadPage(0, engine.sceneManager().currentScene()));
    assert(engine.saveSceneAs("reload-copy"));
  }
  {
    SceneStorageSdl storage;
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    assert(storage.getCurrentSceneName() == "reload-copy");
    if (engine.sceneManager().currentScene().synthABanks[0].patterns[1].steps[0].note != 73 ||
        !engine.sceneManager().currentScene().drumBanks[0].patterns[0].voices[0].steps[1].hit) {
      std::fprintf(stderr, "BOOT FAIL: stale page overrode saved Pattern/drum edits\n"); ++failures;
    }
    for (int v = 0; v < 2; ++v) {
      const auto* restored = engine.readableWorkingMelody(v);
      if (!restored || !MelodyPromotion::sameMelody(*restored, makeMelody(v))) {
        std::fprintf(stderr, "BOOT FAIL: Save As failed to restore voice %d at 8 bars\n", v);
        ++failures;
      }
    }
    assert(engine.createNewSceneWithName("reload-blank"));
    for (int v = 0; v < 2; ++v) {
      if (engine.readableWorkingMelody(v) != nullptr ||
          engine.currentSequencedSource(v) != MiniAcid::SequencedSource::Pattern) {
        std::fprintf(stderr, "NEW FAIL: blank project retained preceding Melody v%d\n", v);
        ++failures;
      }
    }
  }
  {
    SceneStorageSdl storage;
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    for (int v = 0; v < 2; ++v) {
      engine.workingMaterial_[v].storeMelody(makeMelody(v));
      engine.activeMaterial_[v].kind = GroovePuterMaterial::MaterialKind::Melody;
    }
    RecordingGfx gfx;
    ProjectPage page{gfx, engine, AudioGuard{}};
    page.main_focus_ = ProjectPage::MainFocus::ClearProject;
    page.dialog_type_ = ProjectPage::DialogType::ConfirmClear;
    assert(page.clearProject());
    assert(!GroovePuterState::sceneDirty());
    for (int v = 0; v < 2; ++v) {
      if (engine.readableWorkingMelody(v) != nullptr ||
          engine.currentSequencedSource(v) != MiniAcid::SequencedSource::Pattern) {
        std::fprintf(stderr, "UI CLEAR FAIL: old Working Melody v%d survived Clear Project\n", v); ++failures;
      }
    }
  }
  {
    SceneStorageSdl storage;
    storage.initializeStorage();
    const auto pointer = root / "grooveputer_scene_name.txt";
    fs::remove(pointer);
    fs::create_directory(pointer); // Deterministic last-project pointer I/O failure.
    if (storage.writeScene(std::string("{}"))) {
      std::fprintf(stderr, "SAVE FAIL: last-project pointer failure reported as success\n"); ++failures;
    }
  }
  fs::remove_all(root);
  std::fprintf(stderr, "Project Melody explicit load/cold boot: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
