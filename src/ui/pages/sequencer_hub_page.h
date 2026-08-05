#pragma once

#include <cstdint>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_widgets.h"
#include "../components/drum_sequencer_grid.h"
#include "src/state/scene_revision.h"

#ifndef USE_RETRO_THEME
#define USE_RETRO_THEME
#endif
#ifndef USE_AMBER_THEME
#define USE_AMBER_THEME
#endif
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
#include "../amber_ui_theme.h"
#include "../amber_widgets.h"

#ifdef USE_RETRO_THEME
using namespace RetroTheme;
using namespace RetroWidgets;
#endif

class SequencerHubPageBase : public IPage {
public:
    enum class Mode { OVERVIEW, DETAIL };
    enum class FocusLane { GRID, PATTERN, BANK };

    SequencerHubPageBase(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override { return title_; }

    void setVisualStyle(VisualStyle style) override { hub_style_ = style; }
    void setHubStyle(VisualStyle style) { hub_style_ = style; }
    VisualStyle getHubStyle() const { return hub_style_; }

protected:
    MiniAcid& mini_acid_;
    AudioGuard audio_guard_;
    std::string title_ = "SEQUENCER HUB";
    
    Mode mode_ = Mode::OVERVIEW;
    FocusLane focus_ = FocusLane::GRID;
    int selectedTrack_ = 0; // 0-9: 2 synth + 8 drums
    int overviewScroll_ = 0;
    
    // Cursors
    int stepCursor_ = 0;
    int voiceCursor_ = 0;
    int bankCursor_ = 0;
    int patternCursor_ = 0;

    // Component
    std::unique_ptr<DrumSequencerGridComponent> drumGrid_;

    void drawOverview(IGfx& gfx);
    void drawDetail(IGfx& gfx);
    void drawRetroClassicStyle(IGfx& gfx);
    void drawAmberStyle(IGfx& gfx);
    void drawMinimalStyle(IGfx& gfx);
    void drawTEGridStyle(IGfx& gfx);
    void drawTrackRow(IGfx& gfx, int trackIdx, int y, int h, bool selected);
    void drawOverviewCursor(IGfx& gfx, int trackIdx, int stepIdx, int x, int y, int cellW, int cellH);
    
    VisualStyle hub_style_ = VisualStyle::MINIMAL;
    
    bool handleModeSwitch(UIEvent& e);
    bool handleQuickKeys(UIEvent& e);
    bool handleAppEvent(const UIEvent& e);
    bool handleVolumeInput(UIEvent& e);
    bool handleNavigation(UIEvent& e);
    bool handleGridEdit(UIEvent& e);

    // Helpers
    bool isDrumTrack(int trackIdx) { return trackIdx >= 2; }
    int getDrumVoiceIndex(int trackIdx) { return trackIdx - 2; }
    void syncOverviewScroll();
    
    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
        GroovePuterState::markSceneMutated();
    }
};

class SequencerHubPage final : public SequencerHubPageBase {
public:
    using SequencerHubPageBase::SequencerHubPageBase;

    void onEnter(int context) override;
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& event) override;

private:
    bool midiOverview_{false};
    bool midiReturnToPlayer_{false};
    uint8_t midiSelected_{0};
    uint8_t midiScroll_{0};
    uint32_t midiGeneration_{0};

    void drawMidiOverview(IGfx& gfx);
    bool handleMidiOverviewEvent(UIEvent& event);
    bool toggleMidiLayer(uint8_t layerIndex);
    void syncMidiScroll(uint8_t layerCount);
    void syncMidiSessionSelection();
    void returnFromMidiOverview();
};

#if !defined(GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER)
#define SequencerHubPage SequencerHubPageBase
#endif
