#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MELODY_STORE_H
#define GROOVEPUTER_SRC_STATE_MELODY_STORE_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"

// M2a: the on-disk shape of a melody.
//
// Deliberately not a dump of RuntimeSynthEventBuffer. A struct dump makes the
// C++ layout the file ABI: add a field, change a compiler, and every saved
// melody becomes garbage that still passes a size check. Fields are written one
// at a time, little-endian, behind a header with magic, version, counts and an
// integrity check.
//
// Everything here is about not trusting the bytes. A melody that fails any
// check must leave the melody already in memory untouched -- a half-read file
// replacing good music is the worst outcome available at this boundary.
namespace MelodyStore {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

constexpr uint8_t kMagic[4] = {'G', 'P', 'M', 'L'};
constexpr uint16_t kVersion = 1;

// magic(4) version(2) headerSize(2) eventCount(2) lengthTicks(2) crc32(4)
constexpr size_t kHeaderBytes = 16;
// startTick(2) durationSubticks(2) note velocity probability flags fx fxParam
constexpr size_t kEventBytes = 10;

inline void put16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFFu));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

inline void put32(std::vector<uint8_t>& out, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFFu));
  }
}

inline uint16_t read16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

inline uint32_t read32(const uint8_t* p) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value |= static_cast<uint32_t>(p[i]) << (i * 8);
  return value;
}

inline uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

inline bool encode(const Buffer& melody, std::vector<uint8_t>& out) {
  // The runtime divides by lengthTicks in the onset path, so an illegal value
  // must never reach a file, let alone come back out of one.
  if (!RuntimePhraseEdit::validLengthTicks(melody.lengthTicks)) return false;
  if (melody.count > PhraseRuntime::kMaxSynthEvents) return false;

  std::vector<uint8_t> payload;
  payload.reserve(melody.count * kEventBytes);
  for (uint16_t i = 0; i < melody.count; ++i) {
    const auto& event = melody.events[i];
    put16(payload, event.startTick);
    put16(payload, event.durationSubticks);
    payload.push_back(event.note);
    payload.push_back(event.velocity);
    payload.push_back(event.probability);
    payload.push_back(event.flags);
    payload.push_back(event.fx);
    payload.push_back(event.fxParam);
  }

  out.clear();
  out.reserve(kHeaderBytes + payload.size());
  out.insert(out.end(), kMagic, kMagic + 4);
  put16(out, kVersion);
  put16(out, static_cast<uint16_t>(kHeaderBytes));
  put16(out, melody.count);
  put16(out, melody.lengthTicks);
  put32(out, crc32(payload.data(), payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  return true;
}

// Decodes into a scratch value first and only publishes on success, so a
// rejected file cannot leave the target half-written.
inline bool decode(const uint8_t* data, size_t length, Buffer& out) {
  if (data == nullptr || length < kHeaderBytes) return false;
  if (std::memcmp(data, kMagic, 4) != 0) return false;
  if (read16(data + 4) != kVersion) return false;
  const uint16_t headerBytes = read16(data + 6);
  if (headerBytes != kHeaderBytes) return false;

  const uint16_t count = read16(data + 8);
  const uint16_t lengthTicks = read16(data + 10);
  const uint32_t crc = read32(data + 12);

  if (count > PhraseRuntime::kMaxSynthEvents) return false;
  if (!RuntimePhraseEdit::validLengthTicks(lengthTicks)) return false;

  const size_t payloadBytes = static_cast<size_t>(count) * kEventBytes;
  if (length != kHeaderBytes + payloadBytes) return false;

  const uint8_t* payload = data + kHeaderBytes;
  if (crc32(payload, payloadBytes) != crc) return false;

  Buffer decoded{};
  decoded.lengthTicks = lengthTicks;
  decoded.count = count;
  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t* p = payload + static_cast<size_t>(i) * kEventBytes;
    auto& event = decoded.events[i];
    event.startTick = read16(p);
    event.durationSubticks = read16(p + 2);
    event.note = p[4];
    event.velocity = p[5];
    event.probability = p[6];
    event.flags = p[7];
    event.fx = p[8];
    event.fxParam = p[9];
  }

  // A file can be well-formed and still describe music the editor would refuse
  // to hold. Publishing it would put the runtime into a state its own rules
  // say cannot exist.
  if (!RuntimePhraseEdit::validate(decoded)) return false;

  out = decoded;
  return true;
}

}  // namespace MelodyStore

#endif  // GROOVEPUTER_SRC_STATE_MELODY_STORE_H
