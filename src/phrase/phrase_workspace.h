#pragma once

#include <cstdint>
#include <utility>

#include "phrase_core.h"
#include "src/state/scene_revision.h"

namespace PhraseWorkspace {

struct CaptureRequest {
  PhraseCore::SlotId targetSlot = PhraseCore::SlotId::A;
  uint8_t sourceSongSlot = 0;
  uint8_t startRow = 0;
  uint8_t lengthBars = 1;
  PhraseCore::Role role = PhraseCore::Role::Main;
  PhraseCore::Source source = PhraseCore::Source::InternalPattern;
  uint8_t trackMask = PhraseCore::kAllTracks;
};

struct DeriveRequest {
  PhraseCore::SlotId targetSlot = PhraseCore::SlotId::B;
  PhraseCore::SlotId parentSlot = PhraseCore::SlotId::A;
  PhraseCore::Role role = PhraseCore::Role::Variation;
};

struct WriteRequest {
  PhraseCore::SlotId sourceSlot = PhraseCore::SlotId::A;
  uint8_t destinationSongSlot = 0;
  uint8_t startRow = 0;
  bool overwrite = false;
};

inline const char* errorName(PhraseCore::Error error) {
  switch (error) {
    case PhraseCore::Error::None: return "OK";
    case PhraseCore::Error::InvalidSlot: return "SLOT";
    case PhraseCore::Error::InvalidLength: return "LENGTH";
    case PhraseCore::Error::InvalidTrackMask: return "TRACKS";
    case PhraseCore::Error::InvalidSongSlot: return "SONG";
    case PhraseCore::Error::InvalidSource: return "SOURCE";
    case PhraseCore::Error::InvalidRole: return "ROLE";
    case PhraseCore::Error::RegionOutOfRange: return "RANGE";
    case PhraseCore::Error::EmptyRegion: return "EMPTY";
    case PhraseCore::Error::InvalidParent: return "PARENT";
    case PhraseCore::Error::DestinationOccupied: return "OCCUPIED";
    case PhraseCore::Error::InvalidPhrase: return "EMPTY";
  }
  return "ERROR";
}

inline PhraseCore::SlotSummary summary(const Scene& scene,
                                       PhraseCore::SlotId slot) {
  return PhraseCore::summarize(scene.phraseBank, slot);
}

inline bool barPreview(const Scene& scene,
                       int currentPageIndex,
                       PhraseCore::SlotId slot,
                       uint8_t bar,
                       PhraseCore::BarPreview& output) {
  const PhraseCore::PhraseSlot* phrase =
      PhraseCore::slotAt(scene.phraseBank, slot);
  return phrase && PhraseCore::buildBarPreview(
                       *phrase, bar, scene, currentPageIndex, output);
}

// Commit must execute its callable synchronously under the repository's
// established AudioGuard. The command owns validation and advances Scene
// revision exactly once after a successful mutation.
template <typename Commit>
PhraseCore::Result capture(Scene& scene,
                           const CaptureRequest& request,
                           Commit&& commit) {
  PhraseCore::Result result{};
  result.slot = request.targetSlot;
  if (request.sourceSongSlot > 1) {
    result.error = PhraseCore::Error::InvalidSongSlot;
    return result;
  }

  auto&& guardedCommit = commit;
  guardedCommit([&]() {
    result = PhraseCore::captureSongRegion(
        scene.phraseBank,
        request.targetSlot,
        scene.songs[request.sourceSongSlot],
        request.sourceSongSlot,
        request.startRow,
        request.lengthBars,
        request.role,
        request.source,
        request.trackMask);
  });
  if (result) GroovePuterState::markSceneMutated();
  return result;
}

template <typename Commit>
PhraseCore::Result derive(Scene& scene,
                          const DeriveRequest& request,
                          Commit&& commit) {
  PhraseCore::Result result{};
  result.slot = request.targetSlot;
  auto&& guardedCommit = commit;
  guardedCommit([&]() {
    result = PhraseCore::deriveReferenceView(
        scene.phraseBank,
        request.targetSlot,
        request.parentSlot,
        request.role);
  });
  if (result) GroovePuterState::markSceneMutated();
  return result;
}

template <typename Commit>
PhraseCore::Result clear(Scene& scene,
                         PhraseCore::SlotId slot,
                         Commit&& commit) {
  PhraseCore::Result result{};
  result.slot = slot;
  if (!PhraseCore::summarize(scene.phraseBank, slot).valid) {
    result.error = PhraseCore::Error::InvalidPhrase;
    return result;
  }

  auto&& guardedCommit = commit;
  guardedCommit([&]() { result = PhraseCore::clear(scene.phraseBank, slot); });
  if (result) GroovePuterState::markSceneMutated();
  return result;
}

template <typename Commit>
PhraseCore::Result writeToSong(Scene& scene,
                               const WriteRequest& request,
                               Commit&& commit) {
  PhraseCore::Result result{};
  result.slot = request.sourceSlot;
  if (request.destinationSongSlot > 1) {
    result.error = PhraseCore::Error::InvalidSongSlot;
    return result;
  }

  auto&& guardedCommit = commit;
  guardedCommit([&]() {
    result = PhraseCore::writeToSong(
        scene.phraseBank,
        request.sourceSlot,
        scene.songs[request.destinationSongSlot],
        request.startRow,
        request.overwrite);
  });
  if (result) GroovePuterState::markSceneMutated();
  return result;
}

}  // namespace PhraseWorkspace
