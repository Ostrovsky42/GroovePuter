// Alt+] / Alt+[ page switch must hydrate the TARGET page's accepted material:
// its own Melody (read from the page-aware SD address), or STEPS when the slot
// is a Pattern. Regression: rehydrate ran before setCurrentPage(target), so the
// previous page's Melody kept playing and STEPS turned read-only.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/state/melody_promotion.h"
#include "src/audio/pattern_paging.h"

SerialMock Serial;
SDMock SD;

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

namespace {

int failures = 0;
#define CHECK(cond, msg)                                    \
  do {                                                      \
    if (!(cond)) {                                          \
      std::fprintf(stderr, "FAIL: %s (%s)\n", msg, #cond);  \
      ++failures;                                           \
    }                                                       \
  } while (0)

Buffer makeMelody(int base) {
  Buffer out{};
  out.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  out.count = 4;
  for (int i = 0; i < 4; ++i) {
    auto& e = out.events[i];
    e.startTick = i * (PhraseRuntime::kTicksPerBar / 2);
    e.durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    e.note = base + i;
    e.velocity = 100;
    e.probability = 100;
  }
  return out;
}

// Mirrors MiniAcidDisplay::handlePaging_ (Switched / Created paths).
void switchPage(MiniAcid& engine, int target) {
  Scene& scene = engine.sceneManager().currentScene();
  const int current = engine.currentPageIndex();
  assert(PatternPagingService::savePage(current, scene));
  if (PatternPagingService::pageExists(target)) {
    assert(PatternPagingService::loadPage(target, scene));
  } else {
    PatternPagingService::initializeEmptyPage(scene);
    assert(PatternPagingService::savePage(target, scene));
  }
  assert(engine.rebuildPatternRuntimeEventBank());
#ifdef PAGE_REHYDRATE_OLD_ORDER  // reproduces the regression for the record
  engine.rehydrateAcceptedMaterialForPage();
  engine.setCurrentPage(static_cast<int8_t>(target));
#else
  engine.setCurrentPage(static_cast<int8_t>(target));
  engine.rehydrateAcceptedMaterialForPage();
#endif
}

void acceptMelodyOnSlot0(MiniAcid& engine, const Buffer& melody) {
  engine.set303PatternIndex(0, 0);
  engine.workingMaterial_[0].storeMelody(melody);
  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(engine.isMelodySlot(0, 0, 0));
}

void expectMelody(MiniAcid& engine, const Buffer& want, const char* what) {
  const auto* got = engine.readableWorkingMelody(0);
  CHECK(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase, what);
  CHECK(got && MelodyPromotion::sameMelody(*got, want), what);
  CHECK(engine.activeMaterial_[0].slot ==
            static_cast<uint16_t>(engine.current303GlobalSlot_(0)), what);
  CHECK(!engine.hasUnsavedWorkingMelody(0), what);
}

std::string readFile(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  const fs::path repo = fs::absolute(argc > 1 ? argv[1] : "..");

  // Source contract: the display path sets the page before rehydrating.
  {
    const std::string src = readFile(repo / "src/ui/miniacid_display.cpp");
    const auto set = src.find("mini_acid_.setCurrentPage(target);\n");
    const auto rehydrate = src.find("mini_acid_.rehydrateAcceptedMaterialForPage();");
    CHECK(set != std::string::npos && rehydrate != std::string::npos,
          "display page path must set page and rehydrate");
    CHECK(set < rehydrate, "setCurrentPage(target) must precede rehydrate");
    std::size_t n = 0;
    for (auto at = src.find("rehydrateAcceptedMaterialForPage()"); at != std::string::npos;
         at = src.find("rehydrateAcceptedMaterialForPage()", at + 1)) ++n;
    CHECK(n == 2, "both Switched and Created paths rehydrate");
  }

  const auto root = fs::temp_directory_path() / "gp-page-switch-rehydrate";
  fs::remove_all(root);
  fs::create_directories(root);
  fs::current_path(root);
  SD.setRoot(root);
  {
    SceneStorageSdl storage;
    assert(storage.setCurrentSceneName("page-rehydrate"));
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);

    const Buffer m0 = makeMelody(48);
    const Buffer m1 = makeMelody(60);
    acceptMelodyOnSlot0(engine, m0);           // page 0, A1 = Melody m0
    assert(engine.saveSceneToStorage());

    switchPage(engine, 1);                     // new empty page
    CHECK(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
          "empty page: STEPS live");
    CHECK(engine.readableWorkingMelody(0) == nullptr, "empty page: no old Melody");
    acceptMelodyOnSlot0(engine, m1);           // page 1, A1 = Melody m1

    switchPage(engine, 2);                     // page 2, A1 = Pattern with steps
    engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 45;
    CHECK(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
          "pattern page: STEPS live");

    switchPage(engine, 1);
    expectMelody(engine, m1, "page 1 hydrates its own Melody");
    switchPage(engine, 0);
    expectMelody(engine, m0, "page 0 hydrates its own Melody");
    switchPage(engine, 2);
    CHECK(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
          "back on pattern page: STEPS live");
    CHECK(engine.readableWorkingMelody(0) == nullptr, "pattern page: no Melody");
    CHECK(engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note == 45,
          "pattern page keeps its steps");

    // Melody slot whose file is missing: no data, never the previous page's tune.
    switchPage(engine, 0);
    Scene orphan = engine.sceneManager().currentScene();
    orphan.materialSlots[0][0].kind = GroovePuterMaterial::MaterialKind::Melody;
    orphan.materialSlots[0][0].id = PatternPagingService::allocateMaterialId();
    assert(PatternPagingService::savePage(3, orphan));
    switchPage(engine, 3);
    CHECK(engine.readableWorkingMelody(0) == nullptr,
          "missing Melody file: previous page's Melody must not play");
  }
  fs::remove_all(root);
  std::fprintf(stderr, "Page switch rehydrate: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
