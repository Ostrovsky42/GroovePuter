#pragma once

#include <string>
#include <utility>

#include "../ui_common.h"
#include "../midi_file_manager.h"
#include "src/midi/smf_player_service.h"

class SmfPlayerPage final : public IPage {
public:
    SmfPlayerPage(IGfx& gfx, MiniAcid& miniAcid, AudioGuard audioGuard);

    const std::string& getTitle() const override { return title_; }
    void onEnter(int context) override;
    bool handleEvent(UIEvent& event) override;
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;

private:
    bool loadMidiPath(const char* path);
    bool togglePlayerTransport();
    void toggleGrooveTransport();
    void drawBrowser(IGfx& gfx);
    void drawNowPlaying(IGfx& gfx);
    void drawMuteMixer(IGfx& gfx);
    void drawPerformance(IGfx& gfx);
    void drawChannelInspector(IGfx& gfx);
    void drawMidiWaveOverlay(IGfx& gfx,
                             const GroovePuterMidi::SmfPlayerSnapshot& state,
                             const Rect& region,
                             IGfxColor color);

    MiniAcid& miniAcid_;
    AudioGuard audioGuard_;
    GroovePuterMidi::ISmfPlayerService* player_{nullptr};
    std::string title_{"MIDI PLAYER"};
    bool browserVisible_{true};
    bool performanceVisible_{false};
    bool channelInspectorVisible_{false};
    bool muteMixerVisible_{false};
    int channelInspectorScroll_{0};
    uint32_t lastMidiVisualEpoch_{0};
    uint32_t lastMidiVisualPulse_{0};
    uint16_t midiWavePhase_{0};
    uint8_t midiWaveEnvelope_{0};

    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audioGuard_) audioGuard_(std::forward<F>(fn));
        else fn();
    }
};
