#include "../audio/audio_config.h"
#include "sample_store.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_heap_caps.h>
#include <SD.h>
#define SAMPLE_MALLOC_PSRAM(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define SAMPLE_MALLOC_DRAM(size) heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define USE_SD_OPEN 1
#else
#define SAMPLE_MALLOC_PSRAM(size) malloc(size)
#define SAMPLE_MALLOC_DRAM(size) malloc(size)
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

struct WavProbeResult {
  WavInfo info{};
  uint16_t sourceChannels = 0;
  uint32_t dataBytes = 0;
  uint64_t dataOffset = 0;
  std::size_t decodedBytes = 0;
};

// Shared 0.9.3 metadata parser used by both the allocation-free probe and the
// bounded decoder. It intentionally preserves the current loader's accepted
// RIFF subset. Full odd-chunk padding/order hardening remains 0.9.5-A.
bool probeWavMetadata(const char* path, WavProbeResult& result,
                      std::size_t maxDecodedBytes) {
  if (path == nullptr) return false;
  result = {};

#if USE_SD_OPEN
  File f = SD.open(path, FILE_READ);
  if (!f) {
    printf("loadWavFile: SD.open failed for %s\n", path);
    return false;
  }
  const uint64_t physicalSize = static_cast<uint64_t>(f.size());
  auto closeFile = [&]() { f.close(); };
#else
  FILE* f = fopen(path, "rb");
  if (!f) {
    printf("loadWavFile: fopen failed for %s\n", path);
    return false;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  const long endPos = ftell(f);
  if (endPos < 0 || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  const uint64_t physicalSize = static_cast<uint64_t>(endPos);
  auto closeFile = [&]() { fclose(f); };
#endif

  WavRiffHeader riff{};
#if USE_SD_OPEN
  if (f.read(reinterpret_cast<uint8_t*>(&riff), sizeof(riff)) != sizeof(riff)) {
    closeFile();
    return false;
  }
#else
  if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) {
    closeFile();
    return false;
  }
#endif

  if (strncmp(riff.riff, "RIFF", 4) != 0 ||
      strncmp(riff.wave, "WAVE", 4) != 0) {
    printf("loadWavFile: Invalid RIFF/WAVE header\n");
    closeFile();
    return false;
  }

  bool fmtFound = false;
  bool dataFound = false;
  WavFmtChunk fmt{};

#if USE_SD_OPEN
  while (!dataFound && f.available()) {
    WavChunkHeader header{};
    if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      const std::size_t toRead =
          std::min<std::size_t>(header.size, sizeof(fmt) - 8);
      if (f.read(reinterpret_cast<uint8_t*>(&fmt.audioFormat), toRead) != toRead) break;
      if (header.size > toRead &&
          !f.seek(f.position() + static_cast<uint32_t>(header.size - toRead))) {
        break;
      }
      fmtFound = true;
      printf("loadWavFile: Found fmt. AudioFormat=%d Channels=%d Rate=%d Bits=%d\n",
             fmt.audioFormat, fmt.numChannels, fmt.sampleRate, fmt.bitsPerSample);
    } else if (strncmp(header.id, "data", 4) == 0) {
      result.dataBytes = header.size;
      result.dataOffset = static_cast<uint64_t>(f.position());
      dataFound = true;
      printf("loadWavFile: Found data size=%u\n", result.dataBytes);
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
      printf("loadWavFile: Found fmt. AudioFormat=%d Channels=%d Rate=%d Bits=%d\n",
             fmt.audioFormat, fmt.numChannels, fmt.sampleRate, fmt.bitsPerSample);
    } else if (strncmp(header.id, "data", 4) == 0) {
      const long pos = ftell(f);
      if (pos < 0) break;
      result.dataBytes = header.size;
      result.dataOffset = static_cast<uint64_t>(pos);
      dataFound = true;
      printf("loadWavFile: Found data size=%u\n", result.dataBytes);
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
      result.dataBytes == 0 || result.dataBytes % expectedBlockAlign != 0) {
    printf("loadWavFile: Format not supported (req PCM16 mono/stereo with valid frame alignment)\n");
    closeFile();
    return false;
  }

  // A metadata probe runs before RamSampleStore evicts resident samples. Reject
  // a truncated payload here so malformed input cannot cause destructive LRU
  // churn before the decoder discovers the missing bytes.
  if (result.dataOffset > physicalSize ||
      static_cast<uint64_t>(result.dataBytes) > physicalSize - result.dataOffset) {
    printf("loadWavFile: truncated data payload\n");
    closeFile();
    return false;
  }

  result.sourceChannels = fmt.numChannels;
  result.info.sampleRate = fmt.sampleRate;
  result.info.channels = 1;
  result.info.bitsPerSample = 16;
  result.info.numFrames = result.dataBytes / expectedBlockAlign;

  if (result.info.numFrames >
      std::numeric_limits<std::size_t>::max() / sizeof(int16_t)) {
    closeFile();
    return false;
  }

  result.decodedBytes =
      static_cast<std::size_t>(result.info.numFrames) * sizeof(int16_t);
  if (result.decodedBytes > maxDecodedBytes) {
    printf("loadWavFile: decoded sample too large: %zu > %zu bytes\n",
           result.decodedBytes, maxDecodedBytes);
    closeFile();
    return false;
  }

  closeFile();
  return true;
}

}  // namespace

bool inspectWavFileBounded(const char* path, WavInfo& outInfo,
                           std::size_t maxDecodedBytes) {
  outInfo = {};
  WavProbeResult probe{};
  if (!probeWavMetadata(path, probe, maxDecodedBytes)) return false;
  outInfo = probe.info;
  return true;
}

bool loadWavFileBounded(const char* path, WavInfo& outInfo, int16_t** outPcm,
                        std::size_t maxDecodedBytes) {
  if (path == nullptr || outPcm == nullptr) return false;
  *outPcm = nullptr;
  outInfo = {};
  printf("loadWavFile: %s\n", path);

  // Re-probe with the caller's actual decode budget immediately before any
  // allocation. This catches a file changed between Store inspect/eviction and
  // decode without permitting an over-budget allocation.
  WavProbeResult probe{};
  if (!probeWavMetadata(path, probe, maxDecodedBytes)) return false;
  outInfo = probe.info;

#if USE_SD_OPEN
  if (probe.dataOffset > std::numeric_limits<uint32_t>::max()) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  if (!f.seek(static_cast<uint32_t>(probe.dataOffset))) {
    f.close();
    return false;
  }
#else
  if (probe.dataOffset > static_cast<uint64_t>(std::numeric_limits<long>::max())) {
    return false;
  }
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  if (fseek(f, static_cast<long>(probe.dataOffset), SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
#endif

  const std::size_t rawBytes = static_cast<std::size_t>(probe.dataBytes);
  int16_t* pcm = static_cast<int16_t*>(SAMPLE_MALLOC_PSRAM(rawBytes));
  if (!pcm) {
    printf("loadWavFile: PSRAM alloc failed, trying DRAM\n");
    pcm = static_cast<int16_t*>(SAMPLE_MALLOC_DRAM(rawBytes));
  }
  if (!pcm) {
    printf("loadWavFile: Alloc failed for %zu bytes\n", rawBytes);
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

  // Data-chunk read starts only after shared metadata admission.
#if USE_SD_OPEN
  const bool payloadOk =
      f.read(reinterpret_cast<uint8_t*>(pcm), rawBytes) == rawBytes;
  f.close();
#else
  const bool payloadOk = fread(pcm, 1, rawBytes, f) == rawBytes;
  fclose(f);
#endif

  if (!payloadOk) {
    printf("loadWavFile: Incomplete data read\n");
    free(pcm);
    return false;
  }

  if (probe.sourceChannels == 2) {
    for (uint32_t i = 0; i < outInfo.numFrames; ++i) {
      const int32_t left = pcm[i * 2];
      const int32_t right = pcm[i * 2 + 1];
      pcm[i] = static_cast<int16_t>((left + right) / 2);
    }

    // Historical 0.9.3 stereo path retained for recovery scope. Replacing the
    // full stereo allocation + mono copy with bounded chunk-wise conversion is
    // explicitly 0.9.5-A work.
    int16_t* mono = static_cast<int16_t*>(SAMPLE_MALLOC_PSRAM(probe.decodedBytes));
    if (!mono) mono = static_cast<int16_t*>(SAMPLE_MALLOC_DRAM(probe.decodedBytes));
    if (mono) {
      for (uint32_t i = 0; i < outInfo.numFrames; ++i) mono[i] = pcm[i];
      free(pcm);
      pcm = mono;
    }
  }

  *outPcm = pcm;
  return true;
}

bool loadWavFile(const char* path, WavInfo& outInfo, int16_t** outPcm) {
  return loadWavFileBounded(path, outInfo, outPcm,
                            std::numeric_limits<std::size_t>::max());
}
