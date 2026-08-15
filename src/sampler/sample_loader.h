#pragma once

#include "sample_store.h"

#include <cstddef>
#include <cstdint>
#include <limits>

enum class WavLoadError : uint8_t {
  Ok = 0,
  InvalidArgument,
  OpenFailed,
  IoError,
  InvalidRiff,
  Truncated,
  MissingFmt,
  MissingData,
  UnsupportedEncoding,
  UnsupportedChannels,
  InvalidFormat,
  TooLarge,
  OutOfMemory,
  ChangedAfterInspect,
};

struct WavInspectResult {
  WavInfo info{};
  uint16_t sourceChannels = 0;
  uint32_t sourceDataBytes = 0;
  uint64_t dataOffset = 0;
  std::size_t decodedBytes = 0;
};

const char* wavLoadErrorName(WavLoadError error);

// Parse and validate WAV metadata without allocating PCM or reading the data
// payload. maxDecodedBytes is the caller's admission budget for mono PCM16.
bool inspectWavFileBounded(const char* path, WavInspectResult& out,
                           std::size_t maxDecodedBytes,
                           WavLoadError* error = nullptr);

// Decode a file that has already passed inspect/admission. The file is probed
// again before allocation; metadata changes fail closed rather than exceeding
// the admitted byte budget. Stereo is converted to mono in bounded chunks.
bool decodeWavFileBounded(const char* path, const WavInspectResult& inspected,
                          int16_t** outPcm, std::size_t maxDecodedBytes,
                          WavLoadError* error = nullptr);

// Compatibility wrappers retained for existing callers/tests.
bool inspectWavFileBounded(const char* path, WavInfo& outInfo,
                           std::size_t maxDecodedBytes);
bool loadWavFileBounded(const char* path, WavInfo& outInfo, int16_t** outPcm,
                        std::size_t maxDecodedBytes);
bool loadWavFile(const char* path, WavInfo& outInfo, int16_t** outPcm);
