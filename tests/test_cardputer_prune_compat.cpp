#include <cassert>
#include <string>
#include "src/platform/cardputer_capabilities.h"
#include "src/sampler/sample_scene_persistence.h"

int main() {
  static_assert(GROOVEPUTER_ENABLE_SAMPLER == 0, "test needs sampler removed");
  using namespace GroovePuterSampler;
  std::string loaded, saved;
  const std::string legacy = R"({"samplerPads":[{"id":42,"vol":0.7}],"samplerEnabled":true,"name":"old"})";
  assert(transformSamplerSceneString(legacy, SceneSampleFilterDirection::Load, nullptr, loaded));
  assert(loaded == legacy);
  assert(transformSamplerSceneString(loaded, SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved == legacy);
  const std::string stable = R"({"samplerPads":[{"id":42,"ref":"0123456789abcdef","vol":0.7}],"samplerEnabled":true})";
  assert(transformSamplerSceneString(stable, SceneSampleFilterDirection::Load, nullptr, loaded));
  assert(loaded.find("\"id\":0") != std::string::npos);
  // The dormant assignment survives returning to a sampler-capable build.
  loaded = R"({"samplerPads":[{"id":0,"vol":0.7}],"samplerEnabled":true})";
  assert(transformSamplerSceneString(loaded, SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved.find("0123456789abcdef") != std::string::npos);
  const std::string broken = R"({"samplerPads":[{"id":42,"ref":"broken"}]})";
  assert(!transformSamplerSceneString(broken, SceneSampleFilterDirection::Load, nullptr, loaded));
  assert(transformSamplerSceneString(R"({"samplerPads":[{"id":0}]})", SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved.find("0123456789abcdef") != std::string::npos);

  struct Reader {
    const std::string& source;
    std::size_t at = 0;
    int read() { return at < source.size() ? static_cast<unsigned char>(source[at++]) : -1; }
  };
  const std::string candidate = R"({"samplerPads":[{"id":7,"ref":"fedcba9876543210"}]})";
  {
    Reader source{candidate};
    SamplerSceneReadFilter<Reader> read(source);
    while (read.read() >= 0) {}
    // The Scene decoder rejected the candidate: do not acknowledge it.
  }
  assert(transformSamplerSceneString(R"({"samplerPads":[{"id":0}]})", SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved.find("0123456789abcdef") != std::string::npos);
  {
    Reader source{candidate};
    SamplerSceneReadFilter<Reader> read(source);
    while (read.read() >= 0) {}
    assert(!read.failed());  // storage acknowledges successful Scene decode
  }
  assert(transformSamplerSceneString(R"({"samplerPads":[{"id":0}]})", SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved.find("fedcba9876543210") != std::string::npos);
  assert(transformSamplerSceneString(legacy, SceneSampleFilterDirection::Load, nullptr, loaded));
  assert(transformSamplerSceneString(loaded, SceneSampleFilterDirection::Save, nullptr, saved));
  assert(saved == legacy);

}
