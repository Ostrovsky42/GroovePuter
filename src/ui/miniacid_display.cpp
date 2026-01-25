#include "miniacid_display.h"

#include <algorithm>
#include <cstdarg>
#include <cctype>
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
#include "components/mute_button.h"
#include "components/multipage_title_indicator.h"
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

class PageHintNavigator : public Container {
 public:
  PageHintNavigator(std::function<int()> page_index,
                    std::function<int()> page_count,
                    std::function<void()> previous,
                    std::function<void()> next)
      : page_index_(std::move(page_index)),
        page_count_(std::move(page_count)),
        previous_(std::move(previous)),
        next_(std::move(next)) {}

  void updateLayout(IGfx& gfx, int x, int y, int w) {
    if (!hint_) {
      hint_ = std::make_shared<PageHint>(page_index_, page_count_, previous_, next_);
      addChild(hint_);
    }
    hint_->setBoundaries(Rect(x, y, w, gfx.fontHeight()));
  }

 private:
  std::shared_ptr<PageHint> hint_;
  std::function<int()> page_index_;
  std::function<int()> page_count_;
  std::function<void()> previous_;
  std::function<void()> next_;
};

class TransportBpmHeader : public Container {
 public:
  explicit TransportBpmHeader(MiniAcid& mini_acid) : mini_acid_(mini_acid) {}

  void updateLayout(int x, int y, int w, int h) {
    setBoundaries(Rect(x, y, w, h));
  }

  void draw(IGfx& gfx) override {
    constexpr int kFillInset = 2;
    int fill_x = dx();
    int fill_y = dy() + 1;
    int fill_w = width() - kFillInset * 2;
    int fill_h = height();
    if (fill_w < 0) fill_w = 0;

    IGfxColor transport_color = mini_acid_.songModeEnabled() ? IGfxColor::Green() : IGfxColor::Blue();
    if (mini_acid_.isPlaying()) {
      gfx.fillRect(fill_x, fill_y - 1, fill_w, fill_h, transport_color);
    } else {
      gfx.fillRect(fill_x, fill_y - 1, fill_w, fill_h, IGfxColor::Gray());
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%0.0fbpm", mini_acid_.bpm());
    int text_width = textWidth(gfx, buf);
    if (mini_acid_.isPlaying()) {
      IGfxColor text_color = mini_acid_.songModeEnabled() ? IGfxColor::Black() : IGfxColor::White();
      gfx.setTextColor(text_color);
    }
    gfx.drawText(fill_x + (fill_w - text_width) / 2, fill_y + 1, buf);
    gfx.setTextColor(IGfxColor::White());
  }

 private:
  MiniAcid& mini_acid_;
};

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
  splash_start_ms_ = nowMillis();
  gfx_.setFont(GfxFont::kFont5x7);

  pages_.push_back(std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, audio_guard_, 0));
  pages_.push_back(std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 0));
  pages_.push_back(std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, audio_guard_, 1));
  pages_.push_back(std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 1));
  pages_.push_back(std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<SongPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<ProjectPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<WaveformPage>(gfx_, mini_acid_, audio_guard_));
  pages_.push_back(std::make_unique<HelpPage>());

  multipage_indicator_ = std::make_unique<MultiPageTitleIndicator>(
      [this]() { dispatchMultiPageEvent(MINIACID_APP_EVENT_MULTIPAGE_UP); },
      [this]() { dispatchMultiPageEvent(MINIACID_APP_EVENT_MULTIPAGE_DOWN); });

  page_hint_navigator_ = std::make_unique<PageHintNavigator>(
      [this]() { return page_index_; },
      [this]() { return static_cast<int>(pages_.size()); },
      [this]() { previousPage(); },
      [this]() { nextPage(); });

  transport_bpm_header_ = std::make_unique<TransportBpmHeader>(mini_acid_);

  title_label_ = std::make_unique<LabelComponent>("");
  title_label_->setJustification(JUSTIFY_CENTER);
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
  gfx_.clear(COLOR_BLACK);
  gfx_.setTextColor(COLOR_WHITE);
  char buf[128];

  int margin = 4;

  int content_x = margin;
  int content_w = gfx_.width() - margin * 2;
  int content_y = margin;
  int content_h = 110;
  if (content_h < 0) content_h = 0;

  if (pages_[page_index_]) {
    int kTitleHeight = 11;
    auto title_h = kTitleHeight;
    auto headerRect = Rect(content_x, content_y, content_w, kTitleHeight);

    // draw transport bpm bar header

    int pagesHintWidth = textWidth(gfx_, "[< 0/0 >]");
    if (page_hint_navigator_) {
      auto pagesHintRect = headerRect.removeFromRight(pagesHintWidth);
      page_hint_navigator_->updateLayout(gfx_, pagesHintRect.x, pagesHintRect.y, pagesHintRect.w );
      page_hint_navigator_->draw(gfx_);
      headerRect.removeFromRight(2);
    }

    if (transport_bpm_header_) {
      auto transportBpmHeaderRect = headerRect.removeFromRight(50);
      transport_bpm_header_->setBoundaries(transportBpmHeaderRect);
      transport_bpm_header_->draw(gfx_);
      headerRect.removeFromRight(2);
    }


    MultiPage* multiPage = pages_[page_index_]->asMultiPage();
    if (multiPage && multiPage->pageCount() > 1) {
      auto multiPageSwitcherComponentRect = headerRect.removeFromRight(15);
      multipage_indicator_->setVisible(true);
      multipage_indicator_->setBoundaries(multiPageSwitcherComponentRect);
      multipage_indicator_->draw(gfx_);
      headerRect.removeFromRight(2);
    }


    auto title = pages_[page_index_]->getTitle();
    title_label_->setText(title);
    title_label_->setBoundaries(headerRect);
    gfx_.fillRect(headerRect.x, headerRect.y, headerRect.w, headerRect.h, COLOR_WHITE);
    gfx_.setTextColor(COLOR_BLACK);
    title_label_->draw(gfx_);

    // Draw recording indicator (red square) on the left if recording
    if (audio_recorder_ && audio_recorder_->isRecording()) {
      int indicator_size = 6;
      int indicator_x = headerRect.x + 3;
      int indicator_y = headerRect.y + (kTitleHeight - indicator_size) / 2;
      gfx_.fillRect(indicator_x, indicator_y, indicator_size, indicator_size, IGfxColor::Red());
    }

    // we don't need to do this each time, but for simplicity we do it here
    pages_[page_index_]->setBoundaries(Rect(content_x, content_y + title_h, content_w, content_h - title_h));
    pages_[page_index_]->draw(gfx_);
    if (help_dialog_visible_ && help_dialog_) {
      help_dialog_->setBoundaries(Rect(content_x, content_y + title_h, content_w, content_h - title_h));
      help_dialog_->draw(gfx_);
    }
  }

  drawMutesSection(margin, content_h + margin, gfx_.width() - margin * 2, gfx_.height() - content_h - margin );

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

bool MiniAcidDisplay::dispatchMultiPageEvent(ApplicationEventType type) {
  if (page_index_ >= 0 && page_index_ < static_cast<int>(pages_.size()) && pages_[page_index_]) {
    UIEvent event;
    event.event_type = MINIACID_APPLICATION_EVENT;
    event.app_event_type = type;
    return pages_[page_index_]->handleEvent(event);
  }
  return false;
}

bool MiniAcidDisplay::translateToApplicationEvent(UIEvent& event) {
  if (event.event_type == MINIACID_KEY_DOWN) {
    if ((event.ctrl || event.meta) && event.scancode == MINIACID_DOWN) {
      event.event_type = MINIACID_APPLICATION_EVENT;
      event.app_event_type = MINIACID_APP_EVENT_MULTIPAGE_DOWN;
      return true;
    } else if ((event.ctrl || event.meta) && event.scancode == MINIACID_UP) {
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
    }
  }
  return false;
}

bool MiniAcidDisplay::_handleGlobalKeyEvent(UIEvent& event) {
  // handle global key events
  auto withAudioGuard = [this](const std::function<void()>& fn) {
    if (audio_guard_) {
      audio_guard_(fn);
    } else {
      fn();
    }
  };
  if (event.event_type == MINIACID_KEY_DOWN)
  {
    char key = event.key;
    if (!key)
      return false;

    if (key == '\n' || key == '\r')
    {
      dismissSplash();
      update();
      return true;
    }
    if (key == '[')
    {
      previousPage();
      update();
      return true;
    }
    if (key == ']')
    {
      nextPage();
      update();
      return true;
    }
    if (key == ' ')
    {
      withAudioGuard([this]()
                     {
        if (mini_acid_.isPlaying()) {
          mini_acid_.stop();
        } else {
          mini_acid_.start();
        } });
      update();
      return true;
    }

    char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    if (event.alt)
    {
      if (lowerKey == 'i')
      {
        withAudioGuard([this]()
                       { mini_acid_.randomize303Pattern(0); });
        update();
        return true;
      }
      if (lowerKey == 'o')
      {
        withAudioGuard([this]()
                       { mini_acid_.randomize303Pattern(1); });
        update();
        return true;
      }
      if (lowerKey == 'p')
      {
        withAudioGuard([this]()
                       { mini_acid_.randomizeDrumPattern(); });
        update();
        return true;
      }
    }

    if (lowerKey == 'k')
    {
      withAudioGuard([this]()
                     { mini_acid_.setBpm(mini_acid_.bpm() - 5.0f); });
      update();
      return true;
    }
    if (lowerKey == 'l')
    {
      withAudioGuard([this]()
                     { mini_acid_.setBpm(mini_acid_.bpm() + 5.0f); });
      update();
      return true;
    }

    switch (key)
    {
    case '-':
      withAudioGuard([this]()
                     { mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, -5); });
      update();
      return true;
    case '=':
      withAudioGuard([this]()
                     { mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, 5); });
      update();
      return true;
    case '1':
      withAudioGuard([this]()
                     { mini_acid_.toggleMute303(0); });
      update();
      return true;
    case '2':
      withAudioGuard([this]()
                     { mini_acid_.toggleMute303(1); });
      update();
      return true;
    case '3':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteKick(); });
      update();
      return true;
    case '4':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteSnare(); });
      update();
      return true;
    case '5':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteHat(); });
      update();
      return true;
    case '6':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteOpenHat(); });
      update();
      return true;
    case '7':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteMidTom(); });
      update();
      return true;
    case '8':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteHighTom(); });
      update();
      return true;
    case '9':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteRim(); });
      update();
      return true;
    case '0':
      withAudioGuard([this]()
                     { mini_acid_.toggleMuteClap(); });
      update();
      return true;
    default:
      break;
    }
  }

  return false;
}

bool MiniAcidDisplay::handleEvent(UIEvent event) {
  translateToApplicationEvent(event);

  auto withAudioGuard = [this](const std::function<void()>& fn) {
    if (audio_guard_) {
      audio_guard_(fn);
    } else {
      fn();
    }
  };

  // handle any global application events first
  if (event.event_type == MINIACID_APPLICATION_EVENT) {
    switch (event.app_event_type) {
      case MINIACID_APP_EVENT_TOGGLE_SONG_MODE:
        mini_acid_.toggleSongMode();
        update();
        return true;
      case MINIACID_APP_EVENT_SAVE_SCENE:
        mini_acid_.saveSceneAs(mini_acid_.currentSceneName());
        update();
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
        update();
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
        update();
        return true;
      default:
        break;
    }
  }

  // Handle mute button clicks (only mouse events, not keyboard)
  if (mute_buttons_initialized_ && event.event_type != MINIACID_KEY_DOWN) {
    if (mute_buttons_container_.handleEvent(event)) {
      return true;
    }
  }

  // Handle multipage indicator clicks (only mouse events, not keyboard)
  if (multipage_indicator_ && event.event_type != MINIACID_KEY_DOWN) {
    if (multipage_indicator_->handleEvent(event)) {
      return true;
    }
  }

  // Handle page hint clicks (only mouse events, not keyboard)
  if (page_hint_navigator_ && event.event_type != MINIACID_KEY_DOWN) {
    if (page_hint_navigator_->handleEvent(event)) {
      return true;
    }
  }

  if (help_dialog_visible_) {
    if (help_dialog_) {
      bool handled = help_dialog_->handleEvent(event);
      if (handled) update();
    }
    return true;
  }

  if (event.event_type == MINIACID_KEY_DOWN && event.scancode == MINIACID_ESCAPE) {
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
  }

  if (_handleGlobalKeyEvent(event)) {
    return true;
  }

  // dispatch event to current page
  if (page_index_ >= 0 && page_index_ < static_cast<int>(pages_.size()) && pages_[page_index_])
  {
    bool handled = pages_[page_index_]->handleEvent(event);
    if (handled)
      update();
    if (handled)
      return true;
  }

  return false;
}
