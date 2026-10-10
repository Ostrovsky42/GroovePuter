#pragma once
#ifndef GROOVEPUTER_UI_UI_FRAME_PROBE_H
#define GROOVEPUTER_UI_UI_FRAME_PROBE_H

#include <atomic>
#include <cstdint>

// 0.9.19 UI frame probe. A UI frame on the Cardputer shares core 1 with the
// AudioTask, so its wall time mixes three things: work the UI itself does,
// time the AudioTask takes the core away, and time the UI waits for the audio
// mutation gate. Each stage of a frame is split into those three so a slow
// frame names its cause. Pure accumulation; the clocks are passed in.
namespace UiFrameProbe {

enum class Stage : uint8_t {
  Key = 0,     // syncProjectKey_
  Persist,     // servicePersistence_ (autosave, UI session)
  Style,       // syncVisualStyle_
  Paging,      // handlePaging_
  Song,        // serviceSongMaterial under the audio guard
  Background,  // skin background + tick
  Status,      // status snapshot
  Tick,        // page tick
  Draw,        // page draw
  Chrome,      // status chrome, footer, HUD, overlays, toast
  Flush,       // display flush
  Count
};

constexpr uint8_t kStageCount = static_cast<uint8_t>(Stage::Count);

inline const char* stageName(uint8_t stage) {
  static const char* const kNames[kStageCount] = {
      "key", "persist", "style", "paging", "song", "bg",
      "status", "tick", "draw", "chrome", "flush"};
  return stage < kStageCount ? kNames[stage] : "?";
}

// One reading of the clocks the probe splits time by. audioBusyUs and
// guardWaitUs are running totals; only their differences are used, so they
// may wrap.
struct Reading {
  uint32_t nowUs = 0;
  uint32_t audioBusyUs = 0;
  uint32_t guardWaitUs = 0;
};

struct Split {
  uint32_t wallUs = 0;
  uint32_t audioUs = 0;  // AudioTask render time that fell inside the span
  uint32_t guardUs = 0;  // waiting for the audio mutation gate
  // What the UI thread had for itself (never negative).
  uint32_t ownUs() const {
    const uint32_t taken = audioUs + guardUs;
    return wallUs > taken ? wallUs - taken : 0;
  }
};

inline Split between(const Reading& a, const Reading& b) {
  Split s;
  s.wallUs = b.nowUs - a.nowUs;
  s.audioUs = b.audioBusyUs - a.audioBusyUs;
  s.guardUs = b.guardWaitUs - a.guardWaitUs;
  // Readings are not atomic snapshots; keep the parts inside the wall time.
  if (s.audioUs > s.wallUs) s.audioUs = s.wallUs;
  if (s.guardUs > s.wallUs - s.audioUs) s.guardUs = s.wallUs - s.audioUs;
  return s;
}

struct Window {
  uint32_t frames = 0;
  uint64_t stageWallUs[kStageCount] = {};
  uint32_t stageMaxUs[kStageCount] = {};
  uint64_t wallUs = 0;
  uint64_t audioUs = 0;
  uint64_t guardUs = 0;
  uint32_t wallMaxUs = 0;
  // The slowest frame of the window, whole.
  Split worst;
  Split worstStages[kStageCount] = {};
};

class FrameAccumulator {
 public:
  void beginFrame(const Reading& r) {
    frameStart_ = r;
    last_ = r;
    for (auto& s : current_) s = Split{};
    open_ = true;
  }

  // Closes the span since the previous mark (or beginFrame) as `stage`.
  // A stage marked twice in a frame accumulates.
  void mark(Stage stage, const Reading& r) {
    if (!open_) return;
    const uint8_t i = static_cast<uint8_t>(stage);
    if (i >= kStageCount) return;
    const Split s = between(last_, r);
    current_[i].wallUs += s.wallUs;
    current_[i].audioUs += s.audioUs;
    current_[i].guardUs += s.guardUs;
    last_ = r;
  }

  void endFrame(const Reading& r) {
    if (!open_) return;
    open_ = false;
    const Split total = between(frameStart_, r);
    ++window_.frames;
    window_.wallUs += total.wallUs;
    window_.audioUs += total.audioUs;
    window_.guardUs += total.guardUs;
    for (uint8_t i = 0; i < kStageCount; ++i) {
      window_.stageWallUs[i] += current_[i].wallUs;
      if (current_[i].wallUs > window_.stageMaxUs[i]) {
        window_.stageMaxUs[i] = current_[i].wallUs;
      }
    }
    if (total.wallUs >= window_.wallMaxUs) {
      window_.wallMaxUs = total.wallUs;
      window_.worst = total;
      for (uint8_t i = 0; i < kStageCount; ++i) window_.worstStages[i] = current_[i];
    }
  }

  // Returns the window and starts a new one.
  Window take() {
    Window out = window_;
    window_ = Window{};
    return out;
  }

 private:
  Reading frameStart_{};
  Reading last_{};
  Split current_[kStageCount] = {};
  Window window_{};
  bool open_ = false;
};

// AudioTask render time as a running total that the UI thread can read at any
// moment: completed blocks plus the part of the block being rendered now.
class AudioBusyClock {
 public:
  void beginRender(uint32_t nowUs) {
    renderStartUs_.store(nowUs, std::memory_order_relaxed);
    rendering_.store(true, std::memory_order_release);
  }

  void endRender(uint32_t renderUs) {
    completedUs_.fetch_add(renderUs, std::memory_order_relaxed);
    rendering_.store(false, std::memory_order_release);
  }

  uint32_t busyUs(uint32_t nowUs) const {
    uint32_t busy = completedUs_.load(std::memory_order_relaxed);
    if (rendering_.load(std::memory_order_acquire)) {
      busy += nowUs - renderStartUs_.load(std::memory_order_relaxed);
    }
    return busy;
  }

 private:
  std::atomic<uint32_t> completedUs_{0};
  std::atomic<uint32_t> renderStartUs_{0};
  std::atomic<bool> rendering_{false};
};

// Where the display reads its clocks from; set once by the firmware. Unset
// readers read zero, so the probe still times stages on the host.
struct Sources {
  uint32_t (*audioBusyUs)(uint32_t nowUs) = nullptr;
  uint32_t (*guardWaitUs)() = nullptr;
};

inline Sources& sources() {
  static Sources s;
  return s;
}

}  // namespace UiFrameProbe

#endif  // GROOVEPUTER_UI_UI_FRAME_PROBE_H
