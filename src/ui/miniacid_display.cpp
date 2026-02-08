#include "miniacid_display.h"
#include "src/dsp/miniacid_engine.h"

#ifndef ARDUINO
#include "../../platform_sdl/arduino_compat.h"
#endif


#include "pages/sequencer_hub_page.h"
#include "pages/genre_page.h"
#include "pages/drum_sequencer_page.h"
#include "pages/pattern_edit_page.h"
#include "pages/tape_page.h"
#include "pages/feel_texture_page.h"
#include "pages/settings_page.h"
#include "pages/project_page.h"
#include "pages/tb303_params_page.h"
#include "pages/song_page.h"
#include "pages/tape_page.h"
#include "pages/voice_page.h"
#include "pages/color_test_page.h"
#include "pages/waveform_page.h"
#include "pages/help_dialog.h"
#include "ui_colors.h"
#include "ui_input.h"
#include "ui_common.h"
#include "screen_geometry.h"
#if defined(ESP32) || defined(ESP_PLATFORM)
#ifdef ARDUINO
#include <esp_partition.h>
#endif
#include "esp_heap_caps.h"
#endif
#include <cstdio>
#include "../debug_log.h"

namespace {
VisualStyle nextVisualStyle(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL: return VisualStyle::RETRO_CLASSIC;
        case VisualStyle::RETRO_CLASSIC: return VisualStyle::AMBER;
        case VisualStyle::AMBER: return VisualStyle::MINIMAL;
        default: return VisualStyle::MINIMAL;
    }
}

const char* visualStyleName(VisualStyle style) {
    switch (style) {
        case VisualStyle::MINIMAL: return "CARBON";
        case VisualStyle::RETRO_CLASSIC: return "CYBER";
        case VisualStyle::AMBER: return "AMBER";
        default: return "CARBON";
    }
}
} // namespace

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
    
    LOG_FUNC_ENTRY("UI");
    LOG_INFO_UI("Initializing MiniAcidDisplay...");
    splash_start_ms_ = millis();
    splash_active_ = true;

    // Initialize Cassette Skin as the main frame/theme
    LOG_DEBUG_UI("Initializing skin and pages...");
    skin_ = std::make_unique<CassetteSkin>(gfx, CassetteTheme::WarmTape);
    
    // LAZY LOADING: Reserve space but don't create pages yet
    // Pages will be created on-demand via getPage()
    pages_.resize(kPageCount);  // All nullptr initially
    
    // Only create the first page (PlayPage) to start with
    pages_[0] = createPage_(0);
    
    applyPageBounds_();
    applied_visual_style_ = UI::currentStyle;
    visual_style_initialized_ = true;
    
    LOG_SUCCESS_UI("MiniAcidDisplay initialization complete");
    mute_buttons_initialized_ = true; 
}


MiniAcidDisplay::~MiniAcidDisplay() = default;

std::unique_ptr<IPage> MiniAcidDisplay::createPage_(int index) {
    LOG_DEBUG_UI("Creating page at index %d", index);
    switch (index) {
        case 0:  return std::make_unique<GenrePage>(gfx_, mini_acid_, audio_guard_);
        case 1:  return std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 0);
        case 2:  return std::make_unique<PatternEditPage>(gfx_, mini_acid_, audio_guard_, 1);
        case 3:  return std::make_unique<TB303ParamsPage>(gfx_, mini_acid_, audio_guard_, 0);
        case 4:  return std::make_unique<TB303ParamsPage>(gfx_, mini_acid_, audio_guard_, 1);
        case 5:  return std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, audio_guard_);
        case 6:  return std::make_unique<SongPage>(gfx_, mini_acid_, audio_guard_);
        case 7:  return std::make_unique<SequencerHubPage>(gfx_, mini_acid_, audio_guard_);
        case 8:  return std::make_unique<FeelTexturePage>(gfx_, mini_acid_, audio_guard_);
        case 9:  return std::make_unique<SettingsPage>(gfx_, mini_acid_, audio_guard_);
        case 10: return std::make_unique<ProjectPage>(gfx_, mini_acid_,audio_guard_);        
        case 11: return std::make_unique<TapePage>(gfx_, mini_acid_, audio_guard_);

        //case 11: return std::make_unique<VoicePage>(gfx_, mini_acid_, audio_guard_);
        //case 12: return std::make_unique<ColorTestPage>(gfx_, mini_acid_);
       // case 14: return std::make_unique<WaveformPage>(gfx_, mini_acid_, audio_guard_);

        default: return nullptr;
    }
}

IPage* MiniAcidDisplay::getPage_(int index) {
    if (index < 0 || index >= kPageCount) return nullptr;
    
    // Memory Relief: Purge all pages EXCEPT the one we need AND the previous one (for fast back-toggling)
    // This prevents DRAM accumulation on constrained devices like the Cardputer.
    if (!pages_[index]) {
        for (int i = 0; i < kPageCount; ++i) {
            if (i != index && i != previous_page_index_) {
                if (pages_[i]) {
                    // Serial.printf("[UI] Purging page %d to free RAM\n", i);
                    pages_[i].reset();
                }
            }
        }

        pages_[index] = createPage_(index);
        if (pages_[index]) {
            pages_[index]->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
            pages_[index]->setVisualStyle(UI::currentStyle);
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
    syncVisualStyle_();
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
        currentPage->tick(); // Modern contract: update state before draw
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
    
    updateCyclePulse_();
    UI::drawFeelOverlay(gfx_, mini_acid_, millis() < cycle_pulse_until_ms_);

    // Mutes overlay (always on for now as per user request)
    UI::drawMutesOverlay(gfx_, mini_acid_);
    
    // Global Help Overlay (fullscreen, on top of everything)
    if (global_help_overlay_.isVisible()) {
        global_help_overlay_.setPageContext(page_index_);
        global_help_overlay_.draw(gfx_);
    }
    
    drawToast();
    // drawDebugOverlay();
    gfx_.flush();
    gfx_.endWrite();
}

void MiniAcidDisplay::syncVisualStyle_() {
    if (!visual_style_initialized_ || applied_visual_style_ != UI::currentStyle) {
        for (auto& p : pages_) {
            if (p) p->setVisualStyle(UI::currentStyle);
        }
        applied_visual_style_ = UI::currentStyle;
        visual_style_initialized_ = true;
    }
}

void MiniAcidDisplay::nextPage() {
    transitionToPage_((page_index_ + 1) % kPageCount);
}

void MiniAcidDisplay::previousPage() {
    transitionToPage_((page_index_ - 1 + kPageCount) % kPageCount);
}

void MiniAcidDisplay::goToPage(int index) {
    transitionToPage_(index);
}

void MiniAcidDisplay::togglePreviousPage() {
    // clamp current
    int next = (page_index_ < 0 || page_index_ >= kPageCount) ? 0 : page_index_;
    int prev = (previous_page_index_ < 0 || previous_page_index_ >= kPageCount) ? 0 : previous_page_index_;

    transitionToPage_(prev);
}

void MiniAcidDisplay::transitionToPage_(int index, int context) {
    if (index < 0 || index >= kPageCount) {
        Serial.printf("[UI] transitionToPage(%d) INVALID\n", index);
        return;
    }

    if (page_index_ == index && context == 0) return; // redundant

    IPage* oldPage = getPage_(page_index_);
    if (oldPage) oldPage->onExit();

    previous_page_index_ = page_index_;
    page_index_ = index;

    IPage* newPage = getPage_(index);
    if (newPage) {
        newPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        newPage->onEnter(context);
        Serial.printf("[UI] transition: %d -> %d (%s, ctx=%d)\n", 
                      previous_page_index_, page_index_, newPage->getTitle().c_str(), context);
    }
}

void MiniAcidDisplay::dismissSplash() {
    splash_active_ = false;
}

bool MiniAcidDisplay::handleEvent(UIEvent event) {
    // Global Help Overlay takes priority when visible
    if (global_help_overlay_.isVisible()) {
        if (global_help_overlay_.handleEvent(event)) return true;
    }
    
    if (splash_active_) {
        dismissSplash();
        return true;
    }

    // 0) Hard-global shortcuts: handled before page logic so they work everywhere.
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.ctrl && (event.key == 'h' || event.key == 'H')) {
            global_help_overlay_.toggle();
            return true;
        }

        if (event.alt && (event.key == 'v' || event.key == 'V')) {
            goToPage(11);
            return true;
        }

        if (event.alt && (event.key == 'w' || event.key == 'W')) {
            UI::waveformOverlay.enabled = !UI::waveformOverlay.enabled;
            return true;
        }

        if (event.alt && (event.key == '\\' || event.key == '|')) {
            UI::currentStyle = nextVisualStyle(UI::currentStyle);
            for (auto& p : pages_) {
                if (p) p->setVisualStyle(UI::currentStyle);
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "Style: %s", visualStyleName(UI::currentStyle));
            showToast(buf);
            return true;
        }

        if (event.alt && (event.key == 'm' || event.key == 'M')) {
            static uint32_t lastToggle = 0;
            if (millis() - lastToggle < 400) return true;
            lastToggle = millis();

            bool newState = !mini_acid_.songModeEnabled();
            withAudioGuard([&]() { mini_acid_.setSongMode(newState); });
            showToast(newState ? "Song: ON" : "Song: OFF");
            return true;
        }

        if (event.alt || event.ctrl || event.meta) {
            int targetPage = -1;
            switch (event.key) {
                case '1': targetPage = 1; break;
                case '2': targetPage = 2; break;
                case '3': targetPage = 3; break;
                case '4': targetPage = 4; break;
                case '5': targetPage = 5; break;
                case '6': targetPage = 6; break;
                case '7': targetPage = 7; break;
                case '8': targetPage = 8; break;
                case '9': targetPage = 9; break;
                case '0': targetPage = 10; break;
                default: break;
            }
            if (targetPage >= 0) {
                goToPage(targetPage);
                return true;
            }
        }
    }
    
 
    // 1) Page handling (after hard-global shortcuts)
    IPage* currentPage = getPage_(page_index_);
    if (currentPage) {
        if (currentPage->handleEvent(event)) {
            // Check for requested page transition - Page handled it AND wants to conform transition
            if (currentPage->hasPageRequest()) {
                int nextIndex = currentPage->getRequestedPage();
                int context = currentPage->getRequestedContext();
                currentPage->clearPageRequest();
                transitionToPage_(nextIndex, context);
            }
            return true;
        }
    }

    // 2) Global navigation fallback
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.key == ']') { nextPage(); return true; }
        if (event.key == '[') { previousPage(); return true; }

        if (event.key == 'h') {
            showToast("[ ] nav  Ctrl+# pages  \\ style  v tape  w wave  b back", 2200);
            return true;
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
    
    
    // 2.5) App Events (Inter-page communication)
    if (event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
        if (event.app_event_type == GROOVEPUTER_APP_EVENT_SET_VISUAL_STYLE) {
            UI::currentStyle = nextVisualStyle(UI::currentStyle);
            
            // Propagate to existing (loaded) pages only
            for (auto& p : pages_) {
                if (p) p->setVisualStyle(UI::currentStyle);
            }

            char buf[32];
            snprintf(buf, sizeof(buf), "Style: %s", visualStyleName(UI::currentStyle));
            showToast(buf);
            return true;
        }
    }
    
    // 3) Global Fallback "Back" (if page didn't handle it)
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        const bool isBack = 
            (event.key == '`' || event.key == 0x08 /*backspace*/ || event.key == 0x1B /*esc*/);
             
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

  unsigned long elapsed = millis() - splash_start_ms_;
  
  static const char* const logo[] = {
    "_$$$$__$$$$$___$$$$___$$$$__$$__$$_$$$$$",
    "$$_____$$__$$_$$__$$_$$__$$_$$__$$_$$___",
    "$$_$$$_$$$$$__$$__$$_$$__$$_$$__$$_$$$$_",
    "$$__$$_$$__$$_$$__$$_$$__$$__$$$$__$$___",
    "_$$$$__$$__$$__$$$$___$$$$____$$___$$$$$",
    "________________________________________",
    "___$$$$$__$$__$$_$$$$$$_$$$$$_$$$$$____",
    "___$$__$$_$$__$$___$$___$$____$$__$$___",
    "___$$$$$__$$__$$___$$___$$$$__$$$$$____",
    "___$$_____$$__$$___$$___$$____$$__$$___",
    "___$$______$$$$____$$___$$$$$_$$__$$___",
  };
  constexpr int kLineCount = 11;
  constexpr int kLineDelay = 140; // ms per line

  gfx_.setFont(GfxFont::kFont5x7);
  int small_h = gfx_.fontHeight();
  int logo_h = kLineCount * (small_h + 1);
  int start_y = (gfx_.height() - logo_h - 40) / 2;
  if (start_y < 10) start_y = 10;

  // Draw logo lines
  for (int i = 0; i < kLineCount; ++i) {
    unsigned long lineTrigger = i * kLineDelay;
    if (elapsed < lineTrigger) continue;

    int y = start_y + i * (small_h + 1);
    
    // Flicker effect: first 50ms of a line's life is white
    IGfxColor color = COLOR_ACCENT;
    if (elapsed < lineTrigger + 50) {
        color = COLOR_WHITE;
    }
    
    centerText(y, logo[i], color);
  }

  // Draw info text after logo starts finishing
  if (elapsed > kLineCount * kLineDelay + 200) {
    int info_y = start_y + logo_h + 15;
    centerText(info_y, "Use keys [ ] to move around", COLOR_WHITE);
    centerText(info_y + small_h + 2, "Space - to start/stop sound", COLOR_WHITE);
    centerText(info_y + 2 * small_h + 4, "ESC - for help on each page", COLOR_WHITE);
  }
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
    UI::showToast(msg, durationMs);
}

void MiniAcidDisplay::drawToast() {
    UI::drawToast(gfx_);
}

void MiniAcidDisplay::updateCyclePulse_() {
    uint32_t counter = mini_acid_.cyclePulseCounter();
    if (counter != last_cycle_pulse_counter_) {
        last_cycle_pulse_counter_ = counter;
        cycle_pulse_until_ms_ = millis() + 250;
    }
}
