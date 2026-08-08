#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "src/state/scene_revision.h"

class TB303ParamsPage : public IPage, public IMultiHelpFramesProvider {
 public:
  TB303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voiceIndex);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;

  void showMoreTab(bool more) { setActiveTab(more); }
  bool showingMoreTab() const { return more_tab_; }

  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  class KnobComponent;
  class LabelValueComponent;

  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
      GroovePuterState::markSceneMutated();
  }

  bool isTb303Engine() const;
  void cycleEngine(int direction);
  void adjustGenericParameter(int parameterIndex, int direction, bool fine);
  void adjustFocusedElement(int direction, bool fine = false);
  void initComponents();
  void layoutComponents();
  void setActiveTab(bool more);
  void updateTabFocusability();
  void rememberFocusedSlot();
  void restoreFocusedSlot();
  void focusComponent(Component* component);
  void drawTabSwitcher(IGfx& gfx, const Rect& content);
  void drawMainSummary(IGfx& gfx, const Rect& content);
  void loadModePreset(int index);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_;
  int current_preset_index_ = -1;
  bool initialized_ = false;
  std::shared_ptr<KnobComponent> cutoff_knob_;
  std::shared_ptr<KnobComponent> resonance_knob_;
  std::shared_ptr<KnobComponent> env_amount_knob_;
  std::shared_ptr<KnobComponent> env_decay_knob_;
  std::shared_ptr<LabelValueComponent> engine_type_control_;
  std::shared_ptr<LabelValueComponent> osc_control_;
  std::shared_ptr<LabelValueComponent> filter_control_;
  std::shared_ptr<LabelValueComponent> delay_control_;
  std::shared_ptr<LabelValueComponent> distortion_control_;
  bool more_tab_ = false;
  uint8_t main_focus_slot_ = 0;
  uint8_t more_focus_slot_ = 0;
  std::string title_;
};
