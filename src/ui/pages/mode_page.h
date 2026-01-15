#pragma once
#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"

class ModePage : public IPage {
public:
    ModePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const;

private:
    void toggleMode();
    void applyTo303(int voiceIdx);
    void applyToTape();
    void previewMode();
    
    void drawModeBox(IGfx& gfx, int x, int y, const char* name, bool active, uint16_t color, int w, int h);
    void withAudioGuard(const std::function<void()>& fn);

    MiniAcid& mini_acid_;
    AudioGuard& audio_guard_;
    std::string title_ = "MODE";
};
