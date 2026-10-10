#pragma once
#ifndef GROOVEPUTER_DSP_RENDER_SECTION_PROFILE_H
#define GROOVEPUTER_DSP_RENDER_SECTION_PROFILE_H

#include <atomic>
#include <cstdint>

#if defined(ARDUINO)
#include <Arduino.h>
#include <esp_cpu.h>
#else
#include <chrono>
#endif

// 0.9.19: where MiniAcid::generateAudioBuffer spends a block. The standalone
// render benchmark (src/perf/render_bench.h) showed the voices of 808 + two
// TB303 cost ~8 ms of the ~23 ms the engine takes in PLAY, so every section of
// the render loop is timed on every block. The clock is the CPU cycle counter
// (one instruction on the ESP32-S3, unlike micros() which cost a 47 ms block
// when it was read ten times per sample).
namespace RenderProfile {

inline uint32_t cycles() {
#if defined(ARDUINO)
  return static_cast<uint32_t>(esp_cpu_get_cycle_count());
#else
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
#endif
}

// Cycles per microsecond of cycles() (nanoseconds on the host).
inline uint32_t cyclesPerUs() {
#if defined(ARDUINO)
  return getCpuFrequencyMhz();
#else
  return 1000;
#endif
}

enum Section : uint8_t {
  Pre = 0,   // block setup before the sample loop
  Seq,       // tick advance, step triggers, note releases (playing only)
  Retrig,    // synth and drum retriggers
  Voices,    // both synths with their distortion and delay
  Drums,     // drum voices + drum bus (playing only), synth mix
  Sampler,
  Vocal,
  Tail,      // looper, tape FX, master chain, dither, output
  // Parts of Voices (each only while that synth is not muted):
  SynthA,    // Synth A process()
  FxA,       // its distortion + track volume + delay
  SynthB,
  FxB,
  Count
};

inline const char* sectionName(uint8_t s) {
  static const char* const kNames[Count] = {"pre", "seq", "retrig", "voices",
                                            "drums", "sampler", "vocal", "tail",
                                            "synthA", "fxA", "synthB", "fxB"};
  return s < Count ? kNames[s] : "?";
}

// The AudioTask publishes one block at a time; the UI thread takes a window.
// Totals are monotonic 32-bit counters (windows use differences); maxima are
// exchanged on take.
class SectionProfile {
 public:
  struct Window {
    uint32_t blocks = 0;
    uint32_t avgUs[Count] = {};
    uint32_t maxUs[Count] = {};
  };

  void publish(const uint32_t (&blockCycles)[Count]) {
    blocks_.fetch_add(1, std::memory_order_relaxed);
    for (uint8_t i = 0; i < Count; ++i) {
      sum_[i].fetch_add(blockCycles[i], std::memory_order_relaxed);
      uint32_t seen = max_[i].load(std::memory_order_relaxed);
      while (blockCycles[i] > seen &&
             !max_[i].compare_exchange_weak(seen, blockCycles[i],
                                            std::memory_order_relaxed)) {
      }
    }
  }

  Window take(uint32_t perUs) {
    Window w;
    if (perUs == 0) perUs = 1;
    const uint32_t blocks = blocks_.load(std::memory_order_relaxed);
    w.blocks = blocks - lastBlocks_;
    lastBlocks_ = blocks;
    for (uint8_t i = 0; i < Count; ++i) {
      const uint32_t sum = sum_[i].load(std::memory_order_relaxed);
      const uint32_t delta = sum - lastSum_[i];
      lastSum_[i] = sum;
      w.avgUs[i] = w.blocks ? delta / w.blocks / perUs : 0;
      w.maxUs[i] = max_[i].exchange(0, std::memory_order_relaxed) / perUs;
    }
    return w;
  }

 private:
  std::atomic<uint32_t> blocks_{0};
  std::atomic<uint32_t> sum_[Count] = {};
  std::atomic<uint32_t> max_[Count] = {};
  uint32_t lastBlocks_ = 0;  // UI thread only
  uint32_t lastSum_[Count] = {};
};

}  // namespace RenderProfile

#endif  // GROOVEPUTER_DSP_RENDER_SECTION_PROFILE_H
