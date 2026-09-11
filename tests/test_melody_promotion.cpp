// M2b regression matrix after A2-B identity-bound mutation authority.
// Storage failure semantics remain unchanged; each mutation is now admitted by
// a live MaterialReference instead of a bare resident coordinate.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "src/state/melody_promotion.h"

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M2b FAIL: %s\n", message);
  ++g_failures;
}

struct FakeFs : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;
  bool present = true;
  bool failWrite = false;
  bool failRename = false;
  bool corruptOnRead = false;

  bool available() const override { return present; }
  bool exists(const char* path) const override {
    return files.find(path) != files.end();
  }
  bool write(const char* path, const uint8_t* data, size_t length) override {
    if (failWrite) return false;
    files[path].assign(data, data + length);
    return true;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    auto it = files.find(path);
    if (it == files.end()) return false;
    out = it->second;
    if (corruptOnRead && out.size() > MelodyStore::kHeaderBytes) {
      out[MelodyStore::kHeaderBytes] ^= 0xFFu;
    }
    return true;
  }
  bool rename(const char* from, const char* to) override {
    if (failRename) return false;
    auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }
  bool remove(const char* path) override { return files.erase(path) > 0; }
};

Buffer makeCandidate() {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 2;
  melody.events[0] = {0, 24 * 16, 60, 100, 100, 0, 0, 0};
  melody.events[1] = {96, 24 * 16, 67, 100, 100, 0, 0, 0};
  return melody;
}

MaterialReference ensureReference(Scene& scene, int voice, int globalSlot) {
  const MaterialAddress address{static_cast<uint8_t>(voice),
                                static_cast<uint8_t>(globalSlot)};
  const int residentSlot = GroovePuterMaterial::residentSlotFor(address);
  auto& descriptor = scene.materialSlots[voice][residentSlot];
  if (!descriptor.id.valid()) {
    descriptor.id = MaterialId{static_cast<uint32_t>(
        1 + voice * kMaxGlobalPatterns + globalSlot)};
  }
  return {address, descriptor.id};
}

MelodyPromotion::Error promote(FakeFs& fs, const std::string& project,
                               Scene& scene, int voice, int globalSlot,
                               const Buffer& candidate) {
  const MaterialReference reference =
      ensureReference(scene, voice, globalSlot);
  return MelodyPromotion::promoteResident(
      fs, project, scene, reference,
      GroovePuterMaterial::residentSlotFor(reference.address), candidate);
}

const std::string kProject = "projectA";
constexpr int kVoice = 0;
constexpr int kSlot = 5;

}  // namespace

int main() {
  using MelodyPromotion::Error;

  // 1. Happy path: payload is published before the descriptor becomes Melody.
  {
    FakeFs fs;
    Scene scene{};
    const Buffer candidate = makeCandidate();
    const Error error = promote(fs, kProject, scene, kVoice, kSlot, candidate);
    expect(error == Error::None, "a valid promotion was refused");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Melody,
           "the slot was not promoted");

    Buffer loaded{};
    expect(MelodyPromotion::loadResident(fs, kProject, kVoice, kSlot, loaded),
           "the promoted melody could not be read back");
    expect(loaded.count == candidate.count &&
               loaded.events[1].note == candidate.events[1].note,
           "the melody read back is not the one that was promoted");
    expect(!fs.exists(MelodyPromotion::tempPath(kProject, kVoice, kSlot).c_str()),
           "the temporary file was left behind after success");
  }

  // 2. No storage: fail before any payload or kind mutation.
  {
    FakeFs fs;
    fs.present = false;
    Scene scene{};
    const Error error = promote(fs, kProject, scene, kVoice, kSlot,
                                makeCandidate());
    expect(error == Error::NoStorage, "promotion without storage was allowed");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "the slot was promoted with nowhere to store the melody");
    expect(fs.files.empty(), "something was written with no storage available");
  }

  // 3. Failed write leaves Pattern and publishes nothing.
  {
    FakeFs fs;
    fs.failWrite = true;
    Scene scene{};
    const Error error = promote(fs, kProject, scene, kVoice, kSlot,
                                makeCandidate());
    expect(error == Error::WriteFailed, "a failed write was not reported");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a failed write still promoted the slot");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kVoice, kSlot).c_str()),
           "a failed write still published a payload");
  }

  // 4. Corrupted read-back is rejected and not published.
  {
    FakeFs fs;
    fs.corruptOnRead = true;
    Scene scene{};
    const Error error = promote(fs, kProject, scene, kVoice, kSlot,
                                makeCandidate());
    expect(error == Error::VerifyFailed, "a corrupted read-back was accepted");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a slot was promoted against an unverified payload");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kVoice, kSlot).c_str()),
           "an unverified payload was published");
  }

  // 5. Failed rename cannot move the descriptor.
  {
    FakeFs fs;
    fs.failRename = true;
    Scene scene{};
    const Error error = promote(fs, kProject, scene, kVoice, kSlot,
                                makeCandidate());
    expect(error == Error::PublishFailed, "a failed publish was not reported");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a slot was promoted without a published payload");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kVoice, kSlot).c_str()),
           "a payload appeared at the final path despite the failure");
  }

  // 6. Promotion remains one-way for the same identity.
  {
    FakeFs fs;
    Scene scene{};
    (void)promote(fs, kProject, scene, kVoice, kSlot, makeCandidate());
    Buffer edited = makeCandidate();
    edited.events[0].note = 71;
    const Error error = promote(fs, kProject, scene, kVoice, kSlot, edited);
    expect(error == Error::AlreadyMelody,
           "an already promoted slot was promoted again");

    Buffer loaded{};
    (void)MelodyPromotion::loadResident(fs, kProject, kVoice, kSlot, loaded);
    expect(loaded.events[0].note == 60,
           "a repeated promotion overwrote the stored melody");
  }

  // 7. Voice and slot coordinates remain independent.
  {
    FakeFs fs;
    Scene scene{};
    (void)promote(fs, kProject, scene, 0, 1, makeCandidate());
    expect(GroovePuterMaterial::residentKind(scene, 1, 1) ==
               MaterialKind::Pattern,
           "promoting synth A promoted the same slot on synth B");
    expect(GroovePuterMaterial::residentKind(scene, 0, 2) ==
               MaterialKind::Pattern,
           "promoting one slot promoted its neighbour");
    expect(MelodyPromotion::finalPath(kProject, 0, 1) !=
               MelodyPromotion::finalPath(kProject, 1, 1),
           "two voices share one payload path");
  }

  // 8. Missing durable payload reports failure rather than silent emptiness.
  {
    FakeFs fs;
    Scene scene{};
    (void)promote(fs, kProject, scene, kVoice, kSlot, makeCandidate());
    fs.remove(MelodyPromotion::finalPath(kProject, kVoice, kSlot).c_str());
    Buffer loaded{};
    expect(!MelodyPromotion::loadResident(fs, kProject, kVoice, kSlot, loaded),
           "a missing payload reported success");
  }

  // 9. Project namespaces remain independent.
  {
    FakeFs fs;
    Scene sceneA{};
    Scene sceneB{};

    Buffer melodyA = makeCandidate();
    melodyA.events[0].note = 60;
    Buffer melodyB = makeCandidate();
    melodyB.events[0].note = 72;

    expect(promote(fs, "projectA", sceneA, kVoice, kSlot, melodyA) ==
               Error::None,
           "project A promotion failed");
    expect(promote(fs, "projectB", sceneB, kVoice, kSlot, melodyB) ==
               Error::None,
           "project B promotion failed, so the slot was already taken");

    Buffer backA{};
    Buffer backB{};
    expect(MelodyPromotion::loadResident(fs, "projectA", kVoice, kSlot, backA),
           "project A melody disappeared");
    expect(MelodyPromotion::loadResident(fs, "projectB", kVoice, kSlot, backB),
           "project B melody disappeared");
    expect(backA.events[0].note == 60,
           "project B overwrote project A at the same slot");
    expect(backB.events[0].note == 72,
           "project B did not keep its own melody");
    expect(MelodyPromotion::finalPath("projectA", kVoice, kSlot) !=
               MelodyPromotion::finalPath("projectB", kVoice, kSlot),
           "two projects share one payload path");
  }

  if (g_failures == 0) {
    std::printf("M2b melody promotion: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M2b melody promotion: %d failure(s)\n", g_failures);
  return 1;
}
