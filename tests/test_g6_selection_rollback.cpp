#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/output/output_ownership.h"

SerialMock Serial;
SDMock SD;

class SelectionFaultStorage : public SceneStorageSdl {
public:
  std::filesystem::path root;
  bool failRead = false;
  bool failPointerCommit = false;

  bool readScene(SceneManager& manager, const SceneSynthPreparation& prepare = {},
                 bool publishSelection = false) override {
    if (getCurrentSceneName() == "target-b" && failRead) return false;
    if (getCurrentSceneName() == "target-b" && failPointerCommit) {
      const auto pointer = root / "grooveputer_scene_name.txt";
      const auto backup = root / "grooveputer_scene_name.txt.bak";
      std::filesystem::copy_file(pointer, backup,
          std::filesystem::copy_options::overwrite_existing);
      std::filesystem::remove(pointer);
      std::filesystem::create_directory(pointer);
    }
    return SceneStorageSdl::readScene(manager, prepare, publishSelection);
  }
};

static std::string readText(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), {});
}

int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() /
      ("gp-g6-selection-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  SD.setRoot(root);

  SelectionFaultStorage storage;
  storage.root = root;
  assert(storage.setCurrentSceneName("source-a"));
  MiniAcid engine(kSampleRate, &storage);
  engine.init();
  engine.setSongMode(false);
  assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,
                                    GroovePuterOutput::Mode::Internal));
  engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 73;
  assert(engine.saveSceneToStorage());
  assert(engine.saveSceneAs("target-b"));
  assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,
                                    GroovePuterOutput::Mode::Midi));
  engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 41;
  assert(engine.saveSceneToStorage());

  assert(engine.loadSceneByName("source-a"));
  assert(readText(root / "grooveputer_scene_name.txt") == "source-a");
  std::string canonicalBefore;
  assert(engine.sceneManager().writeSceneJson(canonicalBefore));
  const auto outputBefore = GroovePuterOutput::state(GroovePuterOutput::Track::SynthA);

  storage.failRead = true;
  assert(!engine.loadSceneByName("target-b"));
  std::string canonicalAfter;
  assert(engine.sceneManager().writeSceneJson(canonicalAfter));
  assert(canonicalAfter == canonicalBefore);
  assert(storage.getCurrentSceneName() == "source-a");
  assert(PatternPagingService::currentProjectName() == "source-a");
  assert(readText(root / "grooveputer_scene_name.txt") == "source-a");
  auto outputAfter = GroovePuterOutput::state(GroovePuterOutput::Track::SynthA);
  assert(outputAfter.mode == outputBefore.mode);

  storage.failRead = false;
  storage.failPointerCommit = true;
  assert(!engine.loadSceneByName("target-b"));
  canonicalAfter.clear();
  assert(engine.sceneManager().writeSceneJson(canonicalAfter));
  assert(canonicalAfter == canonicalBefore);
  assert(storage.getCurrentSceneName() == "source-a");
  assert(PatternPagingService::currentProjectName() == "source-a");
  outputAfter = GroovePuterOutput::state(GroovePuterOutput::Track::SynthA);
  assert(outputAfter.mode == outputBefore.mode);
  assert(readText(root / "grooveputer_scene_name.txt.bak") == "source-a");
  fs::remove_all(root / "grooveputer_scene_name.txt");
  SceneStorageSdl backupRecovery;
  backupRecovery.initializeStorage();
  assert(backupRecovery.getCurrentSceneName() == "source-a");

  // With storage available again, a successful load publishes both scene and pointer.
  fs::remove(root / "grooveputer_scene_name.txt.bak");
  storage.failPointerCommit = false;
  assert(engine.loadSceneByName("target-b"));
  assert(storage.getCurrentSceneName() == "target-b");
  assert(PatternPagingService::currentProjectName() == "target-b");
  assert(readText(root / "grooveputer_scene_name.txt") == "target-b");
  assert(GroovePuterOutput::state(GroovePuterOutput::Track::SynthA).mode ==
         GroovePuterOutput::Mode::Midi);

  fs::remove_all(root);
  puts("G6 load selection rollback/pointer publication: PASS");
}
