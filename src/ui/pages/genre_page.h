#pragma once

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_widgets.h"
#include <cstddef>

class GenrePage : public IPage {
public:
    enum class FocusArea { GENRE, TEXTURE, APPLY_MODE };
    
    GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
    
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;
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
    int genreScroll_ = 0;
    int textureScroll_ = 0;
    
    // Данные
    static const char* genreNames[kGenerativeModeCount];
    static const char* textureNames[kTextureModeCount];
    
    // Draw methods for different styles
    void drawMinimalStyle(IGfx& gfx);
    void drawRetroClassicStyle(IGfx& gfx);
    void drawAmberStyle(IGfx& gfx);
    
    void applyCurrent();
    int visibleTextureCount() const;
    int visibleTextureAt(int visibleIndex) const;
    int textureToVisibleIndex(int textureIndex) const;
    bool isCuratedMode() const;
    void setCuratedMode(bool enabled);
    void ensureTextureAllowedForCurrentGenre();
    void buildTextureLabel(int textureIndex, char* out, size_t outSize) const;
    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
    }
    
    void updateFromEngine();
};
