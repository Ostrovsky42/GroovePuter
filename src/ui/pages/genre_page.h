#pragma once

#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_widgets.h"

class GenrePage : public IPage {
public:
    enum class FocusArea { GENRE, TEXTURE, PRESETS };
    
    GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override { return title_; }
    
    // Style switching
    void setVisualStyle(VisualStyle style) override { visualStyle_ = style; }
    VisualStyle getVisualStyle() const { return visualStyle_; }
    
private:
    MiniAcid& mini_acid_;
    AudioGuard audio_guard_;
    std::string title_ = "GENRE";
    FocusArea focus_ = FocusArea::GENRE;
    VisualStyle visualStyle_ = VisualStyle::MINIMAL;  // Default to minimal (existing style)
    
    int genreIndex_ = 0;
    int prevGenreIndex_ = -1;
    int textureIndex_ = 0;
    int prevTextureIndex_ = -1;
    int presetIndex_ = 0;
    
    // Данные
    static const char* genreNames[5];
    static const char* textureNames[4];
    static const char* presetNames[8];
    
    // Draw methods for different styles
    void drawMinimalStyle(IGfx& gfx);
    void drawRetroClassicStyle(IGfx& gfx);
    void drawAmberStyle(IGfx& gfx);
    
    void applyCurrent();
    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
    }
    
    void updateFromEngine();
};
