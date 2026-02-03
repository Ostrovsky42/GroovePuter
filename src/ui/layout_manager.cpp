#include "layout_manager.h"
#include "ui_colors.h"
#include <stdio.h>
#include <string.h>

void LayoutManager::drawHeader(IGfx& gfx,
                               const char* scene,
                               int bpm,
                               const char* status,
                               bool recording) {
    gfx.fillRect(Layout::HEADER.x, Layout::HEADER.y, Layout::HEADER.w, Layout::HEADER.h, COLOR_BLACK);

    // Left: SC:xx
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_1, 3, "SC:");
    gfx.setTextColor(COLOR_KNOB_1);
    Widgets::drawClippedText(gfx, Layout::COL_1 + 18, 3, 26, scene ? scene : "--");

    // Mid: BPM:xxx
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(Layout::COL_2, 3, "BPM:");
    char bpmStr[8];
    snprintf(bpmStr, sizeof(bpmStr), "%d", bpm);
    gfx.setTextColor(COLOR_KNOB_2);
    gfx.drawText(Layout::COL_2 + 28, 3, bpmStr);

    // Right: status (clipped)
    // Reserve space for REC dot at x~232
    int statusX = 166;
    int statusW = 240 - statusX - 14;
    gfx.setTextColor(COLOR_WHITE);
    Widgets::drawClippedText(gfx, statusX, 3, statusW, status ? status : "");

    if (recording) {
        gfx.fillCircle(232, 8, 3, COLOR_RED);
    }
}

void LayoutManager::drawFooter(IGfx& gfx, const char* left, const char* right) {
    gfx.fillRect(Layout::FOOTER.x, Layout::FOOTER.y, Layout::FOOTER.w, Layout::FOOTER.h, COLOR_DARK_GRAY);

    // left line
    Widgets::drawKeyHelp(gfx, Layout::CONTENT_PAD_X, Layout::FOOTER.y + 3, 120, left ? left : "");

    // right line
    if (right && right[0] != '\0') {
        Widgets::drawKeyHelp(gfx, Layout::CONTENT_PAD_X + 120, Layout::FOOTER.y + 3, 116, right);
    }
}
