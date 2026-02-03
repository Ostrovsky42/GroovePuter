#pragma once
#include <stdint.h>

struct LayoutRect {
    int x, y, w, h;
};

namespace Layout {
    // Screen: 240x135, coords: x=0..239, y=0..134
    constexpr int SCREEN_W = 240;
    constexpr int SCREEN_H = 135;

    // Fixed zones
    constexpr LayoutRect HEADER  = {0, 0, 240, 16};    // y: 0..15
    constexpr LayoutRect CONTENT = {0, 16, 240, 103};  // y: 16..118
    constexpr LayoutRect FOOTER  = {0, 119, 240, 16};  // y: 119..134

    // Content padding and typography
    constexpr int CONTENT_PAD_X = 4;
    constexpr int CONTENT_PAD_Y = 2;

    // 103px height, pads 2+2 => 99px usable.
    // With LINE_HEIGHT=12 => floor(99/12)=8 lines.
    constexpr int LINE_HEIGHT = 12;
    constexpr int MAX_LINES   = 8;

    // Column grid (safe for 240px)
    // - Left column starts at 4
    // - Mid column starts at 120 (gives two 116px columns: 120..235)
    // - Right column reserved mostly for header status; in content use COL_2 for right half.
    constexpr int COL_1      = 4;
    constexpr int COL_2      = 120;
    constexpr int COL_WIDTH  = 116;

    // Widget sizing
    constexpr int LIST_ITEM_H = 12;
    constexpr int BAR_HEIGHT  = 8;
    constexpr int STEP_ROW_H  = 22; // two-line step row
}
