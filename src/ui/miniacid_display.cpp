#include "miniacid_display.h"

#ifndef ARDUINO
#include "../../platform_sdl/arduino_compat.h"
#endif

#include "pages/play_page.h"
#include "pages/sequencer_hub_page.h"
#include "pages/genre_page.h"
#include "pages/drum_sequencer_page.h"
#include "pages/pattern_edit_page.h"
#include "pages/tape_page.h"
#include "pages/mode_page.h"
#include "pages/settings_page.h"
#include "pages/project_page.h"
#include "pages/tb303_params_page.h"
#include "pages/song_page.h"
#include "pages/voice_page.h"
#include "pages/help_dialog.h"
#include "ui_colors.h"
#include "ui_input.h"
#include "ui_common.h"
#include "screen_geometry.h"
#if defined(ESP32) || defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif
#include <cstdio>

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
    
    Serial.println("MiniAcidDisplay: init splash");
    splash_start_ms_ = millis();
    splash_active_ = true;

    // Initialize Cassette Skin as the main frame/theme
    Serial.println("MiniAcidDisplay: init skin");
    skin_ = std::make_unique<CassetteSkin>(gfx, CassetteTheme::WarmTape);
    
    // Register the pages in order
    Serial.println("MiniAcidDisplay: reg pages...");
    pages_.push_back(std::make_unique<PlayPage>(gfx, mini_acid, audio_guard_));              // 0
    pages_.push_back(std::make_unique<SequencerHubPage>(gfx, mini_acid, audio_guard_));      // 1 NEW!
    pages_.push_back(std::make_unique<GenrePage>(gfx, mini_acid, audio_guard_));             // 2
    pages_.push_back(std::make_unique<DrumSequencerPage>(gfx, mini_acid, audio_guard_));     // 3 'd'
    pages_.push_back(std::make_unique<PatternEditPage>(gfx, mini_acid, audio_guard_, 0));    // 4 'e' (303A Pattern)
    pages_.push_back(std::make_unique<PatternEditPage>(gfx, mini_acid, audio_guard_, 1));    // 5 'E' (303B Pattern)
    pages_.push_back(std::make_unique<TB303ParamsPage>(gfx, mini_acid, audio_guard_, 0)); // 6 'y' (303A Params)
    pages_.push_back(std::make_unique<TB303ParamsPage>(gfx, mini_acid, audio_guard_, 1)); // 7 'Y' (303B Params)
    pages_.push_back(std::make_unique<ModePage>(gfx, mini_acid, audio_guard_));              // 9 'm'
    pages_.push_back(std::make_unique<SettingsPage>(gfx, mini_acid));                        // 10 's'
    pages_.push_back(std::make_unique<ProjectPage>(gfx, mini_acid, audio_guard_));           // 11 'p'
    pages_.push_back(std::make_unique<SongPage>(gfx, mini_acid, audio_guard_));              // 12 'W' -> Song Mode
    pages_.push_back(std::make_unique<VoicePage>(gfx, mini_acid, audio_guard_));             // 13 'v' (Vocal Synth)

    applyPageBounds_();
    
    Serial.println("MiniAcidDisplay: init DONE");
    mute_buttons_initialized_ = true; 
}


MiniAcidDisplay::~MiniAcidDisplay() = default;

void MiniAcidDisplay::setAudioGuard(AudioGuard guard) {
    audio_guard_ = guard;
}

void MiniAcidDisplay::setAudioRecorder(IAudioRecorder* recorder) {
    audio_recorder_ = recorder;
}

void MiniAcidDisplay::update() {
    gfx_.startWrite();
    if (splash_active_) {
        drawSplashScreen();
        if (millis() - splash_start_ms_ > 1500) dismissSplash();
        if (splash_active_) {
            gfx_.flush();
            return;
        }
    }
    
    // Draw background (Cassette Skin)
    if (skin_) {
        skin_->drawBackground();
        skin_->tick();
    } else {
        gfx_.clear(COLOR_BLACK);
    }
    
    // Draw active page
    if (page_index_ >= 0 && page_index_ < (int)pages_.size()) {
        // CRITICAL: setBoundaries() MUST be called before draw() every frame
        // Without this, pages using getBoundaries() get zero/stale rects → "dark screens" / "one line" displays
        pages_[page_index_]->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        
        const char* pageTitle = pages_[page_index_]->getTitle().c_str();
        // Serial.printf("[UI] draw page %d/%d: %s\n", page_index_, (int)pages_.size(), pageTitle);
        pages_[page_index_]->draw(gfx_);
    } else {
        // "Dark screen" fix: show WIP/Invalid placeholder
        LayoutManager::drawHeader(gfx_, "--", mini_acid_.bpm(), "WIP/INVALID PAGE", false);
        LayoutManager::clearContent(gfx_);
        gfx_.setTextColor(COLOR_WHITE);
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(2), "PAGE INDEX INVALID");
        char buf[32];
        snprintf(buf, sizeof(buf), "idx=%d size=%d", page_index_, (int)pages_.size());
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(3), buf);
        LayoutManager::drawFooter(gfx_, "[ ] pages", "[b] back");
    }
    
    // Waveform overlay (if enabled)
    UI::drawWaveformOverlay(gfx_, mini_acid_);
    
    drawToast();
    // drawDebugOverlay();
    gfx_.flush();
    gfx_.endWrite();
}

void MiniAcidDisplay::nextPage() {
    if (pages_.empty()) return;
    previous_page_index_ = page_index_;
    page_index_ = (page_index_ + 1) % pages_.size();
    applyPageBounds_();
    Serial.printf("[UI] nextPage: %d -> %d (total %d)\n", previous_page_index_, page_index_, (int)pages_.size());
}

void MiniAcidDisplay::previousPage() {
    if (pages_.empty()) return;
    previous_page_index_ = page_index_;
    page_index_ = (page_index_ - 1 + pages_.size()) % pages_.size();
    applyPageBounds_();
    Serial.printf("[UI] previousPage: %d -> %d\n", previous_page_index_, page_index_);
}

void MiniAcidDisplay::goToPage(int index) {
    if (index >= 0 && index < (int)pages_.size()) {
        previous_page_index_ = page_index_;
        page_index_ = index;
        applyPageBounds_();
        Serial.printf("[UI] goToPage(%d): %s\n", index, pages_[index]->getTitle().c_str());
    } else {
        Serial.printf("[UI] goToPage(%d) INVALID (size=%d)\n", index, (int)pages_.size());
    }
}

void MiniAcidDisplay::togglePreviousPage() {
    if (pages_.empty()) return;

    // clamp current
    if (page_index_ < 0 || page_index_ >= (int)pages_.size()) {
        page_index_ = 0;
    }

    // if previous invalid -> just go to 0
    if (previous_page_index_ < 0 || previous_page_index_ >= (int)pages_.size()) {
        previous_page_index_ = 0;
    }

    int tmp = page_index_;
    page_index_ = previous_page_index_;
    previous_page_index_ = tmp;
    applyPageBounds_();
}

void MiniAcidDisplay::dismissSplash() {
    splash_active_ = false;
}

bool MiniAcidDisplay::handleEvent(UIEvent event) {
    if (splash_active_) {
        dismissSplash();
        return true;
    }
    
    // 1) Global Navigation (Always First)
    // CRITICAL: Direct page jumps REQUIRE Ctrl to prevent stealing editing keys (g/s/d/...) from pages
    if (event.event_type == MINIACID_KEY_DOWN) {
        if (event.key == ']') { nextPage(); return true; }
        if (event.key == '[') { previousPage(); return true; }

        if (event.key == 'h') {
            showToast("[ ] nav  Ctrl+# pages  v voice  w wave  b back", 2200);
            return true;
        }

        // Waveform overlay toggle
        if (event.key == 'w' || event.key == 'W') {
            UI::waveformOverlay.enabled = !UI::waveformOverlay.enabled;
            return true;
        }
        
        // Voice page (v key - no Ctrl needed, it's unique)
        if (event.key == 'v' || event.key == 'V') {
            goToPage(13);  // Voice Synth page
            return true;
        }

        // Direct page jumps: require CTRL (or META) to prevent stealing normal editing keys
        if (event.ctrl || event.meta) {
            switch (event.key) {
                case '1': goToPage(1); return true; // Genre/Style
                case '2': goToPage(2); return true; // Drum Sequencer
                case '3': goToPage(3); return true; // 303A Pattern Edit
                case '4': goToPage(4); return true; // 303B Pattern Edit
                case '5': goToPage(5); return true; // 303A Params
                case '6': goToPage(6); return true; // 303B Params
                case '7': goToPage(7); return true; // Tape
                case '8': goToPage(8); return true; // Mode
                case '9': goToPage(9); return true; // Settings
                case '0': goToPage(10); return true; // Project
                default:
                    break;
            }
        }
    }
    
    // 2) Page Handling
    if (page_index_ >= 0 && page_index_ < (int)pages_.size()) {
        if (pages_[page_index_]->handleEvent(event)) {
            return true;
        }
    }
    
    // 2.5) App Events (Inter-page communication)
    if (event.event_type == MINIACID_APPLICATION_EVENT) {
        if (event.app_event_type == MINIACID_APP_EVENT_SET_VISUAL_STYLE) {
            UI::currentStyle = (UI::currentStyle == VisualStyle::MINIMAL) ? 
                                VisualStyle::RETRO_CLASSIC : VisualStyle::MINIMAL;
            
            for (auto& p : pages_) {
                if (p) p->setVisualStyle(UI::currentStyle);
            }
            
            showToast(UI::currentStyle == VisualStyle::MINIMAL ? "Style: MINIMAL" : "Style: RETRO");
            return true;
        }
    }
    
    // 3) Global Fallback "Back" (if page didn't handle it)
    if (event.event_type == MINIACID_KEY_DOWN) {
        const bool isBack = 
            (event.key == 'b' || event.key == 'B' || event.key == '`' || 
             event.key == 0x08 /*backspace*/ || event.key == 0x1B /*esc*/);
             
        if (isBack) {
            togglePreviousPage();
            return true;
        }
    }
    
    return false;
}


// Private stubs
void MiniAcidDisplay::initMuteButtons(int x, int y, int w, int h) {}
void MiniAcidDisplay::initPageHint(int x, int y, int w) {}
void MiniAcidDisplay::drawMutesSection(int x, int y, int w, int h) {}
int MiniAcidDisplay::drawPageTitle(int x, int y, int w, const char* text) { return 0; }


void MiniAcidDisplay::drawSplashScreen() {
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

  gfx_.setFont(GfxFont::kFreeSerif18pt);
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
}

void MiniAcidDisplay::drawDebugOverlay() {
    // Lock-free snapshot of performance stats
    auto& stats = mini_acid_.perfStats;
    uint32_t s1, s2;
    uint32_t underruns;
    float cpuIdeal, cpuActual;
    
    // Retry loop until we get a consistent snapshot
    do {
        s1 = stats.seq;
        underruns = stats.audioUnderruns;
        cpuIdeal = stats.cpuAudioPctIdeal;
        cpuActual = stats.cpuAudioPctActual;
        s2 = stats.seq;
    } while (s1 != s2 || (s1 & 1));  // Retry if torn read or mid-write
    
    char buf[64];
    int yy = 2;
    
    // Using small text, green for debug info
    gfx_.setTextColor(IGfxColor(0x00FF00)); // Bright Green
    
    // DRAM (8-bit internal heap)
#if defined(ESP32) || defined(ESP_PLATFORM)
    uint32_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t minDRAM = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snprintf(buf, sizeof(buf), "DRAM:%u/%u", (unsigned)freeDRAM, (unsigned)minDRAM);
#else
    snprintf(buf, sizeof(buf), "DRAM: N/A");
#endif
    gfx_.drawText(2, yy, buf); yy += 10;
    
    // Note: PSRAM display disabled - this board has no PSRAM
    
    // CPU% - show both ideal and actual if they differ significantly
    gfx_.drawText(2, yy, buf); yy += 10;
    
    // Underruns
    snprintf(buf, sizeof(buf), "UNDR:%u", underruns);
    gfx_.drawText(2, yy, buf); yy += 10;
}
bool MiniAcidDisplay::translateToApplicationEvent(UIEvent& event) { return false; }

void MiniAcidDisplay::applyPageBounds_() {
    Rect r{0, 0, gfx_.width(), gfx_.height()};
    for (auto& p : pages_) {
        if (p) p->setBoundaries(r);
    }
}

HeaderState MiniAcidDisplay::buildHeaderState() const { return {}; }
FooterState MiniAcidDisplay::buildFooterState() const { return {}; }

void MiniAcidDisplay::showToast(const char* msg, int durationMs) {
    if (!msg) return;
    snprintf(toastMsg_, sizeof(toastMsg_), "%s", msg);
    toastEndTime_ = millis() + durationMs;
}

void MiniAcidDisplay::drawToast() {
    if (millis() < toastEndTime_) {
        int w = gfx_.width();
        int tw = gfx_.textWidth(toastMsg_);
        int x = (w - tw) / 2;
        int y = gfx_.height() - 25;
        gfx_.fillRect(x - 4, y - 2, tw + 8, 11, COLOR_BLACK);
        gfx_.drawRect(x - 4, y - 2, tw + 8, 11, COLOR_KNOB_2);
        gfx_.setTextColor(COLOR_WHITE);
        gfx_.drawText(x, y, toastMsg_);
    }
}
