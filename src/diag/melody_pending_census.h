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
#include "src/state/undo_owner.h"

namespace MelodyCensus {

constexpr int kPendingBuffers = GROOVEPUTER_MELODY_CENSUS;   // 1 = M1, 2 = M2
constexpr size_t kPayloadBytes =
    sizeof(PhraseRuntime::RuntimeSynthEventBuffer);          // 1284
constexpr const char* kPayloadDirectory = "/__gp_diag";
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
  int allocationFailures = 0;
  bool payloadReady = false;
  bool payloadOwned = false;
  char payloadPath[48]{};
  uint32_t loadMinUs = 0xFFFFFFFFu;
  uint32_t loadMaxUs = 0;
  int loadFailures = 0;
  int cycle = 0;
  bool done = false;
  bool cleanupSucceeded = false;
  bool completed = false;
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
  if (!SD.exists(kPayloadDirectory) && !SD.mkdir(kPayloadDirectory)) {
    Serial.println("[CENSUS] diagnostic directory create FAILED");
    return false;
  }
  for (unsigned suffix = 0; suffix < 100; ++suffix) {
    snprintf(c.payloadPath, sizeof(c.payloadPath),
             "%s/lm-%02u.bin", kPayloadDirectory, suffix);
    if (!SD.exists(c.payloadPath)) break;
    c.payloadPath[0] = '\0';
  }
  if (c.payloadPath[0] == '\0') {
    Serial.println("[CENSUS] no unused diagnostic payload name");
    return false;
  }
  File out = SD.open(c.payloadPath, FILE_WRITE);
  if (!out) {
    Serial.println("[CENSUS] payload create FAILED (no card?)");
    return false;
  }
  c.payloadOwned = true;
  PhraseRuntime::RuntimeSynthEventBuffer sample{};
  sample.lengthTicks = 8u * PhraseRuntime::kTicksPerBar;
  sample.count = PhraseRuntime::kMaxSynthEvents;      // worst case, 128 events
  for (uint16_t i = 0; i < sample.count; ++i) {
    sample.events[i].startTick = static_cast<uint16_t>(i * 24u);
    sample.events[i].durationSubticks = 16;
    sample.events[i].note = static_cast<uint8_t>(48 + (i % 24));
    sample.events[i].velocity = 100;
    sample.events[i].probability = 100;
  }
  const size_t written = out.write(
      reinterpret_cast<const uint8_t*>(&sample), kPayloadBytes);
  out.close();
  c.payloadReady = written == kPayloadBytes;
  Serial.printf("[CENSUS] payload %s path=%s (%u bytes)\n",
                c.payloadReady ? "written" : "SHORT", c.payloadPath,
                (unsigned)written);
  if (!c.payloadReady) {
    c.payloadOwned = !SD.remove(c.payloadPath);
  }
  return c.payloadReady;
}

// Fixed-size diagnostic allocations remain resident through the churn run.
// The repeated free/largest reports show whether other activity ratchets heap.
inline PhraseRuntime::RuntimeSynthEventBuffer* allocateBuffer() {
  Census& c = state();
  auto* buffer = reinterpret_cast<PhraseRuntime::RuntimeSynthEventBuffer*>(
      heap_caps_malloc(kPayloadBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (buffer) ++c.allocations;
  else ++c.allocationFailures;
  return buffer;
}

inline bool loadInto(PhraseRuntime::RuntimeSynthEventBuffer* target) {
  Census& c = state();
  if (!target) return false;
  const uint32_t began = micros();
  File in = SD.open(c.payloadPath, FILE_READ);
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

inline void finish(bool completed) {
  Census& c = state();
  c.cleanupSucceeded = !c.payloadOwned || SD.remove(c.payloadPath);
  c.completed = completed && c.cleanupSucceeded;
  c.done = true;
  Serial.printf("[CENSUS] M%d %s: allocations=%d allocFailures=%d "
                "loadUs min=%lu max=%lu failures=%d payload=%u cleanup=%s\n",
                kPendingBuffers, c.completed ? "done" : "FAILED",
                c.allocations, c.allocationFailures,
                (unsigned long)(c.loadMinUs == 0xFFFFFFFFu ? 0 : c.loadMinUs),
                (unsigned long)c.loadMaxUs, c.loadFailures,
                (unsigned)kPayloadBytes,
                c.cleanupSucceeded ? "removed" : "FAILED");
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
      Serial.printf("[CENSUS] M%d %s: allocations=%d allocFailures=%d "
                    "loadUs min=%lu max=%lu failures=%d payload=%u cleanup=%s\n",
                    kPendingBuffers, c.completed ? "done" : "FAILED",
                    c.allocations, c.allocationFailures,
                    (unsigned long)(c.loadMinUs == 0xFFFFFFFFu ? 0 : c.loadMinUs),
                    (unsigned long)c.loadMaxUs, c.loadFailures,
                    (unsigned)kPayloadBytes,
                    c.cleanupSucceeded ? "removed" : "FAILED");
    }
    return;
  }

  if (c.cycle == 0) {
    report("sd-mounted");
    if (!ensurePayload()) { finish(false); return; }
    // These fixed-size diagnostic buffers are additive stand-ins for the
    // experimental active/pending residency. Product working material, Undo,
    // audio state, and the mounted filesystem remain live alongside them.
    c.active[0] = allocateBuffer();
    report(c.active[0] ? "A-active" : "A-active-alloc-fail");
    if (!c.active[0]) { finish(false); return; }
    *c.active[0] = PhraseRuntime::RuntimeSynthEventBuffer{};
    c.active[0]->lengthTicks = 8u * PhraseRuntime::kTicksPerBar;
    c.pending[0] = allocateBuffer();
    if (!c.pending[0] || !loadInto(c.pending[0])) {
      report("pending-A-fail");
      finish(false);
      return;
    }
    report("A-active+pending-A");
    activate(0);
    c.active[1] = allocateBuffer();
    if (c.active[1]) *c.active[1] = *c.active[0];
    report(c.active[1] ? "A+B-active" : "B-active-alloc-fail");
    if (!c.active[1]) { finish(false); return; }
    if (kPendingBuffers == 2) {
      c.pending[1] = allocateBuffer();
      if (!c.pending[1] || !loadInto(c.pending[1])) {
        report("pending-B-fail");
        finish(false);
        return;
      }
      report("A+B-active+pending-A+B");
      activate(1);
    }
    report("residency-ready");
    Serial.printf("[CENSUS] playing=%d allocations=%d allocFailures=%d "
                  "payload=%u undoObjectBytes=%u undoCapacity=%u\n",
                  playing ? 1 : 0, c.allocations, c.allocationFailures,
                  (unsigned)kPayloadBytes,
                  (unsigned)sizeof(GroovePuterUndo::UndoOwner),
                  (unsigned)GroovePuterUndo::UndoOwner::payloadCapacity());
    c.cycle = 1;
    return;
  }

  // Repeated switching: the ratchet check. If free or largest drifts down
  // across cycles rather than returning to a plateau, the design leaks.
  if (c.cycle <= kChurnCycles) {
    const int voice = c.cycle % 2;
    const int slot = kPendingBuffers == 1 ? 0 : voice;
    if (!loadInto(c.pending[slot])) {
      report("churn-load-fail");
      finish(false);
      return;
    }
    activate(voice);
    if (c.cycle % 10 == 0) {
      char label[24];
      snprintf(label, sizeof(label), "churn-%d", c.cycle);
      report(label);
    }
    ++c.cycle;
    return;
  }

  report("settled");
  finish(true);
}

}  // namespace MelodyCensus

#define MELODY_CENSUS_TICK(playing) MelodyCensus::tick(playing)
#else
#define MELODY_CENSUS_TICK(playing) ((void)0)
#endif

#endif  // GROOVEPUTER_SRC_DIAG_MELODY_PENDING_CENSUS_H
