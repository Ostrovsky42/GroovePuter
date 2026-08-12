#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/phrase/phrase_workspace.h"

class PhrasePage : public IPage {
 public:
  PhrasePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

 private:
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

  // Fixed-size/allocation-free cache for the complete bounded Phrase shape.
  // It is refreshed only when the Phrase identity, Scene revision or source
  // pattern page changes; moving the preview cursor does not rescan all bars.
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
