#include "genre_page.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include <cstdio>
#include <cstdlib>
#include "../../debug_log.h"

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
static const uint8_t kPresetGenre[8]   = { 0, 1, 3, 2, 4, 5, 6, 7 };
static const uint8_t kPresetTexture[8] = { 0, 0, 3, 1, 0, 1, 2, 4 };

namespace {
constexpr int kGenreVisibleRows = 4;
constexpr int kTextureVisibleRows = 4;

enum class ApplyMode : uint8_t {
    SoundOnly = 0,
    SoundPattern = 1,
    SoundPatternTempo = 2
};

ApplyMode currentApplyMode(const MiniAcid& mini) {
    const auto& gs = mini.sceneManager().currentScene().genre;
    if (!gs.regenerateOnApply) return ApplyMode::SoundOnly;
    if (gs.applyTempoOnApply) return ApplyMode::SoundPatternTempo;
    return ApplyMode::SoundPattern;
}

void setApplyMode(GenreSettings& gs, ApplyMode mode) {
    switch (mode) {
        case ApplyMode::SoundOnly:
            gs.regenerateOnApply = false;
            gs.applyTempoOnApply = false;
            break;
        case ApplyMode::SoundPattern:
            gs.regenerateOnApply = true;
            gs.applyTempoOnApply = false;
            break;
        case ApplyMode::SoundPatternTempo:
            gs.regenerateOnApply = true;
            gs.applyTempoOnApply = true;
            break;
    }
}

void cycleApplyMode(GenreSettings& gs) {
    ApplyMode mode = ApplyMode::SoundOnly;
    if (gs.regenerateOnApply) {
        mode = gs.applyTempoOnApply ? ApplyMode::SoundPatternTempo : ApplyMode::SoundPattern;
    }
    switch (mode) {
        case ApplyMode::SoundOnly: setApplyMode(gs, ApplyMode::SoundPattern); break;
        case ApplyMode::SoundPattern: setApplyMode(gs, ApplyMode::SoundPatternTempo); break;
        case ApplyMode::SoundPatternTempo: setApplyMode(gs, ApplyMode::SoundOnly); break;
    }
}

const char* applyModeToast(const MiniAcid& mini) {
    switch (currentApplyMode(mini)) {
        case ApplyMode::SoundOnly: return "Apply Mode: SOUND ONLY (Keeps patterns)";
        case ApplyMode::SoundPattern: return "Apply Mode: SOUND+PATTERN (Regenerates)";
        case ApplyMode::SoundPatternTempo: return "Apply Mode: SOUND+PATTERN+TEMPO";
    }
    return "Apply Mode";
}

const char* applyModeShort(const MiniAcid& mini) {
    switch (currentApplyMode(mini)) {
        case ApplyMode::SoundOnly: return "SND";
        case ApplyMode::SoundPattern: return "S+P";
        case ApplyMode::SoundPatternTempo: return "S+T";
    }
    return "SND";
}

const char* applyModeLong(const MiniAcid& mini) {
    switch (currentApplyMode(mini)) {
        case ApplyMode::SoundOnly: return "[ ] SOUND ONLY";
        case ApplyMode::SoundPattern: return "[X] SOUND+PATTERN";
        case ApplyMode::SoundPatternTempo: return "[X] SOUND+PATTERN+TEMPO";
    }
    return "[ ] SOUND ONLY";
}

const char* curatedModeShort(const MiniAcid& mini) {
    return mini.sceneManager().currentScene().genre.curatedMode ? "CUR" : "ADV";
}

const char* grooveModeShort(const MiniAcid& mini) {
    switch (mini.grooveboxMode()) {
        case GrooveboxMode::Acid: return "ACD";
        case GrooveboxMode::Minimal: return "MIN";
        case GrooveboxMode::Breaks: return "BRK";
        case GrooveboxMode::Dub: return "DUB";
        case GrooveboxMode::Electro: return "ELC";
        default: return "MIN";
    }
}

bool regenOnApply(const MiniAcid& mini) {
    return currentApplyMode(mini) != ApplyMode::SoundOnly;
}

bool tempoOnApply(const MiniAcid& mini) {
    return currentApplyMode(mini) == ApplyMode::SoundPatternTempo;
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
    "CLEAN", "DUB", "LOFI", "INDUS", "PSY"
};

const char* GenrePage::presetNames[8] = {
    "CLASSIC ACID", "OUTRUN LEAD", "EBM INDUS",
    "DUB TECHNO", "RAVE", "DUB REGGAE",
    "TRIPHOP DUST", "BROKEN BEAT"
};

// Curated BPM targets for 1..8 preset buttons.
static const uint8_t kPresetBpm[8] = {128, 112, 136, 118, 128, 92, 88, 122};
// Manual genre/texture apply BPM hint by generative mode.
static const uint8_t kGenreBpm[kGenerativeModeCount] = {128, 112, 136, 122, 138, 92, 88, 118, 140};

namespace {
const char* kPresetShortNames[8] = {
    "ACID", "OUT", "EBM", "DUB", "RAVE", "REG", "TRI", "BRK"
};
}

GenrePage::GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
    (void)gfx;
    visualStyle_ = UI::currentStyle;
    updateFromEngine();
}

// =================================================================
// MAIN DRAW: STYLE DISPATCHER
// =================================================================

void GenrePage::drawHeader(IGfx& gfx) {
    char titleStr[32];
    std::snprintf(titleStr, sizeof(titleStr), "%s/%s",
                  genreNames[genreIndex_], textureNames[textureIndex_]);

    switch (visualStyle_) {
        case VisualStyle::RETRO_CLASSIC:
#ifdef USE_RETRO_THEME
            RetroWidgets::drawHeaderBar(gfx, 0, 0, 240, 14,
                          "GENRE", titleStr,
                          mini_acid_.isPlaying(),
                          (int)(mini_acid_.bpm() + 0.5f),
                          mini_acid_.currentStep());
#else
            UI::drawStandardHeader(gfx, mini_acid_, titleStr);
#endif
            break;
        case VisualStyle::AMBER:
#ifdef USE_AMBER_THEME
            AmberWidgets::drawHeaderBar(gfx, 0, 0, 240, 14,
                          "GENRE", titleStr,
                          mini_acid_.isPlaying(),
                          (int)(mini_acid_.bpm() + 0.5f),
                          mini_acid_.currentStep());
#else
            UI::drawStandardHeader(gfx, mini_acid_, titleStr);
#endif
            break;
        case VisualStyle::MINIMAL:
        default:
            UI::drawStandardHeader(gfx, mini_acid_, titleStr);
            LayoutManager::clearContent(gfx);
            break;
    }
}

void GenrePage::drawContent(IGfx& gfx) {
    switch (visualStyle_) {
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

void GenrePage::drawFooter(IGfx& gfx) {
    const char* left = nullptr;
    const char* right = nullptr;
    const char* focusMode = nullptr;

    switch (focus_) {
        case FocusArea::GENRE:
            left = "UP/DN:Genre  ENT:Apply";
            right = "TAB:Texture";
            focusMode = nullptr;
            break;
        case FocusArea::TEXTURE:
            left = "UP/DN:Texture  L/R:TX%";
            right = "TAB:Presets";
            focusMode = nullptr;
            break;
        case FocusArea::PRESETS:
            left = "1..8/ARW:Pick  ENT:Load";
            right = "TAB:Apply  M:Mode C:Cur";
            focusMode = nullptr;
            break;
        case FocusArea::APPLY_MODE:
            left = "SPACE/M:Toggle Apply";
            right = tempoOnApply(mini_acid_) ? "S+T: +BPM" : (regenOnApply(mini_acid_) ? "S+P: REGENERATES" : "SND: KEEPS PATTERNS");
            focusMode = nullptr;
            break;
    }

    switch (visualStyle_) {
        case VisualStyle::RETRO_CLASSIC:
#ifdef USE_RETRO_THEME
            RetroWidgets::drawFooterBar(gfx, 0, 135 - 12, 240, 12, left, right, focusMode);
#else
            UI::drawStandardFooter(gfx, left, right);
#endif
            break;
        case VisualStyle::AMBER:
#ifdef USE_AMBER_THEME
            AmberWidgets::drawFooterBar(gfx, 0, 135 - 12, 240, 12, left, right, focusMode);
#else
            UI::drawStandardFooter(gfx, left, right);
#endif
            break;
        default:
            UI::drawStandardFooter(gfx, left, right);
            break;
    }
}

// =================================================================
// MINIMAL STYLE (Original clean 'pro 80-x' aesthetic)
// =================================================================

void GenrePage::drawMinimalStyle(IGfx& gfx) {
    // Content
    LayoutManager::clearContent(gfx);

    // Minimal focus markers instead of big labels
    const int y0 = LayoutManager::lineY(0);
    gfx.setTextColor(IGfxColor(0x808080)); // dim
    gfx.drawText(Layout::COL_1, y0, (focus_ == FocusArea::GENRE) ? "G>" : "G ");
    gfx.drawText(Layout::COL_2, y0, (focus_ == FocusArea::TEXTURE) ? "T>" : "T ");
    char modeBuf[30];
    std::snprintf(modeBuf, sizeof(modeBuf), "A:%s C:%s", applyModeShort(mini_acid_), curatedModeShort(mini_acid_));
    gfx.drawText(168, y0, modeBuf);
    if (focus_ == FocusArea::APPLY_MODE) {
        gfx.drawRect(164, y0 - 1, 74, 10, COLOR_ACCENT);
    }

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

    const int texCount = visibleTextureCount();
    const int selectedVisible = textureToVisibleIndex(textureIndex_);
    textureScroll_ = computeScrollTop(texCount, kTextureVisibleRows, selectedVisible, textureScroll_);
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int vIdx = textureScroll_ + i;
        if (vIdx >= texCount) break;
        int idx = visibleTextureAt(vIdx);
        int rowY = listY + i * Layout::LINE_HEIGHT;
        bool selected = (idx == textureIndex_) && (focus_ == FocusArea::TEXTURE);
        bool hasIcon = (idx == prevTextureIndex_);
        
        bool recommended = GenreManager::isTextureAllowed(
            static_cast<GenerativeMode>(genreIndex_),
            static_cast<TextureMode>(idx));
        bool showRecommendation = isCuratedMode() && recommended;

        char label[20];
        buildTextureLabel(idx, label, sizeof(label));

        IGfxColor textColor = selected ? COLOR_WHITE : (showRecommendation ? COLOR_ACCENT : COLOR_LABEL);
        if (selected) {
             Widgets::drawListRow(gfx, Layout::COL_2, rowY, Layout::COL_WIDTH, label, selected, hasIcon);
        } else {
             gfx.setTextColor(textColor);
             gfx.drawText(Layout::COL_2 + 10, rowY, label);
             if (hasIcon) gfx.drawText(Layout::COL_2 + 2, rowY, ">>");
        }
    }
    drawScrollBar(gfx, Layout::COL_2 + Layout::COL_WIDTH - 3, listY,
                  kTextureVisibleRows * Layout::LINE_HEIGHT,
                  texCount, kTextureVisibleRows, textureScroll_,
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
}

// =================================================================
// RETRO CLASSIC STYLE (80s Neon Cyberpunk)
// =================================================================

void GenrePage::drawRetroClassicStyle(IGfx& gfx) {
#ifdef USE_RETRO_THEME
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
    std::snprintf(amtBuf, sizeof(amtBuf), "TX %d%%", (int)mini_acid_.sceneManager().currentScene().genre.textureAmount);
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

    {
        const int texCount = visibleTextureCount();
        const int selectedVisible = textureToVisibleIndex(textureIndex_);
        textureScroll_ = computeScrollTop(texCount, kTextureVisibleRows, selectedVisible, textureScroll_);
    }
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int vIdx = textureScroll_ + i;
        if (vIdx >= visibleTextureCount()) break;
        int idx = visibleTextureAt(vIdx);
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
        
        bool recommended = GenreManager::isTextureAllowed(
            static_cast<GenerativeMode>(genreIndex_),
            static_cast<TextureMode>(idx));
        bool showRecommendation = isCuratedMode() && recommended;

        if (focused) {
            char label[20];
            buildTextureLabel(idx, label, sizeof(label));
            drawGlowText(gfx, TEX_X + 12, rowY, label, color, TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : 
                                   (showRecommendation ? IGfxColor(color) : IGfxColor(TEXT_SECONDARY));
            gfx.setTextColor(textColor);
            char label[20];
            buildTextureLabel(idx, label, sizeof(label));
            gfx.drawText(TEX_X + 12, rowY, label);
        }
    }
    drawScrollBar(gfx, TEX_X + TEX_W - 3, LIST_Y, kTextureVisibleRows * ROW_H,
                  visibleTextureCount(), kTextureVisibleRows, textureScroll_,
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
        
        IGfxColor textColor = selected ? genreColor : IGfxColor(RetroTheme::TEXT_DIM);
        if (focused) textColor = IGfxColor(RetroTheme::TEXT_PRIMARY);
        
        gfx.setTextColor(textColor);
        const char* name = kPresetShortNames[i];
        int textW = strlen(name) * 6;
        int textX = btnX + (BTN_W - textW) / 2;
        if (textX < btnX + 8) textX = btnX + 8;
        
        gfx.drawText(textX, btnY + 1, name);
    }
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
    std::snprintf(amtBuf, sizeof(amtBuf), "TX %d%%", (int)mini_acid_.sceneManager().currentScene().genre.textureAmount);
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

    {
        const int texCount = visibleTextureCount();
        const int selectedVisible = textureToVisibleIndex(textureIndex_);
        textureScroll_ = computeScrollTop(texCount, kTextureVisibleRows, selectedVisible, textureScroll_);
    }
    for (int i = 0; i < kTextureVisibleRows; i++) {
        int vIdx = textureScroll_ + i;
        if (vIdx >= visibleTextureCount()) break;
        int idx = visibleTextureAt(vIdx);
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
        
        bool recommended = GenreManager::isTextureAllowed(
            static_cast<GenerativeMode>(genreIndex_),
            static_cast<TextureMode>(idx));
        bool showRecommendation = isCuratedMode() && recommended;

        if (focused) {
            char label[20];
            buildTextureLabel(idx, label, sizeof(label));
            AmberWidgets::drawGlowText(gfx, TEX_X + 12, rowY, label, color, AmberTheme::TEXT_PRIMARY);
        } else {
            IGfxColor textColor = (isActive || isCursor) ? IGfxColor(color) : 
                                   (showRecommendation ? IGfxColor(color) : IGfxColor(AmberTheme::TEXT_SECONDARY));
            gfx.setTextColor(textColor);
            char label[20];
            buildTextureLabel(idx, label, sizeof(label));
            gfx.drawText(TEX_X + 12, rowY, label);
        }
    }
    drawScrollBar(gfx, TEX_X + TEX_W - 3, LIST_Y, kTextureVisibleRows * ROW_H,
                  visibleTextureCount(), kTextureVisibleRows, textureScroll_,
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
        const char* shortName = kPresetShortNames[i];
        int tw = strlen(shortName) * 6;
        int tx = bx + (BTN_W - tw) / 2;
        if (tx < bx + 2) tx = bx + 2;
        gfx.drawText(tx, by + 2, shortName);
    }

    if (presetFocus) {
        int fx = 4;
        int fy = GRID_Y - 2;
        int fw = 4 * (BTN_W + GAP) - GAP;
        int fh = 2 * (BTN_H + GAP) - GAP + 2;
        AmberWidgets::drawGlowBorder(gfx, fx, fy, fw, fh, AmberTheme::NEON_ORANGE, 1);
    }
    
#else
    drawMinimalStyle(gfx);
#endif
}

// =================================================================
// INPUT HANDLING (unchanged)
// =================================================================

bool GenrePage::handleEvent(UIEvent& e) {
    if (e.event_type != GROOVEPUTER_KEY_DOWN) return false;
    
    // Helper lambdas for navigation
    auto moveUp = [&]() {
        if (focus_ == FocusArea::GENRE) {
            genreIndex_ = (genreIndex_ - 1 + kGenerativeModeCount) % kGenerativeModeCount;
            ensureTextureAllowedForCurrentGenre();
        } else if (focus_ == FocusArea::TEXTURE) {
            int vis = textureToVisibleIndex(textureIndex_);
            int cnt = visibleTextureCount();
            if (cnt > 0) {
                vis = (vis - 1 + cnt) % cnt;
                textureIndex_ = visibleTextureAt(vis);
            }
        }
        else if (focus_ == FocusArea::PRESETS) {
            if (presetIndex_ >= 4) presetIndex_ -= 4;
        }
    };
    auto moveDown = [&]() {
        if (focus_ == FocusArea::GENRE) {
            genreIndex_ = (genreIndex_ + 1) % kGenerativeModeCount;
            ensureTextureAllowedForCurrentGenre();
        } else if (focus_ == FocusArea::TEXTURE) {
            int vis = textureToVisibleIndex(textureIndex_);
            int cnt = visibleTextureCount();
            if (cnt > 0) {
                vis = (vis + 1) % cnt;
                textureIndex_ = visibleTextureAt(vis);
            }
        }
        else if (focus_ == FocusArea::PRESETS) {
            if (presetIndex_ < 4) presetIndex_ += 4;
        }
    };
    auto moveLeft = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount - 5;
            if (v < 0) v = 0;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::PRESETS) {
            if ((presetIndex_ % 4) > 0) --presetIndex_;
        }
    };
    auto moveRight = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount + 5;
            if (v > 100) v = 100;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::PRESETS) {
            if ((presetIndex_ % 4) < 3) ++presetIndex_;
        }
    };

    int nav = UIInput::navCode(e);
    switch (nav) {
        case GROOVEPUTER_UP:    moveUp();    return true;
        case GROOVEPUTER_DOWN:  moveDown();  return true;
        case GROOVEPUTER_LEFT:  moveLeft();  return true;
        case GROOVEPUTER_RIGHT: moveRight(); return true;
        default: break;
    }

    char key = e.key;
    if (!key) return false;

    // TAB: cycle focus
    if (key == '\t') {
        if (focus_ == FocusArea::GENRE)   focus_ = FocusArea::TEXTURE;
        else if (focus_ == FocusArea::TEXTURE) focus_ = FocusArea::PRESETS;
        else if (focus_ == FocusArea::PRESETS) focus_ = FocusArea::APPLY_MODE;
        else focus_ = FocusArea::GENRE;
        return true;
    }

    // ENTER: apply current selection / toggle apply mode
    if (key == '\n' || key == '\r') {
        if (focus_ == FocusArea::APPLY_MODE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            cycleApplyMode(gs);
            UI::showToast(applyModeToast(mini_acid_), 1800);
            return true;
        }
        applyCurrent();
        return true;
    }

    // SPACE: toggle apply mode when focused
    if (key == ' ' && focus_ == FocusArea::APPLY_MODE) {
        auto& gs = mini_acid_.sceneManager().currentScene().genre;
        cycleApplyMode(gs);
        UI::showToast(applyModeToast(mini_acid_), 1800);
        return true;
    }

    // M: toggle apply mode (SOUND / SOUND+PATTERN / SOUND+PATTERN+TEMPO)
    if (key == 'm' || key == 'M') {
        auto& gs = mini_acid_.sceneManager().currentScene().genre;
        cycleApplyMode(gs);
        UI::showToast(applyModeToast(mini_acid_), 1800);
        return true;
    }

    // G: toggle groovebox mode (ACID/MINIMAL) in genre context.
    if (key == 'g' || key == 'G') {
        withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
        char toast[40];
        const char* shortName = grooveModeShort(mini_acid_);
        std::snprintf(toast, sizeof(toast), "Groove Mode: %s", shortName);
        UI::showToast(toast);
        return true;
    }

    // C: toggle curated compatibility mode
    if (key == 'c' || key == 'C') {
        bool next = !isCuratedMode();
        setCuratedMode(next);
        UI::showToast(next ? "Texture Mode: CURATED (recommended first)"
                           : "Texture Mode: ADVANCED (all combos)", 1800);
        return true;
    }

    /*
    // START: Number keys disabled for Mute priority
    // Direct preset selection (1-8)
    if (key >= '1' && key <= '8') {
        presetIndex_ = key - '1';
        focus_ = FocusArea::PRESETS;
        applyCurrent();
        return true;
    }

    // '0': randomize
    if (key == '0') {
        withAudioGuard([&]() {
            int gen = std::rand() % kGenerativeModeCount;
            int tex = std::rand() % kTextureModeCount;
            if (isCuratedMode()) {
                // Curated mode biases random to recommended texture.
                tex = static_cast<int>(GenreManager::firstAllowedTexture(static_cast<GenerativeMode>(gen)));
            }
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(gen));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(tex));
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(gen);
            gs.textureMode = static_cast<uint8_t>(tex);
        });
        updateFromEngine();
        return true;
    }
    // END: Number keys disabled
    */

    return false;
}

// =================================================================
// INTERNAL METHODS (unchanged)
// =================================================================

bool GenrePage::isCuratedMode() const {
    return mini_acid_.sceneManager().currentScene().genre.curatedMode;
}

void GenrePage::setCuratedMode(bool enabled) {
    mini_acid_.sceneManager().currentScene().genre.curatedMode = enabled;
}

int GenrePage::visibleTextureCount() const {
    return kTextureModeCount;
}

int GenrePage::visibleTextureAt(int visibleIndex) const {
    if (visibleIndex < 0) return 0;
    if (visibleIndex >= kTextureModeCount) return kTextureModeCount - 1;
    return visibleIndex;
}

int GenrePage::textureToVisibleIndex(int textureIndex) const {
    if (textureIndex < 0) return 0;
    if (textureIndex >= kTextureModeCount) return kTextureModeCount - 1;
    return textureIndex;
}

void GenrePage::ensureTextureAllowedForCurrentGenre() {
    // Curated mode is recommendation-first only, not a hard limiter.
}

void GenrePage::buildTextureLabel(int textureIndex, char* out, size_t outSize) const {
    if (!out || outSize == 0) return;
    if (textureIndex < 0 || textureIndex >= kTextureModeCount) {
        std::snprintf(out, outSize, "?");
        return;
    }
    std::snprintf(out, outSize, "%s", textureNames[textureIndex]);
}

void GenrePage::applyCurrent() {
    const bool doRegenerate = regenOnApply(mini_acid_);
    const bool doApplyTempo = tempoOnApply(mini_acid_);
    int targetBpm = kGenreBpm[genreIndex_ < 0 ? 0 : (genreIndex_ >= kGenerativeModeCount ? (kGenerativeModeCount - 1) : genreIndex_)];
    if (focus_ == FocusArea::PRESETS) {
        targetBpm = kPresetBpm[presetIndex_ < 0 ? 0 : (presetIndex_ >= 8 ? 7 : presetIndex_)];
        withAudioGuard([&]() {
            mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(kPresetGenre[presetIndex_]));
            mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(kPresetTexture[presetIndex_]));
            
            // Apply base timbre, reset bias tracking, then apply texture as delta from 0
            mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
            mini_acid_.genreManager().resetTextureBiasTracking();
            mini_acid_.genreManager().applyTexture(mini_acid_);
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            gs.generativeMode = static_cast<uint8_t>(kPresetGenre[presetIndex_]);
            gs.textureMode = static_cast<uint8_t>(mini_acid_.genreManager().textureMode());
            if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
            if (doApplyTempo) mini_acid_.setBpm(static_cast<float>(targetBpm));
        });
        updateFromEngine();
        prevGenreIndex_ = genreIndex_;
        prevTextureIndex_ = textureIndex_;
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
            if (doApplyTempo) mini_acid_.setBpm(static_cast<float>(targetBpm));
        });
        prevGenreIndex_ = genreIndex_;
        prevTextureIndex_ = textureIndex_;
    }

    char toast[80];
    const int g = genreIndex_;
    const int t = textureIndex_;
    std::snprintf(toast, sizeof(toast), "%s/%s applied (%s%s)",
                  genreNames[g], textureNames[t],
                  doRegenerate ? "Regenerated" : "Patterns kept",
                  doApplyTempo ? ", BPM set" : "");
    UI::showToast(toast, 1800);
}

void GenrePage::updateFromEngine() {
    genreIndex_ = static_cast<int>(mini_acid_.genreManager().generativeMode());
    textureIndex_ = static_cast<int>(mini_acid_.genreManager().textureMode());
    prevGenreIndex_ = genreIndex_;
    prevTextureIndex_ = textureIndex_;
}
