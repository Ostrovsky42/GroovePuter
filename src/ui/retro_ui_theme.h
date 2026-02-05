#pragma once

#ifdef USE_RETRO_THEME

#include <stdint.h>
#include "ui_colors.h"

namespace RetroTheme {

// ═══════════════════════════════════════════════════════════
// NEON COLOR PALETTE (RGB888, Cyber-Night)
// ═══════════════════════════════════════════════════════════

constexpr uint32_t NEON_CYAN      = 0x00FFFF; // True cyan
constexpr uint32_t NEON_MAGENTA   = 0xFF00FF; // Hot magenta
constexpr uint32_t NEON_YELLOW    = 0xFFF300; // Neon yellow
constexpr uint32_t NEON_GREEN     = 0x00FF6A; // Electric green
constexpr uint32_t NEON_PURPLE    = 0x8A2BE2; // Neon purple
constexpr uint32_t NEON_ORANGE    = 0xFF8000; // Neon orange

// ═══════════════════════════════════════════════════════════
// BACKGROUNDS & PANELS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t BG_DEEP_BLACK  = 0x05070A; // Deep blue-black
constexpr uint32_t BG_DARK_GRAY   = 0x0A0F16; // Dark slate
constexpr uint32_t BG_PANEL       = 0x0E1621; // Panel background
constexpr uint32_t BG_INSET       = 0x070C14; // Sunken area

// ═══════════════════════════════════════════════════════════
// NAVIGATION & FOCUS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t FOCUS_BORDER   = NEON_CYAN;
constexpr uint32_t FOCUS_GLOW     = 0x007A7A; // Dim cyan glow
constexpr uint32_t SELECT_BG      = 0x102033; // Subtle select blue
constexpr uint32_t SELECT_BRIGHT  = 0x66FFF2; // Bright highlight

// ═══════════════════════════════════════════════════════════
// TEXT COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t TEXT_PRIMARY   = 0xE6F1FF; // Cool off-white
constexpr uint32_t TEXT_SECONDARY = 0xA3B4C6; // Muted cool
constexpr uint32_t TEXT_DIM       = 0x637083; // Dim blue-gray
constexpr uint32_t TEXT_INVERT    = BG_DEEP_BLACK;

// ═══════════════════════════════════════════════════════════
// GRID & LINES
// ═══════════════════════════════════════════════════════════

constexpr uint32_t GRID_DIM       = 0x1C2430; // Faint grid
constexpr uint32_t GRID_MEDIUM    = 0x2A3546; // Middle grid
constexpr uint32_t GRID_BRIGHT    = 0x3A4B63; // Strong grid

// ═══════════════════════════════════════════════════════════
// STATUS COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t STATUS_ACTIVE  = NEON_GREEN;
constexpr uint32_t STATUS_ACCENT  = NEON_ORANGE; // Accent
constexpr uint32_t STATUS_PLAYING = NEON_GREEN;  // Playing

// ═══════════════════════════════════════════════════════════
// CRT FX CONSTANTS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t SCANLINE_COLOR = 0x070C14;
constexpr int SCANLINE_SPACING    = 2;

} // namespace RetroTheme

#endif // USE_RETRO_THEME
