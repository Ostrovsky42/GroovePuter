#pragma once
#include <cstdint>

struct PerfStats {
  uint32_t audioUnderruns;
  float cpuAudioPct;
  uint32_t heapFree;
  uint32_t heapMinFree;
};
