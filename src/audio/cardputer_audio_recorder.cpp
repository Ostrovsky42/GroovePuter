#include "cardputer_audio_recorder.h"

#if defined(ARDUINO)

#include <cstring>
#include <M5Cardputer.h>

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

CardputerAudioRecorder::CardputerAudioRecorder() = default;

CardputerAudioRecorder::~CardputerAudioRecorder() {
  stop();
}

bool CardputerAudioRecorder::start(int sampleRate, int channels) {
  if (file_) {
    return false;
  }

  // Initialize SD card if not already initialized
  if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    return false;
  }

  filename_ = generateTimestampFilename();
  file_ = SD.open(filename_.c_str(), FILE_WRITE);
  if (!file_) {
    Serial.print("Failed to open file for recording: ");
    Serial.println(filename_.c_str());
    filename_.clear();
    return false;
  }

  sampleRate_ = sampleRate;
  channels_ = channels;
  dataBytes_ = 0;
  writeHeaderPlaceholder();
  
  Serial.print("Recording started: ");
  Serial.println(filename_.c_str());
  return true;
}

void CardputerAudioRecorder::stop() {
  if (!file_) {
    return;
  }

  finalizeHeader();
  file_.close();
  dataBytes_ = 0;
  
  Serial.print("Recording stopped: ");
  Serial.println(filename_.c_str());
}

bool CardputerAudioRecorder::isRecording() const {
  return file_ ? true : false;
}

void CardputerAudioRecorder::writeSamples(const int16_t* samples, size_t sampleCount) {
  if (!file_ || !samples || sampleCount == 0) {
    return;
  }

  size_t written = file_.write(reinterpret_cast<const uint8_t*>(samples), 
                                sampleCount * sizeof(int16_t));
  dataBytes_ += static_cast<std::uint32_t>(written);
}

const std::string& CardputerAudioRecorder::filename() const {
  return filename_;
}

std::string CardputerAudioRecorder::generateTimestampFilename() const {
  // Use millis() for timestamp since we don't have real-time clock
  unsigned long now = millis();
  char timestamp[32];
  std::snprintf(timestamp, sizeof(timestamp), "%08lx", now);

  std::string filename = "/grooveputer_";
  filename += timestamp;
  filename += ".wav";
  return filename;
}

void CardputerAudioRecorder::writeHeaderPlaceholder() {
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

  file_.write(header, sizeof(header));
}

void CardputerAudioRecorder::finalizeHeader() {
  std::uint8_t sizeField[4];
  writeLE32(sizeField, 36 + dataBytes_);
  file_.seek(4);
  file_.write(sizeField, sizeof(sizeField));

  writeLE32(sizeField, dataBytes_);
  file_.seek(40);
  file_.write(sizeField, sizeof(sizeField));

  file_.flush();
}

#endif // ARDUINO
