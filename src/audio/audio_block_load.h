#pragma once
#ifndef GROOVEPUTER_AUDIO_AUDIO_BLOCK_LOAD_H
#define GROOVEPUTER_AUDIO_AUDIO_BLOCK_LOAD_H

#include <atomic>
#include <cstdint>

// 0.9.19: render time of every audio block, for the [AUDIO-BLOCK] line. The
// average load hides what starves the UI: runs of consecutive blocks that
// take the whole block period. The AudioTask records, the UI thread takes a
// window; no locks, the counters are monotonic and the maxima are exchanged.
class AudioBlockLoad {
 public:
  struct Window {
    uint32_t blocks = 0;
    uint64_t renderUs = 0;
    uint32_t maxUs = 0;
    uint32_t overBudget = 0;   // blocks that took longer than the block period
    uint32_t longestRun = 0;   // most consecutive over-budget blocks
    uint32_t avgUs() const {
      return blocks ? static_cast<uint32_t>(renderUs / blocks) : 0;
    }
  };

  // AudioTask only.
  void record(uint32_t renderUs, uint32_t budgetUs) {
    blocks_.fetch_add(1, std::memory_order_relaxed);
    renderUs_.fetch_add(renderUs, std::memory_order_relaxed);
    raise(maxUs_, renderUs);
    if (renderUs > budgetUs) {
      overBudget_.fetch_add(1, std::memory_order_relaxed);
      ++run_;
      raise(longestRun_, run_);
    } else {
      run_ = 0;
    }
  }

  // UI thread only.
  Window take() {
    Window w;
    const uint32_t blocks = blocks_.load(std::memory_order_relaxed);
    const uint32_t render = renderUs_.load(std::memory_order_relaxed);
    const uint32_t over = overBudget_.load(std::memory_order_relaxed);
    w.blocks = blocks - lastBlocks_;
    w.renderUs = render - lastRenderUs_;
    w.overBudget = over - lastOverBudget_;
    w.maxUs = maxUs_.exchange(0, std::memory_order_relaxed);
    w.longestRun = longestRun_.exchange(0, std::memory_order_relaxed);
    lastBlocks_ = blocks;
    lastRenderUs_ = render;
    lastOverBudget_ = over;
    return w;
  }

 private:
  static void raise(std::atomic<uint32_t>& target, uint32_t value) {
    uint32_t seen = target.load(std::memory_order_relaxed);
    while (value > seen &&
           !target.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {
    }
  }

  std::atomic<uint32_t> blocks_{0};
  std::atomic<uint32_t> renderUs_{0};  // wraps; windows use differences
  std::atomic<uint32_t> overBudget_{0};
  std::atomic<uint32_t> maxUs_{0};
  std::atomic<uint32_t> longestRun_{0};
  uint32_t run_ = 0;  // AudioTask only

  uint32_t lastBlocks_ = 0;  // UI thread only
  uint32_t lastRenderUs_ = 0;
  uint32_t lastOverBudget_ = 0;
};

#endif  // GROOVEPUTER_AUDIO_AUDIO_BLOCK_LOAD_H
