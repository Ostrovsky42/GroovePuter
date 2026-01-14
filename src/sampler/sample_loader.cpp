#include "../audio/audio_config.h"
#include "sample_store.h" // for WavInfo
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define SAMPLE_MALLOC_PSRAM(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define SAMPLE_MALLOC_DRAM(size) heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#else
#define SAMPLE_MALLOC_PSRAM(size) malloc(size)
#define SAMPLE_MALLOC_DRAM(size) malloc(size)
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

bool loadWavFile(const char* path, WavInfo& outInfo, int16_t** outPcm) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;

  WavRiffHeader riff;
  if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) { fclose(f); return false; }

  if (strncmp(riff.riff, "RIFF", 4) != 0 || strncmp(riff.wave, "WAVE", 4) != 0) {
    fclose(f); return false;
  }

  bool fmtFound = false, dataFound = false;
  uint32_t dataSize = 0;
  WavFmtChunk fmt;
  memset(&fmt, 0, sizeof(fmt));

  while (!dataFound && !feof(f)) {
    WavChunkHeader header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      size_t toRead = (header.size < sizeof(fmt) - 8) ? header.size : (sizeof(fmt) - 8);
      if (fread(&fmt.audioFormat, 1, toRead, f) != toRead) break;
      if (header.size > toRead) fseek(f, header.size - toRead, SEEK_CUR);
      fmtFound = true;
    } else if (strncmp(header.id, "data", 4) == 0) {
      dataSize = header.size;
      dataFound = true;
    } else {
      fseek(f, header.size, SEEK_CUR);
    }
  }

  if (!fmtFound || !dataFound || fmt.audioFormat != 1 || fmt.bitsPerSample != 16) {
    fclose(f); return false;
  }

  outInfo.sampleRate = fmt.sampleRate;
  outInfo.channels = fmt.numChannels; 
  outInfo.bitsPerSample = fmt.bitsPerSample;
  outInfo.numFrames = dataSize / ((fmt.bitsPerSample / 8) * fmt.numChannels);

  // Allocate with PSRAM priority
  size_t bytes = dataSize;
  *outPcm = (int16_t*)SAMPLE_MALLOC_PSRAM(bytes);
  if (!*outPcm) {
    *outPcm = (int16_t*)SAMPLE_MALLOC_DRAM(bytes);
  }
  if (!*outPcm) { fclose(f); return false; }

  if (fread(*outPcm, 2, dataSize / 2, f) != dataSize / 2) {
    free(*outPcm); *outPcm = nullptr;
    fclose(f); return false;
  }
  
  // Stereo to Mono mixdown if needed
  if (fmt.numChannels == 2) {
      // Very simple in-place mix: L = (L+R)/2
      // Input: L0, R0, L1, R1...
      // Output: M0, M1...
      int16_t* data = *outPcm;
      for (uint32_t i = 0; i < outInfo.numFrames; ++i) {
          int32_t l = data[i*2];
          int32_t r = data[i*2+1];
          data[i] = (l + r) / 2;
      }
      outInfo.channels = 1;
      // Realloc? No, just keep the detailed buffer or use less space.
      // For V1, we just waste half space or realloc down. 
      // Realloc down is better for small RAM.
      // But careful with `new[]`. We can't realloc easily.
      // Allocate new smaller buffer with PSRAM priority
      int16_t* mono = (int16_t*)SAMPLE_MALLOC_PSRAM(outInfo.numFrames * 2);
      if (!mono) mono = (int16_t*)SAMPLE_MALLOC_DRAM(outInfo.numFrames * 2);
      
      if (mono) {
          for(uint32_t i=0; i<outInfo.numFrames; ++i) mono[i] = data[i];
          free(data);
          *outPcm = mono;
      } else {
          // If mono allocation fails, we keep the original (larger) buffer and just use it as mono
          // (wasteful but prevents crash)
          outInfo.channels = 1;
      }
  }
  
  fclose(f);
  return true;
}
