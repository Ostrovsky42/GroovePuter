#pragma once

#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

class ISmfExportByteSink {
 public:
  virtual ~ISmfExportByteSink() = default;
  virtual bool write(const uint8_t* data, std::size_t length) = 0;
};

bool writeSmfHeader(ISmfExportByteSink& sink,
                    uint16_t trackCount,
                    uint16_t division);
bool writeSmfTrackHeader(ISmfExportByteSink& sink, uint32_t trackLength);

class SmfExportTrackWriter {
 public:
  bool begin(ISmfExportByteSink& sink, const char* trackName);
  bool writeText(uint32_t tick, uint8_t metaType,
                 const char* text, std::size_t length);
  bool writeTempo(uint32_t tick, uint32_t microsecondsPerQuarter);
  bool writeTimeSignature(uint32_t tick, uint8_t numerator,
                          uint8_t denominatorExponent,
                          uint8_t clocksPerMetronome,
                          uint8_t thirtySecondsPerQuarter);
  bool writeChannelEvent(uint32_t tick, uint8_t status,
                         uint8_t data1, uint8_t data2);
  bool finish(uint32_t endTick);

  bool good() const { return good_; }
  uint32_t lastTick() const { return lastTick_; }
  uint32_t bytesWritten() const { return bytesWritten_; }

 private:
  bool writeDelta(uint32_t tick);
  bool writeBytes(const uint8_t* bytes, std::size_t length);

  ISmfExportByteSink* sink_{nullptr};
  uint32_t lastTick_{0};
  uint32_t bytesWritten_{0};
  bool begun_{false};
  bool finished_{false};
  bool good_{true};
};

}  // namespace GroovePuterMidi
