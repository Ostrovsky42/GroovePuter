#pragma once

#include <string>
#include <vector>

#include "../ui_common.h"
#include "src/midi/smf_player_service.h"

class SmfPlayerPage final : public IPage {
public:
    SmfPlayerPage(IGfx& gfx, MiniAcid& miniAcid);

    const std::string& getTitle() const override { return title_; }
    void onEnter(int context) override;
    bool handleEvent(UIEvent& event) override;
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;

private:
    void refreshFiles();
    bool playSelected();
    void drawBrowser(IGfx& gfx);
    void drawNowPlaying(IGfx& gfx);
    void ensureSelectionVisible(int visibleRows);

    MiniAcid& miniAcid_;
    GroovePuterMidi::ISmfPlayerService* player_{nullptr};
    std::string title_{"MIDI PLAYER"};
    std::vector<std::string> files_;
    int selection_{0};
    int scroll_{0};
    bool browserVisible_{true};
};
