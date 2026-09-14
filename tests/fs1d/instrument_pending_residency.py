#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: instrument_pending_residency.py <source-root>")

root = Path(sys.argv[1])
ino_path = root / "GroovePuter.ino"


def replace_once(path: Path, anchor: str, replacement: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"FS1D instrumentation: expected one {label} anchor in {path}, found {count}"
        )
    path.write_text(text.replace(anchor, replacement, 1), encoding="utf-8")


state_anchor = "Encoder8Miniacid* g_encoder8 = nullptr;\n"
state_injection = r'''Encoder8Miniacid* g_encoder8 = nullptr;

// FS1D diagnostic-build-only observer. It reads already-resident production
// state and emits one line at a low rate. No history, candidate, load, staging,
// activation, or allocation is introduced by this probe.
static void logFs1dPendingResidency() {
  if (!g_miniAcid) return;

  const uintptr_t pendingA = reinterpret_cast<uintptr_t>(
      g_miniAcid->pendingMaterialAddress(0));
  const uintptr_t pendingB = reinterpret_cast<uintptr_t>(
      g_miniAcid->pendingMaterialAddress(1));
  const uint32_t freeInternal = static_cast<uint32_t>(
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  const uint32_t minimumFreeInternal = static_cast<uint32_t>(
      heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  const uint32_t largestInternal = static_cast<uint32_t>(
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  // This is deliberately the production AudioTask watermark. Sampling the
  // loop-task watermark would measure the diagnostic printf itself on later
  // samples and contaminate the stack observation.
  const UBaseType_t taskStackHwmBytes = g_audioTaskHandle
      ? uxTaskGetStackHighWaterMark(g_audioTaskHandle)
      : 0;
  const uint32_t audioUnderruns =
      g_miniAcid->perfStats.audioUnderruns.load(std::memory_order_relaxed);
  const uint32_t nowMs = millis();

  Serial.printf(
      "[FS1D_SAMPLE] ms=%u playing=%u "
      "pendingA_present=%u pendingB_present=%u "
      "pendingA_address=0x%08lx pendingB_address=0x%08lx "
      "pending_distinct=%u free_internal=%u minimum_free_internal=%u "
      "largest_internal=%u task_stack_hwm_bytes=%u audio_underruns=%u\n",
      (unsigned)nowMs,
      (unsigned)(g_miniAcid->isPlaying() ? 1 : 0),
      (unsigned)(pendingA != 0 ? 1 : 0),
      (unsigned)(pendingB != 0 ? 1 : 0),
      (unsigned long)pendingA,
      (unsigned long)pendingB,
      (unsigned)(pendingA != 0 && pendingB != 0 && pendingA != pendingB ? 1 : 0),
      (unsigned)freeInternal,
      (unsigned)minimumFreeInternal,
      (unsigned)largestInternal,
      (unsigned)taskStackHwmBytes,
      (unsigned)audioUnderruns);
}

static void pollFs1dPendingResidency() {
  static uint32_t lastSampleMs = 0;
  const uint32_t nowMs = millis();
  // Leave boot/setup alone, then keep telemetry sparse enough that the observer
  // does not become the workload being measured.
  if (nowMs < 10000u) return;
  if (lastSampleMs != 0u &&
      static_cast<int32_t>(nowMs - lastSampleMs) < 15000) return;
  lastSampleMs = nowMs;
  logFs1dPendingResidency();
}
'''
replace_once(ino_path, state_anchor, state_injection, "global state")

loop_anchor = '''  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
  M5Cardputer.update();'''
loop_injection = '''  MELODY_CENSUS_TICK(g_miniAcid && g_miniAcid->isPlaying());
  M5Cardputer.update();
  pollFs1dPendingResidency();'''
replace_once(ino_path, loop_anchor, loop_injection, "loop observer")
