#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "src/state/scene_revision.h"

struct SampleFileInfo;

class SamplerPage : public IPage {
 public:
  SamplerPage(MiniAcid& mini_acid, AudioGuard audio_guard);
  SamplerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
  }

 private:
  class LabelValueComponent;

  void initComponents();
  void adjustFocusedElement(int direction);
  bool assignIndexedSample(const SampleFileInfo& candidate);
  bool selectIndexedSample(int direction);
  bool clearCurrentPad();
  void toggleSampleLayer();
  int assignedPadCount() const;
  void prelisten();

  void openSampleBrowser();
  void closeSampleBrowser();
  void refreshSampleBrowser();
  void drawSampleBrowser(IGfx& gfx);
  bool handleSampleBrowserEvent(UIEvent& ui_event);
  bool activateSampleBrowserSelection();
  bool browserGoParent();

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  std::string title_ = "SAMPLES";

  bool initialized_ = false;
  int current_pad_ = 0;

  // 0.9.3 exposes only the eight sampler pads wired into the drum sequencer.
  // Pads 9..16 remain internal/reserved until later productization.
  static constexpr int kRecoveredPadCount = 8;

  // Folder browser is a control-side view over SampleIndex. It never rescans
  // SD and never replaces the global registry while the page is active.
  bool sample_browser_open_ = false;
  std::string browser_dir_;
  std::vector<std::string> browser_subdirs_;
  std::vector<const SampleFileInfo*> browser_files_;
  int browser_selection_ = 0;
  int browser_scroll_offset_ = 0;

  std::shared_ptr<LabelValueComponent> layer_ctrl_;
  std::shared_ptr<LabelValueComponent> pad_ctrl_;
  std::shared_ptr<LabelValueComponent> file_ctrl_;
  std::shared_ptr<LabelValueComponent> volume_ctrl_;
  std::shared_ptr<LabelValueComponent> pitch_ctrl_;
  std::shared_ptr<LabelValueComponent> start_ctrl_;
  std::shared_ptr<LabelValueComponent> end_ctrl_;
  std::shared_ptr<LabelValueComponent> loop_ctrl_;
  std::shared_ptr<LabelValueComponent> reverse_ctrl_;
  std::shared_ptr<LabelValueComponent> choke_ctrl_;
};
