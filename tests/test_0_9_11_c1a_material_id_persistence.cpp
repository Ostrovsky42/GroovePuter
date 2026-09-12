#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"
#include "../src/state/material_identity.h"

#include <cstdio>
#include <filesystem>
#include <type_traits>
#include <utility>

SerialMock Serial;
SDMock SD;

namespace {
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::materialReferenceMatches;

constexpr int kPage = 3;
constexpr int kVoice = 0;
constexpr int kResidentSlot = 3;

bool require(bool condition, const char* message) {
  if (condition) return true;
  std::fprintf(stderr, "C1A_PERSISTENCE_FAIL: %s\n", message);
  return false;
}

template <typename Paging, typename SceneT, typename = void>
struct HasC1AIdentityPersistence : std::false_type {};

template <typename Paging, typename SceneT>
struct HasC1AIdentityPersistence<
    Paging, SceneT,
    std::void_t<decltype(Paging::allocateMaterialId()),
                decltype(std::declval<SceneT&>().materialSlots[0][0].id)>>
    : std::true_type {};

template <typename Paging, typename SceneT>
int runWitness(const std::filesystem::path& root) {
  if constexpr (!HasC1AIdentityPersistence<Paging, SceneT>::value) {
    std::fprintf(stderr,
                 "C1A_PERSISTENCE_RED: allocator or persisted MaterialId owner is missing\n");
    return 1;
  } else {
    static_assert(std::is_same<decltype(Paging::allocateMaterialId()), MaterialId>::value,
                  "allocator must return MaterialId");
    const MaterialAddress address{static_cast<uint8_t>(kVoice),
                                  static_cast<uint8_t>(kResidentSlot)};

    if (!require(Paging::setProjectName("c1a-source"), "select source project")) return 2;
    if (!require(Paging::clearProjectPages(), "clear source pages")) return 2;

    const MaterialId first = Paging::allocateMaterialId();
    if (!require(first.valid(), "first id invalid")) return 2;
    SceneT source{};
    source.materialSlots[kVoice][kResidentSlot].id = first;
    if (!require(Paging::savePage(kPage, source), "save first id")) return 2;

    SceneT loaded{};
    if (!require(Paging::loadPage(kPage, loaded), "reload first id")) return 2;
    if (!require(loaded.materialSlots[kVoice][kResidentSlot].id == first,
                 "page round-trip lost identity")) return 2;

    const MaterialReference stale{address, first};
    if (!require(Paging::setProjectName("c1a-other"), "select other project")) return 2;
    const MaterialId other = Paging::allocateMaterialId();
    if (!require(other.valid(), "other project id invalid")) return 2;

    if (!require(Paging::setProjectName("c1a-source"), "return source project")) return 2;
    const MaterialId replacement = Paging::allocateMaterialId();
    if (!require(replacement.valid() && replacement != first,
                 "project high-water reused first id")) return 2;

    loaded.materialSlots[kVoice][kResidentSlot].id = replacement;
    if (!require(Paging::savePage(kPage, loaded), "save replacement id")) return 2;
    SceneT reloaded{};
    if (!require(Paging::loadPage(kPage, reloaded), "reload replacement")) return 2;
    if (!require(!materialReferenceMatches(
                     stale, address,
                     reloaded.materialSlots[kVoice][kResidentSlot].id),
                 "stale reference resolved replacement at same address")) return 2;

    if (!require(Paging::copyProjectPages("c1a-source", "c1a-copy"),
                 "Save As failed")) return 2;
    if (!require(Paging::setProjectName("c1a-copy"), "select copy")) return 2;
    SceneT copied{};
    if (!require(Paging::loadPage(kPage, copied), "load copied page")) return 2;
    if (!require(copied.materialSlots[kVoice][kResidentSlot].id == replacement,
                 "Save As lost identity")) return 2;
    const MaterialId copiedNext = Paging::allocateMaterialId();
    if (!require(copiedNext.valid() && copiedNext != first && copiedNext != replacement,
                 "Save As high-water collided")) return 2;

    if (!require(Paging::clearProjectPages(), "clear copied pages")) return 2;
    const MaterialId afterClear = Paging::allocateMaterialId();
    if (!require(afterClear.valid() && afterClear != copiedNext &&
                 afterClear != replacement && afterClear != first,
                 "Clear reset project identity high-water")) return 2;

    const auto projectDir = root / "patterns" / "c1a-copy";
    if (!require(std::filesystem::exists(projectDir), "identity namespace vanished")) return 2;
    std::puts("C1A MaterialId project persistence: PASS");
    return 0;
  }
}
}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    "grooveputer-c1a-material-id-persistence";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  SD.setRoot(root);
  return runWitness<PatternPagingService, Scene>(root);
}
