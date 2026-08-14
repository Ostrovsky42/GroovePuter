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

struct WavRiffHeader {
  char riff[4]; uint32_t totalSize; char wave[4];
};

struct WavFmtChunk {
  char fmt[4]; uint32_t chunkSize;
  uint16_t audioFormat; uint16_t numChannels;
  uint32_t sampleRate; uint32_t byteRate;
  uint16_t blockAlign; uint16_t bitsPerSample;
};

struct WavChunkHeader { char id[4]; uint32_t size; };

bool loadWavFileBounded(const char* path, WavInfo& outInfo, int16_t** outPcm,
                        std::size_t maxDecodedBytes) {
  if (path == nullptr || outPcm == nullptr) return false;
  *outPcm = nullptr;
  outInfo = {};
  printf("loadWavFile: %s\n", path);

#if USE_SD_OPEN
  File f = SD.open(path, FILE_READ);
  if (!f) {
    printf("loadWavFile: SD.open failed for %s\n", path);
    return false;
  }
#else
  FILE* f = fopen(path, "rb");
  if (!f) {
    printf("loadWavFile: fopen failed for %s\n", path);
    return false;
  }
#endif

  WavRiffHeader riff{};
#if USE_SD_OPEN
  if (f.read(reinterpret_cast<uint8_t*>(&riff), sizeof(riff)) != sizeof(riff)) {
    f.close();
    return false;
  }
#else
  if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) {
    fclose(f);
    return false;
  }
#endif

  if (strncmp(riff.riff, "RIFF", 4) != 0 || strncmp(riff.wave, "WAVE", 4) != 0) {
    printf("loadWavFile: Invalid RIFF/WAVE header\n");
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

  bool fmtFound = false;
  bool dataFound = false;
  uint32_t dataSize = 0;
  WavFmtChunk fmt{};

#if USE_SD_OPEN
  while (!dataFound && f.available()) {
    WavChunkHeader header{};
    if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      const size_t toRead = std::min<std::size_t>(header.size, sizeof(fmt) - 8);
      if (f.read(reinterpret_cast<uint8_t*>(&fmt.audioFormat), toRead) != toRead) break;
      if (header.size > toRead) f.seek(f.position() + (header.size - toRead));
      fmtFound = true;
      printf("loadWavFile: Found fmt. AudioFormat=%d Channels=%d Rate=%d Bits=%d\n",
             fmt.audioFormat, fmt.numChannels, fmt.sampleRate, fmt.bitsPerSample);
    } else if (strncmp(header.id, "data", 4) == 0) {
      dataSize = header.size;
      dataFound = true;
      printf("loadWavFile: Found data size=%u\n", dataSize);
    } else {
      f.seek(f.position() + header.size);
    }
  }
#else
  while (!dataFound && !feof(f)) {
    WavChunkHeader header{};
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      const size_t toRead = std::min<std::size_t>(header.size, sizeof(fmt) - 8);
      if (fread(&fmt.audioFormat, 1, toRead, f) != toRead) break;
      if (header.size > toRead) fseek(f, static_cast<long>(header.size - toRead), SEEK_CUR);
      fmtFound = true;
      printf("loadWavFile: Found fmt. AudioFormat=%d Channels=%d Rate=%d Bits=%d\n",
             fmt.audioFormat, fmt.numChannels, fmt.sampleRate, fmt.bitsPerSample);
    } else if (strncmp(header.id, "data", 4) == 0) {
      dataSize = header.size;
      dataFound = true;
      printf("loadWavFile: Found data size=%u\n", dataSize);
    } else {
      fseek(f, static_cast<long>(header.size), SEEK_CUR);
    }
  }
#endif

  const uint32_t expectedBlockAlign = static_cast<uint32_t>(fmt.numChannels) * sizeof(int16_t);
  if (!fmtFound || !dataFound || fmt.audioFormat != 1 || fmt.bitsPerSample != 16 ||
      (fmt.numChannels != 1 && fmt.numChannels != 2) || fmt.sampleRate == 0 ||
      fmt.blockAlign != expectedBlockAlign || dataSize == 0 ||
      dataSize % expectedBlockAlign != 0) {
    printf("loadWavFile: Format not supported (req PCM16 mono/stereo with valid frame alignment)\n");
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

  outInfo.sampleRate = fmt.sampleRate;
  outInfo.channels = 1;
  outInfo.bitsPerSample = 16;
  outInfo.numFrames = dataSize / expectedBlockAlign;

  if (outInfo.numFrames > std::numeric_limits<std::size_t>::max() / sizeof(int16_t)) {
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

  const std::size_t decodedBytes =
      static_cast<std::size_t>(outInfo.numFrames) * sizeof(int16_t);

  // Admission gate: the sampler budget applies to final decoded mono PCM.
  // Reject before any PCM allocation or data-chunk read.
  if (decodedBytes > maxDecodedBytes) {
    printf("loadWavFile: decoded sample too large: %zu > %zu bytes\n",
           decodedBytes, maxDecodedBytes);
#if USE_SD_OPEN
    f.close();
#else
    fclose(f);
#endif
    return false;
  }

  const std::size_t rawBytes = static_cast<std::size_t>(dataSize);
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

  // Data-chunk read starts only after final decoded-size admission.
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

  if (fmt.numChannels == 2) {
    for (uint32_t i = 0; i < outInfo.numFrames; ++i) {
      const int32_t left = pcm[i * 2];
      const int32_t right = pcm[i * 2 + 1];
      pcm[i] = static_cast<int16_t>((left + right) / 2);
    }

    int16_t* mono = static_cast<int16_t*>(SAMPLE_MALLOC_PSRAM(decodedBytes));
    if (!mono) mono = static_cast<int16_t*>(SAMPLE_MALLOC_DRAM(decodedBytes));
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
