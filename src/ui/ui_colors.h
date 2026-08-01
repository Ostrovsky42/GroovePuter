#pragma once

#include "display.h"

// CARBON base constants used by legacy/minimal pages that have not yet moved
// to the semantic runtime palette. Keep these restrained: bright white is not
// the default body color, and focus/activity own the brightest pixels.
inline constexpr IGfxColor COLOR_BG = IGfxColor(0x020406);
inline constexpr IGfxColor COLOR_PANEL = IGfxColor(0x080D12);
inline constexpr IGfxColor COLOR_TEXT = IGfxColor(0xD2DAE2);
inline constexpr IGfxColor COLOR_MUTED = IGfxColor(0x82909C);
inline constexpr IGfxColor COLOR_ACCENT = IGfxColor(0x4AC3E8);
inline constexpr IGfxColor COLOR_WARN = IGfxColor(0xE6B85C);
inline constexpr IGfxColor COLOR_DANGER = IGfxColor(0xE35D72);
inline constexpr IGfxColor COLOR_INFO = IGfxColor(0x56C8EA);

inline constexpr IGfxColor COLOR_WHITE = COLOR_TEXT;
inline constexpr IGfxColor COLOR_BLACK = COLOR_BG;
inline constexpr IGfxColor COLOR_GRAY = IGfxColor(0x101820);
inline constexpr IGfxColor COLOR_LIGHT_GRAY = IGfxColor(0x27323C);
inline constexpr IGfxColor COLOR_DARKER = COLOR_BG;
inline constexpr IGfxColor COLOR_WAVE = COLOR_INFO;
inline constexpr IGfxColor COLOR_SLIDE = COLOR_INFO;
inline constexpr IGfxColor COLOR_303_NOTE = IGfxColor(0x05090D);
inline constexpr IGfxColor COLOR_SYNTH_A = IGfxColor(0x69D89A);
inline constexpr IGfxColor COLOR_SYNTH_B = COLOR_INFO;
inline constexpr IGfxColor COLOR_STEP_HILIGHT = IGfxColor(0xE8EEF3);
inline constexpr IGfxColor COLOR_DRUM_KICK = COLOR_DANGER;
inline constexpr IGfxColor COLOR_DRUM_SNARE = COLOR_INFO;
inline constexpr IGfxColor COLOR_DRUM_HAT = COLOR_WARN;
inline constexpr IGfxColor COLOR_DRUM_OPEN_HAT = COLOR_WARN;
inline constexpr IGfxColor COLOR_DRUM_MID_TOM = IGfxColor(0x69D89A);
inline constexpr IGfxColor COLOR_DRUM_HIGH_TOM = IGfxColor(0x69D89A);
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
inline constexpr IGfxColor COLOR_KNOB_4 = IGfxColor(0x69D89A);
inline constexpr IGfxColor COLOR_KNOB_CONTROL = COLOR_WARN;
inline constexpr IGfxColor COLOR_STEP_SELECTED = COLOR_WARN;
inline constexpr IGfxColor COLOR_PATTERN_SELECTED_FILL = COLOR_INFO;
inline constexpr IGfxColor WAVE_COLORS[] = {
  COLOR_INFO,
  IGfxColor(0x69D89A),
  COLOR_WARN,
  COLOR_DANGER,
  COLOR_TEXT,
};
inline constexpr int NUM_WAVE_COLORS = static_cast<int>(sizeof(WAVE_COLORS) / sizeof(WAVE_COLORS[0]));
