#pragma once

// Diagnostic image only (-DGROOVEPUTER_SAVE_PROBE, never set by default): one line per Save,
// Save As, Load and autosave with the audio-guard hold, the whole call, heap and stack. Units: us.
// hold_us is the time spent inside the guarded callback (the audio task cannot run meanwhile);
// total_us also includes waiting for the guard. stackMinFreeBytes is the minimum free since task start.
#if defined(GROOVEPUTER_SAVE_PROBE) && defined(ARDUINO_M5STACK_CARDPUTER)
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace SaveProbe {
struct Window {
  const char* op;
  bool playing;
  uint32_t startUs, holdStartUs = 0, holdUs = 0;
  uint32_t freeBefore, largestBefore;
  Window(const char* o, bool p) : op(o), playing(p), startUs(micros()) {
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    freeBefore = heap_caps_get_free_size(caps);
    largestBefore = heap_caps_get_largest_free_block(caps);
  }
  void holdBegin() { holdStartUs = micros(); }
  void holdEnd() { holdUs = micros() - holdStartUs; }
  void report(bool ok) const {
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    Serial.printf(
        "[SAVE-PROBE] op=%s ok=%d playing=%d hold_us=%lu total_us=%lu "
        "stackMinFreeBytes=%lu internalFree before=%lu after=%lu largestBlock before=%lu after=%lu "
        "minEverFree=%lu\n",
        op, ok ? 1 : 0, playing ? 1 : 0, static_cast<unsigned long>(holdUs),
        static_cast<unsigned long>(micros() - startUs),
        static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)),
        static_cast<unsigned long>(freeBefore),
        static_cast<unsigned long>(heap_caps_get_free_size(caps)),
        static_cast<unsigned long>(largestBefore),
        static_cast<unsigned long>(heap_caps_get_largest_free_block(caps)),
        static_cast<unsigned long>(heap_caps_get_minimum_free_size(caps)));
  }
};
}  // namespace SaveProbe
#define SAVE_PROBE_BEGIN(op, playing) SaveProbe::Window saveProbe_((op), (playing));
#define SAVE_PROBE_HOLD_BEGIN() saveProbe_.holdBegin()
#define SAVE_PROBE_HOLD_END() saveProbe_.holdEnd()
#define SAVE_PROBE_END(ok) saveProbe_.report((ok))
#else
#define SAVE_PROBE_BEGIN(op, playing)
#define SAVE_PROBE_HOLD_BEGIN() ((void)0)
#define SAVE_PROBE_HOLD_END() ((void)0)
#define SAVE_PROBE_END(ok) ((void)0)
#endif
