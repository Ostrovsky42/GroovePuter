#include "smf_export_writer.h"

#include <cstring>
#include <limits>

namespace GroovePuterMidi {
namespace {

void putBe16(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value >> 8);
  out[1] = static_cast<uint8_t>(value);
}

void putBe32(uint8_t* out, uint32_t value) {
  out[0] = static_cast<uint8_t>(value >> 24);
  out[1] = static_cast<uint8_t>(value >> 16);
  out[2] = static_cast<uint8_t>(value >> 8);
  out[3] = static_cast<uint8_t>(value);
}

std::size_t encodeVarLen(uint32_t value, uint8_t (&out)[4]) {
  if (value > 0x0FFFFFFFu) return 0;
  uint8_t reversed[4]{};
  std::size_t count = 0;
  reversed[count++] = static_cast<uint8_t>(value & 0x7Fu);
  value >>= 7;
  while (value != 0) {
    reversed[count++] = static_cast<uint8_t>((value & 0x7Fu) | 0x80u);
    value >>= 7;
  }
  for (std::size_t i = 0; i < count; ++i) out[i] = reversed[count - i - 1];
  return count;
}

}  // namespace

bool writeSmfHeader(ISmfExportByteSink& sink,
                    uint16_t trackCount,
                    uint16_t division) {
  if (trackCount == 0 || division == 0 || (division & 0x8000u) != 0) {
    return false;
  }
  const uint8_t header[] = {
      'M', 'T', 'h', 'd', 0, 0, 0, 6,
      0, 1, 0, 0, 0, 0,
  };
  uint8_t encoded[sizeof(header)];
  std::memcpy(encoded, header, sizeof(header));
  putBe16(encoded + 10, trackCount);
  putBe16(encoded + 12, division);
  return sink.write(encoded, sizeof(encoded));
}

bool writeSmfTrackHeader(ISmfExportByteSink& sink, uint32_t trackLength) {
  uint8_t header[] = {'M', 'T', 'r', 'k', 0, 0, 0, 0};
  putBe32(header + 4, trackLength);
  return sink.write(header, sizeof(header));
}

bool SmfExportTrackWriter::begin(ISmfExportByteSink& sink,
                                 const char* trackName) {
  if (begun_ || trackName == nullptr) return false;
  sink_ = &sink;
  begun_ = true;
  const std::size_t length = std::strlen(trackName);
  return writeText(0, 0x03, trackName, length);
}

bool SmfExportTrackWriter::writeBytes(const uint8_t* bytes,
                                      std::size_t length) {
  if (!good_ || !begun_ || finished_ || sink_ == nullptr ||
      (length != 0 && bytes == nullptr) ||
      length > std::numeric_limits<uint32_t>::max() - bytesWritten_) {
    good_ = false;
    return false;
  }
  if (length != 0 && !sink_->write(bytes, length)) {
    good_ = false;
    return false;
  }
  bytesWritten_ += static_cast<uint32_t>(length);
  return true;
}

bool SmfExportTrackWriter::writeDelta(uint32_t tick) {
  if (!good_ || !begun_ || finished_ || tick < lastTick_) {
    good_ = false;
    return false;
  }
  uint8_t delta[4]{};
  const std::size_t length = encodeVarLen(tick - lastTick_, delta);
  if (length == 0 || !writeBytes(delta, length)) {
    good_ = false;
    return false;
  }
  lastTick_ = tick;
  return true;
}

bool SmfExportTrackWriter::writeText(uint32_t tick, uint8_t metaType,
                                     const char* text,
                                     std::size_t length) {
  if (metaType < 0x01 || metaType > 0x0F ||
      (length != 0 && text == nullptr) || length > 0x0FFFFFFFu ||
      !writeDelta(tick)) {
    good_ = false;
    return false;
  }
  uint8_t prefix[2] = {0xFF, metaType};
  uint8_t lengthBytes[4]{};
  const std::size_t lengthSize =
      encodeVarLen(static_cast<uint32_t>(length), lengthBytes);
  if (lengthSize == 0 || !writeBytes(prefix, sizeof(prefix)) ||
      !writeBytes(lengthBytes, lengthSize)) return false;
  return length == 0 ||
      writeBytes(reinterpret_cast<const uint8_t*>(text), length);
}

bool SmfExportTrackWriter::writeTempo(uint32_t tick,
                                      uint32_t microsecondsPerQuarter) {
  if (microsecondsPerQuarter == 0 || microsecondsPerQuarter > 0xFFFFFFu ||
      !writeDelta(tick)) {
    good_ = false;
    return false;
  }
  const uint8_t bytes[] = {
      0xFF, 0x51, 0x03,
      static_cast<uint8_t>(microsecondsPerQuarter >> 16),
      static_cast<uint8_t>(microsecondsPerQuarter >> 8),
      static_cast<uint8_t>(microsecondsPerQuarter),
  };
  return writeBytes(bytes, sizeof(bytes));
}

bool SmfExportTrackWriter::writeTimeSignature(
    uint32_t tick, uint8_t numerator, uint8_t denominatorExponent,
    uint8_t clocksPerMetronome, uint8_t thirtySecondsPerQuarter) {
  if (numerator == 0 || denominatorExponent > 7 || !writeDelta(tick)) {
    good_ = false;
    return false;
  }
  const uint8_t bytes[] = {
      0xFF, 0x58, 0x04, numerator, denominatorExponent,
      clocksPerMetronome, thirtySecondsPerQuarter,
  };
  return writeBytes(bytes, sizeof(bytes));
}

bool SmfExportTrackWriter::writeChannelEvent(uint32_t tick, uint8_t status,
                                            uint8_t data1, uint8_t data2) {
  const uint8_t type = status & 0xF0u;
  if (status < 0x80u || status >= 0xF0u || data1 >= 0x80u ||
      data2 >= 0x80u || !writeDelta(tick)) {
    good_ = false;
    return false;
  }
  const uint8_t bytes[] = {status, data1, data2};
  return writeBytes(bytes, type == 0xC0u || type == 0xD0u ? 2 : 3);
}

bool SmfExportTrackWriter::finish(uint32_t endTick) {
  if (!good_ || !begun_ || finished_ || !writeDelta(endTick)) {
    good_ = false;
    return false;
  }
  const uint8_t endOfTrack[] = {0xFF, 0x2F, 0x00};
  if (!writeBytes(endOfTrack, sizeof(endOfTrack))) return false;
  finished_ = true;
  return true;
}

}  // namespace GroovePuterMidi
