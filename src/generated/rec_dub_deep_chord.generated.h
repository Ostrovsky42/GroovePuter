#pragma once

#include "atlas_runtime_types.generated.h"

namespace AtlasGenerated {

inline constexpr Event kEvents_PAT_ED_DUB_DEEP_CHORD_P1[] = {
  {0, 0, -1, 112, 0, 100, 1},
  {0, 4, -1, 108, 0, 100, 1},
  {0, 8, -1, 112, 0, 100, 1},
  {0, 12, -1, 108, 0, 100, 1},
  {1, 4, -1, 76, 0, 100, 1},
  {1, 12, -1, 82, 0, 100, 1},
  {2, 2, -1, 56, 0, 82, 1},
  {2, 6, -1, 60, 0, 88, 1},
  {2, 10, -1, 54, 0, 78, 1},
  {2, 14, -1, 64, 0, 92, 1},
  {3, 6, -1, 68, 0, 64, 1},
  {3, 14, -1, 74, 0, 76, 1},
  {4, 3, -1, 46, 0, 56, 1},
  {4, 11, -1, 50, 0, 64, 1},
  {5, 7, -1, 42, 0, 48, 1},
  {5, 15, -1, 48, 0, 58, 1},
  {8, 0, 36, 92, 0, 100, 9},
  {8, 3, 31, 72, 0, 100, 9},
  {8, 8, 34, 88, 0, 100, 9},
  {8, 11, 31, 70, 0, 100, 9},
  {9, 2, 48, 82, 0, 100, 1},
  {9, 6, 48, 74, 0, 100, 1},
  {9, 7, 72, 48, 0, 52, 1},
  {9, 10, 56, 78, 0, 100, 1},
  {9, 14, 55, 86, 0, 100, 1},
  {9, 15, 67, 54, 0, 60, 1},
};

inline constexpr Event kEvents_PAT_ED_DUB_DEEP_CHORD_P2[] = {
  {0, 0, -1, 114, 0, 100, 1},
  {0, 4, -1, 108, 0, 100, 1},
  {0, 8, -1, 114, 0, 100, 1},
  {0, 12, -1, 110, 0, 100, 1},
  {1, 4, -1, 78, 0, 100, 1},
  {1, 12, -1, 84, 0, 100, 1},
  {2, 2, -1, 58, 0, 84, 1},
  {2, 6, -1, 62, 0, 90, 1},
  {2, 9, -1, 40, 0, 52, 1},
  {2, 10, -1, 56, 0, 80, 1},
  {2, 14, -1, 66, 0, 94, 1},
  {3, 6, -1, 70, 0, 68, 1},
  {3, 14, -1, 76, 0, 80, 1},
  {4, 3, -1, 46, 0, 58, 1},
  {4, 11, -1, 52, 0, 68, 1},
  {4, 15, -1, 44, 0, 52, 1},
  {5, 7, -1, 42, 0, 50, 1},
  {5, 13, -1, 38, 0, 46, 1},
  {5, 15, -1, 50, 0, 62, 1},
  {7, 12, -1, 42, 0, 48, 1},
  {8, 0, 36, 92, 0, 100, 9},
  {8, 3, 31, 72, 0, 100, 9},
  {8, 8, 34, 88, 0, 100, 9},
  {8, 11, 31, 70, 0, 100, 9},
  {8, 14, 39, 62, 0, 100, 1},
  {9, 2, 48, 84, 0, 100, 1},
  {9, 5, 75, 42, 0, 44, 1},
  {9, 6, 48, 76, 0, 100, 1},
  {9, 7, 72, 50, 0, 56, 1},
  {9, 10, 56, 80, 0, 100, 1},
  {9, 14, 55, 88, 0, 100, 1},
  {9, 15, 67, 58, 0, 66, 1},
};

inline constexpr Event kEvents_PAT_ED_DUB_DEEP_CHORD_P3[] = {
  {0, 0, -1, 108, 0, 100, 1},
  {0, 8, -1, 110, 0, 100, 1},
  {0, 12, -1, 104, 0, 100, 1},
  {1, 4, -1, 72, 0, 100, 1},
  {1, 12, -1, 78, 0, 100, 1},
  {2, 6, -1, 56, 0, 74, 1},
  {2, 14, -1, 62, 0, 86, 1},
  {3, 14, -1, 72, 0, 72, 1},
  {4, 11, -1, 48, 0, 58, 1},
  {5, 15, -1, 46, 0, 56, 1},
  {8, 0, 36, 88, 0, 100, 9},
  {8, 8, 34, 84, 0, 100, 9},
  {9, 2, 48, 78, 0, 100, 1},
  {9, 14, 55, 84, 0, 100, 1},
  {9, 15, 67, 52, 0, 58, 1},
};

inline constexpr Pattern kPatterns_REC_DUB_DEEP_CHORD[] = {
  {"PAT_ED_DUB_DEEP_CHORD_P1", "P1", "BASE", kEvents_PAT_ED_DUB_DEEP_CHORD_P1, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P1) / sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P1[0]))},
  {"PAT_ED_DUB_DEEP_CHORD_P2", "P2", "DEVELOPMENT", kEvents_PAT_ED_DUB_DEEP_CHORD_P2, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P2) / sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P2[0]))},
  {"PAT_ED_DUB_DEEP_CHORD_P3", "P3", "SPACE_BREAK", kEvents_PAT_ED_DUB_DEEP_CHORD_P3, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P3) / sizeof(kEvents_PAT_ED_DUB_DEEP_CHORD_P3[0]))},
};

inline constexpr Recipe kRecipe_REC_DUB_DEEP_CHORD = {10, "REC_DUB_DEEP_CHORD", "Deep Stab", 120, 54, kPatterns_REC_DUB_DEEP_CHORD, static_cast<uint8_t>(sizeof(kPatterns_REC_DUB_DEEP_CHORD) / sizeof(kPatterns_REC_DUB_DEEP_CHORD[0]))};

}  // namespace AtlasGenerated
