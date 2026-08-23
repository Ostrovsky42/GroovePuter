#pragma once

#include <cstdint>

class MiniAcid;

namespace GroovePuterRhythm {

enum class F08ListenVariant : uint8_t {
  Old = 0,
  New,
};

struct F08ListenCaseInfo {
  const char* group = "";
  const char* mode = "";
  uint8_t ordinal = 0;
  char voice = 'A';
  const char* focus = "";
  const char* progression = "";
  const char* oldClock = "";
  const char* newClock = "";
  uint16_t bpm = 0;
  bool fingerprintChanged = false;
};

uint8_t f08ListenCaseCount();
F08ListenCaseInfo f08ListenCaseInfo(uint8_t index);
bool applyF08ListenCase(MiniAcid& engine,
                        uint8_t index,
                        F08ListenVariant variant);

}  // namespace GroovePuterRhythm
