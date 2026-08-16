#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/phrase/phrase_workspace.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

class PhrasePage : public IPage {
 public:
  PhrasePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

 private:
  template <typename PrepareFn>
  PhraseCore::Result commitPhraseMutation(PrepareFn&& prepare) {
    using GroovePuterUndo::PhraseUndoPayload;
    using GroovePuterUndo::UndoKind;
    SceneManager& manager = mini_acid_.sceneManager();
    const int page = manager.currentPageIndex();
    PhraseUndoPayload before{};
    before.pageIndex = page >= 0 && page < kMaxPages
        ? static_cast<uint8_t>(page)
        : static_cast<uint8_t>(kMaxPages);
    before.before = manager.currentScene().phraseBank;
    PhraseCore::PhraseBank after = before.before;
    PhraseCore::Result result = std::forward<PrepareFn>(prepare)(after);
    if (!result || GroovePuterUndo::samePhraseBank(before.before, after)) return result;
    if (!GroovePuterUndo::phraseUndoTargetAvailable(manager, before)) return result;
    GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Phrase, before, [&]() {
          const auto apply = [&]() { manager.currentScene().phraseBank = after; };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
    return result;
  }

  template <typename PrepareFn>
  PhraseCore::Result commitSongMutation(PrepareFn&& prepare) {
    using GroovePuterUndo::SongUndoPayload;
    using GroovePuterUndo::UndoKind;
    SceneManager& manager = mini_acid_.sceneManager();
    SongUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSongUndo(manager, before)) {
      PhraseCore::Result failed{};
      failed.error = PhraseCore::Error::InvalidSongSlot;
      return failed;
    }
    Song after = before.before;
    PhraseCore::Result result = std::forward<PrepareFn>(prepare)(after);
    if (!result || GroovePuterUndo::sameSong(before.before, after)) return result;
    if (!GroovePuterUndo::songUndoTargetAvailable(manager, before)) return result;
    GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Song, before, [&]() {
          const auto apply = [&]() {
            manager.currentScene().songs[before.songSlot] = after;
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
    return result;
  }

  bool undoPreparedOwnedState();

  static PhraseCore::Role defaultRoleForSlot(PhraseCore::SlotId slot);
  static PhraseCore::SlotId slotFromIndex(int index);
  static int indexFromSlot(PhraseCore::SlotId slot);

  void selectSlot(int index);
  void cycleLength(int delta);
  void cycleRole(int delta);
  void cyclePreviewBar(int delta);
  void cycleParent(int delta);
  void cycleDestinationRow(int delta);
  bool captureCurrentRegion();
  bool generatePhraseToSong();
  bool deriveFromParent();
  bool writeToCurrentRow(bool overwrite);
  bool clearCurrentSlot();
  void invalidatePreview();
  void refreshPreview();
  void showResult(const char* action, const PhraseCore::Result& result);

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  PhraseCore::SlotId selected_slot_ = PhraseCore::SlotId::A;
  PhraseCore::SlotId parent_slot_ = PhraseCore::SlotId::B;
  PhraseCore::Role capture_role_ = PhraseCore::Role::Main;
  uint8_t capture_length_ = 4;
  uint8_t preview_bar_ = 0;
  uint8_t destination_row_ = 0;

  std::array<PhraseCore::BarPreview, PhraseCore::kMaxBars> bar_previews_{};
  std::array<bool, PhraseCore::kMaxBars> bar_preview_valid_{};
  uint8_t cached_preview_bars_ = 0;

  PhraseCore::BarPreview preview_{};
  bool preview_valid_ = false;
  int preview_page_ = -1;
  uint16_t preview_phrase_id_ = PhraseCore::kNoPhraseId;
  uint32_t preview_revision_ = 0;
  std::string title_ = "PHRASE";
};
