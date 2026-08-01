#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../ui_common.h"
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
    void refreshFiles();
    bool playSelected();
    bool togglePlayerTransport();
    void toggleGrooveTransport();
    void drawBrowser(IGfx& gfx);
    void drawNowPlaying(IGfx& gfx);
    void drawPerformance(IGfx& gfx);
    void drawChannelInspector(IGfx& gfx);
    void ensureSelectionVisible(int visibleRows);
    bool navigateIntoDir(const std::string& dirName);
    bool navigateUpDir();
    bool hasParentEntry() const;
    int entryCount() const;
    bool isDirEntry(int index) const;
    const std::string& displayName(int index) const;

    MiniAcid& miniAcid_;
    AudioGuard audioGuard_;
    GroovePuterMidi::ISmfPlayerService* player_{nullptr};
    std::string title_{"MIDI PLAYER"};
    std::vector<std::string> dirs_;
    std::vector<std::string> files_;
    std::string currentPath_{"/midi"};
    int selection_{0};
    int scroll_{0};
    bool browserVisible_{true};
    bool performanceVisible_{false};
    bool channelInspectorVisible_{false};
    int channelInspectorScroll_{0};

    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audioGuard_) audioGuard_(std::forward<F>(fn));
        else fn();
    }
};
