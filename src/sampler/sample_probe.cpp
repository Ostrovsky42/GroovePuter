#include "sample_store.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <SD.h>
#define USE_SD_OPEN 1
#else
#include <cstdio>
#define USE_SD_OPEN 0
#endif

namespace {

struct WavRiffHeader {
  char riff[4];
  uint32_t totalSize;
  char wave[4];
};

struct WavFmtChunk {
  char fmt[4];
  uint32_t chunkSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

struct WavChunkHeader {
  char id[4];
  uint32_t size;
};

bool failProbe(
#if USE_SD_OPEN
    File& f,
#else
    FILE* f,
#endif
    const char* message) {
  if (message) printf("inspectWavFile: %s\n", message);
#if USE_SD_OPEN
  f.close();
#else
  fclose(f);
#endif
  return false;
}

}  // namespace

// Metadata-only sampler admission pass. This deliberately mirrors the current
// 0.9.3 loader format/chunk policy and does not allocate or read PCM payload.
// Broader RIFF traversal (including odd-chunk padding) remains a 0.9.5 task.
bool inspectWavFileBounded(const char* path, WavInfo& outInfo,
                           std::size_t maxDecodedBytes) {
  if (path == nullptr) return false;
  outInfo = {};

#if USE_SD_OPEN
  File f = SD.open(path, FILE_READ);
  if (!f) {
    printf("inspectWavFile: SD.open failed for %s\n", path);
    return false;
  }
  const uint64_t physicalSize = static_cast<uint64_t>(f.size());
#else
  FILE* f = fopen(path, "rb");
  if (!f) {
    printf("inspectWavFile: fopen failed for %s\n", path);
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) return failProbe(f, "seek-end failed");
  const long endPos = ftell(f);
  if (endPos < 0 || fseek(f, 0, SEEK_SET) != 0) {
    return failProbe(f, "file-size query failed");
  }
  const uint64_t physicalSize = static_cast<uint64_t>(endPos);
#endif

  WavRiffHeader riff{};
#if USE_SD_OPEN
  if (f.read(reinterpret_cast<uint8_t*>(&riff), sizeof(riff)) != sizeof(riff)) {
    return failProbe(f, "truncated RIFF header");
  }
#else
  if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) {
    return failProbe(f, "truncated RIFF header");
  }
#endif

  if (strncmp(riff.riff, "RIFF", 4) != 0 ||
      strncmp(riff.wave, "WAVE", 4) != 0) {
    return failProbe(f, "invalid RIFF/WAVE header");
  }

  bool fmtFound = false;
  bool dataFound = false;
  uint32_t dataSize = 0;
  uint64_t dataOffset = 0;
  WavFmtChunk fmt{};

#if USE_SD_OPEN
  while (!dataFound && f.available()) {
    WavChunkHeader header{};
    if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
      break;
    }

    if (strncmp(header.id, "fmt ", 4) == 0) {
      const std::size_t toRead =
          std::min<std::size_t>(header.size, sizeof(fmt) - 8);
      if (f.read(reinterpret_cast<uint8_t*>(&fmt.audioFormat), toRead) != toRead) break;
      if (header.size > toRead &&
          !f.seek(f.position() + static_cast<uint32_t>(header.size - toRead))) {
        break;
      }
      fmtFound = true;
    } else if (strncmp(header.id, "data", 4) == 0) {
      dataSize = header.size;
      dataOffset = static_cast<uint64_t>(f.position());
      dataFound = true;
    } else {
      if (!f.seek(f.position() + header.size)) break;
    }
  }
#else
  while (!dataFound && !feof(f)) {
    WavChunkHeader header{};
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      const std::size_t toRead =
          std::min<std::size_t>(header.size, sizeof(fmt) - 8);
      if (fread(&fmt.audioFormat, 1, toRead, f) != toRead) break;
      if (header.size > toRead &&
          fseek(f, static_cast<long>(header.size - toRead), SEEK_CUR) != 0) {
        break;
      }
      fmtFound = true;
    } else if (strncmp(header.id, "data", 4) == 0) {
      const long pos = ftell(f);
      if (pos < 0) break;
      dataSize = header.size;
      dataOffset = static_cast<uint64_t>(pos);
      dataFound = true;
    } else {
      if (fseek(f, static_cast<long>(header.size), SEEK_CUR) != 0) break;
    }
  }
#endif

  const uint32_t expectedBlockAlign =
      static_cast<uint32_t>(fmt.numChannels) * sizeof(int16_t);
  if (!fmtFound || !dataFound || fmt.audioFormat != 1 ||
      fmt.bitsPerSample != 16 ||
      (fmt.numChannels != 1 && fmt.numChannels != 2) ||
      fmt.sampleRate == 0 || fmt.blockAlign != expectedBlockAlign ||
      dataSize == 0 || dataSize % expectedBlockAlign != 0) {
    return failProbe(f, "format not supported");
  }

  if (dataOffset > physicalSize ||
      static_cast<uint64_t>(dataSize) > physicalSize - dataOffset) {
    return failProbe(f, "truncated data payload");
  }

  outInfo.sampleRate = fmt.sampleRate;
  outInfo.channels = 1;
  outInfo.bitsPerSample = 16;
  outInfo.numFrames = dataSize / expectedBlockAlign;

  if (outInfo.numFrames >
      std::numeric_limits<std::size_t>::max() / sizeof(int16_t)) {
    return failProbe(f, "decoded size overflow");
  }

  const std::size_t decodedBytes =
      static_cast<std::size_t>(outInfo.numFrames) * sizeof(int16_t);
  if (decodedBytes > maxDecodedBytes) {
    printf("inspectWavFile: decoded sample too large: %zu > %zu bytes\n",
           decodedBytes, maxDecodedBytes);
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

#if USE_SD_OPEN
  f.close();
#else
  fclose(f);
#endif
  return true;
}
