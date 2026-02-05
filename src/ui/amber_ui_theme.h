#pragma once

#include <stdint.h>

namespace AmberTheme {

// ═══════════════════════════════════════════════════════════
// AMBER TERMINAL PALETTE (RGB888)
// ═══════════════════════════════════════════════════════════

constexpr uint32_t NEON_CYAN      = 0xFFB000; // Amber text
constexpr uint32_t NEON_MAGENTA   = 0xFFD36A; // Bright amber highlight
constexpr uint32_t NEON_YELLOW    = 0xFFD36A; // Bright amber
constexpr uint32_t NEON_GREEN     = 0xFFB000; // Amber
constexpr uint32_t NEON_PURPLE    = 0xC88700; // Muted amber
constexpr uint32_t NEON_ORANGE    = 0xFF6A00; // Warm orange

// ═══════════════════════════════════════════════════════════
// BACKGROUNDS & PANELS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t BG_DEEP_BLACK  = 0x0A0700; // Deep brown-black
constexpr uint32_t BG_DARK_GRAY   = 0x120C00; // Dark amber-brown
constexpr uint32_t BG_PANEL       = 0x161000; // Panel background
constexpr uint32_t BG_INSET       = 0x0F0A00; // Sunken area

// ═══════════════════════════════════════════════════════════
// NAVIGATION & FOCUS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t FOCUS_BORDER   = NEON_CYAN;
constexpr uint32_t FOCUS_GLOW     = 0x553200; // Dim amber glow
constexpr uint32_t SELECT_BG      = 0x241600; // Subtle select
constexpr uint32_t SELECT_BRIGHT  = 0xFFD36A; // Bright highlight

// ═══════════════════════════════════════════════════════════
// TEXT COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t TEXT_PRIMARY   = 0xFFB000; // Amber text
constexpr uint32_t TEXT_SECONDARY = 0xC88700; // Muted amber
constexpr uint32_t TEXT_DIM       = 0x8A6A00; // Dim amber
constexpr uint32_t TEXT_INVERT    = BG_DEEP_BLACK;

// ═══════════════════════════════════════════════════════════
// GRID & LINES
// ═══════════════════════════════════════════════════════════

constexpr uint32_t GRID_DIM       = 0x2B1B00; // Faint grid
constexpr uint32_t GRID_MEDIUM    = 0x3A2600; // Middle grid
constexpr uint32_t GRID_BRIGHT    = 0x4A3300; // Strong grid

// ═══════════════════════════════════════════════════════════
// STATUS COLORS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t STATUS_ACTIVE  = NEON_CYAN;
constexpr uint32_t STATUS_ACCENT  = NEON_ORANGE;
constexpr uint32_t STATUS_PLAYING = NEON_MAGENTA;

// ═══════════════════════════════════════════════════════════
// CRT FX CONSTANTS
// ═══════════════════════════════════════════════════════════

constexpr uint32_t SCANLINE_COLOR = 0x120C00;
constexpr int SCANLINE_SPACING    = 2;

} // namespace AmberTheme
