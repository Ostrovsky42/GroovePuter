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
#include "pages/tape_page.h"
#include "pages/voice_page.h"
#include "pages/waveform_page.h"
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
    
    // LAZY LOADING: Reserve space but don't create pages yet
    // Pages will be created on-demand via getPage()
    Serial.println("MiniAcidDisplay: reserve pages (lazy)");
    pages_.resize(kPageCount);  // All nullptr initially
    
    // Only create the first page (PlayPage) to start with
    pages_[0] = createPage_(0);
    
    applyPageBounds_();
    
    Serial.println("MiniAcidDisplay: init DONE (lazy)");
    mute_buttons_initialized_ = true; 
}


MiniAcidDisplay::~MiniAcidDisplay() = default;

std::unique_ptr<IPage> MiniAcidDisplay::createPage_(int index) {
    Serial.printf("[UI] createPage_(%d)\n", index);
    switch (index) {
        case 0:  return std::make_unique<PlayPage>(gfx_, mini_acid_, audio_guard_);
        case 1:  return std::make_unique<GenrePage>(gfx_, mini_acid_, audio_guard_);
        case 2:  return std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 0);
        case 3:  return std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 1);
        case 4:  return std::make_unique<TB303ParamsPage>(gfx_, mini_acid_, audio_guard_, 0);
        case 5:  return std::make_unique<TB303ParamsPage>(gfx_, mini_acid_, audio_guard_, 1);
        case 6:  return std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, audio_guard_);
        case 7:  return std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_);
        case 8:  return std::make_unique<SequencerHubPage>(gfx_, mini_acid_, audio_guard_);
        case 9: return std::make_unique<SongPage>(gfx_, mini_acid_, audio_guard_);
        case 10: return std::make_unique<ProjectPage>(gfx_, mini_acid_, audio_guard_);
        case 11:  return std::make_unique<SettingsPage>(gfx_, mini_acid_);        
        case 12: return std::make_unique<VoicePage>(gfx_, mini_acid_, audio_guard_);
       // case 14: return std::make_unique<WaveformPage>(gfx_, mini_acid_, audio_guard_);
       // case 8:  return std::make_unique<TapePage>(gfx_, mini_acid_, audio_guard_);

        default: return nullptr;
    }
}

IPage* MiniAcidDisplay::getPage_(int index) {
    if (index < 0 || index >= kPageCount) return nullptr;
    if (!pages_[index]) {
        pages_[index] = createPage_(index);
        if (pages_[index]) {
            pages_[index]->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        }
    }
    return pages_[index].get();
}

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
    
    // Draw active page (lazy loading)
    IPage* currentPage = getPage_(page_index_);
    if (currentPage) {
        // CRITICAL: setBoundaries() MUST be called before draw() every frame
        currentPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        currentPage->draw(gfx_);
    } else {
        // "Dark screen" fix: show WIP/Invalid placeholder
        LayoutManager::drawHeader(gfx_, "--", mini_acid_.bpm(), "WIP/INVALID PAGE", false);
        LayoutManager::clearContent(gfx_);
        gfx_.setTextColor(COLOR_WHITE);
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(2), "PAGE INDEX INVALID");
        char buf[32];
        snprintf(buf, sizeof(buf), "idx=%d kPageCount=%d", page_index_, kPageCount);
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(3), buf);
        LayoutManager::drawFooter(gfx_, "[ ] pages", "[b] back");
    }
    
    // Waveform overlay (if enabled)
    UI::drawWaveformOverlay(gfx_, mini_acid_);
    
    // Mutes overlay (always on for now as per user request)
    UI::drawMutesOverlay(gfx_, mini_acid_);
    
    drawToast();
    // drawDebugOverlay();
    gfx_.flush();
    gfx_.endWrite();
}

void MiniAcidDisplay::nextPage() {
    previous_page_index_ = page_index_;
    page_index_ = (page_index_ + 1) % kPageCount;
    Serial.printf("[UI] nextPage: %d -> %d\n", previous_page_index_, page_index_);
}

void MiniAcidDisplay::previousPage() {
    previous_page_index_ = page_index_;
    page_index_ = (page_index_ - 1 + kPageCount) % kPageCount;
    Serial.printf("[UI] previousPage: %d -> %d\n", previous_page_index_, page_index_);
}

void MiniAcidDisplay::goToPage(int index) {
    if (index >= 0 && index < kPageCount) {
        previous_page_index_ = page_index_;
        page_index_ = index;
        IPage* p = getPage_(index);
        Serial.printf("[UI] goToPage(%d): %s\n", index, p ? p->getTitle().c_str() : "null");
    } else {
        Serial.printf("[UI] goToPage(%d) INVALID (kPageCount=%d)\n", index, kPageCount);
    }
}

void MiniAcidDisplay::togglePreviousPage() {
    // clamp current
    if (page_index_ < 0 || page_index_ >= kPageCount) {
        page_index_ = 0;
    }

    // if previous invalid -> just go to 0
    if (previous_page_index_ < 0 || previous_page_index_ >= kPageCount) {
        previous_page_index_ = 0;
    }

    int tmp = page_index_;
    page_index_ = previous_page_index_;
    previous_page_index_ = tmp;
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

       
        
        // Voice page (v key - no Ctrl needed, it's unique)
        if (event.key == 'v' || event.key == 'V') {
            goToPage(13);  // Voice Synth page
            return true;
        }

        // Waveform overlay toggle
        if (event.alt && event.key == 'w') {
            UI::waveformOverlay.enabled = !UI::waveformOverlay.enabled;
            return true;
        }

        // Direct page jumps: require Alt (standard) OR CTRL (fallback)
        if (event.alt || event.ctrl || event.meta) {

            
    
            int targetPage = -1;
            switch (event.key) {
                case '1': targetPage = 1; break;  // 0 Play (Home)
                case '2': targetPage = 2; break;  // Synth A Params
                case '3': targetPage = 3; break;  // Synth B Params
                case '4': targetPage = 4; break;  // 3 Drum Sequencer
                case '5': targetPage = 5; break;  // 1 Seq Hub
                case '6': targetPage = 6; break;  //8 Tape/FX
                case '7': targetPage = 7; break;  // Genre/Style
                case '8': targetPage = 8; break; // Song Mode
                case '9': targetPage = 9; break; // Settings
                case '0': targetPage = 10; break; // Voice Synth
                default: break;
            }
            if (targetPage >= 0) {
                goToPage(targetPage);
                return true;
            }
        }
        
        // Global Mutes (1-9) - only if no modifiers
        if (!event.alt && !event.ctrl && !event.meta && !event.shift) {
            if (event.key >= '1' && event.key <= '9') {
                int trackIdx = event.key - '1';
                withAudioGuard([&]() {
                    if (trackIdx == 0) mini_acid_.toggleMute303(0);
                    else if (trackIdx == 1) mini_acid_.toggleMute303(1);
                    else if (trackIdx == 2) mini_acid_.toggleMuteKick();
                    else if (trackIdx == 3) mini_acid_.toggleMuteSnare();
                    else if (trackIdx == 4) mini_acid_.toggleMuteHat();
                    else if (trackIdx == 5) mini_acid_.toggleMuteOpenHat();
                    else if (trackIdx == 6) mini_acid_.toggleMuteMidTom();
                    else if (trackIdx == 7) mini_acid_.toggleMuteHighTom();
                    else if (trackIdx == 8) mini_acid_.toggleMuteRim();
                    // Note: 9th key can toggle Rim/Clap together or just Rim
                });
                return true;
            } else if (event.key == '0') {
                withAudioGuard([&]() {
                    mini_acid_.toggleMuteClap();
                });
                return true;
            }
        }
    }
    
    // 2) Page Handling (lazy loading)
    IPage* currentPage = getPage_(page_index_);
    if (currentPage && currentPage->handleEvent(event)) {
        return true;
    }
    
    // 2.5) App Events (Inter-page communication)
    if (event.event_type == MINIACID_APPLICATION_EVENT) {
        if (event.app_event_type == MINIACID_APP_EVENT_SET_VISUAL_STYLE) {
            UI::currentStyle = (UI::currentStyle == VisualStyle::MINIMAL) ? 
                                VisualStyle::RETRO_CLASSIC : VisualStyle::MINIMAL;
            
            // Propagate to existing (loaded) pages only
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
