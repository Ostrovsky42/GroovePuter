#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "display.h"

inline int textWidth(IGfx& gfx, const char* s) {
  if (!s) return 0;
  return gfx.textWidth(s);
}

inline void drawLineColored(IGfx& gfx, int x0, int y0, int x1, int y1, IGfxColor color) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    gfx.drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

inline void formatNoteName(int note, char* buf, size_t bufSize) {
  if (!buf || bufSize == 0) return;
  if (note == -2) {
    snprintf(buf, bufSize, "TI");
    return;
  }
  if (note < 0) {
    snprintf(buf, bufSize, "--");
    return;
  }
  static const char* names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = note / 12 - 1;
  const char* name = names[note % 12];
  snprintf(buf, bufSize, "%s%d", name, octave);
}

