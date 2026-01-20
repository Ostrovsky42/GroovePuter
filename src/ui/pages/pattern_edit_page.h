#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class BankSelectionBarComponent;
class PatternSelectionBarComponent;

class PatternEditPage : public MultiPage, public IMultiHelpFramesProvider {
 public:
  PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index);
  const std::string & getTitle() const override;
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  std::string title_;
};
