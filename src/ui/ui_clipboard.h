#pragma once

#include "../../scenes.h"

struct PatternClipboard {
  bool has_pattern = false;
  SynthPattern pattern{};
};

struct DrumPatternClipboard {
  bool has_pattern = false;
  DrumPatternSet pattern{};
};

extern PatternClipboard g_pattern_clipboard;
extern DrumPatternClipboard g_drum_pattern_clipboard;
