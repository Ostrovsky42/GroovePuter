#include "sample_loader.h"

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

constexpr uint64_t kRiffHeaderBytes = 12;
constexpr uint64_t kChunkHeaderBytes = 8;
constexpr uint32_t kPcmFmtBytes = 16;
constexpr std::size_t kStereoScratchBytes = 512;

void setError(WavLoadError* error, WavLoadError value) {
  if (error != nullptr) *error = value;
}

uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

class WavFile {
 public:
  bool open(const char* path) {
#if USE_SD_OPEN
    file_ = SD.open(path, FILE_READ);
    if (!file_) return false;
    size_ = static_cast<uint64_t>(file_.size());
    return true;
#else
    file_ = fopen(path, "rb");
    if (file_ == nullptr) return false;
    if (fseek(file_, 0, SEEK_END) != 0) {
      close();
      return false;
    }
    const long end = ftell(file_);
    if (end < 0 || fseek(file_, 0, SEEK_SET) != 0) {
      close();
      return false;
    }
    size_ = static_cast<uint64_t>(end);
    return true;
#endif
  }

  ~WavFile() { close(); }

  void close() {
#if USE_SD_OPEN
    if (file_) file_.close();
#else
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
    }
#endif
  }

  uint64_t size() const { return size_; }

  bool seek(uint64_t offset) {
    if (offset > size_) return false;
#if USE_SD_OPEN
    if (offset > std::numeric_limits<uint32_t>::max()) return false;
    return file_.seek(static_cast<uint32_t>(offset));
#else
    if (offset > static_cast<uint64_t>(std::numeric_limits<long>::max())) {
      return false;
    }
    return fseek(file_, static_cast<long>(offset), SEEK_SET) == 0;
#endif
  }

  bool readExact(void* dst, std::size_t bytes) {
    if (bytes == 0) return true;
#if USE_SD_OPEN
    return file_.read(reinterpret_cast<uint8_t*>(dst), bytes) == bytes;
#else
    return fread(dst, 1, bytes, file_) == bytes;
#endif
  }

 private:
#if USE_SD_OPEN
  File file_;
#else
  FILE* file_ = nullptr;
#endif
  uint64_t size_ = 0;
};

bool sameInspectResult(const WavInspectResult& a, const WavInspectResult& b) {
  return a.info.sampleRate == b.info.sampleRate &&
         a.info.channels == b.info.channels &&
         a.info.bitsPerSample == b.info.bitsPerSample &&
         a.info.numFrames == b.info.numFrames &&
         a.sourceChannels == b.sourceChannels &&
         a.sourceDataBytes == b.sourceDataBytes &&
         a.dataOffset == b.dataOffset &&
         a.decodedBytes == b.decodedBytes;
}

bool inspectOpenFile(WavFile& file, WavInspectResult& result,
                     std::size_t maxDecodedBytes, WavLoadError* error) {
  result = {};

  const uint64_t physicalSize = file.size();
  if (physicalSize < kRiffHeaderBytes) {
    setError(error, WavLoadError::Truncated);
    return false;
  }

  uint8_t riff[12]{};
  if (!file.seek(0) || !file.readExact(riff, sizeof(riff))) {
    setError(error, WavLoadError::IoError);
    return false;
  }
  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
    setError(error, WavLoadError::InvalidRiff);
    return false;
  }

  const uint64_t declaredRiffBytes = static_cast<uint64_t>(readLe32(riff + 4));
  if (declaredRiffBytes < 4) {
    setError(error, WavLoadError::InvalidRiff);
    return false;
  }
  const uint64_t riffEnd = declaredRiffBytes + 8u;
  if (riffEnd < kRiffHeaderBytes || riffEnd > physicalSize) {
    setError(error, WavLoadError::Truncated);
    return false;
  }

  bool fmtFound = false;
  bool dataFound = false;
  uint16_t audioFormat = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint32_t byteRate = 0;
  uint16_t blockAlign = 0;
  uint16_t bitsPerSample = 0;

  uint64_t cursor = kRiffHeaderBytes;
  while (cursor < riffEnd) {
    if (riffEnd - cursor < kChunkHeaderBytes) {
      setError(error, WavLoadError::Truncated);
      return false;
    }

    uint8_t header[8]{};
    if (!file.seek(cursor) || !file.readExact(header, sizeof(header))) {
      setError(error, WavLoadError::IoError);
      return false;
    }

    const uint32_t chunkSize = readLe32(header + 4);
    const uint64_t payloadOffset = cursor + kChunkHeaderBytes;
    const uint64_t paddedBytes =
        static_cast<uint64_t>(chunkSize) + static_cast<uint64_t>(chunkSize & 1u);
    if (payloadOffset > riffEnd || paddedBytes > riffEnd - payloadOffset) {
      setError(error, WavLoadError::Truncated);
      return false;
    }

    if (memcmp(header, "fmt ", 4) == 0) {
      if (fmtFound) {
        setError(error, WavLoadError::InvalidFormat);
        return false;
      }
      if (chunkSize < kPcmFmtBytes) {
        setError(error, WavLoadError::InvalidFormat);
        return false;
      }
      uint8_t fmt[kPcmFmtBytes]{};
      if (!file.seek(payloadOffset) || !file.readExact(fmt, sizeof(fmt))) {
        setError(error, WavLoadError::IoError);
        return false;
      }
      audioFormat = readLe16(fmt);
      channels = readLe16(fmt + 2);
      sampleRate = readLe32(fmt + 4);
      byteRate = readLe32(fmt + 8);
      blockAlign = readLe16(fmt + 12);
      bitsPerSample = readLe16(fmt + 14);
      fmtFound = true;
    } else if (memcmp(header, "data", 4) == 0) {
      if (dataFound) {
        setError(error, WavLoadError::InvalidFormat);
        return false;
      }
      result.sourceDataBytes = chunkSize;
      result.dataOffset = payloadOffset;
      dataFound = true;
    }

    // Traverse the complete declared RIFF boundary even after both required
    // chunks are known. This rejects malformed trailing chunks and enforces the
    // strict duplicate fmt/data policy deterministically.
    cursor = payloadOffset + paddedBytes;
  }

  if (!fmtFound) {
    setError(error, WavLoadError::MissingFmt);
    return false;
  }
  if (!dataFound) {
    setError(error, WavLoadError::MissingData);
    return false;
  }
  if (audioFormat != 1 || bitsPerSample != 16) {
    setError(error, WavLoadError::UnsupportedEncoding);
    return false;
  }
  if (channels != 1 && channels != 2) {
    setError(error, WavLoadError::UnsupportedChannels);
    return false;
  }
  if (sampleRate == 0) {
    setError(error, WavLoadError::InvalidFormat);
    return false;
  }

  const uint32_t expectedBlockAlign =
      static_cast<uint32_t>(channels) * sizeof(int16_t);
  const uint64_t expectedByteRate64 =
      static_cast<uint64_t>(sampleRate) * expectedBlockAlign;
  if (blockAlign != expectedBlockAlign ||
      expectedByteRate64 > std::numeric_limits<uint32_t>::max() ||
      byteRate != static_cast<uint32_t>(expectedByteRate64) ||
      result.sourceDataBytes == 0 ||
      result.sourceDataBytes % expectedBlockAlign != 0) {
    setError(error, WavLoadError::InvalidFormat);
    return false;
  }

  const uint64_t frames = result.sourceDataBytes / expectedBlockAlign;
  if (frames > std::numeric_limits<uint32_t>::max()) {
    setError(error, WavLoadError::TooLarge);
    return false;
  }
  if (frames > std::numeric_limits<uint64_t>::max() / sizeof(int16_t)) {
    setError(error, WavLoadError::TooLarge);
    return false;
  }
  const uint64_t decodedBytes64 = frames * sizeof(int16_t);
  if (decodedBytes64 > std::numeric_limits<std::size_t>::max() ||
      decodedBytes64 > maxDecodedBytes) {
    setError(error, WavLoadError::TooLarge);
    return false;
  }

  result.info.sampleRate = sampleRate;
  result.info.channels = 1;
  result.info.bitsPerSample = 16;
  result.info.numFrames = static_cast<uint32_t>(frames);
  result.sourceChannels = channels;
  result.decodedBytes = static_cast<std::size_t>(decodedBytes64);
  setError(error, WavLoadError::Ok);
  return true;
}

int16_t decodeLe16(const uint8_t* p) {
  return static_cast<int16_t>(readLe16(p));
}

}  // namespace

const char* wavLoadErrorName(WavLoadError error) {
  switch (error) {
    case WavLoadError::Ok: return "ok";
    case WavLoadError::InvalidArgument: return "invalid-argument";
    case WavLoadError::OpenFailed: return "open-failed";
    case WavLoadError::IoError: return "io-error";
    case WavLoadError::InvalidRiff: return "invalid-riff";
    case WavLoadError::Truncated: return "truncated";
    case WavLoadError::MissingFmt: return "missing-fmt";
    case WavLoadError::MissingData: return "missing-data";
    case WavLoadError::UnsupportedEncoding: return "unsupported-encoding";
    case WavLoadError::UnsupportedChannels: return "unsupported-channels";
    case WavLoadError::InvalidFormat: return "invalid-format";
    case WavLoadError::TooLarge: return "too-large";
    case WavLoadError::OutOfMemory: return "out-of-memory";
    case WavLoadError::ChangedAfterInspect: return "changed-after-inspect";
  }
  return "unknown";
}

bool inspectWavFileBounded(const char* path, WavInspectResult& out,
                           std::size_t maxDecodedBytes, WavLoadError* error) {
  out = {};
  setError(error, WavLoadError::Ok);
  if (path == nullptr || path[0] == '\0') {
    setError(error, WavLoadError::InvalidArgument);
    return false;
  }

  WavFile file;
  if (!file.open(path)) {
    setError(error, WavLoadError::OpenFailed);
    return false;
  }

  return inspectOpenFile(file, out, maxDecodedBytes, error);
}

bool decodeWavFileBounded(const char* path, const WavInspectResult& inspected,
                          int16_t** outPcm, std::size_t maxDecodedBytes,
                          WavLoadError* error) {
  setError(error, WavLoadError::Ok);
  if (path == nullptr || path[0] == '\0' || outPcm == nullptr) {
    setError(error, WavLoadError::InvalidArgument);
    return false;
  }
  *outPcm = nullptr;
  if (inspected.decodedBytes == 0 ||
      inspected.decodedBytes > maxDecodedBytes) {
    setError(error, WavLoadError::TooLarge);
    return false;
  }

  WavFile file;
  if (!file.open(path)) {
    setError(error, WavLoadError::OpenFailed);
    return false;
  }

  WavInspectResult current{};
  WavLoadError inspectError = WavLoadError::Ok;
  if (!inspectOpenFile(file, current, maxDecodedBytes, &inspectError)) {
    setError(error, inspectError);
    return false;
  }
  if (!sameInspectResult(inspected, current)) {
    setError(error, WavLoadError::ChangedAfterInspect);
    return false;
  }
  if (!file.seek(current.dataOffset)) {
    setError(error, WavLoadError::IoError);
    return false;
  }

  int16_t* pcm = static_cast<int16_t*>(SAMPLE_MALLOC_PSRAM(current.decodedBytes));
  if (pcm == nullptr) {
    pcm = static_cast<int16_t*>(SAMPLE_MALLOC_DRAM(current.decodedBytes));
  }
  if (pcm == nullptr) {
    setError(error, WavLoadError::OutOfMemory);
    return false;
  }

  if (current.sourceChannels == 1) {
    if (!file.readExact(pcm, current.decodedBytes)) {
      free(pcm);
      setError(error, WavLoadError::IoError);
      return false;
    }
  } else {
    static_assert(kStereoScratchBytes % (2 * sizeof(int16_t)) == 0,
                  "stereo scratch must contain whole frames");
    uint8_t scratch[kStereoScratchBytes]{};
    uint32_t frameIndex = 0;
    while (frameIndex < current.info.numFrames) {
      const uint32_t remaining = current.info.numFrames - frameIndex;
      const uint32_t scratchFrames =
          static_cast<uint32_t>(sizeof(scratch) / (2 * sizeof(int16_t)));
      const uint32_t framesNow = std::min(remaining, scratchFrames);
      const std::size_t bytesNow =
          static_cast<std::size_t>(framesNow) * 2 * sizeof(int16_t);
      if (!file.readExact(scratch, bytesNow)) {
        free(pcm);
        setError(error, WavLoadError::IoError);
        return false;
      }

      for (uint32_t i = 0; i < framesNow; ++i) {
        const uint8_t* frame = scratch + i * 4;
        const int32_t left = decodeLe16(frame);
        const int32_t right = decodeLe16(frame + 2);
        pcm[frameIndex + i] = static_cast<int16_t>((left + right) / 2);
      }
      frameIndex += framesNow;
    }
  }

  *outPcm = pcm;
  setError(error, WavLoadError::Ok);
  return true;
}

bool inspectWavFileBounded(const char* path, WavInfo& outInfo,
                           std::size_t maxDecodedBytes) {
  outInfo = {};
  WavInspectResult inspected{};
  if (!inspectWavFileBounded(path, inspected, maxDecodedBytes, nullptr)) {
    return false;
  }
  outInfo = inspected.info;
  return true;
}

bool loadWavFileBounded(const char* path, WavInfo& outInfo, int16_t** outPcm,
                        std::size_t maxDecodedBytes) {
  outInfo = {};
  if (outPcm == nullptr) return false;
  *outPcm = nullptr;

  WavInspectResult inspected{};
  WavLoadError error = WavLoadError::Ok;
  if (!inspectWavFileBounded(path, inspected, maxDecodedBytes, &error)) {
    printf("loadWavFile: %s: %s\n", path == nullptr ? "(null)" : path,
           wavLoadErrorName(error));
    return false;
  }
  if (!decodeWavFileBounded(path, inspected, outPcm, maxDecodedBytes, &error)) {
    printf("loadWavFile: %s: %s\n", path, wavLoadErrorName(error));
    return false;
  }

  outInfo = inspected.info;
  return true;
}

bool loadWavFile(const char* path, WavInfo& outInfo, int16_t** outPcm) {
  return loadWavFileBounded(path, outInfo, outPcm,
                            std::numeric_limits<std::size_t>::max());
}
