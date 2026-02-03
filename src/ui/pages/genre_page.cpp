#define USE_RETRO_THEME          // включаем ретро‑тему проектно

#include "genre_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include <cstdio>
#include <cstdlib>

// Optional: Include retro theme for RETRO_CLASSIC style
#ifdef USE_RETRO_THEME
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
using namespace RetroTheme;
using namespace RetroWidgets;
#endif

// Preset mappings: (genre, texture)
// genre: 0=Acid, 1=Minimal, 2=Techno, 3=Electro, 4=Rave
// texture: 0=Clean, 1=Dub, 2=Dark, 3=Hard
static const uint8_t kPresetGenre[8]   = { 0, 2, 2, 3, 4, 0, 1, 2 };
static const uint8_t kPresetTexture[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };

const char* GenrePage::genreNames[5] = {
    "ACID", "MINIMAL", "TECHNO", "ELECTRO", "RAVE"
};

const char* GenrePage::textureNames[4] = {
    "CLEAN", "DUB", "DARK", "HARD"
};

const char* GenrePage::presetNames[8] = {
    "303 ACID", "DUB TECHNO", "DARK TECHNO", 
    "HARD ELECTRO", "RAVE", "ACID DUB",
    "MINIMAL DARK", "HARD TECHNO"
};

GenrePage::GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
    (void)gfx;
    visualStyle_ = UI::currentStyle;
    updateFromEngine();
}

// =================================================================
// MAIN DRAW: STYLE DISPATCHER
// =================================================================

void GenrePage::draw(IGfx& gfx) {
    switch (UI::currentStyle) {
        case VisualStyle::RETRO_CLASSIC:
            drawRetroClassicStyle(gfx);
            break;
        case VisualStyle::MINIMAL:
        default:
            drawMinimalStyle(gfx);
            break;
    }
}

// =================================================================
// MINIMAL STYLE (Original clean 'pro 80-x' aesthetic)
// =================================================================

void GenrePage::drawMinimalStyle(IGfx& gfx) {
    // Header: compact "GENRE/TEXTURE" (ASCII safe)
    char genreStr[24];
    std::snprintf(genreStr, sizeof(genreStr), "%s/%s",
                  genreNames[genreIndex_], textureNames[textureIndex_]);

    UI::drawStandardHeader(gfx, mini_acid_, genreStr);

    // Content
    LayoutManager::clearContent(gfx);

    // Minimal focus markers instead of big labels
    const int y0 = LayoutManager::lineY(0);
    gfx.setTextColor(IGfxColor(0x808080)); // dim
    gfx.drawText(Layout::COL_1, y0, (focus_ == FocusArea::GENRE) ? "G>" : "G ");
    gfx.drawText(Layout::COL_2, y0, (focus_ == FocusArea::TEXTURE) ? "T>" : "T ");

    // Two columns: Genre / Texture
    const int listY = LayoutManager::lineY(1);

    UI::drawVerticalList(
        gfx,
        Layout::COL_1,
        listY,
        Layout::COL_WIDTH,
        genreNames,
        5,
        genreIndex_,
        focus_ == FocusArea::GENRE,
        prevGenreIndex_);

    UI::drawVerticalList(
        gfx,
        Layout::COL_2,
        listY,
        Layout::COL_WIDTH,
        textureNames,
        4,
        textureIndex_,
        focus_ == FocusArea::TEXTURE,
        prevTextureIndex_);

    // Presets grid: secondary lane, no label
    const int gridY = LayoutManager::lineY(4);

    UI::drawButtonGridHelper(
        gfx,
        Layout::COL_1,
        gridY,
        presetNames,
        8,
        presetIndex_,
        focus_ == FocusArea::PRESETS
    );

    // Footer: short, context-aware
    const char* left = nullptr;
    const char* right = nullptr;

    if (focus_ == FocusArea::PRESETS) {
        left  = "[1-8] PICK  [ENT] APPLY";
        right = "[TAB] NEXT";
    } else {
        left  = "[ARROWS] MOVE  [ENT] APPLY";
        right = "[TAB] NEXT  [0] RAND";
    }

    UI::drawStandardFooter(gfx, left, right);
}

// =================================================================
// RETRO CLASSIC STYLE (80s Neon Cyberpunk)
// =================================================================

void GenrePage::drawRetroClassicStyle(IGfx& gfx) {
#ifdef USE_RETRO_THEME
    // Header with retro theme
    char genreStr[32];
    std::snprintf(genreStr, sizeof(genreStr), "%s/%s",
                  genreNames[genreIndex_], textureNames[textureIndex_]);
    
    drawHeaderBar(gfx, 0, 0, 240, 14,
                  "GENRE", genreStr,
                  mini_acid_.isPlaying(),
                  (int)(mini_acid_.bpm() + 0.5f),
                  mini_acid_.currentStep());
    
    // Content area setup
    const int CONTENT_Y = 16;
    const int CONTENT_H = 135 - 16 - 12;
    gfx.fillRect(0, CONTENT_Y, 240, CONTENT_H, BG_DEEP_BLACK);
    
    // Focus indicators
    const int INDICATOR_Y = CONTENT_Y + 2;
    bool genreFocus = (focus_ == FocusArea::GENRE);
    bool textureFocus = (focus_ == FocusArea::TEXTURE);
    bool presetFocus = (focus_ == FocusArea::PRESETS);
    
    if (genreFocus) {
        drawGlowText(gfx, 4, INDICATOR_Y, "G>", FOCUS_GLOW, NEON_CYAN);
    } else {
        gfx.setTextColor(GRID_DIM);
        gfx.drawText(4, INDICATOR_Y, "G ");
    }
    
    if (textureFocus) {
        drawGlowText(gfx, 124, INDICATOR_Y, "T>", FOCUS_GLOW, NEON_MAGENTA);
    } else {
        gfx.setTextColor(GRID_DIM);
        gfx.drawText(124, INDICATOR_Y, "T ");
    }
    
    // Genre column (left)
    const int LIST_Y = CONTENT_Y + 14;
    const int LIST_W = 110;
    const int ROW_H = 12;
    
    static const uint16_t genreColors[] = {
        NEON_CYAN, NEON_PURPLE, NEON_MAGENTA, NEON_YELLOW, NEON_ORANGE
    };
    
    for (int i = 0; i < 5; i++) {
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (i == genreIndex_);
        bool isActive = (i == prevGenreIndex_);
        bool focused = genreFocus && isCursor;
        uint16_t color = genreColors[i];
        
        if (isCursor) {
            gfx.fillRect(4, rowY - 1, LIST_W - 4, ROW_H - 1, BG_INSET);
            if (focused) {
                drawGlowBorder(gfx, 4, rowY - 1, LIST_W - 4, ROW_H - 1, color, 1);
            } else {
                gfx.drawRect(4, rowY - 1, LIST_W - 4, ROW_H - 1, GRID_MEDIUM);
            }
        }
        
        int ledX = 8;
        int ledY = rowY + ROW_H / 2;
        // LED reflects ACTIVE state (what is playing)
        drawLED(gfx, ledX, ledY, 2, isActive, color);
        
        if (focused) {
            drawGlowText(gfx, 16, rowY, genreNames[i], color, TEXT_PRIMARY);
        } else {
            // Text is colored if active OR cursor
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(TEXT_SECONDARY);
            if (isActive && !isCursor) textColor = IGfxColor(TEXT_DIM); // Dim if active but not selected? Or just keep colored.
            // Let's keep it simple: Color if Active or Cursor.
            gfx.setTextColor(textColor);
            gfx.drawText(16, rowY, genreNames[i]);
        }
    }
    
    // Texture column (right)
    const int TEX_X = 124;
    const int TEX_W = 112;
    
    static const uint16_t textureColors[] = {
        0xAD55, 0x07E0, 0x8010, 0xF800  // CLEAN, DUB, DARK, HARD
    };
    
    for (int i = 0; i < 4; i++) {
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (i == textureIndex_);
        bool isActive = (i == prevTextureIndex_);
        bool focused = textureFocus && isCursor;
        uint16_t color = textureColors[i];
        
        if (isCursor) {
            gfx.fillRect(TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, BG_INSET);
            if (focused) {
                drawGlowBorder(gfx, TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, color, 1);
            } else {
                gfx.drawRect(TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, GRID_MEDIUM);
            }
        }
        
        int ledX = TEX_X + 4;
        int ledY = rowY + ROW_H / 2;
        drawLED(gfx, ledX, ledY, 2, isActive, color);
        
        if (focused) {
            drawGlowText(gfx, TEX_X + 12, rowY, textureNames[i], color, TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(TEXT_SECONDARY);
            gfx.setTextColor(textColor);
            gfx.drawText(TEX_X + 12, rowY, textureNames[i]);
        }
    }
    
    // Preset grid (bottom)
    const int GRID_Y = LIST_Y + 5 * ROW_H + 4;
    const int BTN_W = 56;
    const int BTN_H = 10;
    const int GAP = 2;
    
    for (int i = 0; i < 8; i++) {
        int col = i % 4;
        int row = i / 4;
        int btnX = 4 + col * (BTN_W + GAP);
        int btnY = GRID_Y + row * (BTN_H + GAP);
        
        bool selected = (i == presetIndex_);
        bool focused = presetFocus && selected;
        
        uint16_t genreColor = genreColors[kPresetGenre[i]];
        uint16_t texColor = textureColors[kPresetTexture[i]];
        
        if (selected) {
            gfx.fillRect(btnX, btnY, BTN_W, BTN_H, BG_INSET);
            if (focused) {
                drawGlowBorder(gfx, btnX, btnY, BTN_W, BTN_H, genreColor, 1);
                gfx.drawLine(btnX + 1, btnY + BTN_H - 2, btnX + BTN_W - 2, btnY + BTN_H - 2, texColor);
            } else {
                gfx.drawRect(btnX, btnY, BTN_W, BTN_H, GRID_MEDIUM);
            }
        } else {
            gfx.fillRect(btnX, btnY, BTN_W, BTN_H, BG_DARK_GRAY);
            gfx.drawRect(btnX, btnY, BTN_W, BTN_H, GRID_DIM);
        }
        
        if (selected) {
            drawLED(gfx, btnX + 3, btnY + 3, 1, true, genreColor);
        }
        
        uint16_t textColor = selected ? genreColor : TEXT_DIM;
        if (focused) textColor = TEXT_PRIMARY;
        
        gfx.setTextColor(textColor);
        const char* name = presetNames[i];
        int textW = strlen(name) * 6;
        int textX = btnX + (BTN_W - textW) / 2;
        if (textX < btnX + 8) textX = btnX + 8;
        
        gfx.drawText(textX, btnY + 1, name);
    }
    
    // Footer
    const char* leftHints = "";
    const char* rightHints = "";
    const char* focusMode = nullptr;
    
    switch (focus_) {
        case FocusArea::GENRE:
            leftHints = "ARROWS:Select  ENT:Apply";
            rightHints = "TAB:Texture";
            focusMode = "GENRE";
            break;
        case FocusArea::TEXTURE:
            leftHints = "ARROWS:Select  ENT:Apply";
            rightHints = "TAB:Presets";
            focusMode = "TEXTURE";
            break;
        case FocusArea::PRESETS:
            leftHints = "ARROWS:Navigate  ENT:Load";
            rightHints = "TAB:Genre";
            focusMode = "PRESETS";
            break;
    }
    
    drawFooterBar(gfx, 0, 135 - 12, 240, 12, leftHints, rightHints, focusMode);
#else
    // Fallback to minimal if retro theme not included
    drawMinimalStyle(gfx);
#endif
}

// =================================================================
// INPUT HANDLING (unchanged)
// =================================================================

bool GenrePage::handleEvent(UIEvent& e) {
    if (e.event_type != MINIACID_KEY_DOWN) return false;
    
    // Helper lambdas for navigation
    auto moveUp = [&]() {
        if (focus_ == FocusArea::GENRE) genreIndex_ = (genreIndex_ - 1 + 5) % 5;
        else if (focus_ == FocusArea::TEXTURE) textureIndex_ = (textureIndex_ - 1 + 4) % 4;
        else if (focus_ == FocusArea::PRESETS) presetIndex_ = (presetIndex_ - 1 + 8) % 8;
    };
    auto moveDown = [&]() {
        if (focus_ == FocusArea::GENRE) genreIndex_ = (genreIndex_ + 1) % 5;
        else if (focus_ == FocusArea::TEXTURE) textureIndex_ = (textureIndex_ + 1) % 4;
        else if (focus_ == FocusArea::PRESETS) presetIndex_ = (presetIndex_ + 1) % 8;
    };
    auto moveLeft = [&]() {
        if (focus_ == FocusArea::PRESETS) {
            if (presetIndex_ >= 4) presetIndex_ -= 4;
        }
    };
    auto moveRight = [&]() {
        if (focus_ == FocusArea::PRESETS) {
            if (presetIndex_ < 4) presetIndex_ += 4;
        }
    };

    int nav = UIInput::navCode(e);
    switch (nav) {
        case MINIACID_UP:    moveUp();    return true;
        case MINIACID_DOWN:  moveDown();  return true;
        case MINIACID_LEFT:  moveLeft();  return true;
        case MINIACID_RIGHT: moveRight(); return true;
        default: break;
    }

    char key = e.key;
    if (!key) return false;

    // TAB: cycle focus
    if (key == '\t') {
        if (focus_ == FocusArea::GENRE)   focus_ = FocusArea::TEXTURE;
        else if (focus_ == FocusArea::TEXTURE) focus_ = FocusArea::PRESETS;
        else focus_ = FocusArea::GENRE;
        return true;
    }

    // ENTER: apply current selection
    if (key == '\n' || key == '\r') {
        applyCurrent();
        return true;
    }

    // Direct preset selection (1-8)
    if (key >= '1' && key <= '8') {
        presetIndex_ = key - '1';
        focus_ = FocusArea::PRESETS;
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(kPresetGenre[presetIndex_]));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(kPresetTexture[presetIndex_]));
        });
        updateFromEngine();
        return true;
    }

    // '0': randomize
    if (key == '0') {
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(std::rand() % 5));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(std::rand() % 4));
        });
        updateFromEngine();
        return true;
    }

    return false;
}

// =================================================================
// INTERNAL METHODS (unchanged)
// =================================================================

void GenrePage::applyCurrent() {
    if (focus_ == FocusArea::PRESETS) {
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(kPresetGenre[presetIndex_]));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(kPresetTexture[presetIndex_]));
            
            // Apply base timbre, reset bias tracking, then apply texture as delta from 0
            mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
            mini_acid_.genreManager().resetTextureBiasTracking();
            mini_acid_.genreManager().applyTexture(mini_acid_);
            
            mini_acid_.regeneratePatternsWithGenre();
        });
        updateFromEngine();
    } else {
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(genreIndex_));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(textureIndex_));
            
            // Apply base timbre, reset bias tracking, then apply texture as delta from 0
            mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
            mini_acid_.genreManager().resetTextureBiasTracking();
            mini_acid_.genreManager().applyTexture(mini_acid_);
            
            mini_acid_.regeneratePatternsWithGenre();
        });
        prevGenreIndex_ = genreIndex_;
        prevTextureIndex_ = textureIndex_;
    }
}

void GenrePage::updateFromEngine() {
    genreIndex_ = static_cast<int>(mini_acid_.genreManager().generativeMode());
    textureIndex_ = static_cast<int>(mini_acid_.genreManager().textureMode());
    prevGenreIndex_ = genreIndex_;
    prevTextureIndex_ = textureIndex_;
}
