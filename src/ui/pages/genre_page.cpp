#include "genre_page.h"
#include <cstdio>
#include "../ui_colors.h"

GenrePage::GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
}

void GenrePage::draw(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    GenreManager& gm = mini_acid_.genreManager();

    // Clear background
    gfx.fillRect(x, y, w, h, COLOR_BLACK);

    // Header: Current genre name
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 5, y + 3, "GENRE:");
    gfx.setTextColor(COLOR_KNOB_1);
    gfx.drawText(x + 55, y + 3, gm.getCurrentGenreName());

    // === GENERATIVE AXIS (Left Side) ===
    int leftX = x + 5;
    int colY = y + 18;
    
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(leftX, colY, "GENERATIVE");
    
    const char* genNames[] = {"Acid", "Minimal", "Hypnotic", "Electro", "Rave"};
    int currentGen = static_cast<int>(gm.generativeMode());
    
    // Selection box
    int boxY = colY + 10;
    if (focusAxis_ == GENERATIVE) {
        gfx.fillRect(leftX, boxY, 55, 14, COLOR_KNOB_1);
        gfx.setTextColor(COLOR_BLACK);
    } else {
        gfx.drawRect(leftX, boxY, 55, 14, COLOR_KNOB_2);
        gfx.setTextColor(COLOR_WHITE);
    }
    gfx.drawText(leftX + 4, boxY + 3, genNames[currentGen]);
    
    // Mode list
    int listY = boxY + 18;
    for (int i = 0; i < 5; i++) {
        gfx.setTextColor((i == currentGen) ? COLOR_WHITE : COLOR_LABEL);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d %s", i + 1, genNames[i]);
        gfx.drawText(leftX, listY + i * 10, buf);
    }

    // === TEXTURE AXIS (Right Side) ===
    int rightX = x + 115;
    
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(rightX, colY, "TEXTURE");
    
    const char* texNames[] = {"Clean", "Dub", "LoFi", "Industrial"};
    int currentTex = static_cast<int>(gm.textureMode());
    
    // Selection box
    if (focusAxis_ == TEXTURE) {
        gfx.fillRect(rightX, boxY, 55, 14, COLOR_KNOB_2);
        gfx.setTextColor(COLOR_BLACK);
    } else {
        gfx.drawRect(rightX, boxY, 55, 14, COLOR_KNOB_1);
        gfx.setTextColor(COLOR_WHITE);
    }
    gfx.drawText(rightX + 4, boxY + 3, texNames[currentTex]);
    
    // Mode list
    for (int i = 0; i < 4; i++) {
        gfx.setTextColor((i == currentTex) ? COLOR_WHITE : COLOR_LABEL);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d %s", i + 6, texNames[i]);
        gfx.drawText(rightX, listY + i * 10, buf);
    }

    // === HINTS ===
    int hintY = y + h - 20;
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 5, hintY, "[up/dn] axis [L/R] change [ENTER] regen");
}

bool GenrePage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

    switch (ui_event.key) {
        // Numbers 1-5: generative mode
        case '1':
            setGenerativeMode(GenerativeMode::Acid);
            return true;
        case '2':
            setGenerativeMode(GenerativeMode::Outrun);
            return true;
        case '3':
            setGenerativeMode(GenerativeMode::Darksynth);
            return true;
        case '4':
            setGenerativeMode(GenerativeMode::Electro);
            return true;
        case '5':
            setGenerativeMode(GenerativeMode::Rave);
            return true;
        
        // Numbers 6-9: texture mode
        case '6':
            setTextureMode(TextureMode::Clean);
            return true;
        case '7':
            setTextureMode(TextureMode::Dub);
            return true;
        case '8':
            setTextureMode(TextureMode::LoFi);
            return true;
        case '9':
            setTextureMode(TextureMode::Industrial);
            return true;
        
        // Navigation between axes
        case MINIACID_UP:
        case MINIACID_DOWN:
            focusAxis_ = (focusAxis_ == GENERATIVE) ? TEXTURE : GENERATIVE;
            return true;
        
        // Cycle within focused axis
        case MINIACID_LEFT:
            if (focusAxis_ == GENERATIVE) {
                cycleGenerative(-1);
            } else {
                cycleTexture(-1);
            }
            return true;
        
        case MINIACID_RIGHT:
            if (focusAxis_ == GENERATIVE) {
                cycleGenerative(1);
            } else {
                cycleTexture(1);
            }
            return true;
        
        // Regenerate patterns with current genre
        case '\n':
        case '\r':
            regenerate();
            return true;
        
        // Space to preview (start playback)
        case ' ':
            withAudioGuard([&](){
                mini_acid_.regeneratePatternsWithGenre();
                if (!mini_acid_.isPlaying()) {
                    mini_acid_.start();
                }
            });
            return true;
    }

    return false;
}

const std::string& GenrePage::getTitle() const {
    return title_;
}

void GenrePage::setGenerativeMode(GenerativeMode mode) {
    withAudioGuard([&](){
        mini_acid_.genreManager().setGenerativeMode(mode);
        
        // 1. Enforce Genre Timbre BASE (overwrites base params)
        mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
        
        // 2. Reset bias tracking (new base established)
        mini_acid_.genreManager().resetTextureBiasTracking();
        
        // 3. Re-apply current texture (as delta from new base)
        mini_acid_.genreManager().applyTexture(mini_acid_);
    });
}

void GenrePage::setTextureMode(TextureMode mode) {
    withAudioGuard([&](){
        mini_acid_.genreManager().setTextureMode(mode);
        // Apply texture params (delta bias + FX)
        mini_acid_.genreManager().applyTexture(mini_acid_);
        // Texture affects stepMask, so regenerate
        mini_acid_.regeneratePatternsWithGenre();
    });
}

void GenrePage::cycleGenerative(int dir) {
    withAudioGuard([&](){
        mini_acid_.genreManager().cycleGenerative(dir);
        
        // 1. Enforce Genre Timbre BASE
        mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
        
        // 2. Reset bias tracking
        mini_acid_.genreManager().resetTextureBiasTracking();
        
        // 3. Re-apply texture
        mini_acid_.genreManager().applyTexture(mini_acid_);
    });
}

void GenrePage::cycleTexture(int dir) {
    withAudioGuard([&](){
        mini_acid_.genreManager().cycleTexture(dir);
        // Apply texture params (delta bias + FX)
        mini_acid_.genreManager().applyTexture(mini_acid_);
        // Texture affects stepMask
        mini_acid_.regeneratePatternsWithGenre();
    });
}

void GenrePage::applyGenrePreset(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= kGenrePresetCount) return;
    
    // Use high-level wrappers to ensure proper timbre/texture logic sequence
    // 1. setGenerativeMode: Applies Base Timbre -> Resets Bias Tracking -> Applies Old Texture (Delta)
    // 2. setTextureMode: Sets New Texture -> Applies New Texture (Delta) -> Regenerates
    const GenrePreset& preset = kGenrePresets[presetIndex];
    setGenerativeMode(preset.generative);
    setTextureMode(preset.texture);
}

void GenrePage::regenerate() {
    withAudioGuard([&](){
        mini_acid_.regeneratePatternsWithGenre();
    });
}

void GenrePage::withAudioGuard(const std::function<void()>& fn) {
    if (audio_guard_) {
        audio_guard_(fn);
    } else {
        fn();
    }
}
