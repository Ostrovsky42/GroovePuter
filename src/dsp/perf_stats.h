#pragma once

#include <atomic>
#include <cstdint>

// Cross-core telemetry published by the audio task and sampled by UI/control
// code. Every field is atomic so the odd/even sequence protocol does not rely
// on undefined concurrent access to non-atomic payload values.
struct PerfStats {
  std::atomic<uint32_t> seq{0};  // odd = writer active, even = stable snapshot
  std::atomic<uint32_t> audioUnderruns{0};
  std::atomic<float> cpuAudioPctIdeal{0.0f};
  std::atomic<float> cpuAudioPctActual{0.0f};
  std::atomic<float> cpuAudioPeakPct{0.0f};
  std::atomic<uint32_t> dspTimeUs{0};

  std::atomic<uint32_t> dspVoicesUs{0};
  std::atomic<uint32_t> dspDrumsUs{0};
  std::atomic<uint32_t> dspFxUs{0};
  std::atomic<uint32_t> dspSamplerUs{0};

  std::atomic<uint32_t> heapFree{0};
  std::atomic<uint32_t> heapMinFree{0};
  std::atomic<uint32_t> lastCallbackMicros{0};

  void beginWrite() {
    seq.fetch_add(1, std::memory_order_acq_rel);
  }

  void endWrite() {
    seq.fetch_add(1, std::memory_order_release);
  }
};
