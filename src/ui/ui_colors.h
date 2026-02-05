#pragma once

#include "display.h"

// High-contrast dark palette (readability-first)
inline constexpr IGfxColor COLOR_BG = IGfxColor(0x0B0F14);
inline constexpr IGfxColor COLOR_PANEL = IGfxColor(0x111827);
inline constexpr IGfxColor COLOR_TEXT = IGfxColor(0xE5E7EB);
inline constexpr IGfxColor COLOR_MUTED = IGfxColor(0x9CA3AF);
inline constexpr IGfxColor COLOR_ACCENT = IGfxColor(0x22C55E); // OK / Accent
inline constexpr IGfxColor COLOR_WARN = IGfxColor(0xF59E0B);
inline constexpr IGfxColor COLOR_DANGER = IGfxColor(0xEF4444);
inline constexpr IGfxColor COLOR_INFO = IGfxColor(0x38BDF8);

inline constexpr IGfxColor COLOR_WHITE = COLOR_TEXT;
inline constexpr IGfxColor COLOR_BLACK = COLOR_BG;
inline constexpr IGfxColor COLOR_GRAY = IGfxColor(0x1F2937);
inline constexpr IGfxColor COLOR_LIGHT_GRAY = IGfxColor(0x374151);
inline constexpr IGfxColor COLOR_DARKER = COLOR_BG;
inline constexpr IGfxColor COLOR_WAVE = COLOR_INFO;
inline constexpr IGfxColor COLOR_SLIDE = COLOR_INFO;
inline constexpr IGfxColor COLOR_303_NOTE = IGfxColor(0x0F172A);
inline constexpr IGfxColor COLOR_STEP_HILIGHT = COLOR_TEXT;
inline constexpr IGfxColor COLOR_DRUM_KICK = COLOR_DANGER;
inline constexpr IGfxColor COLOR_DRUM_SNARE = COLOR_INFO;
inline constexpr IGfxColor COLOR_DRUM_HAT = COLOR_WARN;
inline constexpr IGfxColor COLOR_DRUM_OPEN_HAT = COLOR_WARN;
inline constexpr IGfxColor COLOR_DRUM_MID_TOM = COLOR_ACCENT;
inline constexpr IGfxColor COLOR_DRUM_HIGH_TOM = COLOR_ACCENT;
inline constexpr IGfxColor COLOR_DRUM_RIM = COLOR_TEXT;
inline constexpr IGfxColor COLOR_DRUM_CLAP = COLOR_MUTED;
inline constexpr IGfxColor COLOR_LABEL = COLOR_MUTED;
inline constexpr IGfxColor COLOR_DARK_GRAY = COLOR_PANEL;
inline constexpr IGfxColor COLOR_RED = COLOR_DANGER;
inline constexpr IGfxColor COLOR_MUTE_BACKGROUND = COLOR_PANEL;
inline constexpr IGfxColor COLOR_GRAY_DARKER = COLOR_PANEL;
inline constexpr IGfxColor COLOR_KNOB_1 = COLOR_WARN;
inline constexpr IGfxColor COLOR_KNOB_2 = COLOR_INFO;
inline constexpr IGfxColor COLOR_KNOB_3 = COLOR_DANGER;
inline constexpr IGfxColor COLOR_KNOB_4 = COLOR_ACCENT;
inline constexpr IGfxColor COLOR_KNOB_CONTROL = COLOR_WARN;
inline constexpr IGfxColor COLOR_STEP_SELECTED = COLOR_WARN;
inline constexpr IGfxColor COLOR_PATTERN_SELECTED_FILL = COLOR_INFO;
inline constexpr IGfxColor WAVE_COLORS[] = {
  COLOR_INFO,
  COLOR_ACCENT,
  COLOR_WARN,
  COLOR_DANGER,
  COLOR_TEXT,
};
inline constexpr int NUM_WAVE_COLORS = static_cast<int>(sizeof(WAVE_COLORS) / sizeof(WAVE_COLORS[0]));
