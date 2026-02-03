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

    static void clearContent(IGfx& gfx) {
        gfx.fillRect(Layout::CONTENT.x, Layout::CONTENT.y, Layout::CONTENT.w, Layout::CONTENT.h, COLOR_BLACK);
    }

    static void drawHeader(IGfx& gfx,
                           const char* scene,
                           int bpm,
                           const char* status,
                           bool recording);

    static void drawFooter(IGfx& gfx,
                           const char* left,
                           const char* right = nullptr);
};
