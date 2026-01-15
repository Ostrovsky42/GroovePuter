#include "../audio/audio_config.h"
#include "sample_store.h" // for WavInfo
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

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

bool loadWavFile(const char* path, WavInfo& outInfo, int16_t** outPcm) {
  printf("loadWavFile: %s\n", path);

#if USE_SD_OPEN
  File f = SD.open(path, FILE_READ);
  if (!f) {
      printf("loadWavFile: SD.open failed for %s\n", path);
      return false;
  }

  WavRiffHeader riff;
  if (f.read((uint8_t*)&riff, sizeof(riff)) != sizeof(riff)) { f.close(); return false; }

  if (strncmp(riff.riff, "RIFF", 4) != 0 || strncmp(riff.wave, "WAVE", 4) != 0) {
    printf("loadWavFile: Invalid RIFF/WAVE header\n");
    f.close(); return false;
  }

  bool fmtFound = false, dataFound = false;
  uint32_t dataSize = 0;
  WavFmtChunk fmt;
  memset(&fmt, 0, sizeof(fmt));

  while (!dataFound && f.available()) {
    WavChunkHeader header;
    if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) break;

    if (strncmp(header.id, "fmt ", 4) == 0) {
      size_t toRead = (header.size < sizeof(fmt) - 8) ? header.size : (sizeof(fmt) - 8);
      if (f.read((uint8_t*)&fmt.audioFormat, toRead) != toRead) break;
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

  if (!fmtFound || !dataFound || fmt.audioFormat != 1 || fmt.bitsPerSample != 16) {
    printf("loadWavFile: Format not supported (req PCM 16-bit)\n");
    f.close(); return false;
  }

  outInfo.sampleRate = fmt.sampleRate;
  outInfo.channels = fmt.numChannels; 
  outInfo.bitsPerSample = fmt.bitsPerSample;
  outInfo.numFrames = dataSize / ((fmt.bitsPerSample / 8) * fmt.numChannels);

  size_t bytes = dataSize;
  *outPcm = (int16_t*)SAMPLE_MALLOC_PSRAM(bytes);
  if (!*outPcm) {
    printf("loadWavFile: PSRAM alloc failed, trying DRAM\n");
    *outPcm = (int16_t*)SAMPLE_MALLOC_DRAM(bytes);
  }
  if (!*outPcm) { 
      printf("loadWavFile: Alloc failed for %zu bytes\n", bytes);
      f.close(); return false; 
  }

  if (f.read((uint8_t*)*outPcm, dataSize) != dataSize) {
    printf("loadWavFile: Incomplete read\n");
    free(*outPcm); *outPcm = nullptr;
    f.close(); return false;
  }
  f.close();

#else
  // Desktop/SDL: use fopen
  FILE* f = fopen(path, "rb");
  if (!f) {
      printf("loadWavFile: fopen failed for %s\n", path);
      return false;
  }

  WavRiffHeader riff;
  if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) { fclose(f); return false; }

  if (strncmp(riff.riff, "RIFF", 4) != 0 || strncmp(riff.wave, "WAVE", 4) != 0) {
    printf("loadWavFile: Invalid RIFF/WAVE header\n");
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
      printf("loadWavFile: Found fmt. AudioFormat=%d Channels=%d Rate=%d Bits=%d\n", 
             fmt.audioFormat, fmt.numChannels, fmt.sampleRate, fmt.bitsPerSample);
    } else if (strncmp(header.id, "data", 4) == 0) {
      dataSize = header.size;
      dataFound = true;
      printf("loadWavFile: Found data size=%u\n", dataSize);
    } else {
      fseek(f, header.size, SEEK_CUR);
    }
  }

  if (!fmtFound || !dataFound || fmt.audioFormat != 1 || fmt.bitsPerSample != 16) {
    printf("loadWavFile: Format not supported (req PCM 16-bit)\n");
    fclose(f); return false;
  }

  outInfo.sampleRate = fmt.sampleRate;
  outInfo.channels = fmt.numChannels; 
  outInfo.bitsPerSample = fmt.bitsPerSample;
  outInfo.numFrames = dataSize / ((fmt.bitsPerSample / 8) * fmt.numChannels);

  size_t bytes = dataSize;
  *outPcm = (int16_t*)SAMPLE_MALLOC_PSRAM(bytes);
  if (!*outPcm) {
    printf("loadWavFile: PSRAM alloc failed, trying DRAM\n");
    *outPcm = (int16_t*)SAMPLE_MALLOC_DRAM(bytes);
  }
  if (!*outPcm) { 
      printf("loadWavFile: Alloc failed for %zu bytes\n", bytes);
      fclose(f); return false; 
  }

  if (fread(*outPcm, 2, dataSize / 2, f) != dataSize / 2) {
    printf("loadWavFile: Incomplete read\n");
    free(*outPcm); *outPcm = nullptr;
    fclose(f); return false;
  }
  fclose(f);
#endif
  
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
  
  return true;
}
