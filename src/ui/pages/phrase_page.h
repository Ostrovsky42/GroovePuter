#pragma once

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
  PhraseCore::SlotId parent_slot_ = PhraseCore::SlotId::A;
  PhraseCore::Role capture_role_ = PhraseCore::Role::Main;
  uint8_t capture_length_ = 4;
  uint8_t preview_bar_ = 0;
  PhraseCore::BarPreview preview_{};
  bool preview_valid_ = false;
  int preview_page_ = -1;
  uint16_t preview_phrase_id_ = PhraseCore::kNoPhraseId;
  uint32_t preview_revision_ = 0;
  std::string title_ = "PHRASE";
};
