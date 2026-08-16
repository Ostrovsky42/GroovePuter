#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class PatternEditPage;
class TB303ParamsPage;

class SynthSequencerPage : public MultiPage, public IMultiHelpFramesProvider {
 public:
  SynthSequencerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setContext(int context) override;
  void setVisualStyle(VisualStyle style) override;
  void tick() override;

  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  enum class SynthTab : uint8_t {
    Notes = 0,
    Knobs,
    More,
  };

  void setSynthTab(SynthTab tab);
  void drawTabIndicator(IGfx& gfx) const;
  const char* activeTabName() const;

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_ = 0;
  uint32_t last_tab_switch_ms_ = 0;
  SynthTab synth_tab_ = SynthTab::Notes;
  std::shared_ptr<PatternEditPage> pattern_page_;
  std::shared_ptr<TB303ParamsPage> params_page_;
  std::string fallback_title_;
};
