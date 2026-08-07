#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class BankSelectionBarComponent;
class PatternSelectionBarComponent;

// Keeps the previous MultiPage event path available after the source-level
// handler is renamed to handleEventLegacy by the input-lock wrapper.
class DrumSequencerLegacyMultiPage : public MultiPage {
 public:
  bool handleEventLegacy(UIEvent& ui_event) {
    return MultiPage::handleEvent(ui_event);
  }
};

class DrumSequencerPage : public DrumSequencerLegacyMultiPage,
                          public IMultiHelpFramesProvider {
 public:
  DrumSequencerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;
  void setContext(int context) override; // context: (voice << 8) | step

 private:
  bool handleEventLegacy(UIEvent& ui_event);
};
