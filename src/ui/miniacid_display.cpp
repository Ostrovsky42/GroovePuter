#include "miniacid_display.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <utility>
#include <string>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <chrono>
#endif

#include "../audio/audio_recorder.h"
#include "ui_colors.h"
#include "ui_utils.h"
#include "pages/drum_sequencer_page.h"
#include "pages/help_page.h"
#include "pages/help_dialog.h"
#include "pages/pattern_edit_page.h"
#include "pages/project_page.h"
#include "pages/song_page.h"
#include "pages/tb303_params_page.h"
#include "pages/waveform_page.h"
#include "pages/sampler_page.h"
#include "pages/tape_page.h"
#include "pages/mode_page.h"
#include "pages/settings_page.h"
#include "components/mute_button.h"
#include "components/page_hint.h"

namespace {
unsigned long nowMillis() {
#if defined(ARDUINO)
  return millis();
#else
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
  return static_cast<unsigned long>(elapsed.count());
#endif
}
} // namespace

CassetteTheme g_currentTheme = CassetteTheme::WarmTape;

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
  splash_start_ms_ = nowMillis();
  gfx_.setFont(GfxFont::kFont5x7);

  // Initialize cassette skin with WarmTape theme
  skin_ = std::make_unique<CassetteSkin>(gfx_, g_currentTheme);

  pages_.push_back(std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, audio_guard_, 0));
  pages_.push_back(std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 0));
  pages_.push_back(std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, audio_guard_, 1));
  pages_.push_back(std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 1));
  pages_.push_back(std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<SongPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<ProjectPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<WaveformPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<SamplerPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<TapePage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<SettingsPage>(gfx_, mini_acid_));
  pages_.push_back(std::make_unique<HelpPage>());
}

MiniAcidDisplay::~MiniAcidDisplay() = default;

void MiniAcidDisplay::setAudioGuard(AudioGuard guard) {
  audio_guard_ = std::move(guard);
}

void MiniAcidDisplay::setAudioRecorder(IAudioRecorder* recorder) {
  audio_recorder_ = recorder;
}

void MiniAcidDisplay::dismissSplash() {
  splash_active_ = false;
}

void MiniAcidDisplay::nextPage() {
  page_index_ = (page_index_ + 1) % static_cast<int>(pages_.size());
  help_dialog_visible_ = false;
  help_dialog_.reset();
}

void MiniAcidDisplay::previousPage() {
  page_index_ = (page_index_ - 1 + static_cast<int>(pages_.size())) % static_cast<int>(pages_.size());
  help_dialog_visible_ = false;
  help_dialog_.reset();
}

void MiniAcidDisplay::update() {
  if (splash_active_) {
    unsigned long now = nowMillis();
    if (now - splash_start_ms_ >= 5000UL) splash_active_ = false;
  }
  if (splash_active_) {
    drawSplashScreen();
    return;
  }

  gfx_.setFont(GfxFont::kFont5x7);
  gfx_.startWrite();

  // Advance skin animation
  if (skin_) {
    skin_->tick();
  }

  // Draw cassette skin background with dither pattern
  if (skin_) {
    skin_->drawBackground();
    skin_->drawHeader(buildHeaderState());
    
    // Panel frame around content area (leave room for mutes at bottom)
    int margin = 4;
    int mutes_h = 14;  // Reduced from 20 to save vertical space
    Rect panelBounds(margin, skin_->headerHeight() + margin,
                     gfx_.width() - margin * 2,
                     gfx_.height() - skin_->headerHeight() - skin_->footerHeight() - mutes_h - margin * 2);
    skin_->drawPanelFrame(panelBounds);
  }

  // Use skin's content bounds for page rendering
  int mutes_h = 14;
  Rect contentBounds = skin_ ? 
      Rect(skin_->contentBounds().x, skin_->contentBounds().y,
           skin_->contentBounds().w, skin_->contentBounds().h - mutes_h) :
      Rect(4, 4, gfx_.width() - 8, gfx_.height() - 8);
  gfx_.setTextColor(skin_ ? skin_->palette().ink : COLOR_WHITE);

  if (pages_[page_index_]) {
    // Draw page title inside content area
    const char* title = pages_[page_index_]->getTitle().c_str();
    int title_h = gfx_.fontHeight() + 2;
    gfx_.setTextColor(skin_ ? skin_->palette().accent : COLOR_ACCENT);
    gfx_.drawText(contentBounds.x, contentBounds.y, title);
    gfx_.setTextColor(skin_ ? skin_->palette().ink : COLOR_WHITE);
    
    // Page content below title
    Rect pageBounds(contentBounds.x, contentBounds.y + title_h,
                    contentBounds.w, contentBounds.h - title_h);
    pages_[page_index_]->setBoundaries(pageBounds);
    pages_[page_index_]->draw(gfx_);
    
    if (help_dialog_visible_ && help_dialog_) {
      help_dialog_->setBoundaries(pageBounds);
      help_dialog_->draw(gfx_);
    }
  }

  // Draw mute buttons section (above footer)
  int margin = 4;
  int mutes_y = gfx_.height() - (skin_ ? skin_->footerHeight() : 0) - mutes_h;
  drawMutesSection(margin, mutes_y, gfx_.width() - margin * 2, mutes_h);

  // Draw footer with reels
  if (skin_) {
    skin_->drawFooterReels(buildFooterState());
  }

  // Page hint for navigation with [ ]
  int hint_w = textWidth(gfx_, "[< 0/0 >]");
  if (!page_hint_initialized_) {
    initPageHint(gfx_.width() - hint_w - margin, margin + 2, hint_w);
  }
  page_hint_container_.draw(gfx_);
  
  // enable debug overlay by default for now
  drawDebugOverlay();

  gfx_.flush();
  gfx_.endWrite();
}

void MiniAcidDisplay::drawSplashScreen() {
  gfx_.startWrite();
  gfx_.clear(COLOR_BLACK);

  auto centerText = [&](int y, const char* text, IGfxColor color) {
    if (!text) return;
    int x = (gfx_.width() - textWidth(gfx_, text)) / 2;
    if (x < 0) x = 0;
    gfx_.setTextColor(color);
    gfx_.drawText(x, y, text);
  };

  gfx_.setFont(GfxFont::kFreeSerif18pt);
  int title_h = gfx_.fontHeight();
  gfx_.setFont(GfxFont::kFont5x7);
  int small_h = gfx_.fontHeight();

  int gap = 6;
  int total_h = title_h + gap + small_h * 2;
  int start_y = (gfx_.height() - total_h) / 2;
  if (start_y < 6) start_y = 6;

  gfx_.setFont(GfxFont::kFreeMono24pt);
  centerText(start_y, "MiniAcid", COLOR_ACCENT);

  gfx_.setFont(GfxFont::kFont5x7);
  int info_y = start_y + title_h + gap;
  centerText(info_y, "Use keys [ ] to move around", COLOR_WHITE);
  centerText(info_y + small_h, "Space - to start/stop sound", COLOR_WHITE);
  centerText(info_y + 2 * small_h, "ESC - for help on each page", COLOR_WHITE);
  
  centerText(info_y + 3 * small_h + 5, "v0.0.7-dev", IGfxColor::Gray());
  
  // char build_info[64];
  // snprintf(build_info, sizeof(build_info), "Built: %s %s", __DATE__, __TIME__);
  // centerText(info_y + 4 * small_h + 6, build_info, IGfxColor::DarkGray());

  gfx_.flush();
  gfx_.endWrite();
}

void MiniAcidDisplay::initMuteButtons(int x, int y, int w, int h) {
  int rect_w = w / 10;
  
  struct MuteButtonConfig {
    const char* label;
    std::function<bool()> is_muted;
    std::function<void()> toggle;
    int index;
  };
  
  std::vector<MuteButtonConfig> configs = {
    {"S1", [this]() { return mini_acid_.is303Muted(0); }, [this]() { mini_acid_.toggleMute303(0); }, 0},
    {"S2", [this]() { return mini_acid_.is303Muted(1); }, [this]() { mini_acid_.toggleMute303(1); }, 1},
    {"BD", [this]() { return mini_acid_.isKickMuted(); }, [this]() { mini_acid_.toggleMuteKick(); }, 2},
    {"SD", [this]() { return mini_acid_.isSnareMuted(); }, [this]() { mini_acid_.toggleMuteSnare(); }, 3},
    {"CH", [this]() { return mini_acid_.isHatMuted(); }, [this]() { mini_acid_.toggleMuteHat(); }, 4},
    {"OH", [this]() { return mini_acid_.isOpenHatMuted(); }, [this]() { mini_acid_.toggleMuteOpenHat(); }, 5},
    {"MT", [this]() { return mini_acid_.isMidTomMuted(); }, [this]() { mini_acid_.toggleMuteMidTom(); }, 6},
    {"HT", [this]() { return mini_acid_.isHighTomMuted(); }, [this]() { mini_acid_.toggleMuteHighTom(); }, 7},
    {"RS", [this]() { return mini_acid_.isRimMuted(); }, [this]() { mini_acid_.toggleMuteRim(); }, 8},
    {"CP", [this]() { return mini_acid_.isClapMuted(); }, [this]() { mini_acid_.toggleMuteClap(); }, 9},
  };
  
  for (const auto& config : configs) {
    auto button = std::make_shared<MuteButton>(config.label, config.is_muted, config.toggle);
    button->setBoundaries(Rect(x + rect_w * config.index, y, rect_w, h));
    mute_buttons_container_.addChild(button);
  }
  
  mute_buttons_initialized_ = true;
}

void MiniAcidDisplay::drawMutesSection(int x, int y, int w, int h) {
  if (!mute_buttons_initialized_) {
    initMuteButtons(x, y, w, h);
  }
  
  gfx_.setTextColor(COLOR_WHITE);
  mute_buttons_container_.draw(gfx_);
}

int MiniAcidDisplay::drawPageTitle(int x, int y, int w, const char* text) {
  if (w <= 0 || !text) return 0;

  int transport_info_w = 60;

  w = w - transport_info_w; 

  constexpr int kTitleHeight = 9;  // Reduced from 11 to save vertical space
  constexpr int kReservedRight = 60; 


  int title_w = w;
  if (title_w > kReservedRight)
    title_w -= kReservedRight;

  gfx_.fillRect(x, y, title_w, kTitleHeight, COLOR_WHITE);

  // Draw recording indicator (red square) on the left if recording
  if (audio_recorder_ && audio_recorder_->isRecording()) {
    int indicator_size = 6;
    int indicator_x = x + 3;
    int indicator_y = y + (kTitleHeight - indicator_size) / 2;
    gfx_.fillRect(indicator_x, indicator_y, indicator_size, indicator_size, IGfxColor::Red());
  }

  int text_x = x + (title_w - textWidth(gfx_, text)) / 2;
  if (text_x < x) text_x = x;
  gfx_.setTextColor(COLOR_BLACK);
  gfx_.drawText(text_x, y + 1, text);
  gfx_.setTextColor(COLOR_WHITE);

  {
    int info_x = x + title_w + 2;
    int info_y = y + 1;
    char buf[32];
    IGfxColor transportColor = mini_acid_.songModeEnabled() ? IGfxColor::Green() : IGfxColor::Blue();
    if (mini_acid_.isPlaying()) {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, transportColor);
    } else {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, IGfxColor::Gray());
    }
    snprintf(buf, sizeof(buf), "  %0.0fbpm", mini_acid_.bpm());
    if (mini_acid_.isPlaying()) {
      IGfxColor textColor = mini_acid_.songModeEnabled() ? IGfxColor::Black() : IGfxColor::White();
      gfx_.setTextColor(textColor);
    }
    gfx_.drawText(info_x, info_y, buf);
    gfx_.setTextColor(IGfxColor::White());
  }
  return kTitleHeight;
}

void MiniAcidDisplay::initPageHint(int x, int y, int w) {
  auto hint = std::make_shared<PageHint>(
    [this]() { return page_index_; },
    [this]() { return static_cast<int>(pages_.size()); },
    [this]() { previousPage(); },
    [this]() { nextPage(); }
  );
  hint->setBoundaries(Rect(x, y, w, gfx_.fontHeight()));
  page_hint_container_.addChild(hint);
  page_hint_initialized_ = true;
}

bool MiniAcidDisplay::translateToApplicationEvent(UIEvent& event) {
  if (event.event_type == MINIACID_KEY_DOWN) {
    if (event.shift && event.ctrl&& event.scancode == MINIACID_DOWN) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_MULTIPAGE_DOWN;
      return true;
    } else if (event.shift && event.ctrl && event.scancode == MINIACID_UP) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_MULTIPAGE_UP;
      return true;
    }
    if (event.key == 'c' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_COPY;
      return true;
    } else if (event.key == 'v' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_PASTE;
      return true;
    } else if (event.key == 'x' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_CUT;
      return true;
    } else if (event.key == 'z' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_UNDO;
      return true;
    } else if (event.key == 'p' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_TOGGLE_SONG_MODE;
      return true;
    } else if (event.key == 's' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_SAVE_SCENE;
      return true;
    } else if (event.key == 'j' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      if (audio_recorder_ && audio_recorder_->isRecording()) {
        event.app_event_type = MINIACID_APP_EVENT_STOP_RECORDING;
      } else {
        event.app_event_type = MINIACID_APP_EVENT_START_RECORDING;
      }
      return true;
    } else if (event.key == 'm' && (event.ctrl || event.meta)) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_TOGGLE_MODE;
      return true;
    }
  }
  return false;
}

bool MiniAcidDisplay::handleEvent(UIEvent event) {
  translateToApplicationEvent(event);

  // handle any global application events first
  if (event.event_type == MINIACID_APPLICATION_EVENT) {
    switch (event.app_event_type) {
      case MINIACID_APP_EVENT_TOGGLE_SONG_MODE:
        mini_acid_.toggleSongMode();
        return true;
      case MINIACID_APP_EVENT_SAVE_SCENE:
        mini_acid_.saveSceneAs(mini_acid_.currentSceneName());
        return true;
      case MINIACID_APP_EVENT_START_RECORDING:
#if defined(ARDUINO)
        Serial.println("Handling START_RECORDING event");
#endif
        if (audio_recorder_ && !audio_recorder_->isRecording()) {
          if (audio_guard_) {
            audio_guard_([this]() {
              if (audio_recorder_->start(static_cast<int>(mini_acid_.sampleRate()), 1)) {
#if defined(ARDUINO)
                Serial.println("Recording started successfully");
#endif
                // Recording started successfully
              } else {
#if defined(ARDUINO)
                Serial.println("Failed to start recording");
#endif
              }
            });
          }
        }
        return true;
      case MINIACID_APP_EVENT_STOP_RECORDING:
#if defined(ARDUINO)
        Serial.println("Handling STOP_RECORDING event");
#endif
        if (audio_recorder_ && audio_recorder_->isRecording()) {
          if (audio_guard_) {
            audio_guard_([this]() {
              audio_recorder_->stop();
#if defined(ARDUINO)
              Serial.println("Recording stopped");
#endif
            });
          }
        }
        return true;
      case MINIACID_APP_EVENT_TOGGLE_MODE:
        mini_acid_.toggleGrooveboxMode();
        return true;
      default:
        break;
    }
  }

  // Handle mute button clicks
  if (mute_buttons_initialized_ && mute_buttons_container_.handleEvent(event)) {
    return true;
  }

  // Handle page hint clicks
  if (page_hint_initialized_ && page_hint_container_.handleEvent(event)) {
    return true;
  }

  if (help_dialog_visible_) {
    if (help_dialog_) {
      bool handled = help_dialog_->handleEvent(event);
      if (handled) update();
    }
    return true;
  }

  if (event.event_type == MINIACID_KEY_DOWN) {
    if (event.scancode == MINIACID_TAB) {
      if (page_index_ >= 0 && page_index_ < static_cast<int>(pages_.size()) && pages_[page_index_]) {
        auto dialog = pages_[page_index_]->getHelpDialog();
        if (dialog) {
          help_dialog_ = std::move(dialog);
          help_dialog_visible_ = true;
          help_dialog_->setExitRequestedCallback([this]() {
            help_dialog_visible_ = false;
            help_dialog_.reset();
          });
          update();
          return true;
        }
      }
    } else if (event.scancode == MINIACID_ESCAPE) {
      if (help_dialog_visible_) {
        help_dialog_visible_ = false;
        help_dialog_.reset();
        update();
        return true;
      } else {
        // Universal "Back/Home" key: go to ProjectPage (index 6)
        if (page_index_ != 6) {
          page_index_ = 6;
          help_dialog_visible_ = false;
          help_dialog_.reset();
          update();
          return true;
        }
      }
    }
  }

  switch(event.event_type) {
    case MINIACID_KEY_DOWN:
      if (event.key == '-') {
        mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, -5);
        return true;
      } else if (event.key == '=') {
        mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, 5);
        return true;
      }
      break;
    default:
      break;
  }

  if (page_index_ >= 0 && page_index_ < static_cast<int>(pages_.size()) && pages_[page_index_]) {
    bool handled = pages_[page_index_]->handleEvent(event);
    if (handled) update();
    return handled;
  }
  return false;
}

void MiniAcidDisplay::drawDebugOverlay() {
  const PerfStats& stats = mini_acid_.perfStats;
  char buf[32];
  
  int w = 80;
  int h = 20;  // Reduced from 22
  int x = gfx_.width() - w - 2;
  int y = gfx_.height() - h - 14 - 10;  // Moved to just above mutes section  
  gfx_.fillRect(x, y, w, h, COLOR_BLACK);
  gfx_.drawRect(x, y, w, h, IGfxColor::Gray());
  
  gfx_.setTextColor(IGfxColor::Yellow());
  snprintf(buf, sizeof(buf), "C:%3.0f%% U:%u", stats.cpuAudioPct * 100.0f, stats.audioUnderruns);
  gfx_.drawText(x + 2, y + 2, buf);
  
  snprintf(buf, sizeof(buf), "H:%uK", stats.heapFree / 1024);
  gfx_.drawText(x + 2, y + 10, buf); 
}

HeaderState MiniAcidDisplay::buildHeaderState() const {
  HeaderState state;
  
  // Scene name (use current scene from miniacid)
  state.sceneName = mini_acid_.currentSceneName().c_str();
  
  state.bpm = static_cast<int>(mini_acid_.bpm());
  
  // Pattern name - show current bank and pattern
  static char patternNameBuf[16];
  snprintf(patternNameBuf, sizeof(patternNameBuf), "D%d", mini_acid_.displayDrumPatternIndex() + 1);
  state.patternName = patternNameBuf;
  
  state.isNavMode = true;  // TODO: track nav/edit mode
  state.isRecording = audio_recorder_ && audio_recorder_->isRecording();
  state.tapeEnabled = true;   // Tape is always processing if present
  state.fxEnabled = false;    // TODO: get from FX state
  state.songMode = mini_acid_.songModeEnabled();
  state.swingPercent = 50;    // TODO: get from swing parameter
  
  return state;
}

FooterState MiniAcidDisplay::buildFooterState() const {
  FooterState state;
  
  state.currentStep = mini_acid_.currentStep();
  state.totalSteps = 16;
  state.songPosition = mini_acid_.songModeEnabled() ? mini_acid_.songPlayheadPosition() : 0;
  state.loopPosition = mini_acid_.loopModeEnabled() ? mini_acid_.loopStartRow() : -1;
  state.isPlaying = mini_acid_.isPlaying();
  state.animFrame = skin_ ? skin_->animFrame() : 0;
  
  return state;
}
