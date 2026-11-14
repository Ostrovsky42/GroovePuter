#pragma once
#ifndef GROOVEPUTER_SRC_UI_PERFORM_KEY_HEADER_H
#define GROOVEPUTER_SRC_UI_PERFORM_KEY_HEADER_H

// Pure helpers of the PERFORM surface (no gfx, no engine): the key line of
// the header and the EUCLID step test. Kept free of dependencies so a host
// test can run them without SDL.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace PerformKeyHeader {

// The pattern the EUCLID row draws: step `step` (0-based) of `pulses` hits in
// `length` steps, rotated. Same rule as the keyboard's pulse gate.
inline bool euclidStepHit(uint8_t step, uint8_t pulses, uint8_t length, uint8_t rotation) {
  if (length == 0) length = 1;
  if (pulses == 0 || pulses >= length) return true;
  const uint8_t rotated = static_cast<uint8_t>((step % length + rotation) % length);
  return static_cast<uint8_t>((rotated * pulses) % length) < pulses;
}

struct KeyHeaderInput {
  const char* root{"C"};
  const char* scale{""};
  int octaveShift{0};
  const char* chord{nullptr};  // nullptr or "OFF": no badge
  const char* arp{nullptr};    // nullptr: ARP off
  uint8_t ratchet{1};          // <= 1: no badge
  uint8_t euclidPulses{0};     // 0: no badge
  uint8_t euclidLength{16};
  uint8_t strumMs{0};          // 0: no badge
};

// "C DORIAN +0 [C:SCL3] [A:UP] [R:3] [E:5/16] [S:30]". The key and shift come
// first; a badge that would not fit in maxChars is left out whole. Returns the
// length written.
inline std::size_t formatKeyHeader(char* out, std::size_t size, const KeyHeaderInput& in,
                                   std::size_t maxChars) {
  if (out == nullptr || size == 0) return 0;
  const std::size_t limit = maxChars + 1 < size ? maxChars + 1 : size;
  out[0] = '\0';
  std::size_t len = 0;
  auto add = [&](const char* fmt, auto... args) {
    char piece[40];
    std::snprintf(piece, sizeof(piece), fmt, args...);
    const std::size_t n = std::strlen(piece);
    if (len + n + 1 > limit) return;
    std::memcpy(out + len, piece, n + 1);
    len += n;
  };
  add("%s %s %+d", in.root ? in.root : "", in.scale ? in.scale : "", in.octaveShift);
  if (in.chord && std::strcmp(in.chord, "OFF") != 0) add(" [C:%s]", in.chord);
  if (in.arp && std::strcmp(in.arp, "OFF") != 0) add(" [A:%s]", in.arp);
  if (in.ratchet > 1) add(" [R:%u]", static_cast<unsigned>(in.ratchet));
  if (in.euclidPulses > 0)
    add(" [E:%u/%u]", static_cast<unsigned>(in.euclidPulses), static_cast<unsigned>(in.euclidLength));
  if (in.strumMs > 0) add(" [S:%u]", static_cast<unsigned>(in.strumMs));
  return len;
}

}  // namespace PerformKeyHeader

#endif  // GROOVEPUTER_SRC_UI_PERFORM_KEY_HEADER_H
