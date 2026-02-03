#include "layout_manager.h"
#include "ui_widgets.h"
#include <stdio.h>

void testAllWidgets(IGfx& gfx) {
    LayoutManager::drawHeader(gfx, "01", 999, "VERY LONG MODE NAME THAT MUST CLIP", true);
    LayoutManager::clearContent(gfx);

    // grid lines check
    for (int i = 0; i < Layout::MAX_LINES; i++) {
        int y = LayoutManager::lineY(i);
        gfx.drawPixel(0, y, COLOR_RED);
        gfx.drawPixel(239, y, COLOR_RED);
    }

    int y0 = LayoutManager::lineY(0);

    Widgets::drawListRow(gfx, Layout::COL_1, y0, Layout::COL_WIDTH, "Acid", true, false);
    Widgets::drawListRow(gfx, Layout::COL_1, y0 + Layout::LINE_HEIGHT, Layout::COL_WIDTH, "Minimal", false, false);
    Widgets::drawListRow(gfx, Layout::COL_1, y0 + 2 * Layout::LINE_HEIGHT, Layout::COL_WIDTH, "Rave (icon)", false, true);

    Widgets::drawBarRow(gfx, Layout::COL_2, y0, Layout::COL_WIDTH, "CUTOFF", 0.64f, true);
    Widgets::drawBarRow(gfx, Layout::COL_2, y0 + Layout::LINE_HEIGHT, Layout::COL_WIDTH, "RES", 0.32f, true);

    uint16_t mask = 0;
    for (int i = 0; i < 16; i += 3) mask |= (1u << i);
    Widgets::drawStepRow(gfx, Layout::COL_1, y0 + 4 * Layout::LINE_HEIGHT, 232, "BASS", mask, 8);

    Widgets::drawToggleRow(gfx, Layout::COL_1, y0 + 6 * Layout::LINE_HEIGHT, Layout::COL_WIDTH, "DELAY", true, "40");
    Widgets::drawToggleRow(gfx, Layout::COL_2, y0 + 6 * Layout::LINE_HEIGHT, Layout::COL_WIDTH, "REVERB", false, "24");

    const char* info[] = {
        "Impact: SW +  DENS ++  SLIDE +",
        "Synth:  CUT -  RES +   ENV +"
    };
    Widgets::drawInfoBox(gfx, Layout::COL_1, y0 + 7 * Layout::LINE_HEIGHT, 232, info, 2);

    LayoutManager::drawFooter(gfx,
        "[A]PLAY [B]MENU [X]STYLE [Y]DETAIL",
        "[HOLD]ADV  [0]RND");
}
