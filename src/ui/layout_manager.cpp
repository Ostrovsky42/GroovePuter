#include "layout_manager.h"
#include "pages/smf_player_redraw.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

void LayoutManager::clearContent(IGfx& gfx) {
    if (GroovePuterUi::interceptSmfPlayerContentClear(gfx)) return;
    const UI::ThemePalette p = UI::themePalette();
    gfx.fillRect(Layout::CONTENT.x, Layout::CONTENT.y, Layout::CONTENT.w, Layout::CONTENT.h, p.background);
}

void LayoutManager::drawHeader(IGfx& gfx,
                               const char* scene,
                               int bpm,
                               const char* status,
                               bool recording) {
    const UI::ThemePalette p = UI::themePalette();
    gfx.fillRect(Layout::HEADER.x, Layout::HEADER.y, Layout::HEADER.w, Layout::HEADER.h, p.background);
    gfx.drawLine(Layout::HEADER.x, Layout::HEADER.y + Layout::HEADER.h - 1,
                 Layout::HEADER.x + Layout::HEADER.w - 1, Layout::HEADER.y + Layout::HEADER.h - 1, p.dim);

    // Left: scene. Label is secondary; the actual value gets the warm accent.
    gfx.setTextColor(p.secondary);
    gfx.drawText(Layout::COL_1, 3, "SC:");
    gfx.setTextColor(p.warning);
    Widgets::drawClippedText(gfx, Layout::COL_1 + 18, 3, 26, scene ? scene : "--");

    // Mid: BPM. Fixed origin prevents wider values moving adjacent fields.
    gfx.setTextColor(p.secondary);
    gfx.drawText(Layout::COL_2, 3, "BPM:");
    char bpmStr[8];
    snprintf(bpmStr, sizeof(bpmStr), "%3d", bpm);
    gfx.setTextColor(p.accent);
    gfx.drawText(Layout::COL_2 + 28, 3, bpmStr);

    // Right: page/status title. Reserve REC indicator space at the edge.
    const int statusX = 166;
    const int statusW = 240 - statusX - 14;
    gfx.setTextColor(p.text);
    Widgets::drawClippedText(gfx, statusX, 3, statusW, status ? status : "");

    if (recording) {
        gfx.fillCircle(232, 8, 3, p.danger);
    }
}

void LayoutManager::drawFooter(IGfx& gfx, const char* left, const char* right) {
    const UI::ThemePalette p = UI::themePalette();
    gfx.fillRect(Layout::FOOTER.x, Layout::FOOTER.y, Layout::FOOTER.w, Layout::FOOTER.h, p.panel);
    gfx.drawLine(Layout::FOOTER.x, Layout::FOOTER.y,
                 Layout::FOOTER.x + Layout::FOOTER.w - 1, Layout::FOOTER.y, p.dim);

    Widgets::drawKeyHelp(gfx, Layout::CONTENT_PAD_X, Layout::FOOTER.y + 3, 120, left ? left : "");

    if (right && right[0] != '\0') {
        Widgets::drawKeyHelp(gfx, Layout::CONTENT_PAD_X + 120, Layout::FOOTER.y + 3, 116, right);
    }
}
