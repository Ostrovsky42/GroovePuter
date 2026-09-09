// M2b: promoting a slot from Pattern to Melody, transactionally.
//
// The rule that matters is not "it usually works". It is that a descriptor
// saying MELODY must never exist without a readable melody behind it. A slot
// pointing at a missing file is a slot the engine will try to play and cannot,
// and no amount of later validation recovers the music.
//
// So the descriptor is the *last* thing to change, after the payload has been
// written, closed, read back and verified. Every failure before that leaves the
// slot exactly as it was: still Pattern, pattern bytes untouched.
//
// An orphaned temporary after power loss is acceptable -- it wastes a little
// space and nothing points at it. The reverse is not.

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "src/state/melody_promotion.h"

namespace {

using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
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

const std::string kProject = "projectA";
constexpr int kPage = 0;
constexpr int kVoice = 0;
constexpr int kSlot = 5;
constexpr MaterialId kId{0, 5};

}  // namespace

int main() {
  using MelodyPromotion::Error;

  // 1. The happy path publishes the payload and only then the descriptor.
  {
    FakeFs fs;
    Scene scene{};
    const Buffer candidate = makeCandidate();
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, candidate);
    expect(error == Error::None, "a valid promotion was refused");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Melody,
           "the slot was not promoted");

    Buffer loaded{};
    expect(MelodyPromotion::loadMaterial(fs, kProject, kId, loaded),
           "the promoted melody could not be read back");
    expect(loaded.count == candidate.count &&
               loaded.events[1].note == candidate.events[1].note,
           "the melody read back is not the one that was promoted");
    expect(!fs.exists(MelodyPromotion::tempPath(kProject, kId).c_str()),
           "the temporary file was left behind after success");
  }

  // 2. No card: refused before anything is written, and before the descriptor
  //    moves. Promotion is a storage commitment; without storage there is
  //    nothing to commit to.
  {
    FakeFs fs;
    fs.present = false;
    Scene scene{};
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    expect(error == Error::NoStorage, "promotion without storage was allowed");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "the slot was promoted with nowhere to store the melody");
    expect(fs.files.empty(), "something was written with no storage available");
  }

  // 3. A failed write leaves the slot a Pattern and publishes nothing.
  {
    FakeFs fs;
    fs.failWrite = true;
    Scene scene{};
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    expect(error == Error::WriteFailed, "a failed write was not reported");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a failed write still promoted the slot");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kId).c_str()),
           "a failed write still published a payload");
  }

  // 4. The read-back is what makes this transactional. A payload that does not
  //    come back as it went in must not be published.
  {
    FakeFs fs;
    fs.corruptOnRead = true;
    Scene scene{};
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    expect(error == Error::VerifyFailed, "a corrupted read-back was accepted");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a slot was promoted against an unverified payload");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kId).c_str()),
           "an unverified payload was published");
  }

  // 5. If publishing fails at the last step, the descriptor must not move.
  {
    FakeFs fs;
    fs.failRename = true;
    Scene scene{};
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    expect(error == Error::PublishFailed, "a failed publish was not reported");
    expect(GroovePuterMaterial::residentKind(scene, kVoice, kSlot) ==
               MaterialKind::Pattern,
           "a slot was promoted without a published payload");
    expect(!fs.exists(MelodyPromotion::finalPath(kProject, kId).c_str()),
           "a payload appeared at the final path despite the failure");
  }

  // 6. Promotion is one-way per slot. Re-promoting would overwrite edits.
  {
    FakeFs fs;
    Scene scene{};
    (void)MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    Buffer edited = makeCandidate();
    edited.events[0].note = 71;
    const Error error = MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, edited);
    expect(error == Error::AlreadyMelody,
           "an already promoted slot was promoted again");

    Buffer loaded{};
    (void)MelodyPromotion::loadMaterial(fs, kProject, kId, loaded);
    expect(loaded.events[0].note == 60,
           "a repeated promotion overwrote the stored melody");
  }

  // 7. Two slots, and two voices, are independent objects.
  {
    FakeFs fs;
    Scene scene{};
    constexpr MaterialId synthAId{0, 1};
    constexpr MaterialId synthBId{1, 1};
    (void)MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, synthAId, makeCandidate());
    expect(GroovePuterMaterial::residentKind(scene, 1, 1) ==
               MaterialKind::Pattern,
           "promoting synth A promoted the same slot on synth B");
    expect(GroovePuterMaterial::residentKind(scene, 0, 2) ==
               MaterialKind::Pattern,
           "promoting one slot promoted its neighbour");
    expect(MelodyPromotion::finalPath(kProject, synthAId) !=
               MelodyPromotion::finalPath(kProject, synthBId),
           "two voices share one payload path");
  }

  // 8. Deleting a published payload afterwards is a broken project; load must
  //    report failure rather than returning silence as an empty melody.
  {
    FakeFs fs;
    Scene scene{};
    (void)MelodyPromotion::promoteResident(
        fs, kProject, scene, kPage, kId, makeCandidate());
    fs.remove(MelodyPromotion::finalPath(kProject, kId).c_str());
    Buffer loaded{};
    expect(!MelodyPromotion::loadMaterial(fs, kProject, kId, loaded),
           "a missing payload reported success");
  }

  // 9. Project namespace remains part of complete persistent identity.
  {
    FakeFs fs;
    Scene sceneA{};
    Scene sceneB{};

    Buffer melodyA = makeCandidate();
    melodyA.events[0].note = 60;
    Buffer melodyB = makeCandidate();
    melodyB.events[0].note = 72;

    expect(MelodyPromotion::promoteResident(
               fs, "projectA", sceneA, kPage, kId, melodyA) == Error::None,
           "project A promotion failed");
    expect(MelodyPromotion::promoteResident(
               fs, "projectB", sceneB, kPage, kId, melodyB) == Error::None,
           "project B promotion failed, so the slot was already taken");

    Buffer backA{};
    Buffer backB{};
    expect(MelodyPromotion::loadMaterial(fs, "projectA", kId, backA),
           "project A melody disappeared");
    expect(MelodyPromotion::loadMaterial(fs, "projectB", kId, backB),
           "project B melody disappeared");
    expect(backA.events[0].note == 60,
           "project B overwrote project A at the same slot");
    expect(backB.events[0].note == 72,
           "project B did not keep its own melody");
    expect(MelodyPromotion::finalPath("projectA", kId) !=
               MelodyPromotion::finalPath("projectB", kId),
           "two projects share one payload path");
  }

  if (g_failures == 0) {
    std::printf("M2b melody promotion: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M2b melody promotion: %d failure(s)\n", g_failures);
  return 1;
}
