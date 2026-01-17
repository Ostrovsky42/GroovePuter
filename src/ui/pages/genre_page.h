#pragma once

#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"

class GenrePage : public IPage {
public:
    GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;
    
private:
    MiniAcid& mini_acid_;
    AudioGuard& audio_guard_;
    
    enum FocusAxis { GENERATIVE, TEXTURE };
    FocusAxis focusAxis_ = GENERATIVE;
    
    std::string title_ = "GENRE";
    
    void setGenerativeMode(GenerativeMode mode);
    void setTextureMode(TextureMode mode);
    void cycleGenerative(int dir);
    void cycleTexture(int dir);
    void applyGenrePreset(int presetIndex);
    void regenerate();
    
    void withAudioGuard(const std::function<void()>& fn);
};
