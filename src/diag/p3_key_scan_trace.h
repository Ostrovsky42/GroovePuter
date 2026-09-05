#pragma once

// Raw keyboard-scan trace — DIAGNOSTIC IMAGE ONLY.
//
// Whether a held note key stays continuously present in the matrix scan has
// been assumed twice and measured never. Both the retrigger diagnosis and the
// hysteresis that replaced it with stuck notes rested on that assumption. This
// records what the scan actually reports.
//
// Only changes are printed, with a millisecond stamp: the loop runs at roughly
// 200 Hz and printing every pass would flood the port and distort the timing it
// is meant to measure. A clean press then release is two lines. Bounce is a
// burst, and the stamps give its duration.
//
// Deliberately reads the scan before any modifier gate, so it shows the matrix
// rather than what the note layer chooses to accept.

#if defined(P3_KEY_SCAN_TRACE)

namespace P3KeyScanTrace {

inline void observe(const Keyboard_Class::KeysState& keys) {
  uint32_t mask = 0;
  for (const auto hid : keys.hid_keys) {
    const uint8_t value = static_cast<uint8_t>(hid);
    if (value >= 0x04 && value <= 0x1D) {
      mask |= (1u << (value - 0x04));
    }
  }

  static uint32_t previousMask = 0;
  static bool seeded = false;
  if (!seeded) {
    seeded = true;
    previousMask = mask;
    return;
  }
  if (mask == previousMask) return;

  Serial.printf("[KEYSCAN] ms=%lu mask=0x%08lX count=%u\n",
                static_cast<unsigned long>(millis()),
                static_cast<unsigned long>(mask),
                static_cast<unsigned>(__builtin_popcount(mask)));
  previousMask = mask;
}

}  // namespace P3KeyScanTrace

#endif  // P3_KEY_SCAN_TRACE
