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
  // `coreMode` selects which of the two independently-reachable pages this
  // instance is. PHRASE (coreMode=false) is the generated-Phrase product
  // workflow; PHRASE CORE (coreMode=true) is the legacy capture/derive/write
  // workspace. The two are separate objects with separate placement state.
  PhrasePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard,
             bool coreMode);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

  // Consumes a one-shot SONG -> PHRASE placement handoff carried through the
  // existing IPage transition context (see MiniAcidDisplay::transitionToPage_
  // / IPage::requestPageTransition). context == row+1 requests EXPLICIT at
  // `row`; context == 0 (normal entry) resets to APPEND. No-op on the CORE
  // instance, which keeps its own unrelated destination_row_ semantics.
  void onEnter(int context) override;

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

  // Product Generated Phrase view. These functions manipulate only the
  // session request/read-model owners; musical policy remains in P1R/I1.
  //
  // NEXT REQUEST (LENGTH/DEPTH/TO) and LAST ACCEPTED (BAR/activity) are
  // different objects -- see spec sections 1-2. TO placement is a two-mode
  // state machine (section 7), never seeded from transport position.
  enum class PlacementMode : uint8_t { Append, Explicit };
  enum class Admissibility : uint8_t { Free, Occupied, NoRoom };
  enum class ProductFocus : uint8_t { Length, Depth, To, Bar };

  struct BarActivity {
    uint16_t synthAMask = 0;
    uint16_t synthBMask = 0;
    uint16_t drumMask = 0;
    bool contentAvailable = false;
  };

  void drawProductView(IGfx& gfx);
  bool handleProductEvent(UIEvent& ui_event);
  void cycleRequestedLength(int delta);
  void cycleProductBar(int delta);
  bool focusProductBar();

  // APPEND always resolves against the current authoritative Song logical
  // end (MiniAcid::songLength()) -- never a cached/previous destination.
  int resolvedAppendRow() const;
  int resolvedToRow() const;
  void adjustToField(int delta);
  bool handleToEnter();

  // FREE/OCCUPIED/NO ROOM mirrors the exact predicate generation itself
  // uses (PhraseGenerator::songRowsAreAvailable + Song::kMaxPositions) --
  // it must never diverge from what G will actually do.
  Admissibility admissibilityFor(int row, int bars) const;

  void cycleProductFocus(int delta);
  bool hasLiveBarFocus() const;

  // Structural liveness is a pure function of the stored candidate plus
  // current Song content: every accepted bar's Synth A anchor must still
  // hold its expected generated global pattern. Page-independent by
  // construction (no currentPageIndex() dependency) -- see spec sections
  // 12-14.
  bool acceptedLive() const;

  // Activity strips read current Song/pattern material, never the
  // snapshot. Content for a bar can only be read when the candidate's page
  // is the currently loaded page (only one page's pattern banks are
  // resident at a time); otherwise this honestly reports unavailable
  // rather than fabricating a mask -- see spec section 20 and the existing
  // PhraseCore::buildBarPreview cross-page convention it mirrors.
  BarActivity readAcceptedBarActivity(uint8_t bar) const;

  // Retained PhraseCore workspace. Its capture length remains independent of
  // the Generated Phrase request length exposed by the product view.
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

  // Fixed at construction: PHRASE (false) vs PHRASE CORE (true) are two
  // separate page instances, never a runtime-toggled view of one object.
  const bool core_mode_;

  uint8_t product_bar_cursor_ = 0;
  ProductFocus product_focus_ = ProductFocus::Length;

  // NEXT REQUEST placement session state (product view only). Discarded
  // whenever PHRASE is (re)entered without a fresh SONG handoff and without
  // an intervening successful G -- see spec section 7.
  PlacementMode placement_mode_ = PlacementMode::Append;
  int16_t explicit_row_ = 0;

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
