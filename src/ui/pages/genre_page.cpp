#ifndef USE_RETRO_THEME
#define USE_RETRO_THEME          // включаем ретро‑тему проектно
#endif
#ifndef USE_AMBER_THEME
#define USE_AMBER_THEME
#endif

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
#ifdef USE_AMBER_THEME
#include "../amber_ui_theme.h"
#include "../amber_widgets.h"
#endif

// Preset mappings: (genre, texture)
// genre indices follow GenerativeMode order (see genre_manager.h)
// texture: 0=Clean, 1=Dub, 2=LoFi, 3=Industrial
static const uint8_t kPresetGenre[8]   = { 0, 5, 6, 8, 4, 0, 1, 2 };
static const uint8_t kPresetTexture[8] = { 0, 1, 2, 0, 0, 1, 2, 3 };

namespace {
constexpr int kGenreVisibleRows = 4;
constexpr int kTextureVisibleRows = 4;

const char* applyModeShort(const MiniAcid& mini) {
    return mini.sceneManager().currentScene().genre.regenerateOnApply ? "S+P" : "SND";
}

const char* grooveModeShort(const MiniAcid& mini) {
    return mini.grooveboxMode() == GrooveboxMode::Acid ? "ACD" : "MIN";
}

bool regenOnApply(const MiniAcid& mini) {
    return mini.sceneManager().currentScene().genre.regenerateOnApply;
}

int computeScrollTop(int itemCount, int visibleRows, int selectedIndex, int currentTop) {
    if (itemCount <= visibleRows) return 0;
    int top = currentTop;
    if (selectedIndex < top) top = selectedIndex;
    if (selectedIndex >= top + visibleRows) top = selectedIndex - visibleRows + 1;
    int maxTop = itemCount - visibleRows;
    if (top > maxTop) top = maxTop;
    if (top < 0) top = 0;
    return top;
}

void drawScrollBar(IGfx& gfx, int x, int y, int h,
                   int itemCount, int visibleRows, int topIndex,
                   IGfxColor track, IGfxColor thumb) {
    if (itemCount <= visibleRows) return;
    gfx.drawRect(x, y, 3, h, track);
    int trackH = h - 2;
    int thumbH = (trackH * visibleRows) / itemCount;
    if (thumbH < 6) thumbH = 6;
    int maxTop = itemCount - visibleRows;
    int thumbY = y + 1;
    if (maxTop > 0) {
        thumbY = y + 1 + (trackH - thumbH) * topIndex / maxTop;
    }
    gfx.fillRect(x + 1, thumbY, 1, thumbH, thumb);
}
} // namespace

const char* GenrePage::genreNames[kGenerativeModeCount] = {
    "ACID", "MINIMAL", "TECHNO", "ELECTRO", "RAVE", "REGGAE", "TRIPHOP", "BROKEN", "CHIP"
};

const char* GenrePage::textureNames[kTextureModeCount] = {
    "CLEAN", "DUB", "LOFI", "HARD", "PSY"
};

const char* GenrePage::presetNames[8] = {
    "303 ACID", "REGGAE DUB", "TRIPHOP", 
    "CHIP", "RAVE", "ACID DUB",
    "MINIMAL LOFI", "TECHNO DUB"
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
        case VisualStyle::AMBER:
            drawAmberStyle(gfx);
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
    UI::drawFeelHeaderHud(gfx, mini_acid_, 166, 9);

    // Content
    LayoutManager::clearContent(gfx);

    // Minimal focus markers instead of big labels
    const int y0 = LayoutManager::lineY(0);
    gfx.setTextColor(IGfxColor(0x808080)); // dim
    gfx.drawText(Layout::COL_1, y0, (focus_ == FocusArea::GENRE) ? "G>" : "G ");
    gfx.drawText(Layout::COL_2, y0, (focus_ == FocusArea::TEXTURE) ? "T>" : "T ");
    char modeBuf[26];
    std::snprintf(modeBuf, sizeof(modeBuf), "A:%s G:%s", applyModeShort(mini_acid_), grooveModeShort(mini_acid_));
    gfx.drawText(168, y0, modeBuf);

    // Two columns: Genre / Texture
    const int listY = LayoutManager::lineY(1);
    genreScroll_ = computeScrollTop(kGenerativeModeCount, kGenreVisibleRows, genreIndex_, genreScroll_);

    // Genre list (windowed)
    for (int i = 0; i < kGenreVisibleRows; i++) {
        int idx = genreScroll_ + i;
        if (idx >= kGenerativeModeCount) break;
        int rowY = listY + i * Layout::LINE_HEIGHT;
        bool selected = (idx == genreIndex_) && (focus_ == FocusArea::GENRE);
        bool hasIcon = (idx == prevGenreIndex_);
        Widgets::drawListRow(gfx, Layout::COL_1, rowY, Layout::COL_WIDTH, genreNames[idx], selected, hasIcon);
    }
    drawScrollBar(gfx, Layout::COL_1 + Layout::COL_WIDTH - 3, listY,
                  kGenreVisibleRows * Layout::LINE_HEIGHT,
                  kGenerativeModeCount, kGenreVisibleRows, genreScroll_,
                  COLOR_LABEL, COLOR_KNOB_2);

    textureScroll_ = computeScrollTop(kTextureModeCount, kTextureVisibleRows, textureIndex_, textureScroll_);
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int idx = textureScroll_ + i;
        if (idx >= kTextureModeCount) break;
        int rowY = listY + i * Layout::LINE_HEIGHT;
        bool selected = (idx == textureIndex_) && (focus_ == FocusArea::TEXTURE);
        bool hasIcon = (idx == prevTextureIndex_);
        Widgets::drawListRow(gfx, Layout::COL_2, rowY, Layout::COL_WIDTH, textureNames[idx], selected, hasIcon);
    }
    drawScrollBar(gfx, Layout::COL_2 + Layout::COL_WIDTH - 3, listY,
                  kTextureVisibleRows * Layout::LINE_HEIGHT,
                  kTextureModeCount, kTextureVisibleRows, textureScroll_,
                  COLOR_LABEL, COLOR_KNOB_2);

    Widgets::drawBarRow(
        gfx,
        Layout::COL_2,
        LayoutManager::lineY(5),
        Layout::COL_WIDTH,
        "TX",
        mini_acid_.sceneManager().currentScene().genre.textureAmount / 100.0f,
        true
    );

    // Presets grid: secondary lane, no label
    const int gridY = LayoutManager::lineY(6);

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
        right = "[TAB] NEXT [M]APPLY [G]MODE";
    } else {
        left  = "[UP/DN] SCROLL  [ENT] APPLY";
        right = "[TAB] NEXT [M]APPLY [G]MODE";
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
    char amtBuf[16];
    std::snprintf(amtBuf, sizeof(amtBuf), "TX:%d", (int)mini_acid_.sceneManager().currentScene().genre.textureAmount);
    gfx.setTextColor(TEXT_DIM);
    gfx.drawText(196, INDICATOR_Y, amtBuf);
    
    // Genre column (left)
    const int LIST_Y = CONTENT_Y + 14;
    const int LIST_W = 110;
    const int ROW_H = 12;

    static const uint32_t genreColors[] = {
        NEON_CYAN, NEON_PURPLE, NEON_MAGENTA, NEON_YELLOW,
        NEON_ORANGE, NEON_GREEN, NEON_CYAN, NEON_PURPLE
    };

    genreScroll_ = computeScrollTop(kGenerativeModeCount, kGenreVisibleRows, genreIndex_, genreScroll_);
    for (int i = 0; i < kGenreVisibleRows; i++) {
        int idx = genreScroll_ + i;
        if (idx >= kGenerativeModeCount) break;
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (idx == genreIndex_);
        bool isActive = (idx == prevGenreIndex_);
        bool focused = genreFocus && isCursor;
        uint32_t color = genreColors[idx % (int)(sizeof(genreColors)/sizeof(genreColors[0]))];
        
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
            drawGlowText(gfx, 16, rowY, genreNames[idx], color, TEXT_PRIMARY);
        } else {
            // Text is colored if active OR cursor
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(TEXT_SECONDARY);
            if (isActive && !isCursor) textColor = IGfxColor(TEXT_DIM); // Dim if active but not selected? Or just keep colored.
            // Let's keep it simple: Color if Active or Cursor.
            gfx.setTextColor(textColor);
            gfx.drawText(16, rowY, genreNames[idx]);
        }
    }

    drawScrollBar(gfx, 4 + LIST_W - 3, LIST_Y, kGenreVisibleRows * ROW_H,
                  kGenerativeModeCount, kGenreVisibleRows, genreScroll_,
                  GRID_MEDIUM, FOCUS_GLOW);
    
    // Texture column (right)
    const int TEX_X = 124;
    const int TEX_W = 112;
    
    static const uint32_t textureColors[] = {
        0x9CA3AF, 0x38BDF8, 0xF59E0B, 0xEF4444, 0xA78BFA  // CLEAN, DUB, LOFI, HARD, PSY
    };

    textureScroll_ = computeScrollTop(kTextureModeCount, kTextureVisibleRows, textureIndex_, textureScroll_);
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int idx = textureScroll_ + i;
        if (idx >= kTextureModeCount) break;
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (idx == textureIndex_);
        bool isActive = (idx == prevTextureIndex_);
        bool focused = textureFocus && isCursor;
        uint32_t color = textureColors[idx % (int)(sizeof(textureColors)/sizeof(textureColors[0]))];
        
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
            drawGlowText(gfx, TEX_X + 12, rowY, textureNames[idx], color, TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(TEXT_SECONDARY);
            gfx.setTextColor(textColor);
            gfx.drawText(TEX_X + 12, rowY, textureNames[idx]);
        }
    }
    drawScrollBar(gfx, TEX_X + TEX_W - 3, LIST_Y, kTextureVisibleRows * ROW_H,
                  kTextureModeCount, kTextureVisibleRows, textureScroll_,
                  GRID_MEDIUM, FOCUS_GLOW);
    
    // Preset grid (bottom)
    const int GRID_Y = LIST_Y + kGenreVisibleRows * ROW_H + 4;
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
            leftHints = "UP/DN:Scroll  ENT:Apply";
            rightHints = "TAB:Texture";
            focusMode = "GENRE";
            break;
        case FocusArea::TEXTURE:
            leftHints = "UP/DN:Scroll  ENT:Apply";
            rightHints = "TAB:Presets";
            focusMode = "TEXTURE";
            break;
        case FocusArea::PRESETS:
            leftHints = "ARROWS:Navigate  ENT:Load";
            rightHints = "TAB:Genre";
            focusMode = "PRESETS";
            break;
    }

    char modeBuf[24];
    std::snprintf(modeBuf, sizeof(modeBuf), "A:%s G:%s", applyModeShort(mini_acid_), grooveModeShort(mini_acid_));
    rightHints = modeBuf;
    
    drawFooterBar(gfx, 0, 135 - 12, 240, 12, leftHints, rightHints, focusMode);
#else
    // Fallback to minimal if retro theme not included
    drawMinimalStyle(gfx);
#endif
}

// =================================================================
// AMBER STYLE (Terminal)
// =================================================================

void GenrePage::drawAmberStyle(IGfx& gfx) {
#ifdef USE_AMBER_THEME
    char genreStr[32];
    std::snprintf(genreStr, sizeof(genreStr), "%s/%s",
                  genreNames[genreIndex_], textureNames[textureIndex_]);
    
    AmberWidgets::drawHeaderBar(gfx, 0, 0, 240, 14,
                  "GENRE", genreStr,
                  mini_acid_.isPlaying(),
                  (int)(mini_acid_.bpm() + 0.5f),
                  mini_acid_.currentStep());
    
    const int CONTENT_Y = 16;
    const int CONTENT_H = 135 - 16 - 12;
    gfx.fillRect(0, CONTENT_Y, 240, CONTENT_H, AmberTheme::BG_DEEP_BLACK);
    
    const int INDICATOR_Y = CONTENT_Y + 2;
    bool genreFocus = (focus_ == FocusArea::GENRE);
    bool textureFocus = (focus_ == FocusArea::TEXTURE);
    bool presetFocus = (focus_ == FocusArea::PRESETS);
    
    if (genreFocus) {
        AmberWidgets::drawGlowText(gfx, 4, INDICATOR_Y, "G>", AmberTheme::FOCUS_GLOW, AmberTheme::NEON_CYAN);
    } else {
        gfx.setTextColor(AmberTheme::GRID_DIM);
        gfx.drawText(4, INDICATOR_Y, "G ");
    }
    
    if (textureFocus) {
        AmberWidgets::drawGlowText(gfx, 124, INDICATOR_Y, "T>", AmberTheme::FOCUS_GLOW, AmberTheme::NEON_MAGENTA);
    } else {
        gfx.setTextColor(AmberTheme::GRID_DIM);
        gfx.drawText(124, INDICATOR_Y, "T ");
    }
    char amtBuf[16];
    std::snprintf(amtBuf, sizeof(amtBuf), "TX:%d", (int)mini_acid_.sceneManager().currentScene().genre.textureAmount);
    gfx.setTextColor(AmberTheme::TEXT_DIM);
    gfx.drawText(196, INDICATOR_Y, amtBuf);
    
    const int LIST_Y = CONTENT_Y + 14;
    const int LIST_W = 110;
    const int ROW_H = 12;
    
    static const uint32_t genreColors[] = {
        AmberTheme::NEON_CYAN, AmberTheme::NEON_PURPLE, AmberTheme::NEON_MAGENTA,
        AmberTheme::NEON_YELLOW, AmberTheme::NEON_ORANGE,
        AmberTheme::NEON_GREEN, AmberTheme::NEON_CYAN, AmberTheme::NEON_PURPLE
    };
    
    genreScroll_ = computeScrollTop(kGenerativeModeCount, kGenreVisibleRows, genreIndex_, genreScroll_);
    for (int i = 0; i < kGenreVisibleRows; i++) {
        int idx = genreScroll_ + i;
        if (idx >= kGenerativeModeCount) break;
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (idx == genreIndex_);
        bool isActive = (idx == prevGenreIndex_);
        bool focused = genreFocus && isCursor;
        uint32_t color = genreColors[idx % (int)(sizeof(genreColors)/sizeof(genreColors[0]))];
        
        if (isCursor) {
            gfx.fillRect(4, rowY - 1, LIST_W - 4, ROW_H - 1, AmberTheme::BG_INSET);
            if (focused) {
                AmberWidgets::drawGlowBorder(gfx, 4, rowY - 1, LIST_W - 4, ROW_H - 1, color, 1);
            } else {
                gfx.drawRect(4, rowY - 1, LIST_W - 4, ROW_H - 1, AmberTheme::GRID_MEDIUM);
            }
        }
        
        int ledX = 8;
        int ledY = rowY + ROW_H / 2;
        AmberWidgets::drawLED(gfx, ledX, ledY, 2, isActive, color);
        
        if (focused) {
            AmberWidgets::drawGlowText(gfx, 16, rowY, genreNames[idx], color, AmberTheme::TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(AmberTheme::TEXT_SECONDARY);
            if (isActive && !isCursor) textColor = IGfxColor(AmberTheme::TEXT_DIM);
            gfx.setTextColor(textColor);
            gfx.drawText(16, rowY, genreNames[idx]);
        }
    }

    drawScrollBar(gfx, 4 + LIST_W - 3, LIST_Y, kGenreVisibleRows * ROW_H,
                  kGenerativeModeCount, kGenreVisibleRows, genreScroll_,
                  AmberTheme::GRID_MEDIUM, AmberTheme::NEON_ORANGE);
    
    const int TEX_X = 124;
    const int TEX_W = 112;
    
    static const uint32_t textureColors[] = {
        AmberTheme::TEXT_SECONDARY, AmberTheme::NEON_CYAN, AmberTheme::NEON_ORANGE, AmberTheme::NEON_MAGENTA, AmberTheme::NEON_YELLOW
    };

    textureScroll_ = computeScrollTop(kTextureModeCount, kTextureVisibleRows, textureIndex_, textureScroll_);
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int idx = textureScroll_ + i;
        if (idx >= kTextureModeCount) break;
        int rowY = LIST_Y + i * ROW_H;
        bool isCursor = (idx == textureIndex_);
        bool isActive = (idx == prevTextureIndex_);
        bool focused = textureFocus && isCursor;
        uint32_t color = textureColors[idx % (int)(sizeof(textureColors)/sizeof(textureColors[0]))];
        
        if (isCursor) {
            gfx.fillRect(TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, AmberTheme::BG_INSET);
            if (focused) {
                AmberWidgets::drawGlowBorder(gfx, TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, color, 1);
            } else {
                gfx.drawRect(TEX_X, rowY - 1, TEX_W - 4, ROW_H - 1, AmberTheme::GRID_MEDIUM);
            }
        }
        
        int ledX = TEX_X + 4;
        int ledY = rowY + ROW_H / 2;
        AmberWidgets::drawLED(gfx, ledX, ledY, 2, isActive, color);
        
        if (focused) {
            AmberWidgets::drawGlowText(gfx, TEX_X + 12, rowY, textureNames[idx], color, AmberTheme::TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : IGfxColor(AmberTheme::TEXT_SECONDARY);
            gfx.setTextColor(textColor);
            gfx.drawText(TEX_X + 12, rowY, textureNames[idx]);
        }
    }
    drawScrollBar(gfx, TEX_X + TEX_W - 3, LIST_Y, kTextureVisibleRows * ROW_H,
                  kTextureModeCount, kTextureVisibleRows, textureScroll_,
                  AmberTheme::GRID_MEDIUM, AmberTheme::NEON_ORANGE);
    
    const int GRID_Y = LIST_Y + kGenreVisibleRows * ROW_H + 4;
    const int BTN_W = 56;
    const int BTN_H = 10;
    const int GAP = 2;
    
    for (int i = 0; i < 8; i++) {
        int bx = 4 + (i % 4) * (BTN_W + GAP);
        int by = GRID_Y + (i / 4) * (BTN_H + GAP);
        bool selected = (i == presetIndex_);
        
        gfx.fillRect(bx, by, BTN_W, BTN_H, AmberTheme::BG_PANEL);
        gfx.drawRect(bx, by, BTN_W, BTN_H, selected ? AmberTheme::NEON_ORANGE : AmberTheme::GRID_MEDIUM);
        gfx.setTextColor(selected ? AmberTheme::TEXT_PRIMARY : AmberTheme::TEXT_SECONDARY);
        gfx.drawText(bx + 3, by + 2, presetNames[i]);
    }

    if (presetFocus) {
        int fx = 4;
        int fy = GRID_Y - 2;
        int fw = 4 * (BTN_W + GAP) - GAP;
        int fh = 2 * (BTN_H + GAP) - GAP + 2;
        AmberWidgets::drawGlowBorder(gfx, fx, fy, fw, fh, AmberTheme::NEON_ORANGE, 1);
    }
    
    char modeBuf[24];
    std::snprintf(modeBuf, sizeof(modeBuf), "A:%s G:%s", applyModeShort(mini_acid_), grooveModeShort(mini_acid_));
    AmberWidgets::drawFooterBar(gfx, 0, 135 - 12, 240, 12,
                                "[UP/DN]Scroll [ENT]Apply",
                                modeBuf,
                                "GENRE");
#else
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
        if (focus_ == FocusArea::GENRE) genreIndex_ = (genreIndex_ - 1 + kGenerativeModeCount) % kGenerativeModeCount;
        else if (focus_ == FocusArea::TEXTURE) textureIndex_ = (textureIndex_ - 1 + kTextureModeCount) % kTextureModeCount;
        else if (focus_ == FocusArea::PRESETS) presetIndex_ = (presetIndex_ - 1 + 8) % 8;
    };
    auto moveDown = [&]() {
        if (focus_ == FocusArea::GENRE) genreIndex_ = (genreIndex_ + 1) % kGenerativeModeCount;
        else if (focus_ == FocusArea::TEXTURE) textureIndex_ = (textureIndex_ + 1) % kTextureModeCount;
        else if (focus_ == FocusArea::PRESETS) presetIndex_ = (presetIndex_ + 1) % 8;
    };
    auto moveLeft = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount - 5;
            if (v < 0) v = 0;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::PRESETS) {
            if (presetIndex_ >= 4) presetIndex_ -= 4;
        }
    };
    auto moveRight = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount + 5;
            if (v > 100) v = 100;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::PRESETS) {
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

    // M: toggle apply mode (SOUND+PATTERN / SOUND ONLY)
    if (key == 'm' || key == 'M') {
        auto& gs = mini_acid_.sceneManager().currentScene().genre;
        gs.regenerateOnApply = !gs.regenerateOnApply;
        UI::showToast(gs.regenerateOnApply ? "Genre Apply: SOUND+PATTERN" : "Genre Apply: SOUND ONLY");
        return true;
    }

    // G: toggle groovebox mode (ACID/MINIMAL) in genre context.
    if (key == 'g' || key == 'G') {
        withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
        UI::showToast(mini_acid_.grooveboxMode() == GrooveboxMode::Acid ? "Groove Mode: ACID" : "Groove Mode: MINIMAL");
        return true;
    }

    // Direct preset selection (1-8)
    if (key >= '1' && key <= '8') {
        presetIndex_ = key - '1';
        focus_ = FocusArea::PRESETS;
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(kPresetGenre[presetIndex_]));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(kPresetTexture[presetIndex_]));
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(kPresetGenre[presetIndex_]);
            gs.textureMode = static_cast<uint8_t>(kPresetTexture[presetIndex_]);
        });
        updateFromEngine();
        return true;
    }

    // '0': randomize
    if (key == '0') {
        withAudioGuard([&]() {
            int gen = std::rand() % kGenerativeModeCount;
            int tex = std::rand() % kTextureModeCount;
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(gen));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(tex));
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(gen);
            gs.textureMode = static_cast<uint8_t>(tex);
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
    const bool doRegenerate = regenOnApply(mini_acid_);
    if (focus_ == FocusArea::PRESETS) {
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(kPresetGenre[presetIndex_]));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(kPresetTexture[presetIndex_]));
            
            // Apply base timbre, reset bias tracking, then apply texture as delta from 0
            mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
            mini_acid_.genreManager().resetTextureBiasTracking();
            mini_acid_.genreManager().applyTexture(mini_acid_);
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(kPresetGenre[presetIndex_]);
            gs.textureMode = static_cast<uint8_t>(kPresetTexture[presetIndex_]);
            if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
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
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(genreIndex_);
            gs.textureMode = static_cast<uint8_t>(textureIndex_);
            if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
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
