// Host run of src/perf/render_bench.h (the Cardputer runs the same code at
// boot in a GROOVEPUTER_RENDER_BENCH build). On the host both orders should
// cost about the same; on the device the gap is instruction-cache misses.
// Usage: component_bench [blocks=400]
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "src/audio/audio_config.h"
#include "src/perf/render_bench.h"

SerialMock Serial;
SDMock SD;

int main(int argc, char** argv) {
  const int blocks = argc > 1 ? std::atoi(argv[1]) : 400;
  RenderBench::runAll(
      static_cast<float>(kSampleRate), blocks,
      []() -> uint32_t {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch())
                                         .count());
      },
      nullptr, [](const char* line) { std::puts(line); });
  return 0;
}
