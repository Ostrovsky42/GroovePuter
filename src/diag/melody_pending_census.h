#pragma once
#ifndef GROOVEPUTER_SRC_DIAG_MELODY_PENDING_CENSUS_H
#define GROOVEPUTER_SRC_DIAG_MELODY_PENDING_CENSUS_H

// Memory census for the NEXT-preparation buffer, M1 vs M2.
//
// The question this answers is narrow on purpose: what does *adding pending
// preparation* cost, on top of the per-voice working buffers P3 already pays
// for. Nothing here designs the feature -- no source migration, no Song
// reference, no key handling, no on-disk format. It allocates buffers with the
// lifetime the real thing would have and reads a payload of the real size.
//
//   M1  one shared pending buffer   +1284 B, A/B requests serialise
//   M2  one pending buffer per voice +2568 B, A and B can queue independently
//
// Built only when GROOVEPUTER_MELODY_CENSUS is defined; otherwise every entry
// point below compiles to nothing, so the product image is unchanged.

#if defined(GROOVEPUTER_MELODY_CENSUS)

#include <SD.h>
#include "esp_heap_caps.h"
#include "src/phrase/runtime_synth_events.h"

namespace MelodyCensus {

constexpr int kPendingBuffers = GROOVEPUTER_MELODY_CENSUS;   // 1 = M1, 2 = M2
constexpr size_t kPayloadBytes =
    sizeof(PhraseRuntime::RuntimeSynthEventBuffer);          // 1284
constexpr const char* kPayloadPath = "/melody_census.bin";
constexpr int kChurnCycles = 40;

inline uint32_t freeInternal() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
inline uint32_t largestInternal() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

inline void report(const char* state) {
  Serial.printf("[CENSUS] M%d %-22s free=%lu largest=%lu\n",
                kPendingBuffers, state,
                (unsigned long)freeInternal(), (unsigned long)largestInternal());
}

struct Census {
  PhraseRuntime::RuntimeSynthEventBuffer* pending[2] = {nullptr, nullptr};
  PhraseRuntime::RuntimeSynthEventBuffer* active[2] = {nullptr, nullptr};
  int allocations = 0;
  bool payloadReady = false;
  uint32_t loadMinUs = 0xFFFFFFFFu;
  uint32_t loadMaxUs = 0;
  int loadFailures = 0;
  int cycle = 0;
  bool done = false;
};

inline Census& state() {
  static Census census;
  return census;
}

// One payload of the real size, written once so the read path is measured
// against a real file rather than an empty one.
inline bool ensurePayload() {
  Census& c = state();
  if (c.payloadReady) return true;
  File out = SD.open(kPayloadPath, FILE_WRITE);
  if (!out) {
    Serial.println("[CENSUS] payload create FAILED (no card?)");
    return false;
  }
  PhraseRuntime::RuntimeSynthEventBuffer sample{};
  sample.lengthTicks = PhraseRuntime::kTicksPerBar;
  sample.count = PhraseRuntime::kMaxSynthEvents;      // worst case, 128 events
  for (uint16_t i = 0; i < sample.count; ++i) {
    sample.events[i].startTick = static_cast<uint16_t>(i * 3);
    sample.events[i].durationSubticks = 16;
    sample.events[i].note = static_cast<uint8_t>(48 + (i % 24));
    sample.events[i].velocity = 100;
    sample.events[i].probability = 100;
  }
  const size_t written = out.write(
      reinterpret_cast<const uint8_t*>(&sample), kPayloadBytes);
  out.close();
  c.payloadReady = written == kPayloadBytes;
  Serial.printf("[CENSUS] payload %s (%u bytes)\n",
                c.payloadReady ? "written" : "SHORT", (unsigned)written);
  return c.payloadReady;
}

// The allocation the real feature would make: once, at startup, fixed size.
// Addresses are printed so a later run can prove they did not move.
inline void allocatePending() {
  Census& c = state();
  for (int i = 0; i < kPendingBuffers; ++i) {
    c.pending[i] = reinterpret_cast<PhraseRuntime::RuntimeSynthEventBuffer*>(
        heap_caps_malloc(kPayloadBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (c.pending[i]) ++c.allocations;
    Serial.printf("[CENSUS] pending[%d] = %p\n", i, (void*)c.pending[i]);
  }
  // Stand-ins for the two per-voice working buffers, so the peak measured here
  // is the real one: both active, both pending, filesystem buffers, all live.
  for (int i = 0; i < 2; ++i) {
    c.active[i] = reinterpret_cast<PhraseRuntime::RuntimeSynthEventBuffer*>(
        heap_caps_malloc(kPayloadBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (c.active[i]) ++c.allocations;
  }
}

inline bool loadInto(PhraseRuntime::RuntimeSynthEventBuffer* target) {
  Census& c = state();
  if (!target) return false;
  const uint32_t began = micros();
  File in = SD.open(kPayloadPath, FILE_READ);
  if (!in) { ++c.loadFailures; return false; }
  const size_t read = in.read(reinterpret_cast<uint8_t*>(target), kPayloadBytes);
  in.close();
  const uint32_t took = micros() - began;
  if (took < c.loadMinUs) c.loadMinUs = took;
  if (took > c.loadMaxUs) c.loadMaxUs = took;
  if (read != kPayloadBytes) { ++c.loadFailures; return false; }
  return true;
}

// Activation is a value copy under the caller's guard -- no allocation, which
// is the property that makes a bar-boundary swap safe.
inline void activate(int voice) {
  Census& c = state();
  const int slot = kPendingBuffers == 1 ? 0 : voice;
  if (!c.pending[slot] || !c.active[voice]) return;
  *c.active[voice] = *c.pending[slot];
}

inline void tick(bool playing) {
  Census& c = state();

  // The whole sequence runs in milliseconds, which is long before a serial
  // monitor can attach after a flash. Wait, announce, and repeat the result --
  // otherwise the measurement exists and nobody ever sees it.
  static uint32_t lastBeat = 0;
  if (millis() < 20000u) {
    if (millis() - lastBeat > 2000u) {
      lastBeat = millis();
      Serial.printf("[CENSUS] M%d waiting, starts at 20s (now %lus)\n",
                    kPendingBuffers, (unsigned long)(millis() / 1000u));
    }
    return;
  }

  if (c.done) {
    if (millis() - lastBeat > 5000u) {
      lastBeat = millis();
      report("settled-repeat");
      Serial.printf("[CENSUS] M%d done: allocations=%d loadUs min=%lu max=%lu "
                    "failures=%d payload=%u\n",
                    kPendingBuffers, c.allocations,
                    (unsigned long)c.loadMinUs, (unsigned long)c.loadMaxUs,
                    c.loadFailures, (unsigned)kPayloadBytes);
    }
    return;
  }

  if (c.cycle == 0) {
    report("sd-mounted");
    if (!ensurePayload()) { c.done = true; return; }
    allocatePending();
    report("pending-allocated");
    if (loadInto(c.pending[0])) report("melody-A-loaded");
    if (kPendingBuffers == 2) {
      if (loadInto(c.pending[1])) report("melody-B-loaded");
      report("both-pending-ready");
    }
    activate(0);
    if (kPendingBuffers == 2) activate(1);
    report("after-activation");
    Serial.printf("[CENSUS] playing=%d allocations=%d\n",
                  playing ? 1 : 0, c.allocations);
    c.cycle = 1;
    return;
  }

  // Repeated switching: the ratchet check. If free or largest drifts down
  // across cycles rather than returning to a plateau, the design leaks.
  if (c.cycle <= kChurnCycles) {
    const int voice = c.cycle % 2;
    const int slot = kPendingBuffers == 1 ? 0 : voice;
    if (loadInto(c.pending[slot])) activate(voice);
    if (c.cycle % 10 == 0) {
      char label[24];
      snprintf(label, sizeof(label), "churn-%d", c.cycle);
      report(label);
    }
    ++c.cycle;
    return;
  }

  report("settled");
  Serial.printf("[CENSUS] M%d done: allocations=%d loadUs min=%lu max=%lu "
                "failures=%d payload=%u\n",
                kPendingBuffers, c.allocations,
                (unsigned long)c.loadMinUs, (unsigned long)c.loadMaxUs,
                c.loadFailures, (unsigned)kPayloadBytes);
  c.done = true;
}

}  // namespace MelodyCensus

#define MELODY_CENSUS_TICK(playing) MelodyCensus::tick(playing)
#else
#define MELODY_CENSUS_TICK(playing) ((void)0)
#endif

#endif  // GROOVEPUTER_SRC_DIAG_MELODY_PENDING_CENSUS_H
