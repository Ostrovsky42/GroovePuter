// H5: SDL persistence must survive an OS-process restart, while session-only
// CURRENT/NEXT/Undo/lineage state must not.  The parent invokes this exact
// binary in child modes so no static test or engine state crosses a witness.

#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialVersionToken;
using GroovePuterMaterial::versionForMelody;

constexpr const char* kScene = "h5_process_restart";
constexpr uint8_t kVoice = 0;

struct ExpectedC {
  MaterialId id{};
  MaterialVersionToken version{};
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
};

int failures = 0;
std::filesystem::path storageRoot;

void fail(const std::string& field, const std::string& expected,
          const std::string& actual) {
  std::fprintf(stderr, "H5 FAIL %s expected=%s actual=%s\n", field.c_str(),
               expected.c_str(), actual.c_str());
  ++failures;
}

std::string number(uint32_t value) { return std::to_string(value); }

PhraseRuntime::RuntimeSynthEventBuffer melodyC() {
  PhraseRuntime::RuntimeSynthEventBuffer out{};
  out.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  out.count = 3;
  for (uint16_t i = 0; i < out.count; ++i) {
    out.events[i].startTick = static_cast<uint16_t>(i * 144);
    out.events[i].durationSubticks = 96;
    out.events[i].note = static_cast<uint8_t>(55 + i * 3);
    out.events[i].velocity = 105;
    out.events[i].probability = 100;
  }
  return out;
}

PhraseRuntime::RuntimeSynthEventBuffer melodyD() {
  auto out = melodyC();
  out.events[0].note = 72;
  return out;
}

void configureChildStorage(const std::filesystem::path& root,
                           SceneStorageSdl& storage) {
  storageRoot = root;
  std::filesystem::current_path(root);
  SD.setRoot(root);
  if (!storage.setCurrentSceneName(kScene)) {
    std::fprintf(stderr, "H5 FAIL storage scene selection\n");
    std::exit(2);
  }
}

bool writeExpected(const ExpectedC& expected) {
  std::ofstream out(storageRoot / "h5_expected_c.txt", std::ios::trunc);
  if (!out.is_open()) return false;
  out << expected.id.value << ' ' << expected.version.low << ' '
      << expected.version.high << ' ' << expected.melody.count << ' '
      << expected.melody.lengthTicks << '\n';
  for (uint16_t i = 0; i < expected.melody.count; ++i) {
    const auto& event = expected.melody.events[i];
    out << event.startTick << ' ' << event.durationSubticks << ' '
        << static_cast<unsigned>(event.note) << ' '
        << static_cast<unsigned>(event.velocity) << ' '
        << static_cast<unsigned>(event.probability) << ' '
        << static_cast<unsigned>(event.flags) << ' '
        << static_cast<unsigned>(event.fx) << ' '
        << static_cast<unsigned>(event.fxParam) << '\n';
  }
  return out.good();
}

bool readExpected(ExpectedC& expected) {
  std::ifstream in(storageRoot / "h5_expected_c.txt");
  if (!in.is_open()) return false;
  if (!(in >> expected.id.value >> expected.version.low >> expected.version.high >>
        expected.melody.count >> expected.melody.lengthTicks)) return false;
  if (expected.melody.count > PhraseRuntime::kMaxSynthEvents) return false;
  for (uint16_t i = 0; i < expected.melody.count; ++i) {
    unsigned note = 0, velocity = 0, probability = 0, flags = 0, fx = 0, fxParam = 0;
    auto& event = expected.melody.events[i];
    if (!(in >> event.startTick >> event.durationSubticks >> note >> velocity >>
          probability >> flags >> fx >> fxParam)) return false;
    event.note = static_cast<uint8_t>(note);
    event.velocity = static_cast<uint8_t>(velocity);
    event.probability = static_cast<uint8_t>(probability);
    event.flags = static_cast<uint8_t>(flags);
    event.fx = static_cast<uint8_t>(fx);
    event.fxParam = static_cast<uint8_t>(fxParam);
  }
  return true;
}

int acceptWrite(const std::filesystem::path& root) {
  SceneStorageSdl storage;
  configureChildStorage(root, storage);
  MiniAcid engine{44100.0f, &storage};
  engine.init();

  Scene& scene = engine.sceneManager().currentScene();
  scene.materialSlots[kVoice][0].id = PatternPagingService::allocateMaterialId();
  scene.materialSlots[kVoice][0].kind = MaterialKind::Pattern;
  const auto c = melodyC();
  engine.workingMaterial_[kVoice].storeMelody(c);
  if (engine.acceptMaterialWorking(kVoice) != MiniAcid::AcceptResult::Accepted ||
      !engine.saveSceneToStorage()) {
    std::fprintf(stderr, "H5 FAIL accept_C_written\n");
    return 1;
  }

  const auto& descriptor = scene.materialSlots[kVoice][0];
  const ExpectedC expected{descriptor.id, versionForMelody(c), c};
  if (!expected.id.valid() || !writeExpected(expected)) {
    std::fprintf(stderr, "H5 FAIL expected-C oracle write\n");
    return 1;
  }
  std::puts("ACCEPT_PROCESS_RESTART writer: C written");
  return 0;
}

int assertFreshRestoredC(const std::filesystem::path& root,
                         const char* witness) {
  SceneStorageSdl storage;
  configureChildStorage(root, storage);
  ExpectedC expected{};
  if (!readExpected(expected)) {
    std::fprintf(stderr, "H5 FAIL %s expected-C oracle missing\n", witness);
    return 1;
  }
  MiniAcid engine{44100.0f, &storage};
  engine.init();

  const auto& descriptor = engine.sceneManager().currentScene().materialSlots[kVoice][0];
  if (descriptor.id != expected.id) fail("MaterialId", number(expected.id.value), number(descriptor.id.value));
  if (descriptor.kind != MaterialKind::Melody) fail("representation", "Melody", "Pattern");
  if (!engine.workingMaterial_[kVoice].holdsMelody()) fail("CURRENT representation", "Melody", "not Melody");
  const auto* actual = engine.workingMaterial_[kVoice].melodyIfHeld();
  if (actual == nullptr) {
    fail("accepted event content", "C", "missing");
  } else {
    const auto actualToken = versionForMelody(*actual);
    if (actualToken != expected.version) {
      fail("version token", number(expected.version.low) + ":" + number(expected.version.high),
           number(actualToken.low) + ":" + number(actualToken.high));
    }
    if (actual->count != expected.melody.count || actual->lengthTicks != expected.melody.lengthTicks) {
      fail("accepted event shape", number(expected.melody.count) + ":" + number(expected.melody.lengthTicks),
           number(actual->count) + ":" + number(actual->lengthTicks));
    }
    for (uint16_t i = 0; i < expected.melody.count && i < actual->count; ++i) {
      if (std::memcmp(&expected.melody.events[i], &actual->events[i], sizeof(actual->events[i])) != 0) {
        fail("accepted event " + std::to_string(i), "C event", "diverged");
      }
    }
  }
  if (engine.hasPendingMaterial(kVoice)) fail("NEXT state", "empty", "queued");
  if (engine.isGoQueued(kVoice)) fail("GO state", "empty", "queued");
  if (GroovePuterUndo::undoOwner().hasUndo()) fail("Undo state", "empty", "present");
  if (engine.hasSourceAnchorSnapshot_[kVoice] ||
      engine.developmentLineage_[kVoice].sourceAnchorBasis.valid() ||
      engine.developmentLineage_[kVoice].predecessorBasis.valid()) {
    fail("lineage state", "fresh/reset", "transient lineage retained");
  }
  const auto restored = engine.sourceAnchor(kVoice);
  if (!restored.valid() || restored.version != expected.version) {
    fail("source anchor reset", number(expected.version.low), number(restored.version.low));
  }
  if (failures == 0) std::printf("%s reader: exact C restored fresh\n", witness);
  return failures == 0 ? 0 : 1;
}

int unacceptedWrite(const std::filesystem::path& root) {
  SceneStorageSdl storage;
  configureChildStorage(root, storage);
  ExpectedC expected{};
  if (!readExpected(expected)) return 1;
  MiniAcid engine{44100.0f, &storage};
  engine.init();
  const auto basis = engine.captureCurrentPreparationBasis(kVoice);
  const auto d = melodyD();
  engine.playing = true;
  const auto prepare = engine.prepareNextMelody(
      kVoice, d, basis, GroovePuterMaterial::IdeaClassification::NewIdea);
  const auto go = prepare == MiniAcid::NextPrepareResult::Prepared
      ? engine.requestGoNextMaterial(kVoice)
      : MiniAcid::GoRequestResult::NoPendingMaterial;
  if (!basis.valid() || prepare != MiniAcid::NextPrepareResult::Prepared ||
      go != MiniAcid::GoRequestResult::Queued) {
    std::fprintf(stderr, "H5 FAIL unaccepted_D basis_valid=%d prepare=%d go=%d\n",
                 basis.valid(), static_cast<int>(prepare), static_cast<int>(go));
    return 1;
  }
  engine.processSequencerEvents(101);
  if (!engine.isGoQueued(kVoice) ||
      versionForMelody(engine.currentPhraseBuffer(kVoice)) != expected.version) {
    std::fprintf(stderr, "H5 FAIL GO activated before boundary\n");
    return 1;
  }
  engine.processSequencerEvents(PhraseRuntime::kTicksPerBar);
  if (versionForMelody(engine.currentPhraseBuffer(kVoice)) != versionForMelody(d) ||
      versionForMelody(expected.melody) != expected.version) {
    std::fprintf(stderr, "H5 FAIL unaccepted D activation/canonical C divergence\n");
    return 1;
  }
  std::puts("UNACCEPTED_PROCESS_RESTART writer: D activated without ACCEPT");
  return 0;
}

std::string quote(const std::string& value) {
  std::string out = "'";
  for (char ch : value) out += ch == '\'' ? "'\\\"'\\\"'" : std::string(1, ch);
  return out + "'";
}

int runChild(const char* self, const char* phase, const std::filesystem::path& root) {
  const std::string command = quote(self) + " --child " + phase + " " + quote(root.string());
  const int result = std::system(command.c_str());
  if (result != 0) std::fprintf(stderr, "H5 FAIL child phase=%s exit=%d\n", phase, result);
  return result;
}

int parent(const char* self) {
  const auto root = std::filesystem::temp_directory_path() /
      ("grooveputer-h5-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const bool w1 = runChild(self, "accept-write", root) == 0 &&
      runChild(self, "accept-read", root) == 0;
  const bool w2 = w1 && runChild(self, "unaccepted-write", root) == 0 &&
      runChild(self, "unaccepted-read", root) == 0;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  if (error) std::fprintf(stderr, "H5 FAIL cleanup=%s\n", error.message().c_str());
  std::printf("ACCEPT_PROCESS_RESTART=%s\n", w1 ? "PASS" : "FAIL");
  std::printf("UNACCEPTED_PROCESS_RESTART=%s\n", w2 ? "PASS" : "FAIL");
  return w1 && w2 && !error ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 4 && std::string(argv[1]) == "--child") {
    const std::filesystem::path root = argv[3];
    const std::string phase = argv[2];
    if (phase == "accept-write") return acceptWrite(root);
    if (phase == "accept-read") return assertFreshRestoredC(root, "ACCEPT_PROCESS_RESTART");
    if (phase == "unaccepted-write") return unacceptedWrite(root);
    if (phase == "unaccepted-read") return assertFreshRestoredC(root, "UNACCEPTED_PROCESS_RESTART");
    return 2;
  }
  return parent(argv[0]);
}
