#pragma once

#ifdef USE_RETRO_THEME

#include <stdint.h>
#include "ui_colors.h"

namespace RetroTheme {

// ═══════════════════════════════════════════════════════════
// CYBER PALETTE (RGB888)
// Bright enough to read on stage, but deliberately below pure 0xFF
// primaries so phone cameras preserve text edges instead of blooming.
// ═══════════════════════════════════════════════════════════

constexpr uint32_t NEON_CYAN      = 0x4FE7FF;
constexpr uint32_t NEON_MAGENTA   = 0xFF5CCF;
constexpr uint32_t NEON_YELLOW    = 0xFFE66D;
constexpr uint32_t NEON_GREEN     = 0x72F1A7;
constexpr uint32_t NEON_PURPLE    = 0xA78BFA;
constexpr uint32_t NEON_ORANGE    = 0xFF9F43;

// ═══════════════════════════════════════════════════════════
// BACKGROUNDS & PANELS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t BG_DEEP_BLACK  = 0x04070B;
constexpr uint32_t BG_DARK_GRAY   = 0x090E14;
constexpr uint32_t BG_PANEL       = 0x0D151E;
constexpr uint32_t BG_INSET       = 0x060B11;

// ═══════════════════════════════════════════════════════════
// NAVIGATION & FOCUS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t FOCUS_BORDER   = NEON_CYAN;
constexpr uint32_t FOCUS_GLOW     = 0x145A66;
constexpr uint32_t SELECT_BG      = 0x132534;
constexpr uint32_t SELECT_BRIGHT  = 0xA5F5FF;

// ═══════════════════════════════════════════════════════════
// TEXT COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t TEXT_PRIMARY   = 0xEEF6FF;
constexpr uint32_t TEXT_SECONDARY = 0xA9BACB;
constexpr uint32_t TEXT_DIM       = 0x657789;
constexpr uint32_t TEXT_INVERT    = BG_DEEP_BLACK;

// ═══════════════════════════════════════════════════════════
// GRID & LINES
// ═══════════════════════════════════════════════════════════

constexpr uint32_t GRID_DIM       = 0x18232E;
constexpr uint32_t GRID_MEDIUM    = 0x273747;
constexpr uint32_t GRID_BRIGHT    = 0x3A5065;

// ═══════════════════════════════════════════════════════════
// STATUS COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t STATUS_ACTIVE  = NEON_GREEN;
constexpr uint32_t STATUS_ACCENT  = NEON_ORANGE;
constexpr uint32_t STATUS_PLAYING = NEON_GREEN;

// ═══════════════════════════════════════════════════════════
// CRT FX CONSTANTS
// Kept for legacy widgets, but current production pages should avoid
// timer-driven scanlines/flicker on the small TFT.
// ═══════════════════════════════════════════════════════════

constexpr uint32_t SCANLINE_COLOR = 0x060B11;
constexpr int SCANLINE_SPACING    = 2;

} // namespace RetroTheme

#endif // USE_RETRO_THEME
