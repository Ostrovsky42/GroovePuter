#pragma once

#include <memory>

#include "ui_core.h"
#include "components/label_component.h"

class IAudioRecorder;
class MultiPageTitleIndicator;
class PageHintNavigator;
class TransportBpmHeader;

class MiniAcidDisplay {
public:
  MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid);
  ~MiniAcidDisplay();
  void setAudioGuard(AudioGuard guard);
  void setAudioRecorder(IAudioRecorder* recorder);
  void update();
  void nextPage();
  void previousPage();
  void dismissSplash();
  bool handleEvent(UIEvent event);

private:
  bool _handleGlobalKeyEvent(UIEvent& event);
  void initMuteButtons(int x, int y, int w, int h);
  void drawMutesSection(int x, int y, int w, int h);
  void drawSplashScreen();
  bool translateToApplicationEvent(UIEvent& event);
  bool dispatchMultiPageEvent(ApplicationEventType type);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  int page_index_ = 0;
  unsigned long splash_start_ms_ = 0;
  bool splash_active_ = true;
  bool help_dialog_visible_ = false;
  std::unique_ptr<MultiPageHelpDialog> help_dialog_;

  AudioGuard audio_guard_;
  IAudioRecorder* audio_recorder_ = nullptr;
  std::vector<std::unique_ptr<IPage>> pages_;
  Container mute_buttons_container_;
  bool mute_buttons_initialized_ = false;
  std::unique_ptr<PageHintNavigator> page_hint_navigator_;
  std::unique_ptr<TransportBpmHeader> transport_bpm_header_;
  std::unique_ptr<MultiPageTitleIndicator> multipage_indicator_;
  std::unique_ptr<LabelComponent> title_label_;
};
