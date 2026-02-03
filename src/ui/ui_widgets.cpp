#include "ui_widgets.h"
#include <string.h>
#include <stdio.h>

namespace {
    inline float clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    constexpr int TEXT_Y_OFFSET = 1;

    // Safe strlen for null
    int safeLen(const char* s) { return s ? (int)strlen(s) : 0; }

    // Draw ASCII icon: ">" (no Unicode)
    void drawIcon(IGfx& gfx, int x, int y) {
        gfx.drawText(x, y, ">");
    }
}

namespace Widgets {

void drawClippedText(IGfx& gfx, int x, int y, int maxWidth, const char* text) {
    if (!text || maxWidth <= 0) return;
    
    // 1. Если текст влезает - рисуем как есть
    int txWidth = gfx.measureText(text);
    if (txWidth <= maxWidth) {
        gfx.drawText(x, y, text);
        return;
    }
    
    // 2. Если даже "..." не влезает - ничего не рисуем
    const char* ellipsis = "...";
    int ellipsisWidth = gfx.measureText(ellipsis);
    if (ellipsisWidth > maxWidth) return;
    
    // 3. Ищем максимальную длину
    char buffer[96]; // increased to 96
    int srcLen = (int)strlen(text);
    
    // Безопасное копирование
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // Укорачиваем с конца
    for (int len = srcLen; len > 0; len--) {
        buffer[len] = '\0';
        if (gfx.measureText(buffer) + ellipsisWidth <= maxWidth) {
            // Нашли подходящую длину
            strcat(buffer, ellipsis);
            gfx.drawText(x, y, buffer);
            return;
        }
    }
    
    // Если ничего не нашли - рисуем только "..."
    gfx.drawText(x, y, ellipsis);
}

void drawListRow(IGfx& gfx, int x, int y, int width,
                 const char* label, bool selected, bool hasIcon) {
    // Clear row
    gfx.fillRect(x, y, width, Layout::LIST_ITEM_H, COLOR_BLACK);

    if (selected) {
        gfx.fillRect(x, y, width, Layout::LIST_ITEM_H, COLOR_KNOB_1);
        gfx.setTextColor(COLOR_BLACK);
    } else {
        gfx.setTextColor(COLOR_WHITE);
    }

    int tx = x + 2;
    if (hasIcon) {
        drawIcon(gfx, tx, y + TEXT_Y_OFFSET);
        tx += 10;
    }

    drawClippedText(gfx, tx, y + TEXT_Y_OFFSET, width - (tx - x) - 2, label ? label : "");
}

void drawBarRow(IGfx& gfx, int x, int y, int width,
                const char* label, float value, bool showPercent) {
    value = clamp01(value);

    // Clear area
    gfx.fillRect(x, y, width, Layout::BAR_HEIGHT + 3, COLOR_BLACK);

    // Label
    gfx.setTextColor(COLOR_WHITE);
    drawClippedText(gfx, x, y + TEXT_Y_OFFSET, 54, label ? label : "");

    // Bar geometry
    const int labelW = 56;
    const int rightW = showPercent ? 28 : 0;
    int barW = width - labelW - rightW - 2;
    if (barW < 32) barW = 32;

    const int segments = 16;
    const int segW = (barW / segments);
    const int filled = (int)(value * segments + 0.0001f);

    int barX = x + labelW;
    for (int i = 0; i < segments; i++) {
        int sx = barX + i * segW;
        int sw = segW - 1;
        if (sw < 1) sw = 1;
        if (i < filled) {
            gfx.fillRect(sx, y, sw, Layout::BAR_HEIGHT, COLOR_KNOB_2);
        } else {
            gfx.drawRect(sx, y, sw, Layout::BAR_HEIGHT, COLOR_LABEL);
        }
    }

    if (showPercent) {
        char p[8];
        snprintf(p, sizeof(p), "%d%%", (int)(value * 100.0f + 0.5f));
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(x + width - rightW, y + TEXT_Y_OFFSET, p);
    }
}

void drawStepRow(IGfx& gfx, int x, int y, int width,
                 const char* label, uint16_t stepMask, int currentStep,
                 bool compact) {
    // Высота в зависимости от режима
    int rowHeight = compact ? (Layout::BAR_HEIGHT + 2) : Layout::STEP_ROW_H;
    
    // Очистка
    gfx.fillRect(x, y, width, rowHeight, COLOR_BLACK);
    
    // Метка
    gfx.setTextColor(COLOR_WHITE);
    int labelW = compact ? 36 : 44;
    drawClippedText(gfx, x, y + 1, labelW - 2, label ? label : "");
    
    // Геометрия шагов
    const int steps = 16;
    int availW = width - labelW - 2;
    if (availW < 100) availW = 100;
    
    // Автоподбор ширины шага
    int spacing = compact ? 1 : 2;
    int stepW = (availW - (steps - 1) * spacing) / steps;
    if (stepW < 4) {
        stepW = 4;
        spacing = 0;
    }
    
    int startX = x + labelW;
    int barY = y + (compact ? 1 : 0);
    int barH = compact ? 6 : Layout::BAR_HEIGHT;
    
    // Рисуем шаги
    for (int i = 0; i < steps; i++) {
        int sx = startX + i * (stepW + spacing);
        bool active = (stepMask & (1u << i)) != 0;
        bool cur = (i == currentStep);
        
        // Цвет в зависимости от состояния
        IGfxColor color;
        if (cur) {
            color = COLOR_KNOB_1;
        } else if (active) {
            color = COLOR_KNOB_2;
        } else {
            color = COLOR_LABEL;
        }
        
        // Заполненный или контур
        if (cur || active) {
            gfx.fillRect(sx, barY, stepW, barH, color);
        } else {
            gfx.drawRect(sx, barY, stepW, barH, color);
        }
    }
    
    // Номера шагов (только в не-compact режиме)
    if (!compact) {
        int numY = y + Layout::BAR_HEIGHT + 2;
        gfx.setTextColor(COLOR_LABEL);
        
        for (int i = 0; i < steps; i++) {
            // Показываем только ключевые номера
            if (i == 0 || i == 7 || i == 8 || i == 15) {
                int sx = startX + i * (stepW + spacing);
                char num[4];
                snprintf(num, sizeof(num), "%d", i + 1);
                gfx.drawText(sx + 1, numY, num);
            }
        }
    }
}

void drawToggleRow(IGfx& gfx, int x, int y, int width,
                   const char* label, bool enabled, const char* valueStr) {
    gfx.fillRect(x, y, width, Layout::LINE_HEIGHT, COLOR_BLACK);

    gfx.setTextColor(COLOR_WHITE);
    drawClippedText(gfx, x, y + TEXT_Y_OFFSET, 60, label ? label : "");

    const char* st = enabled ? "ON" : "OFF";
    gfx.setTextColor(enabled ? COLOR_KNOB_2 : COLOR_LABEL);

    char buf[32];
    if (valueStr && safeLen(valueStr) > 0) {
        snprintf(buf, sizeof(buf), "%s:%s", st, valueStr);
    } else {
        snprintf(buf, sizeof(buf), "%s", st);
    }

    // Right align-ish
    int tw = gfx.measureText(buf);
    int tx = x + width - tw - 2;
    if (tx < x + 64) tx = x + 64;
    gfx.drawText(tx, y + TEXT_Y_OFFSET, buf);
}

void drawValueRow(IGfx& gfx, int x, int y, int width,
                  const char* label, int value, const char* unit) {
    gfx.fillRect(x, y, width, Layout::LINE_HEIGHT, COLOR_BLACK);

    gfx.setTextColor(COLOR_WHITE);
    drawClippedText(gfx, x, y + TEXT_Y_OFFSET, 60, label ? label : "");

    char buf[24];
    if (unit && safeLen(unit) > 0) {
        snprintf(buf, sizeof(buf), "%d%s", value, unit);
    } else {
        snprintf(buf, sizeof(buf), "%d", value);
    }

    gfx.setTextColor(COLOR_KNOB_2);
    int tw = gfx.measureText(buf);
    int tx = x + width - tw - 2;
    if (tx < x + 64) tx = x + 64;
    gfx.drawText(tx, y + TEXT_Y_OFFSET, buf);
}

void drawButtonGrid(IGfx& gfx, int x, int y, int cellW, int cellH,
                    int cols, int rows, const char* const* labels, 
                    int labelsCount, int selectedIndex) {
    int cellCount = cols * rows;
    for (int i = 0; i < cellCount; i++) {
        int cx = x + (i % cols) * cellW;
        int cy = y + (i / cols) * cellH;

        bool sel = (i == selectedIndex);
        bool hasLabel = (labels && i < labelsCount && labels[i]);
        
        if (sel && hasLabel) {
            gfx.fillRect(cx, cy, cellW - 1, cellH - 1, COLOR_KNOB_1);
            gfx.setTextColor(COLOR_BLACK);
        } else if (hasLabel) {
            gfx.drawRect(cx, cy, cellW - 1, cellH - 1, COLOR_LABEL);
            gfx.setTextColor(COLOR_WHITE);
        } else {
            // Empty/disabled cell (no label or out of bounds)
            gfx.drawRect(cx, cy, cellW - 1, cellH - 1, COLOR_BLACK);
        }

        // SAFE: Only read labels[i] if i < labelsCount
        if (hasLabel) {
            int pad = 2;
            drawClippedText(gfx, cx + pad, cy + pad, cellW - 2 * pad, labels[i]);
        }
    }
}

void drawKeyHelp(IGfx& gfx, int x, int y, int maxWidth, const char* text) {
    // single line clipped
    gfx.setTextColor(COLOR_LABEL);
    drawClippedText(gfx, x, y, maxWidth, text ? text : "");
}

void drawInfoBox(IGfx& gfx, int x, int y, int width,
                 const char* const* lines, int linesCount) {
    if (!lines || linesCount <= 0) return;

    int h = linesCount * Layout::LINE_HEIGHT + 2;
    gfx.drawRect(x, y, width, h, COLOR_LABEL);

    for (int i = 0; i < linesCount; i++) {
        int ly = y + 1 + i * Layout::LINE_HEIGHT;
        gfx.setTextColor(COLOR_WHITE);
        drawClippedText(gfx, x + 2, ly + TEXT_Y_OFFSET, width - 4, lines[i] ? lines[i] : "");
    }
}

} // namespace Widgets
