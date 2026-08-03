#include "layout_manager.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

void LayoutManager::clearContent(IGfx& gfx) {
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
                 Layout::HEADER.x + Layout::HEADER.w - 1,
                 Layout::HEADER.y + Layout::HEADER.h - 1, p.dim);

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

    const char* leftText = left ? left : "";
    const bool hasRight = right && right[0] != '\0';
    const int innerX = Layout::FOOTER.x + Layout::CONTENT_PAD_X;
    const int innerW = Layout::FOOTER.w - 2 * Layout::CONTENT_PAD_X;
    constexpr int kColumnGap = 4;
    const int leftColumnW = (innerW - kColumnGap) / 2;
    const int rightColumnW = innerW - leftColumnW - kColumnGap;

    // Most pages use short two-column hints. Long help strings, such as the
    // TB303 parameter-page controls, used to be clipped independently to
    // roughly half of the 240 px display. Stack only those overflowing hints
    // into two full-width rows while preserving the same 16 px footer zone.
    const bool stackRows = hasRight &&
        (gfx.measureText(leftText) > leftColumnW ||
         gfx.measureText(right) > rightColumnW);

    if (stackRows) {
        Widgets::drawKeyHelp(gfx, innerX, Layout::FOOTER.y + 1, innerW, leftText);
        Widgets::drawKeyHelp(gfx, innerX, Layout::FOOTER.y + 8, innerW, right);
        return;
    }

    Widgets::drawKeyHelp(gfx, innerX, Layout::FOOTER.y + 3,
                         hasRight ? leftColumnW : innerW, leftText);

    if (hasRight) {
        Widgets::drawKeyHelp(gfx,
                             innerX + leftColumnW + kColumnGap,
                             Layout::FOOTER.y + 3,
                             rightColumnW,
                             right);
    }
}
