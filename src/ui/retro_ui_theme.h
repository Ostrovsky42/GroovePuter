#pragma once

#ifdef USE_RETRO_THEME

#include <stdint.h>
#include "ui_colors.h"

namespace RetroTheme {

// ═══════════════════════════════════════════════════════════
// NEON COLOR PALETTE (RGB565)
// ═══════════════════════════════════════════════════════════

constexpr uint16_t NEON_CYAN      = 0x07FF; // Cyan
constexpr uint16_t NEON_MAGENTA   = 0xF81F; // Magenta
constexpr uint16_t NEON_YELLOW    = 0xFFE0; // Yellow
constexpr uint16_t NEON_GREEN     = 0x07E0; // Green (pure)
constexpr uint16_t NEON_PURPLE    = 0x781F; // Deep purple
constexpr uint16_t NEON_ORANGE    = 0xFD20; // Warm orange

// ═══════════════════════════════════════════════════════════
// BACKGROUNDS & PANELS
// ═══════════════════════════════════════════════════════════

constexpr uint16_t BG_DEEP_BLACK  = 0x0000;
constexpr uint16_t BG_DARK_GRAY   = 0x1082; // Very dark gray/blue
constexpr uint16_t BG_PANEL       = 0x2104; // Panel background
constexpr uint16_t BG_INSET       = 0x0841; // Sunken area

// ═══════════════════════════════════════════════════════════
// NAVIGATION & FOCUS
// ═══════════════════════════════════════════════════════════

constexpr uint16_t FOCUS_BORDER   = NEON_CYAN;
constexpr uint16_t FOCUS_GLOW     = 0x03EF; // Dim cyan glow
constexpr uint16_t SELECT_BG      = 0x0124; // Subtle select blue
constexpr uint16_t SELECT_BRIGHT  = 0x87FF; // Bright highlight

// ═══════════════════════════════════════════════════════════
// TEXT COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint16_t TEXT_PRIMARY   = 0xFFFF; // Pure white
constexpr uint16_t TEXT_SECONDARY = 0xAD55; // Silver/blue
constexpr uint16_t TEXT_DIM       = 0x632C; // Gray
constexpr uint16_t TEXT_INVERT    = 0x0000; // Black

// ═══════════════════════════════════════════════════════════
// GRID & LINES
// ═══════════════════════════════════════════════════════════

constexpr uint16_t GRID_DIM       = 0x18C3; // Faint grid
constexpr uint16_t GRID_MEDIUM    = 0x4208; // Middle grid
constexpr uint16_t GRID_BRIGHT    = 0x8410; // Strong grid

// ═══════════════════════════════════════════════════════════
// STATUS COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint16_t STATUS_ACTIVE  = NEON_GREEN;
constexpr uint16_t STATUS_ACCENT  = 0xF800; // Red
constexpr uint16_t STATUS_PLAYING = 0x07E0; // Green

// ═══════════════════════════════════════════════════════════
// CRT FX CONSTANTS
// ═══════════════════════════════════════════════════════════

constexpr uint16_t SCANLINE_COLOR = 0x0841;
constexpr int SCANLINE_SPACING    = 2;

} // namespace RetroTheme

#endif // USE_RETRO_THEME
