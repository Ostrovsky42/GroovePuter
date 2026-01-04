#include "wav_recorder.h"

#include <cstring>
#include <ctime>

namespace {

void writeLE16(std::uint8_t* dst, std::uint16_t value) {
  dst[0] = static_cast<std::uint8_t>(value & 0xFF);
  dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

void writeLE32(std::uint8_t* dst, std::uint32_t value) {
  dst[0] = static_cast<std::uint8_t>(value & 0xFF);
  dst[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  dst[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  dst[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

}  // namespace

WavRecorder::WavRecorder() = default;

WavRecorder::~WavRecorder() {
  stop();
}

bool WavRecorder::start(int sampleRate, int channels) {
  if (file_) {
    return false;
  }

  filename_ = generateTimestampFilename();
  file_ = std::fopen(filename_.c_str(), "wb");
  if (!file_) {
    filename_.clear();
    return false;
  }

  sampleRate_ = sampleRate;
  channels_ = channels;
  dataBytes_ = 0;
  writeHeaderPlaceholder();
  return true;
}

void WavRecorder::stop() {
  if (!file_) {
    return;
  }

  finalizeHeader();
  std::fclose(file_);
  file_ = nullptr;
  dataBytes_ = 0;
}

bool WavRecorder::isRecording() const {
  return file_ != nullptr;
}

void WavRecorder::writeSamples(const int16_t* samples, size_t sampleCount) {
  if (!file_ || !samples || sampleCount == 0) {
    return;
  }

  size_t written = std::fwrite(samples, sizeof(int16_t), sampleCount, file_);
  dataBytes_ += static_cast<std::uint32_t>(written * sizeof(int16_t));
}

const std::string& WavRecorder::filename() const {
  return filename_;
}

std::string WavRecorder::generateTimestampFilename() const {
  char timestamp[32] = "unknown";
  std::time_t now = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif

  if (std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm) == 0) {
    std::snprintf(timestamp, sizeof(timestamp), "unknown");
  }

  std::string filename = "miniacid_";
  filename += timestamp;
  filename += ".wav";
  return filename;
}

void WavRecorder::writeHeaderPlaceholder() {
  std::uint8_t header[44];
  std::memcpy(header, "RIFF", 4);
  writeLE32(header + 4, 36 + dataBytes_);
  std::memcpy(header + 8, "WAVE", 4);
  std::memcpy(header + 12, "fmt ", 4);
  writeLE32(header + 16, 16);
  writeLE16(header + 20, 1);
  writeLE16(header + 22, static_cast<std::uint16_t>(channels_));
  writeLE32(header + 24, static_cast<std::uint32_t>(sampleRate_));
  std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRate_ * channels_ * sizeof(int16_t));
  writeLE32(header + 28, byteRate);
  std::uint16_t blockAlign = static_cast<std::uint16_t>(channels_ * sizeof(int16_t));
  writeLE16(header + 32, blockAlign);
  writeLE16(header + 34, 16);
  std::memcpy(header + 36, "data", 4);
  writeLE32(header + 40, dataBytes_);

  std::fwrite(header, sizeof(header), 1, file_);
}

void WavRecorder::finalizeHeader() {
  std::uint8_t sizeField[4];
  writeLE32(sizeField, 36 + dataBytes_);
  std::fseek(file_, 4, SEEK_SET);
  std::fwrite(sizeField, sizeof(sizeField), 1, file_);

  writeLE32(sizeField, dataBytes_);
  std::fseek(file_, 40, SEEK_SET);
  std::fwrite(sizeField, sizeof(sizeField), 1, file_);

  std::fflush(file_);
}
