#pragma once

#include "atlas_runtime_types.generated.h"

namespace AtlasGenerated {

inline constexpr Event kEvents_PAT_ED_DUB_MINIMAL_SPACE_P1[] = {
  {0, 0, -1, 112, 0, 100, 1},
  {0, 4, -1, 108, 0, 100, 1},
  {0, 8, -1, 112, 0, 100, 1},
  {0, 12, -1, 108, 0, 100, 1},
  {1, 4, -1, 76, 0, 100, 1},
  {1, 12, -1, 82, 0, 100, 1},
  {2, 2, -1, 56, 0, 59, 1},
  {2, 6, -1, 60, 0, 63, 1},
  {2, 10, -1, 54, 0, 56, 1},
  {2, 14, -1, 64, 0, 66, 1},
  {3, 6, -1, 68, 0, 46, 1},
  {3, 14, -1, 74, 0, 54, 1},
  {4, 3, -1, 46, 0, 40, 1},
  {4, 11, -1, 50, 0, 46, 1},
  {5, 7, -1, 42, 0, 34, 1},
  {5, 15, -1, 48, 0, 41, 1},
  {8, 0, 36, 92, 0, 100, 9},
  {8, 3, 31, 72, 0, 100, 9},
  {8, 8, 34, 88, 0, 100, 9},
  {8, 11, 31, 70, 0, 100, 9},
  {9, 2, 48, 76, 0, 100, 1},
  {9, 7, 72, 48, 0, 37, 1},
  {9, 10, 56, 70, 0, 68, 1},
  {9, 14, 55, 82, 0, 78, 1},
  {9, 15, 67, 54, 0, 43, 1},
};

inline constexpr Event kEvents_PAT_ED_DUB_MINIMAL_SPACE_P2[] = {
  {0, 0, -1, 110, 0, 100, 1},
  {0, 8, -1, 112, 0, 100, 1},
  {0, 12, -1, 104, 0, 100, 1},
  {1, 4, -1, 78, 0, 100, 1},
  {1, 12, -1, 84, 0, 100, 1},
  {2, 2, -1, 58, 0, 60, 1},
  {2, 6, -1, 62, 0, 64, 1},
  {2, 9, -1, 40, 0, 37, 1},
  {2, 10, -1, 56, 0, 57, 1},
  {2, 14, -1, 66, 0, 67, 1},
  {3, 6, -1, 70, 0, 48, 1},
  {3, 14, -1, 76, 0, 57, 1},
  {4, 3, -1, 46, 0, 41, 1},
  {4, 11, -1, 52, 0, 48, 1},
  {4, 15, -1, 44, 0, 37, 1},
  {5, 7, -1, 42, 0, 36, 1},
  {5, 13, -1, 38, 0, 33, 1},
  {5, 15, -1, 50, 0, 44, 1},
  {7, 12, -1, 42, 0, 48, 1},
  {8, 0, 36, 92, 0, 100, 9},
  {8, 3, 31, 72, 0, 100, 9},
  {8, 8, 34, 88, 0, 100, 9},
  {8, 11, 31, 70, 0, 100, 9},
  {8, 14, 39, 62, 0, 100, 1},
  {9, 2, 48, 84, 0, 100, 1},
  {9, 5, 75, 42, 0, 31, 1},
  {9, 6, 48, 76, 0, 100, 1},
  {9, 7, 72, 50, 0, 40, 1},
  {9, 10, 56, 80, 0, 100, 1},
  {9, 14, 55, 88, 0, 100, 1},
  {9, 15, 67, 58, 0, 47, 1},
};

inline constexpr Event kEvents_PAT_ED_DUB_MINIMAL_SPACE_P3[] = {
  {0, 0, -1, 106, 0, 100, 1},
  {0, 8, -1, 108, 0, 100, 1},
  {1, 4, -1, 72, 0, 100, 1},
  {1, 12, -1, 78, 0, 100, 1},
  {2, 6, -1, 56, 0, 53, 1},
  {2, 14, -1, 62, 0, 61, 1},
  {3, 14, -1, 72, 0, 51, 1},
  {4, 11, -1, 48, 0, 41, 1},
  {5, 15, -1, 46, 0, 40, 1},
  {8, 0, 36, 88, 0, 100, 9},
  {8, 8, 34, 84, 0, 100, 9},
  {9, 2, 48, 78, 0, 100, 1},
  {9, 14, 55, 84, 0, 100, 1},
  {9, 15, 67, 52, 0, 41, 1},
};

inline constexpr Pattern kPatterns_REC_DUB_MINIMAL_SPACE[] = {
  {"PAT_ED_DUB_MINIMAL_SPACE_P1", "P1", "BASE", kEvents_PAT_ED_DUB_MINIMAL_SPACE_P1, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P1) / sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P1[0]))},
  {"PAT_ED_DUB_MINIMAL_SPACE_P2", "P2", "DEVELOPMENT", kEvents_PAT_ED_DUB_MINIMAL_SPACE_P2, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P2) / sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P2[0]))},
  {"PAT_ED_DUB_MINIMAL_SPACE_P3", "P3", "EMPTY_BREAK", kEvents_PAT_ED_DUB_MINIMAL_SPACE_P3, static_cast<uint16_t>(sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P3) / sizeof(kEvents_PAT_ED_DUB_MINIMAL_SPACE_P3[0]))},
};

inline constexpr Recipe kRecipe_REC_DUB_MINIMAL_SPACE = {11, "REC_DUB_MINIMAL_SPACE", "Minimal Space", 116, 51, kPatterns_REC_DUB_MINIMAL_SPACE, static_cast<uint8_t>(sizeof(kPatterns_REC_DUB_MINIMAL_SPACE) / sizeof(kPatterns_REC_DUB_MINIMAL_SPACE[0]))};

}  // namespace AtlasGenerated
