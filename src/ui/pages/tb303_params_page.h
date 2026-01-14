#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class Synth303ParamsPage : public IPage, public IMultiHelpFramesProvider {
 public:
  Synth303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;

  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  class KnobComponent;
  class LabelValueComponent;

  void withAudioGuard(const std::function<void()>& fn);
  void adjustFocusedElement(int direction);
  void initComponents();

  void loadModePreset(int index);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int voice_index_;
  int current_preset_index_ = -1;
  bool initialized_ = false;
  std::shared_ptr<KnobComponent> cutoff_knob_;
  std::shared_ptr<KnobComponent> resonance_knob_;
  std::shared_ptr<KnobComponent> env_amount_knob_;
  std::shared_ptr<KnobComponent> env_decay_knob_;
  std::shared_ptr<LabelValueComponent> osc_control_;
  std::shared_ptr<LabelValueComponent> filter_control_;
  std::shared_ptr<LabelValueComponent> delay_control_;
  std::shared_ptr<LabelValueComponent> distortion_control_;
  std::string title_;
};
