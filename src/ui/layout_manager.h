#pragma once
#include "ui_core.h"
#include "screen_geometry.h"
#include "ui_widgets.h"

class LayoutManager {
public:
    // y coord for content line N (0..MAX_LINES-1)
    static int lineY(int lineNumber) {
        return Layout::CONTENT.y + Layout::CONTENT_PAD_Y + lineNumber * Layout::LINE_HEIGHT;
    }

    // CONTENT holds exactly seven lines: lineY(7) puts a glyph bottom at 110
    // while CONTENT ends at 109, and the performance HUD paints over the
    // remainder. A page that wants a final status line must anchor to this
    // instead of inventing an eighth line, which is what left "NEXT GEN: ..."
    // on FEEL and the recipe summary on GENRE sliced in half.
    static int lastLineY(const IGfx& gfx) {
        return Layout::CONTENT.y + Layout::CONTENT.h - gfx.fontHeight();
    }

    static void clearContent(IGfx& gfx);

    static void drawHeader(IGfx& gfx,
                           const char* scene,
                           int bpm,
                           const char* status,
                           bool recording);

    static void drawFooter(IGfx& gfx,
                           const char* left,
                           const char* right = nullptr);
};
