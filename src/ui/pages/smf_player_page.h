#pragma once

#include <array>
#include <string>
#include <utility>

#include "../ui_common.h"
#include "smf_player_session_state.h"
#include "src/midi/smf_player_service.h"

class SmfPlayerPage final : public IPage {
public:
    SmfPlayerPage(IGfx& gfx, MiniAcid& miniAcid, AudioGuard audioGuard);

    const std::string& getTitle() const override { return title_; }
    void onEnter(int context) override;
    void onExit() override { sessionBinding_.setActive(false); }
    bool handleEvent(UIEvent& event) override;
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;

private:
    struct BrowserRow {
        int logicalIndex{-1};
        char displayName[40]{};
    };

    // The old browser diagnostics wrote synchronously to UART during every
    // page entry, refresh, and visible-window refill. UI feedback already uses
    // toasts and the player snapshot, so suppress only those page-local routine
    // logs. USB/SMF failure diagnostics in platform tasks remain untouched.
    struct BrowserDiagnosticSink {
        template <typename... Args>
        void printf(const char*, Args&&...) const {}
        void println(const char*) const {}
    };
    inline static constexpr BrowserDiagnosticSink Serial{};

    static constexpr int kBrowserVisibleRows = 7;

    void refreshFiles();
    void fillVisibleEntries();
    bool resolveEntry(int logicalIndex,
                      std::string& name,
                      bool& isDirectory) const;
    bool playSelected();
    bool togglePlayerTransport();
    void toggleGrooveTransport();
    void drawBrowser(IGfx& gfx);
    void drawNowPlaying(IGfx& gfx);
    void drawPerformance(IGfx& gfx);
    void drawChannelInspector(IGfx& gfx);
    void drawMidiWaveOverlay(IGfx& gfx,
                             const GroovePuterMidi::SmfPlayerSnapshot& state,
                             const Rect& region,
                             IGfxColor color);
    void ensureSelectionVisible(int visibleRows);
    bool navigateIntoDir(const std::string& dirName);
    bool navigateUpDir();
    bool hasParentEntry() const;
    int entryCount() const;
    bool isDirEntry(int index) const;
    const char* displayName(int index) const;

    MiniAcid& miniAcid_;
    AudioGuard audioGuard_;
    GroovePuterMidi::ISmfPlayerService* player_{nullptr};
    std::string title_{"MIDI PLAYER"};
    std::string currentPath_{"/midi"};
    std::array<BrowserRow, kBrowserVisibleRows> browserRows_{};
    int directoryCount_{0};
    int fileCount_{0};
    int totalEntries_{0};
    int visibleWindowStart_{-1};
    int selection_{0};
    int scroll_{0};
    bool browserStorageReady_{false};
    GroovePuterUi::SmfPlayerTrackedFlag browserVisible_{
        GroovePuterUi::SmfPlayerSessionFlag::BrowserVisible, true};
    GroovePuterUi::SmfPlayerTrackedFlag performanceVisible_{
        GroovePuterUi::SmfPlayerSessionFlag::PerformanceVisible, false};
    GroovePuterUi::SmfPlayerTrackedFlag channelInspectorVisible_{
        GroovePuterUi::SmfPlayerSessionFlag::InspectorVisible, false};
    int channelInspectorScroll_{0};
    uint32_t lastMidiVisualEpoch_{0};
    uint32_t lastMidiVisualPulse_{0};
    uint16_t midiWavePhase_{0};
    uint8_t midiWaveEnvelope_{0};
    GroovePuterUi::SmfPlayerSessionBinding sessionBinding_{
        currentPath_,
        selection_,
        scroll_,
        channelInspectorScroll_,
        browserVisible_,
        performanceVisible_,
        channelInspectorVisible_};

    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audioGuard_) audioGuard_(std::forward<F>(fn));
        else fn();
    }
};
