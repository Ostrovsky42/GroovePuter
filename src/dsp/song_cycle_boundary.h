#pragma once

struct SongCycleBoundary {
  int barIndex;
  bool advanceRow;
};

inline int normalizedSongPatternBars(int patternBars) {
  return (patternBars == 1 || patternBars == 2 ||
patternBars == 4 || patternBars == 8)
      ? patternBars
      : 1;
}

inline SongCycleBoundary nextSongCycleBoundary(int barIndex, int patternBars) {
  const int bars = normalizedSongPatternBars(patternBars);
  if (barIndex < 0) return {0, false};

  const int nextBar = barIndex + 1;
  if (nextBar >= bars) return {0, true};
  return {nextBar, false};
}
