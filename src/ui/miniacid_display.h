#pragma once

#include <memory>

#include "ui_core.h"
#include "cassette_skin.h"
#include "global_help_overlay.h"

class IAudioRecorder;

class MiniAcidDisplay {
public:
  MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid);
  ~MiniAcidDisplay();
  void setAudioGuard(AudioGuard guard);
  void setAudioRecorder(IAudioRecorder* recorder);
  void update();
  
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }

  void nextPage();
  void previousPage();
  void goToPage(int index);
  void togglePreviousPage();
  void dismissSplash();
  bool handleEvent(UIEvent event);

private:
  void initMuteButtons(int x, int y, int w, int h);
  void initPageHint(int x, int y, int w);
  void drawMutesSection(int x, int y, int w, int h);
  int drawPageTitle(int x, int y, int w, const char* text);
  void drawSplashScreen();
  void drawDebugOverlay();
  bool translateToApplicationEvent(UIEvent& event);
  void applyPageBounds_();
  void syncVisualStyle_();
  
  // Lazy page loading
  static constexpr int kPageCount = 11;
  std::unique_ptr<IPage> createPage_(int index);
  IPage* getPage_(int index); // Returns existing or creates on-demand

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  int page_index_ = 0;
  int previous_page_index_ = 0;  // For Backspace/` toggle
  unsigned long splash_start_ms_ = 0;
  bool splash_active_ = true;
  bool help_dialog_visible_ = false;
  std::unique_ptr<MultiPageHelpDialog> help_dialog_;
  GlobalHelpOverlay global_help_overlay_;

  AudioGuard audio_guard_;
  IAudioRecorder* audio_recorder_ = nullptr;
  std::vector<std::unique_ptr<IPage>> pages_;
  std::unique_ptr<IPage> help_page_;  // Separate help page for 'h' key
  Container mute_buttons_container_;
  bool mute_buttons_initialized_ = false;
  Container page_hint_container_;
  bool page_hint_initialized_ = false;
  
  // Cassette skin wrapper
  std::unique_ptr<CassetteSkin> skin_;
  HeaderState buildHeaderState() const;
  FooterState buildFooterState() const;
  
  void showToast(const char* msg, int durationMs = 1500);
  void updateCyclePulse_();

private:
  char toastMsg_[32] = {0};
  unsigned long toastEndTime_ = 0;
  void drawToast();
  uint32_t last_cycle_pulse_counter_ = 0;
  unsigned long cycle_pulse_until_ms_ = 0;
  VisualStyle applied_visual_style_ = VisualStyle::MINIMAL;
  bool visual_style_initialized_ = false;
};
