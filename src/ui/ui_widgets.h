#pragma once
#include <stdint.h>
#include "ui_core.h"
#include "ui_colors.h"
#include "screen_geometry.h"

namespace Widgets {

    // Text helpers (NO unicode: use "..." and ">" only)
    void drawClippedText(IGfx& gfx, int x, int y, int maxWidth, const char* text);

    // 1) ListRow
    void drawListRow(IGfx& gfx, int x, int y, int width,
                     const char* label, bool selected, bool hasIcon = false);

    // 2) BarRow
    // value: 0.0..1.0
    void drawBarRow(IGfx& gfx, int x, int y, int width,
                    const char* label, float value, bool showPercent = true);

    // 3) StepRow
    // stepMask: bit i => step i active, i in [0..15]
    void drawStepRow(IGfx& gfx, int x, int y, int width,
                     const char* label, uint16_t stepMask, int currentStep = -1,
                     bool compact = false);

    // 4) ToggleRow
    void drawToggleRow(IGfx& gfx, int x, int y, int width,
                       const char* label, bool enabled, const char* valueStr = nullptr);

    // 5) ValueRow
    void drawValueRow(IGfx& gfx, int x, int y, int width,
                      const char* label, int value, const char* unit = nullptr);

    // 6) ButtonGrid (SAFE: labelsCount prevents OOB reads)
    // labels: array of pointers, labelsCount = actual size of array
    // cells beyond labelsCount are rendered as empty/disabled
    void drawButtonGrid(IGfx& gfx, int x, int y, int cellW, int cellH,
                        int cols, int rows, const char* const* labels, 
                        int labelsCount, int selectedIndex);

    // 7) KeyHelp (footer helper)
    void drawKeyHelp(IGfx& gfx, int x, int y, int maxWidth, const char* text);

    // 8) InfoBox
    // lines: array of pointers, count linesCount
    void drawInfoBox(IGfx& gfx, int x, int y, int width,
                     const char* const* lines, int linesCount);
}
