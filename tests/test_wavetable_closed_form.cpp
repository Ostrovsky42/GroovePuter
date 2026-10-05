// Saw and square lookups are closed-form. They must equal, bit for bit, the
// tables they replaced, for every index and for phases inside each index cell.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/dsp/audio_wavetables.h"

static int g_failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_failures; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

static bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

int main() {
  // Reference: the exact expressions the removed tables were filled with.
  constexpr uint32_t kDuty = static_cast<uint32_t>(kWavetableSize * 0.3f);
  static float refSaw[kWavetableSize];
  static float refSquare[kWavetableSize];
  for (uint32_t i = 0; i < kWavetableSize; ++i) {
    refSaw[i] = 2.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize) - 1.0f;
    refSquare[i] = (i < kDuty) ? 1.0f : -1.0f;
  }
  for (uint32_t i = 0; i < kWavetableSize; ++i) {
    const uint32_t base = i << 22;
    const uint32_t offsets[] = {0u, 1u, 0x1FFFFFu, 0x3FFFFFu};
    for (uint32_t off : offsets) {
      CHECK(sameBits(Wavetable::lookupSaw(base + off), refSaw[i]), "saw index %u offset %u", i, off);
      CHECK(sameBits(Wavetable::lookupSquare(base + off), refSquare[i]), "square index %u offset %u", i, off);
    }
  }
  CHECK(kDuty == 307, "duty index %u", kDuty);
  CHECK(Wavetable::lookupSaw(0u) == -1.0f, "saw start");
  CHECK(Wavetable::lookupSquare(306u << 22) == 1.0f && Wavetable::lookupSquare(307u << 22) == -1.0f, "square edge");
  if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
  std::printf("wavetable closed-form: all %u indices bit-identical\n", kWavetableSize);
  return 0;
}
