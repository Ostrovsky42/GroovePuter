#pragma once

#include "../ui_core.h"
#include "../ui_view_continuity.h"
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

  void captureViewContinuity(UI::UiViewContinuityState& state) const override {
    if (voice_index_ < 0 || voice_index_ >= 2) return;
    state.synthTab[voice_index_] = static_cast<uint8_t>(synth_tab_);
    state.phraseFocusBar[voice_index_] = phrase_focus_bar_;
  }

  void restoreViewContinuity(const UI::UiViewContinuityState& state) override {
    if (voice_index_ < 0 || voice_index_ >= 2) return;
    uint8_t value = state.synthTab[voice_index_];
    if (value > static_cast<uint8_t>(SynthTab::More)) value = 0;
    setSynthTab(static_cast<SynthTab>(value));
    phrase_focus_bar_ = state.phraseFocusBar[voice_index_];
  }

  void setSynthTab(SynthTab tab);
  void drawTabIndicator(IGfx& gfx) const;
  const char* activeTabName() const;

  // P3-U1 source-aware NOTES controller. Pattern remains the retained child
  // page; PHRASE gets an independent presentation/controller branch without a
  // second source flag. MiniAcid::SequencedSource is the sole source owner.
  void drawPhraseNotes(IGfx& gfx);
  bool handlePhraseNotesEvent(UIEvent& ui_event);

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_ = 0;
  uint32_t last_tab_switch_ms_ = 0;
  SynthTab synth_tab_ = SynthTab::Notes;
  uint8_t phrase_focus_bar_ = 0;
  std::shared_ptr<PatternEditPage> pattern_page_;
  std::shared_ptr<TB303ParamsPage> params_page_;
  std::string fallback_title_;
  std::string phrase_title_;
};
