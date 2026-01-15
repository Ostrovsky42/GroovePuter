#pragma once

#include "ui_colors.h"
#include "ui_utils.h"

struct HelpLayout {
  int line_h;
  int col_w;
  int left_x;
  int right_x;
  int left_y;
  int right_y;
};

inline HelpLayout makeHelpLayout(IGfx& gfx, int x, int y, int w, int h) {
  (void)h;
  HelpLayout layout{};
  layout.line_h = gfx.fontHeight() + 2;
  if (layout.line_h < 10) layout.line_h = 10;
  layout.col_w = w / 2 - 6;
  if (layout.col_w < 40) layout.col_w = w - 8;
  if (layout.col_w < 10) layout.col_w = w;
  layout.left_x = x + 4;
  layout.right_x = x + layout.col_w + 10;
  layout.left_y = y + 4;
  layout.right_y = y + 4 + layout.line_h;
  return layout;
}

inline void drawHelpHeading(IGfx& gfx, int x, int y, const char* text) {
  gfx.setTextColor(COLOR_ACCENT);
  gfx.drawText(x, y, text);
  gfx.setTextColor(COLOR_WHITE);
}

inline void drawHelpItem(IGfx& gfx, int x, int y, const char* key, const char* desc, IGfxColor keyColor) {
  gfx.setTextColor(keyColor);
  gfx.drawText(x, y, key);
  gfx.setTextColor(COLOR_WHITE);
  int key_w = textWidth(gfx, key);
  gfx.drawText(x + key_w + 6, y, desc);
}

inline void drawHelpScrollbar(IGfx& gfx, int x, int y, int w, int h, int pageIndex, int totalPages) {
  if (w <= 0 || h <= 0 || totalPages <= 1) return;
  if (pageIndex < 0) pageIndex = 0;
  if (pageIndex >= totalPages) pageIndex = totalPages - 1;
  int bar_x = x + w - 2;
  gfx.setTextColor(IGfxColor::Gray());
  gfx.drawLine(bar_x, y, bar_x, y + h - 1);
  int page_h = h / totalPages;
  if (page_h < 2) page_h = 2;
  int y1 = y + page_h * pageIndex;
  int y2 = y1 + page_h;
  if (y2 > y + h - 1) y2 = y + h - 1;
  gfx.setTextColor(IGfxColor::White());
  gfx.drawLine(bar_x, y1, bar_x, y2);
  gfx.setTextColor(COLOR_WHITE);
}

inline void drawHelpPageTransport(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "Transport");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "SPACE", "play/stop", IGfxColor::Green());
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "K / L", "BPM -/+", IGfxColor::Cyan());
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Pages");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "[ / ]", "prev/next page", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "TAB", "Open page help", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ESC", "Back / Close Help", COLOR_LABEL);

  left_y += lh;
  drawHelpHeading(gfx, layout.left_x, left_y, "Playback");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "I / O", "303A/303B randomize", IGfxColor::Yellow());
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "P", "drum randomize", IGfxColor::Yellow());
}

inline void drawHelpPage303(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int right_y = layout.right_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "303 Synth");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "A / Z", "cutoff +/-", COLOR_KNOB_1);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "S / X", "res +/-", COLOR_KNOB_2);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "D / C", "env amt +/-", COLOR_KNOB_3);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "F / V", "decay +/-", COLOR_KNOB_4);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "M", "toggle mode", IGfxColor::Magenta());
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "N", "toggle distortion", IGfxColor::Magenta());

  drawHelpHeading(gfx, layout.right_x, right_y, "Presets");
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "1-4", "Load Preset", IGfxColor::Orange());
  right_y += lh;
  drawHelpHeading(gfx, layout.right_x, right_y, "Mutes");
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "I / 2", "303A / 303B", IGfxColor::Orange());
}

inline void drawHelpPageTape(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "Tape Section");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "CTRL+M", "Mode: STOP/REC/PLAY", IGfxColor::Red());
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "1-6", "Load Tape Character", IGfxColor::Cyan());
  left_y += lh;
  
  drawHelpHeading(gfx, layout.left_x, left_y, "DSP Effects");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "WOW", "Pitch instability", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "AGE", "Tape wear/HF loss", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "SAT", "Saturation/Warmth", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "CRSH", "Lo-fi degradation", COLOR_LABEL);
}

inline void drawHelpPage303PatternEdit(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "303 Pattern Edit");
  left_y += lh;
  drawHelpHeading(gfx, layout.left_x, left_y, "Navigation");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "LEFT/RIGHT", "", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "UP/DOWN", "move", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ENTER", "Load pattern", IGfxColor::Green());
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Pattern slots");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "SHIFT+Q..I", "Pick pattern", COLOR_PATTERN_SELECTED_FILL);
  left_y += lh;

  int right_y = y + 4 + lh;
  drawHelpHeading(gfx, layout.right_x, right_y, "Step edits");
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "Q", "Toggle slide", COLOR_SLIDE);
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "W", "Toggle accent", COLOR_ACCENT);
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "A / Z", "Note +1 / -1", COLOR_303_NOTE);
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "S / X", "Octave + / -", COLOR_LABEL);
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "BACK", "Clear step", IGfxColor::Red());
}

inline void drawHelpPageDrumPatternEdit(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int right_y = layout.right_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "Drums Pattern Edit");
  left_y += lh;
  drawHelpHeading(gfx, layout.left_x, left_y, "Navigation");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "LEFT / RIGHT", "", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "UP / DOWN", "move", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ENTER", "Load/toggle ", IGfxColor::Green());
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Patterns");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "SHIFT+Q..I", "Select drum pattern", COLOR_PATTERN_SELECTED_FILL);

  drawHelpHeading(gfx, layout.right_x, right_y, "Step edits");
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "ENTER", "Toggle hit", IGfxColor::Green());
  right_y += lh;
  drawHelpItem(gfx, layout.right_x, right_y, "W", "Toggle accent", COLOR_ACCENT);
}

inline void drawHelpPageSong(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "Song Page");
  left_y += lh;
  drawHelpHeading(gfx, layout.left_x, left_y, "Navigation");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "LEFT/RIGHT", "col / mode focus", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "UP/DOWN", "rows", COLOR_LABEL);
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ALT+UP/DN", "slot +/-", IGfxColor::Yellow());
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Patterns");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "Q..I", "set 1-8", COLOR_PATTERN_SELECTED_FILL);
  // left_y += lh;
  drawHelpItem(gfx, layout.right_x, left_y, "BACK", "clear slot", IGfxColor::Red());
}

inline void drawHelpPageSongCont(IGfx& gfx, int x, int y, int w, int h) {
  HelpLayout layout = makeHelpLayout(gfx, x, y, w, h);
  int left_y = layout.left_y;
  int lh = layout.line_h;

  drawHelpHeading(gfx, layout.left_x, left_y, "Song Page (cont.)");
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Playhead");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ALT+UP/DN @PLAY", "nudge playhead", IGfxColor::Yellow());
  left_y += lh;
  left_y += lh;

  drawHelpHeading(gfx, layout.left_x, left_y, "Mode");
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "ENTER @ MODE", "Song/Pat toggle", IGfxColor::Green());
  left_y += lh;
  drawHelpItem(gfx, layout.left_x, left_y, "M", "toggle mode", IGfxColor::Magenta());
}
