// keyboard_v2 record + migration from keyboard_v1. Host-only, no allocation.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "src/input/performance_keyboard_config.h"

static std::size_t g_allocs = 0;
void* operator new(std::size_t n) {
  ++g_allocs;
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using Codec = PerformanceKeyboardConfigCodec;

static uint16_t crc16(const uint8_t* b, std::size_t n) {
  uint16_t crc = 0xffffu;
  for (std::size_t i = 0; i < n; ++i) {
    crc ^= static_cast<uint16_t>(b[i]) << 8u;
    for (int k = 0; k < 8; ++k)
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1u) ^ 0x1021u)
                            : static_cast<uint16_t>(crc << 1u);
  }
  return crc;
}

static void reseal(uint8_t* rec) {
  const uint16_t c = crc16(rec, Codec::kV2RecordSize - 2u);
  rec[34] = static_cast<uint8_t>(c & 0xffu);
  rec[35] = static_cast<uint8_t>(c >> 8u);
}

static PerformanceKeyboardConfig sample() {
  PerformanceKeyboardConfig c{};
  c.role = GroovePuterPerformance::PerformanceRole::Lead;
  c.arpPitchSource = GroovePuterPerformance::ArpPitchSource::Held;
  c.arpDirection = PerformanceArpDirection::DownUp;
  c.arpRate = PerformanceRate::ThirtySecond;
  c.arpOctaves = 3;
  c.gatePercent = 87;
  c.chordInversion = 2;
  c.rootPitchClass = 7;
  c.velocity = 112;
  c.octaveShift = -1;
  c.arpEnabled = true;
  c.latchEnabled = true;
  c.ratchetCount = 3;
  c.euclideanLength = 12;
  c.euclideanPulses = 5;
  c.euclideanRotation = 4;
  c.strumDirection = PerformanceStrumDirection::LowToHigh;
  c.target = MusicalEventTarget::SynthA;
  c.mutationSeed = 0xdeadbeefu;
  c.mutationSkipPercent = 10;
  c.mutationOctaveJumpPercent = 20;
  c.mutationDeviatePercent = 30;
  assert(validPerformanceKeyboardConfig(c));
  return c;
}

int main() {
  static_assert(Codec::kV2RecordSize == 36, "v2 record size");
  static_assert(Codec::kV2Version != Codec::kCurrentVersion, "distinct versions");
  const std::size_t allocsBefore = g_allocs;

  const PerformanceKeyboardConfig cfg = sample();

  // v2 round trip without any v1 present.
  uint8_t v2[Codec::kV2RecordSize]{};
  assert(Codec::encodeV2(cfg, v2, sizeof(v2)));
  assert(v2[0] == Codec::kV2Version);
  assert(!Codec::encodeV2(cfg, v2, sizeof(v2) - 1));
  PerformanceKeyboardConfig out{};
  assert(Codec::decodeV2(v2, sizeof(v2), out));
  assert(performanceKeyboardConfigEqual(out, cfg));
  PerformanceKeyboardConfig viaResolve{};
  assert(resolvePersistedKeyboardConfig(v2, sizeof(v2), nullptr, 0, viaResolve) ==
         PerformanceKeyboardConfigSource::V2);
  assert(performanceKeyboardConfigEqual(viaResolve, cfg));

  // v1 record (old key) migrates with every field preserved; v1 is not v2.
  uint8_t v1[Codec::kCurrentRecordSize]{};
  assert(Codec::encode(cfg, v1, sizeof(v1)));
  assert(v1[0] == Codec::kCurrentVersion);
  assert(!Codec::decodeV2(v1, sizeof(v1), out));
  PerformanceKeyboardConfig migrated{};
  assert(resolvePersistedKeyboardConfig(nullptr, 0, v1, sizeof(v1), migrated) ==
         PerformanceKeyboardConfigSource::MigratedV1);
  assert(performanceKeyboardConfigEqual(migrated, cfg));
  // ... and the re-encoded v2 of the migrated value reads back identical.
  uint8_t re[Codec::kV2RecordSize]{};
  assert(Codec::encodeV2(migrated, re, sizeof(re)));
  assert(Codec::decodeV2(re, sizeof(re), out));
  assert(performanceKeyboardConfigEqual(out, cfg));

  // Older v1 payloads (18 and 21 bytes) still migrate; missing fields get
  // v1-equivalent defaults.
  const uint8_t legacy18[18] = {2, 3, 2, 2, 3, 5, 4, 87, 9, 2, 1, 0, 7, 4, 112,
                                static_cast<uint8_t>(-1), 1, 1};
  PerformanceKeyboardConfig old{};
  assert(resolvePersistedKeyboardConfig(nullptr, 0, legacy18, sizeof(legacy18), old) ==
         PerformanceKeyboardConfigSource::MigratedV1);
  const PerformanceKeyboardConfig d{};
  assert(old.ratchetCount == d.ratchetCount && old.strumMs == d.strumMs &&
         old.mutationSeed == d.mutationSeed && old.voiceMode == d.voiceMode &&
         old.euclideanLength == d.euclideanLength);

  // v2 wins over v1 when both exist.
  PerformanceKeyboardConfig other = cfg;
  other.velocity = 64;
  uint8_t v1other[Codec::kCurrentRecordSize]{};
  assert(Codec::encode(other, v1other, sizeof(v1other)));
  PerformanceKeyboardConfig both{};
  assert(resolvePersistedKeyboardConfig(v2, sizeof(v2), v1other, sizeof(v1other),
                                        both) == PerformanceKeyboardConfigSource::V2);
  assert(both.velocity == cfg.velocity);
  // A corrupt v2 falls through to v1.
  uint8_t bad[Codec::kV2RecordSize];
  std::memcpy(bad, v2, sizeof(bad));
  bad[10] ^= 0x55u;
  assert(resolvePersistedKeyboardConfig(bad, sizeof(bad), v1other, sizeof(v1other),
                                        both) == PerformanceKeyboardConfigSource::MigratedV1);
  assert(both.velocity == 64);
  // Nothing stored.
  PerformanceKeyboardConfig untouched = cfg;
  assert(resolvePersistedKeyboardConfig(nullptr, 0, nullptr, 0, untouched) ==
         PerformanceKeyboardConfigSource::None);
  assert(performanceKeyboardConfigEqual(untouched, cfg));

  // Unknown values in a valid (CRC-correct) v2 record -> field default, no crash.
  uint8_t fwd[Codec::kV2RecordSize];
  std::memcpy(fwd, v2, sizeof(fwd));
  fwd[5] = 200;   // arpDirection: unknown enum
  fwd[11] = 200;  // chordSpread: unknown enum
  fwd[19] = 7;    // ratchetCount beyond this firmware's range
  reseal(fwd);
  PerformanceKeyboardConfig tolerant{};
  assert(Codec::decodeV2(fwd, sizeof(fwd), tolerant));
  assert(tolerant.arpDirection == d.arpDirection);
  assert(tolerant.chordSpread == d.chordSpread);
  assert(tolerant.ratchetCount == d.ratchetCount);
  assert(tolerant.velocity == cfg.velocity && tolerant.rootPitchClass == cfg.rootPitchClass &&
         tolerant.mutationSeed == cfg.mutationSeed && tolerant.arpOctaves == cfg.arpOctaves);
  assert(validPerformanceKeyboardConfig(tolerant));
  // Structural garbage (bad CRC, wrong version, wrong size, out-of-range number)
  // is rejected, leaving the destination untouched.
  PerformanceKeyboardConfig keep = cfg;
  uint8_t wrongVer[Codec::kV2RecordSize];
  std::memcpy(wrongVer, v2, sizeof(wrongVer));
  wrongVer[0] = 9;
  reseal(wrongVer);
  assert(!Codec::decodeV2(wrongVer, sizeof(wrongVer), keep));
  assert(!Codec::decodeV2(v2, sizeof(v2) - 1, keep));
  assert(!Codec::decodeV2(nullptr, 0, keep));
  uint8_t range[Codec::kV2RecordSize];
  std::memcpy(range, v2, sizeof(range));
  range[7] = 9;  // arpOctaves 9 > 4
  reseal(range);
  assert(!Codec::decodeV2(range, sizeof(range), keep));
  assert(performanceKeyboardConfigEqual(keep, cfg));

  // Fixed-size, allocation-free.
  assert(sizeof(v2) == 36);
  assert(g_allocs == allocsBefore);
  return 0;
}
