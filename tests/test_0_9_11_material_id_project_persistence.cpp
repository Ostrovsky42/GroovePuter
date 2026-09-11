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
  std::fprintf(stderr, "MATERIAL_ID_PROJECT_PERSISTENCE_FAIL: %s\n", message);
  return false;
}

template <typename Paging, typename SceneT, typename = void>
struct HasProjectMaterialIdentityPersistence : std::false_type {};

template <typename Paging, typename SceneT>
struct HasProjectMaterialIdentityPersistence<
    Paging,
    SceneT,
    std::void_t<decltype(Paging::allocateMaterialId()),
                decltype(std::declval<SceneT&>().materialIds[0][0])>>
    : std::true_type {};

template <typename Paging, typename SceneT>
int runProjectPersistenceWitness(const std::filesystem::path& root) {
  if constexpr (!HasProjectMaterialIdentityPersistence<Paging, SceneT>::value) {
    std::fprintf(
        stderr,
        "MATERIAL_ID_PROJECT_PERSISTENCE_RED: project-scoped allocator or page MaterialId persistence owner is missing\n");
    return 1;
  } else {
    static_assert(
        std::is_same<decltype(Paging::allocateMaterialId()), MaterialId>::value,
        "allocateMaterialId() must return the independent MaterialId type");

    const MaterialAddress address{static_cast<uint8_t>(kVoice),
                                  static_cast<uint8_t>(kResidentSlot)};

    if (!require(Paging::setProjectName("identity-source"),
                 "cannot select source project")) return 2;
    if (!require(Paging::clearProjectPages(),
                 "cannot clear source project pages")) return 2;

    const MaterialId idM = Paging::allocateMaterialId();
    if (!require(idM.valid(), "first allocated id is invalid")) return 2;

    SceneT source{};
    source.materialIds[kVoice][kResidentSlot] = idM;
    if (!require(Paging::savePage(kPage, source),
                 "cannot save page carrying idM")) return 2;

    SceneT loadedM{};
    if (!require(Paging::loadPage(kPage, loadedM),
                 "cannot reload page carrying idM")) return 2;
    if (!require(loadedM.materialIds[kVoice][kResidentSlot] == idM,
                 "page round-trip lost idM")) return 2;

    const MaterialReference staleRef{address, idM};

    // Switching projects must not reset the source project's identity high-water.
    if (!require(Paging::setProjectName("identity-other"),
                 "cannot select other project")) return 2;
    const MaterialId otherProjectId = Paging::allocateMaterialId();
    if (!require(otherProjectId.valid(), "other project allocation is invalid"))
      return 2;

    if (!require(Paging::setProjectName("identity-source"),
                 "cannot return to source project")) return 2;
    const MaterialId idN = Paging::allocateMaterialId();
    if (!require(idN.valid(), "replacement allocation is invalid")) return 2;
    if (!require(idN != idM,
                 "same project reused idM after project switch")) return 2;

    SceneT replacement = loadedM;
    replacement.materialIds[kVoice][kResidentSlot] = idN;
    if (!require(Paging::savePage(kPage, replacement),
                 "cannot save replacement idN at the same address")) return 2;

    SceneT loadedN{};
    if (!require(Paging::loadPage(kPage, loadedN),
                 "cannot reload replacement idN")) return 2;
    if (!require(loadedN.materialIds[kVoice][kResidentSlot] == idN,
                 "page round-trip lost idN")) return 2;
    if (!require(!materialReferenceMatches(
                     staleRef, address,
                     loadedN.materialIds[kVoice][kResidentSlot]),
                 "stale M@A reference resolved replacement N@A")) return 2;

    // Save As must copy both page IDs and allocator high-water. Otherwise the
    // first allocation in the copied project can collide with copied material.
    if (!require(Paging::copyProjectPages("identity-source", "identity-copy"),
                 "cannot copy project identity state")) return 2;
    if (!require(Paging::setProjectName("identity-copy"),
                 "cannot select copied project")) return 2;

    SceneT copied{};
    if (!require(Paging::loadPage(kPage, copied),
                 "copied project page cannot be loaded")) return 2;
    if (!require(copied.materialIds[kVoice][kResidentSlot] == idN,
                 "Save As lost copied material id")) return 2;

    const MaterialId copiedNext = Paging::allocateMaterialId();
    if (!require(copiedNext.valid(), "copied project allocation is invalid"))
      return 2;
    if (!require(copiedNext != idM && copiedNext != idN,
                 "Save As allocator collided with copied material ids")) return 2;

    // Clearing pages in the same project namespace must not reset the identity
    // high-water: stale references can outlive a page clear in runtime/UI state.
    if (!require(Paging::clearProjectPages(),
                 "cannot clear copied project pages")) return 2;
    const MaterialId afterClear = Paging::allocateMaterialId();
    if (!require(afterClear.valid(), "post-clear allocation is invalid")) return 2;
    if (!require(afterClear != idM && afterClear != idN && afterClear != copiedNext,
                 "Clear reused an id inside the same project namespace")) return 2;

    const std::filesystem::path projectDir =
        root / "patterns" / "identity-copy";
    if (!require(std::filesystem::exists(projectDir),
                 "project namespace directory disappeared")) return 2;

    std::puts("MaterialId project persistence: PASS");
    return 0;
  }
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "grooveputer-material-id-project-persistence";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  SD.setRoot(root);

  return runProjectPersistenceWitness<PatternPagingService, Scene>(root);
}
