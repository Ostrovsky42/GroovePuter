#include "miniacid_display.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"
#include "undo_ux.h"
#include "src/platform/cardputer_ui_session.h"
#include "src/platform/cardputer_smf_route_persistence.h"

#ifndef ARDUINO
#include "../../platform_sdl/arduino_compat.h"
#endif

#include "pages/sequencer_hub_page.h"
#include "pages/genre_page.h"
#include "pages/drum_sequencer_page.h"
#include "pages/synth_sequencer_page.h"
#include "pages/tape_page.h"
#include "pages/feel_texture_page.h"
#include "pages/settings_page.h"
#include "pages/project_page.h"
#include "pages/mode_page.h"
#include "pages/song_page.h"
#include "pages/phrase_page.h"
#include "pages/help_dialog.h"
#include "pages/sampler_page.h"
#include "pages/perform_page.h"
#include "pages/smf_player_page.h"
#include "workflow_mode.h"
#include "ui_colors.h"
#include "ui_input.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "screen_geometry.h"
#if defined(ESP32) || defined(ESP_PLATFORM)
#ifdef ARDUINO
#include <esp_partition.h>
#endif
#include "esp_heap_caps.h"
#endif
#include "../audio/pattern_paging.h"
#include <cstdio>
#include "../debug_log.h"

namespace {
constexpr int kSmfPlayerPage = WorkflowPages::kPlayer;

VisualStyle nextVisualStyle(VisualStyle style) {
    return UI::nextThemeStyle(style);
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

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx,
                                 MiniAcid& mini_acid,
                                 PerformanceKeyboard& performance_keyboard)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      performance_keyboard_(performance_keyboard) {
    
    LOG_FUNC_ENTRY("UI");
    LOG_INFO_UI("Initializing MiniAcidDisplay...");
    splash_start_ms_ = millis();
    splash_active_ = true;

    ui_session_ = GroovePuterState::defaultUiSessionState();
    ui_session_loaded_ =
        GroovePuterPlatform::loadCardputerUiSession(ui_session_);
    if (!ui_session_loaded_) {
        ui_session_.masterVolumePermille =
            GroovePuterState::masterVolumeToPermille(mini_acid_.mainVolume());
    }
    GroovePuterState::sanitizeUiSessionState(ui_session_);
    page_index_ = WorkflowPages::normalizeLegacyPage(ui_session_.activePage);
    ui_session_.activePage = static_cast<int8_t>(page_index_);
    previous_page_index_ = page_index_;
    active_workspace_ = WorkflowPages::workspaceForPage(page_index_);
    Serial.printf("[SESSION] load=%d active=%d mem=%d,%d,%d,%d,%d\n",
                  ui_session_loaded_ ? 1 : 0,
                  page_index_,
                  static_cast<int>(ui_session_.lastPageByWorkflow[0]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[1]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[2]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[3]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[4]));
    UI::currentStyle = static_cast<VisualStyle>(ui_session_.visualStyle);
    UI::waveformOverlay.enabled = ui_session_.waveformOverlayEnabled != 0;
    if (mini_acid_.lastSceneLoadRecoveredAutosave()) {
        GroovePuterState::markSceneMutated();
    }
    observed_scene_revision_ =
        GroovePuterState::sceneRevisionSnapshot().currentRevision;

    LOG_DEBUG_UI("Initializing skin and pages...");
    skin_ = std::make_unique<CassetteSkin>(gfx, CassetteTheme::WarmTape);
    
    pages_.resize(kPageCount);
    pages_[page_index_] = createPage_(page_index_);
    
    applyPageBounds_();
    applied_visual_style_ = UI::currentStyle;
    visual_style_initialized_ = true;
    
    LOG_SUCCESS_UI("MiniAcidDisplay initialization complete");
    mute_buttons_initialized_ = true; 
}

MiniAcidDisplay::~MiniAcidDisplay() = default;

std::unique_ptr<IPage> MiniAcidDisplay::createPage_(int index) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    uint32_t freeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    Serial.printf("[UI] Creating page %d (DRAM: %u bytes free)\n", index, (unsigned)freeBefore);
#endif
    std::unique_ptr<IPage> page;
    switch (index) {
        case 0:  page = std::make_unique<GenrePage>(gfx_, mini_acid_, audio_guard_); break;
        case 1:  page = std::make_unique<SynthSequencerPage>(gfx_, mini_acid_, audio_guard_, 0); break;
        case 2:  page = std::make_unique<SynthSequencerPage>(gfx_, mini_acid_, audio_guard_, 1); break;
        case 5:  page = std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, audio_guard_); break;
        case 6:  page = std::make_unique<SongPage>(gfx_, mini_acid_, audio_guard_); break;
        case 7:  page = std::make_unique<SequencerHubPage>(gfx_, mini_acid_, audio_guard_); break;
        case 8:  page = std::make_unique<FeelTexturePage>(gfx_, mini_acid_, audio_guard_); break;
        case 9:  page = std::make_unique<SettingsPage>(gfx_, mini_acid_, audio_guard_); break;
        case 10: page = std::make_unique<ProjectPage>(gfx_, mini_acid_, audio_guard_); break;
        case 11: page = std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_); break;
        case 12: page = std::make_unique<PerformPage>(gfx_, mini_acid_, performance_keyboard_); break;
        case WorkflowPages::kPhrase:
            page = std::make_unique<PhrasePage>(gfx_, mini_acid_, audio_guard_);
            break;
        case WorkflowPages::kSampler:
            page = std::make_unique<SamplerPage>(gfx_, mini_acid_, audio_guard_);
            break;
        case kSmfPlayerPage:
            page = std::make_unique<SmfPlayerPage>(gfx_, mini_acid_, audio_guard_);
            break;
    }
#if defined(ESP32) || defined(ESP_PLATFORM)
    uint32_t freeAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (page) {
        Serial.printf("[UI] Page %d created SUCCESS (size: %u, DRAM left: %u)\n", 
                      index, (unsigned)(freeBefore - freeAfter), (unsigned)freeAfter);
    } else {
        Serial.printf("[UI] Page %d creation FAILED or INVALID\n", index);
    }
#endif
    return page;
}

IPage* MiniAcidDisplay::getPage_(int index) {
    if (index < 0 || index >= kPageCount) return nullptr;
    
    if (!pages_[index]) {
#if defined(ESP32) || defined(ESP_PLATFORM)
        uint32_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        bool aggressive = (freeDRAM < 16384);
#else
        bool aggressive = false;
#endif

        for (int i = 0; i < kPageCount; ++i) {
            bool keep = (i == index);
            if (!aggressive && i == previous_page_index_) keep = true;
            if (!keep && pages_[i]) pages_[i].reset();
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
    const float persistedVolume =
        GroovePuterState::masterVolumeFromPermille(
            ui_session_.masterVolumePermille);
    withAudioGuard([&]() {
        mini_acid_.setDeviceMasterVolume(persistedVolume);
    });
}

void MiniAcidDisplay::setAudioRecorder(IAudioRecorder* recorder) {
    audio_recorder_ = recorder;
}

void MiniAcidDisplay::update() {
    servicePersistence_();
    syncVisualStyle_();
    handlePaging_();
    gfx_.startWrite();
    if (splash_active_) {
        drawSplashScreen();
        if (millis() - splash_start_ms_ > 2000) dismissSplash();
        if (splash_active_) {
            gfx_.flush();
            gfx_.endWrite();
            return;
        }
    }

    // Draw background
    if (skin_) {
        skin_->drawBackground();
        skin_->tick();
    } else {
        gfx_.clear(COLOR_BLACK);
    }
    
    IPage* currentPage = getPage_(page_index_);
    if (currentPage) {
        currentPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        currentPage->tick();
        currentPage->draw(gfx_);
    } else {
        LayoutManager::drawHeader(gfx_, "--", mini_acid_.bpm(), "WIP/INVALID PAGE", false);
        LayoutManager::clearContent(gfx_);
        gfx_.setTextColor(COLOR_WHITE);
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(2), "PAGE INDEX INVALID");
        char buf[32];
        snprintf(buf, sizeof(buf), "idx=%d kPageCount=%d", page_index_, kPageCount);
        gfx_.drawText(Layout::COL_1, LayoutManager::lineY(3), buf);
        LayoutManager::drawFooter(gfx_, "[ ] workspaces", "Fn+M menu");
    }
    
    UI::drawLiveMixLockBadge(gfx_, mini_acid_);

    updateCyclePulse_();
    UI::drawPerformanceHud(gfx_, mini_acid_, millis() < cycle_pulse_until_ms_);

    if (workspace_launcher_.isVisible()) {
        workspace_launcher_.draw(gfx_);
    }
    
    if (global_help_overlay_.isVisible()) {
        global_help_overlay_.setPageContext(page_index_);
        global_help_overlay_.draw(gfx_);
    }
    
    drawToast();
    gfx_.flush();
    gfx_.endWrite();
}

void MiniAcidDisplay::captureUiSession_() {
    GroovePuterState::UiSessionState next = ui_session_;
    if (WorkflowPages::isStandalonePage(page_index_)) {
        next.activePage = static_cast<int8_t>(page_index_);
    } else {
        GroovePuterState::rememberWorkflowPage(next, page_index_);
    }
    next.visualStyle = static_cast<uint8_t>(UI::currentStyle);
    next.waveformOverlayEnabled = UI::waveformOverlay.enabled ? 1 : 0;
    next.masterVolumePermille =
        GroovePuterState::masterVolumeToPermille(mini_acid_.mainVolume());
    GroovePuterState::sanitizeUiSessionState(next);
    if (next == ui_session_) return;
    ui_session_ = next;
    scheduleUiSessionSave_();
}

void MiniAcidDisplay::scheduleUiSessionSave_() {
    ui_session_save_pending_ = true;
    ui_session_save_due_ms_ = millis() + 1000;
}

void MiniAcidDisplay::servicePersistence_() {
    GroovePuterPlatform::serviceCardputerSmfRoutePersistence();
    const unsigned long now = millis();
    const auto due = [now](unsigned long deadline) {
        return static_cast<int32_t>(now - deadline) >= 0;
    };

    captureUiSession_();
    if (ui_session_save_pending_ && !mini_acid_.isPlaying() &&
        due(ui_session_save_due_ms_)) {
        if (GroovePuterPlatform::saveCardputerUiSession(ui_session_)) {
            ui_session_save_pending_ = false;
            Serial.printf("[SESSION] saved active=%d mem=%d,%d,%d,%d,%d\n",
                          static_cast<int>(ui_session_.activePage),
                          static_cast<int>(ui_session_.lastPageByWorkflow[0]),
                          static_cast<int>(ui_session_.lastPageByWorkflow[1]),
                          static_cast<int>(ui_session_.lastPageByWorkflow[2]),
                          static_cast<int>(ui_session_.lastPageByWorkflow[3]),
                          static_cast<int>(ui_session_.lastPageByWorkflow[4]));
        } else {
            ui_session_save_due_ms_ = now + 5000;
        }
    }

    const GroovePuterState::SceneRevisionState revision =
        GroovePuterState::sceneRevisionSnapshot();
    if (!revision.dirty()) {
        observed_scene_revision_ = revision.currentRevision;
        recovery_save_pending_ = false;
        return;
    }
    if (revision.currentRevision != observed_scene_revision_) {
        observed_scene_revision_ = revision.currentRevision;
        recovery_save_pending_ = true;
        recovery_save_due_ms_ = now + 3000;
    }
    if (!recovery_save_pending_ || mini_acid_.isPlaying() ||
        !due(recovery_save_due_ms_)) {
        return;
    }

    bool saved = false;
    withAudioGuard([&]() { saved = mini_acid_.autoSaveSceneRecovery(); });
    if (saved) {
        recovery_save_pending_ = false;
        Serial.printf("[AUTOSAVE] recovery revision=%u\n",
                      static_cast<unsigned>(observed_scene_revision_));
    } else {
        recovery_save_due_ms_ = now + 5000;
        Serial.println("[AUTOSAVE] recovery write failed; retry deferred");
    }
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
    const bool workflowModifier =
        WorkflowPages::hardwareWorkflowModifierHeld();
    if (workflowModifier) {
        switchWorkflow_(1);
        return;
    }
    transitionToPage_(GroovePuterState::workflowNavigationTarget(
        ui_session_, page_index_, 1, false));
}

void MiniAcidDisplay::previousPage() {
    const bool workflowModifier =
        WorkflowPages::hardwareWorkflowModifierHeld();
    if (workflowModifier) {
        switchWorkflow_(-1);
        return;
    }
    transitionToPage_(GroovePuterState::workflowNavigationTarget(
        ui_session_, page_index_, -1, false));
}

void MiniAcidDisplay::switchWorkflow_(int direction) {
    const int target = GroovePuterState::rememberedAdjacentWorkflowPage(
        ui_session_, page_index_, direction);
    Serial.printf("[NAV] workflow dir=%d current=%d target=%d mem=%d,%d,%d,%d,%d\n",
                  direction,
                  page_index_,
                  target,
                  static_cast<int>(ui_session_.lastPageByWorkflow[0]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[1]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[2]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[3]),
                  static_cast<int>(ui_session_.lastPageByWorkflow[4]));
    transitionToPage_(target);
}

void MiniAcidDisplay::goToPage(int index) {
    transitionToPage_(index);
}

void MiniAcidDisplay::togglePreviousPage() {
    int next = (page_index_ < 0 || page_index_ >= kPageCount) ? 0 : page_index_;
    (void)next;
    int prev = (previous_page_index_ < 0 || previous_page_index_ >= kPageCount) ? 0 : previous_page_index_;
    transitionToPage_(prev);
}

void MiniAcidDisplay::transitionToPage_(int index, int context) {
    index = WorkflowPages::normalizeLegacyPage(index);
    if (index < 0 || index >= kPageCount) {
        Serial.printf("[UI] transitionToPage(%d) INVALID\n", index);
        return;
    }

    if (page_index_ == index && context == 0) return;

    Serial.printf("[UI] transitionToPage: %d -> %d (ctx=%d)\n", page_index_, index, context);

    IPage* oldPage = getPage_(page_index_);
    if (oldPage) oldPage->onExit();

    previous_page_index_ = page_index_;
    page_index_ = index;
    if (WorkflowPages::isWorkspacePage(index)) {
        active_workspace_ = WorkflowPages::workspaceForPage(index);
    }
    if (WorkflowPages::isStandalonePage(index)) {
        // A direct utility page must not replace the user's remembered
        // workflow child in the compact session state.
        ui_session_.activePage = static_cast<int8_t>(index);
    } else {
        GroovePuterState::rememberWorkflowPage(ui_session_, index);
    }
    scheduleUiSessionSave_();

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
    if (global_help_overlay_.isVisible()) {
        if (global_help_overlay_.handleEvent(event)) return true;
    }

    if (workspace_launcher_.isVisible()) {
        if (workspace_launcher_.handleEvent(event)) {
            int requestedPage = -1;
            if (workspace_launcher_.takePageRequest(requestedPage)) {
                transitionToPage_(requestedPage);
            } else if (workspace_launcher_.takeHelpRequest()) {
                global_help_overlay_.setPageContext(page_index_);
                global_help_overlay_.toggle();
            }
            return true;
        }
    }
    
    if (splash_active_) {
        dismissSplash();
        return true;
    }

    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.meta && (event.key == 'm' || event.key == 'M')) {
            global_help_overlay_.close();
            workspace_launcher_.toggle(
                active_workspace_,
                ui_session_.lastPageByWorkflow,
                GroovePuterState::kWorkflowSessionCount);
            return true;
        }

        if (event.meta && (event.key == '\t' || event.scancode == GROOVEPUTER_TAB)) {
            switchWorkflow_(event.shift ? -1 : 1);
            return true;
        }

        // Modified brackets belong to top-level workflow navigation. Handle
        // them before the current page gets first refusal, otherwise synth and
        // drum pages can consume Fn+[ / ] as ordinary local bracket input.
        if (event.meta && (event.key == '[' || event.key == '{')) {
            switchWorkflow_(-1);
            return true;
        }
        if (event.meta && (event.key == ']' || event.key == '}')) {
            switchWorkflow_(1);
            return true;
        }

        if (event.alt && (event.key == 'h' || event.key == 'H')) {
            workspace_launcher_.close();
            global_help_overlay_.setPageContext(page_index_);
            global_help_overlay_.toggle();
            return true;
        }

        if (event.alt && (event.key == '[' || event.key == '{')) {
            int prev = mini_acid_.currentPageIndex() - 1;
            if (prev < 0) prev = kMaxPages - 1;
            mini_acid_.requestPageSwitch(prev);
            return true;
        }
        if (event.alt && (event.key == ']' || event.key == '}')) {
            int next = (mini_acid_.currentPageIndex() + 1) % kMaxPages;
            mini_acid_.requestPageSwitch(next);
            return true;
        }

        if (event.alt && (event.key == 'v' || event.key == 'V')) {
            Serial.println("[UI] Shortcut Alt+V -> Page 11");
            goToPage(11);
            return true;
        }

        if (event.alt && (event.key == 'p' || event.key == 'P')) {
            goToPage(kSmfPlayerPage);
            return true;
        }

        if (event.alt && (event.key == 'k' || event.key == 'K')) {
            goToPage(WorkflowPages::kSampler);
            return true;
        }

        if (event.alt && (event.key == 'w' || event.key == 'W') &&
            page_index_ != WorkflowPages::kPhrase) {
            UI::waveformOverlay.enabled = !UI::waveformOverlay.enabled;
            return true;
        }

        if (event.alt && (event.key == 'x' || event.key == 'X')) {
            bool enable = !mini_acid_.liveMixModeEnabled();
            withAudioGuard([&]() { mini_acid_.setLiveMixMode(enable); });
            showToast(enable ? "LiveMix: ON" : "LiveMix: OFF", 900);
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
            GroovePuterState::markSceneMutated();
            showToast(newState ? "Song: ON" : "Song: OFF");
            return true;
        }

        if (event.ctrl && event.alt && (event.key == '\b' || event.key == 0x7F)) {
            performance_keyboard_.panic();
            withAudioGuard([&]() {
                mini_acid_.sceneManager().loadDefaultScene();
                mini_acid_.reset();
            });
            GroovePuterState::markSceneMutated();
            showToast("PROJECT RESET", 1500);
            return true;
        }

        const bool smfPlayerFnNumber =
            page_index_ == kSmfPlayerPage && event.meta && !event.alt &&
            !event.ctrl && event.key >= '1' && event.key <= '9';
        if ((event.alt || event.meta) && !event.ctrl && !smfPlayerFnNumber) {
            int targetPage = -1;
            switch (event.key) {
                case '1': targetPage = 1; break;
                case '2': targetPage = 2; break;
                case '3': targetPage = WorkflowPages::kSynthA; break;
                case '4': targetPage = WorkflowPages::kSynthB; break;
                case '5': targetPage = 5; break;
                case '6': targetPage = 6; break;
                case '7': targetPage = 7; break;
                case '8': targetPage = 8; break;
                case '9': targetPage = 9; break;
                case '0': targetPage = 10; break;
                default: break;
            }
            if (targetPage >= 0) {
                Serial.printf("[UI] Shortcut Alt+%c -> Page %d\n", event.key, targetPage);
                goToPage(targetPage);
                return true;
            }
        }
    }
    
    // R6 exposes one global user gesture while preserving page ownership.
    // The active page still decides whether it can restore the retained
    // domain receipt; this layer only promotes Ctrl+U to APP_EVENT_UNDO.
    GroovePuterUndoUx::promoteUndoShortcut(event);

    IPage* currentPage = getPage_(page_index_);
    if (currentPage) {
        if (currentPage->handleEvent(event)) {
            if (currentPage->hasPageRequest()) {
                int nextIndex = currentPage->getRequestedPage();
                int context = currentPage->getRequestedContext();
                currentPage->clearPageRequest();
                transitionToPage_(nextIndex, context);
            }
            return true;
        }
    }

    // Pages get first refusal on Space. This lets MIDI Player own its transport
    // without also toggling the global GroovePuter transport.
    if (event.event_type == GROOVEPUTER_KEY_DOWN && event.key == ' ') {
        if (!mini_acid_.isPlaying()) performance_keyboard_.setTransportPlaying(true);
        withAudioGuard([&]() {
            if (mini_acid_.isPlaying()) mini_acid_.stop();
            else mini_acid_.start();
        });
        performance_keyboard_.setTransportPlaying(mini_acid_.isPlaying());
        showToast(mini_acid_.isPlaying() ? "Play" : "Stop", 500);
        return true;
    }

    if (event.event_type == GROOVEPUTER_KEY_DOWN &&
        !event.alt && !event.ctrl && !event.shift && !event.meta &&
        WorkflowPages::allowsPerformanceKeyboard(page_index_) &&
        performance_keyboard_.keyDown(event.key)) {
        return true;
    }

    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.key == ']') { nextPage(); return true; }
        if (event.key == '[') { previousPage(); return true; }

        if (event.key == 'h') {
            showToast("[ ] workspaces  Fn+M menu  Alt+H help", 2200);
            return true;
        }

        if (!event.alt && !event.ctrl && !event.meta) {
            const bool sp12Swap90 = (mini_acid_.currentDrumEngineName() == "SP12");
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
                    else if (trackIdx == 8) {
                        if (sp12Swap90) mini_acid_.toggleMuteClap();
                        else mini_acid_.toggleMuteRim();
                    }
                });
                GroovePuterState::markSceneMutated();
                return true;
            } else if (event.key == '0') {
                withAudioGuard([&]() {
                    if (sp12Swap90) mini_acid_.toggleMuteRim();
                    else mini_acid_.toggleMuteClap();
                });
                GroovePuterState::markSceneMutated();
                return true;
            }
        }
    }
    
    // If the active page declined Undo, do not restore another domain here.
    // A retained receipt remains intact so the user can return to its owner.
    if (GroovePuterUndoUx::isUndoEvent(event)) {
        const bool hasReceipt = GroovePuterUndo::undoOwner().hasUndo();
        UI::showToast(GroovePuterUndoUx::fallbackToast(hasReceipt), 1000);
        return true;
    }

    if (event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
        if (event.app_event_type == GROOVEPUTER_APP_EVENT_SET_VISUAL_STYLE) {
            UI::currentStyle = nextVisualStyle(UI::currentStyle);
            for (auto& p : pages_) {
                if (p) p->setVisualStyle(UI::currentStyle);
            }

            char buf[32];
            snprintf(buf, sizeof(buf), "Style: %s", visualStyleName(UI::currentStyle));
            showToast(buf);
            return true;
        }
    }
    
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        const bool isBack = 
            (event.key == '`' || event.key == 0x08 || event.key == 0x1B);
             
        if (isBack) {
            togglePreviousPage();
            return true;
        }
    }
    
    return false;
}

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
  constexpr int kLineDelay = 70;

  gfx_.setFont(GfxFont::kFont5x7);
  int small_h = gfx_.fontHeight();
  int logo_h = kLineCount * (small_h + 1);
  int start_y = (gfx_.height() - logo_h - 40) / 2;
  if (start_y < 10) start_y = 10;

  auto drawGradientText = [&](int y, const char* text, unsigned long timeShift) {
      if (!text) return;
      int len = strlen(text);
      int tw = len * 6;
      int sx = (gfx_.width() - tw) / 2;
      
      for (int j = 0; j < len; ++j) {
          if (text[j] != ' ') {
              float t = 0.5f + 0.5f * sinf(timeShift * 0.003f + j * 0.08f + y * 0.03f);
              uint8_t r = (uint8_t)(0x00 + (0x9D - 0x00) * t);
              uint8_t g = (uint8_t)(0xE5 + (0x00 - 0xE5) * t);
              uint8_t b = (uint8_t)(0xFF + (0xFF - 0xFF) * t);
              gfx_.setTextColor(IGfxColor((r << 16) | (g << 8) | b));
              char tmp[2] = {text[j], 0};
              gfx_.drawText(sx + j * 6, y, tmp);
          }
      }
  };

  for (int i = 0; i < kLineCount; ++i) {
    unsigned long lineTrigger = i * kLineDelay;
    if (elapsed < lineTrigger) continue;

    int y = start_y + i * (small_h + 1);
    if (elapsed < lineTrigger + 100) {
        char glitchLine[64];
        strncpy(glitchLine, logo[i], 63);
        glitchLine[63] = '\0';
        int len = strlen(glitchLine);
        for (int j = 0; j < len; ++j) {
            if (glitchLine[j] != ' ') {
                glitchLine[j] = "01#$%&@*"[rand() % 8];
            }
        }
        centerText(y, glitchLine, COLOR_WHITE);
    } else {
        drawGradientText(y, logo[i], elapsed);
    }
  }

  if (elapsed > kLineCount * kLineDelay + 1000) {
    int info_y = start_y + logo_h + 15;
    uint8_t pulse = 160 + 95 * sinf(elapsed * 0.005f);
    IGfxColor pulseColor((pulse << 16) | (pulse << 8) | pulse);

    centerText(info_y, "[ ] Workspaces  Fn+M Menu", pulseColor);
    centerText(info_y + small_h + 2, "Space - start/stop sound", pulseColor);
    centerText(info_y + 2 * small_h + 4, "Alt+H - page-aware help", pulseColor);
  }
}

void MiniAcidDisplay::drawDebugOverlay() {
    auto& stats = mini_acid_.perfStats;
    uint32_t s1, s2;
    uint32_t underruns;
    float cpuIdeal, cpuActual;
    
    do {
        s1 = stats.seq;
        underruns = stats.audioUnderruns;
        cpuIdeal = stats.cpuAudioPctIdeal;
        cpuActual = stats.cpuAudioPctActual;
        s2 = stats.seq;
    } while (s1 != s2 || (s1 & 1));
    
    char buf[64];
    int yy = 2;
    gfx_.setTextColor(IGfxColor(0x00FF00));
#if defined(ESP32) || defined(ESP_PLATFORM)
    uint32_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t minDRAM = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snprintf(buf, sizeof(buf), "DRAM:%u/%u", (unsigned)freeDRAM, (unsigned)minDRAM);
    gfx_.drawText(2, yy, buf); yy += 10;
#endif
    (void)cpuIdeal;
    (void)cpuActual;
    gfx_.drawText(2, yy, buf); yy += 10;
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

void MiniAcidDisplay::handlePaging_() {
    if (!mini_acid_.isPageLoading()) return;

    const int target = mini_acid_.targetPageIndex();
    if (target < 0 || target >= kMaxPages) {
        mini_acid_.setTargetPage(-1);
        mini_acid_.setPageLoading(false);
        showToast("Invalid pattern page", 1500);
        return;
    }

    enum class PageSwitchResult {
        Switched,
        Created,
        SaveCurrentFailed,
        LoadTargetFailed,
        CreateTargetFailed,
        RollbackFailed,
    };

    PageSwitchResult result = PageSwitchResult::LoadTargetFailed;
    const int current = mini_acid_.currentPageIndex();

    withAudioGuard([&]() {
        Scene& scene = mini_acid_.sceneManager().currentScene();
        if (!PatternPagingService::savePage(current, scene)) {
            result = PageSwitchResult::SaveCurrentFailed;
        } else if (PatternPagingService::pageExists(target)) {
            if (PatternPagingService::loadPage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Switched;
            } else {
                result = PageSwitchResult::LoadTargetFailed;
            }
        } else {
            PatternPagingService::initializeEmptyPage(scene);
            if (PatternPagingService::savePage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Created;
            } else if (PatternPagingService::loadPage(current, scene)) {
                result = PageSwitchResult::CreateTargetFailed;
            } else {
                result = PageSwitchResult::RollbackFailed;
            }
        }
        mini_acid_.setTargetPage(-1);
        mini_acid_.setPageLoading(false);
    });

    char message[32];
    switch (result) {
        case PageSwitchResult::Switched:
            std::snprintf(message, sizeof(message), "Pattern Page %d", target + 1);
            showToast(message, 900);
            break;
        case PageSwitchResult::Created:
            std::snprintf(message, sizeof(message), "New Pattern Page %d", target + 1);
            showToast(message, 1200);
            break;
        case PageSwitchResult::SaveCurrentFailed:
            showToast("Page save failed", 1800);
            break;
        case PageSwitchResult::LoadTargetFailed:
            showToast("Page corrupt/unreadable", 1800);
            break;
        case PageSwitchResult::CreateTargetFailed:
            showToast("New page save failed", 1800);
            break;
        case PageSwitchResult::RollbackFailed:
            showToast("PAGE ROLLBACK FAILED", 2500);
            break;
    }
}
