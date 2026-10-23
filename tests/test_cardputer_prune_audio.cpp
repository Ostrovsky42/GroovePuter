#include <cassert>
#include <cstdint>
#include "src/dsp/miniacid_engine.h"
#include "src/sampler/sample_scene_persistence.h"
#include "platform_sdl/scene_storage_sdl.h"
#include <filesystem>
SerialMock Serial;
SDMock SD;
int main() {
  static_assert(!GROOVEPUTER_ENABLE_SAMPLER && !GROOVEPUTER_ENABLE_TAPE && !GROOVEPUTER_ENABLE_WAVEFORM);
  MiniAcid engine(kSampleRate, nullptr);
  engine.setTestTone(true);
  int16_t audio[512]{};
  engine.generateAudioBuffer(audio, 512);
  uint16_t peak = 0;
  for (auto value : audio) {
    const int32_t wide = value;
    const auto magnitude = static_cast<uint16_t>(wide < 0 ? -wide : wide);
    if (magnitude > peak) peak = magnitude;
  }
  assert(peak > 10000 && engine.lastAudioPeak() == peak);
  assert(engine.getWaveformBuffer().count == 0);
  engine.sceneManager().currentScene().tape.mode = TapeMode::Rec;
  assert(!engine.isRecording());
  namespace fs = std::filesystem;
  const auto previous = fs::current_path();
  const auto root = fs::temp_directory_path() / "gp-pruned-new-sample-refs";
  fs::remove_all(root); fs::create_directories(root); fs::current_path(root); SD.setRoot(root);
  {
    SceneStorageSdl storage;
    assert(storage.setCurrentSceneName("pruned-ref-source"));
    MiniAcid project(kSampleRate, &storage);
    project.init();
    std::string loaded, saved;
    using namespace GroovePuterSampler;
    assert(transformSamplerSceneString(R"({"samplerPads":[{"id":42,"ref":"0123456789abcdef"}]})", SceneSampleFilterDirection::Load, nullptr, loaded));
    assert(project.createNewSceneWithName("pruned-ref-empty"));
    assert(transformSamplerSceneString(R"({"samplerPads":[{"id":0}]})", SceneSampleFilterDirection::Save, nullptr, saved));
    assert(saved.find("0123456789abcdef") == std::string::npos);
  }
  fs::current_path(previous); fs::remove_all(root);

}
