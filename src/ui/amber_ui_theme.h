#pragma once

#include <stdint.h>

namespace AmberTheme {

// ═══════════════════════════════════════════════════════════
// AMBER HARDWARE PALETTE (RGB888)
// Warm monochrome character with enough separation between text,
// focus and warnings to remain readable on the Cardputer TFT.
// ═══════════════════════════════════════════════════════════

constexpr uint32_t NEON_CYAN      = 0xFFB84D; // primary amber
constexpr uint32_t NEON_MAGENTA   = 0xFFD27A; // bright selected state
constexpr uint32_t NEON_YELLOW    = 0xFFE09A; // warning / peak
constexpr uint32_t NEON_GREEN     = 0xFFC15A; // active
constexpr uint32_t NEON_PURPLE    = 0xC98932; // muted amber
constexpr uint32_t NEON_ORANGE    = 0xFF8A3D; // destructive/accent

// ═══════════════════════════════════════════════════════════
// BACKGROUNDS & PANELS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t BG_DEEP_BLACK  = 0x080603;
constexpr uint32_t BG_DARK_GRAY   = 0x100B05;
constexpr uint32_t BG_PANEL       = 0x171006;
constexpr uint32_t BG_INSET       = 0x0C0804;

// ═══════════════════════════════════════════════════════════
// NAVIGATION & FOCUS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t FOCUS_BORDER   = NEON_CYAN;
constexpr uint32_t FOCUS_GLOW     = 0x5A3715;
constexpr uint32_t SELECT_BG      = 0x28180A;
constexpr uint32_t SELECT_BRIGHT  = 0xFFE0A0;

// ═══════════════════════════════════════════════════════════
// TEXT COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t TEXT_PRIMARY   = 0xFFC76A;
constexpr uint32_t TEXT_SECONDARY = 0xC98D42;
constexpr uint32_t TEXT_DIM       = 0x7D5A31;
constexpr uint32_t TEXT_INVERT    = BG_DEEP_BLACK;

// ═══════════════════════════════════════════════════════════
// GRID & LINES
// ═══════════════════════════════════════════════════════════

constexpr uint32_t GRID_DIM       = 0x2B1C0D;
constexpr uint32_t GRID_MEDIUM    = 0x422A12;
constexpr uint32_t GRID_BRIGHT    = 0x5B3B1B;

// ═══════════════════════════════════════════════════════════
// STATUS COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t STATUS_ACTIVE  = NEON_CYAN;
constexpr uint32_t STATUS_ACCENT  = NEON_ORANGE;
constexpr uint32_t STATUS_PLAYING = NEON_MAGENTA;

// ═══════════════════════════════════════════════════════════
// CRT FX CONSTANTS
// Retained only for compatible legacy widgets. Production layouts should
// prefer stable geometry over scanline animation for camera readability.
// ═══════════════════════════════════════════════════════════

constexpr uint32_t SCANLINE_COLOR = 0x100B05;
constexpr int SCANLINE_SPACING    = 2;

} // namespace AmberTheme
