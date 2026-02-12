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

const char* onOff(bool v) {
    return v ? "ON" : "OFF";
}

const char* linkStateShort(const MiniAcid& mini) {
    const GrooveboxMode expected =
        GenreManager::grooveboxModeForGenerative(mini.genreManager().generativeMode());
    return (mini.grooveboxMode() == expected) ? "GEN" : "OVR";
}

int clampRecipeIndex(int recipe) {
    const int maxId = static_cast<int>(GenreManager::recipeCount()) - 1;
    if (recipe < 0) return 0;
    if (recipe > maxId) return maxId;
    return recipe;
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
// Manual genre/texture apply BPM hint by generative mode.
static const uint8_t kGenreBpm[kGenerativeModeCount] = {128, 112, 136, 122, 138, 92, 88, 118, 140};

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
        case VisualStyle::RETRO_CLASSIC: {
#ifdef USE_RETRO_THEME
            char subTitle[16];
            std::snprintf(subTitle, sizeof(subTitle), "GENRE P%d", mini_acid_.currentPageIndex() + 1);
            RetroWidgets::drawHeaderBar(gfx, 0, 0, 240, 14,
                          subTitle, titleStr,
                          mini_acid_.isPlaying(),
                          (int)(mini_acid_.bpm() + 0.5f),
                          mini_acid_.currentStep());
#else
            UI::drawStandardHeader(gfx, mini_acid_, titleStr);
#endif
            break;
        }
        case VisualStyle::AMBER: {
#ifdef USE_AMBER_THEME
            char subTitle[16];
            std::snprintf(subTitle, sizeof(subTitle), "GENRE P%d", mini_acid_.currentPageIndex() + 1);
            AmberWidgets::drawHeaderBar(gfx, 0, 0, 240, 14,
                          subTitle, titleStr,
                          mini_acid_.isPlaying(),
                          (int)(mini_acid_.bpm() + 0.5f),
                          mini_acid_.currentStep());
#else
            UI::drawStandardHeader(gfx, mini_acid_, titleStr);
#endif
            break;
        }
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
            right = "TAB:Apply";
            focusMode = nullptr;
            break;
        case FocusArea::APPLY_MODE:
            left = "FN+L/R:Recipe  FN+U/D:Morph";
            right = "M:ApplyMode C/G:Flags";
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

    // Status rows (replaces preset grid)
    const int statusY = LayoutManager::lineY(6);
    const auto& gs = mini_acid_.sceneManager().currentScene().genre;

    char status1[48];
    std::snprintf(status1, sizeof(status1), "Apply:%s Cur:%s R:%s",
                  applyModeShort(mini_acid_), onOff(gs.curatedMode),
                  GenreManager::recipeName(static_cast<GenreRecipeId>(recipeIndex_)));
    gfx.setTextColor((focus_ == FocusArea::APPLY_MODE) ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(Layout::COL_1, statusY, status1);

    char status2[56];
    std::snprintf(status2, sizeof(status2), "Groove:%s Link:%s Morph:%d%%",
                  grooveModeShort(mini_acid_), linkStateShort(mini_acid_),
                  (morphAmount_ * 100) / 255);
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, statusY + Layout::LINE_HEIGHT, status2);
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
    
    // Status block (bottom)
    const int GRID_Y = LIST_Y + kGenreVisibleRows * ROW_H + 4;
    const auto& gs = mini_acid_.sceneManager().currentScene().genre;
    gfx.fillRect(4, GRID_Y, 232, 22, BG_DARK_GRAY);
    gfx.drawRect(4, GRID_Y, 232, 22, (focus_ == FocusArea::APPLY_MODE) ? FOCUS_GLOW : GRID_MEDIUM);

    char s1[64];
    std::snprintf(s1, sizeof(s1), "APPLY:%s CUR:%s R:%s",
                  applyModeShort(mini_acid_), onOff(gs.curatedMode),
                  GenreManager::recipeName(static_cast<GenreRecipeId>(recipeIndex_)));
    gfx.setTextColor(TEXT_PRIMARY);
    gfx.drawText(8, GRID_Y + 3, s1);

    char s2[64];
    std::snprintf(s2, sizeof(s2), "GROOVE:%s LINK:%s M:%d%%",
                  grooveModeShort(mini_acid_), linkStateShort(mini_acid_),
                  (morphAmount_ * 100) / 255);
    gfx.setTextColor(TEXT_SECONDARY);
    gfx.drawText(8, GRID_Y + 12, s2);
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
    const auto& gs = mini_acid_.sceneManager().currentScene().genre;
    gfx.fillRect(4, GRID_Y, 232, 22, AmberTheme::BG_PANEL);
    gfx.drawRect(4, GRID_Y, 232, 22,
                 (focus_ == FocusArea::APPLY_MODE) ? AmberTheme::NEON_ORANGE : AmberTheme::GRID_MEDIUM);

    char s1[64];
    std::snprintf(s1, sizeof(s1), "APPLY:%s CUR:%s R:%s",
                  applyModeShort(mini_acid_), onOff(gs.curatedMode),
                  GenreManager::recipeName(static_cast<GenreRecipeId>(recipeIndex_)));
    gfx.setTextColor(AmberTheme::TEXT_PRIMARY);
    gfx.drawText(8, GRID_Y + 3, s1);

    char s2[64];
    std::snprintf(s2, sizeof(s2), "GROOVE:%s LINK:%s M:%d%%",
                  grooveModeShort(mini_acid_), linkStateShort(mini_acid_),
                  (morphAmount_ * 100) / 255);
    gfx.setTextColor(AmberTheme::TEXT_SECONDARY);
    gfx.drawText(8, GRID_Y + 12, s2);
    
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
        } else if (focus_ == FocusArea::APPLY_MODE) {
            if (e.meta) adjustMorphAmount(16);
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
        } else if (focus_ == FocusArea::APPLY_MODE) {
            if (e.meta) adjustMorphAmount(-16);
        }
    };
    auto moveLeft = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount - 5;
            if (v < 0) v = 0;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::APPLY_MODE) {
            if (e.meta) cycleRecipeSelection(-1);
        }
    };
    auto moveRight = [&]() {
        if (focus_ == FocusArea::TEXTURE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            int v = (int)gs.textureAmount + 5;
            if (v > 100) v = 100;
            gs.textureAmount = static_cast<uint8_t>(v);
        } else if (focus_ == FocusArea::APPLY_MODE) {
            if (e.meta) cycleRecipeSelection(1);
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
        else if (focus_ == FocusArea::TEXTURE) focus_ = FocusArea::APPLY_MODE;
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
        char toast[64];
        const char* shortName = grooveModeShort(mini_acid_);
        std::snprintf(toast, sizeof(toast), "Groove Mode: %s (override)", shortName);
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


    // Bank Selection (Ctrl + 1..2)
    if (e.ctrl && !e.alt && e.key >= '1' && e.key <= '2') {
        int bankIdx = e.key - '1';
        withAudioGuard([&]() {
            mini_acid_.set303BankIndex(0, bankIdx);
        });
        UI::showToast(bankIdx == 0 ? "Bank: A" : "Bank: B", 800);
        return true;
    }

    // Pattern quick select (Q-I) - Standardized Everywhere (ignore shift for CapsLock safety)
    if (!e.ctrl && !e.meta) {
        char lower = std::tolower(e.key);
        int patIdx = qwertyToPatternIndex(lower);
        if (patIdx >= 0) {
            withAudioGuard([&]() {
                // Default to Synth A on this global page
                mini_acid_.set303PatternIndex(0, patIdx);
            });
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Synth A -> Pat %d", patIdx + 1);
            UI::showToast(buf, 800);
            return true;
        }
    }

    return false;
}

// =================================================================
// INTERNAL METHODS (unchanged)
// =================================================================

void GenrePage::cycleRecipeSelection(int direction) {
    const int count = static_cast<int>(GenreManager::recipeCount());
    if (count <= 0) return;
    int next = recipeIndex_ + direction;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    recipeIndex_ = next;

    auto& gs = mini_acid_.sceneManager().currentScene().genre;
    gs.recipe = static_cast<uint8_t>(recipeIndex_);
    if (morphAmount_ > 0) gs.morphTarget = static_cast<uint8_t>(recipeIndex_);
    gs.morphAmount = static_cast<uint8_t>(morphAmount_);

    withAudioGuard([&]() {
        if (mini_acid_.isPlaying()) {
            mini_acid_.genreManager().queueRecipe(static_cast<GenreRecipeId>(recipeIndex_));
        } else {
            mini_acid_.genreManager().setRecipe(static_cast<GenreRecipeId>(recipeIndex_));
        }
        mini_acid_.genreManager().setMorphTarget(
            morphAmount_ > 0 ? static_cast<GenreRecipeId>(recipeIndex_) : kBaseRecipeId);
        mini_acid_.genreManager().setMorphAmount(static_cast<uint8_t>(morphAmount_));
    });

    char toast[72];
    std::snprintf(toast, sizeof(toast), "Recipe: %s%s",
                  GenreManager::recipeName(static_cast<GenreRecipeId>(recipeIndex_)),
                  mini_acid_.isPlaying() ? " (queued)" : "");
    UI::showToast(toast, 900);
}

void GenrePage::adjustMorphAmount(int delta) {
    int next = morphAmount_ + delta;
    if (next < 0) next = 0;
    if (next > 255) next = 255;
    morphAmount_ = next;

    auto& gs = mini_acid_.sceneManager().currentScene().genre;
    gs.morphAmount = static_cast<uint8_t>(morphAmount_);
    gs.morphTarget = static_cast<uint8_t>(morphAmount_ > 0 ? recipeIndex_ : 0);

    withAudioGuard([&]() {
        mini_acid_.genreManager().setMorphTarget(
            morphAmount_ > 0 ? static_cast<GenreRecipeId>(recipeIndex_) : kBaseRecipeId);
        mini_acid_.genreManager().setMorphAmount(static_cast<uint8_t>(morphAmount_));
    });

    char toast[64];
    std::snprintf(toast, sizeof(toast), "Morph: %d%%", (morphAmount_ * 100) / 255);
    UI::showToast(toast, 700);
}

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
    withAudioGuard([&]() {
        mini_acid_.genreManager().setGenerativeMode(static_cast<GenerativeMode>(genreIndex_));
        mini_acid_.genreManager().setTextureMode(static_cast<TextureMode>(textureIndex_));
        mini_acid_.genreManager().setRecipe(static_cast<GenreRecipeId>(recipeIndex_));
        mini_acid_.genreManager().setMorphTarget(
            morphAmount_ > 0 ? static_cast<GenreRecipeId>(recipeIndex_) : kBaseRecipeId);
        mini_acid_.genreManager().setMorphAmount(static_cast<uint8_t>(morphAmount_));
        mini_acid_.setGrooveboxMode(
            GenreManager::grooveboxModeForGenerative(static_cast<GenerativeMode>(genreIndex_)));
        
        // Apply base timbre, reset bias tracking, then apply texture as delta from 0
        mini_acid_.genreManager().applyGenreTimbre(mini_acid_);
        mini_acid_.genreManager().resetTextureBiasTracking();
        mini_acid_.genreManager().applyTexture(mini_acid_);
        auto& gs = mini_acid_.sceneManager().currentScene().genre;
        gs.generativeMode = static_cast<uint8_t>(genreIndex_);
        gs.textureMode = static_cast<uint8_t>(textureIndex_);
        gs.recipe = static_cast<uint8_t>(recipeIndex_);
        gs.morphTarget = static_cast<uint8_t>(morphAmount_ > 0 ? recipeIndex_ : 0);
        gs.morphAmount = static_cast<uint8_t>(morphAmount_);
        if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
        if (doApplyTempo) mini_acid_.setBpm(static_cast<float>(targetBpm));
    });
    prevGenreIndex_ = genreIndex_;
    prevTextureIndex_ = textureIndex_;

    char toast[80];
    const int g = genreIndex_;
    const int t = textureIndex_;
    std::snprintf(toast, sizeof(toast), "%s/%s %s (%s%s)",
                  genreNames[g], textureNames[t],
                  GenreManager::recipeName(static_cast<GenreRecipeId>(recipeIndex_)),
                  doRegenerate ? "Regenerated" : "Patterns kept",
                  doApplyTempo ? ", BPM set" : "");
    UI::showToast(toast, 1800);
}

void GenrePage::updateFromEngine() {
    genreIndex_ = static_cast<int>(mini_acid_.genreManager().generativeMode());
    textureIndex_ = static_cast<int>(mini_acid_.genreManager().textureMode());
    recipeIndex_ = clampRecipeIndex(static_cast<int>(mini_acid_.genreManager().recipe()));
    morphAmount_ = static_cast<int>(mini_acid_.genreManager().morphAmount());
    prevGenreIndex_ = genreIndex_;
    prevTextureIndex_ = textureIndex_;
}
