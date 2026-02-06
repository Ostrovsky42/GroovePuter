#include "wasm_audio_recorder.h"

#if defined(__EMSCRIPTEN__)

#include <cstring>
#include <ctime>
#include <emscripten/emscripten.h>

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

WasmAudioRecorder::WasmAudioRecorder() = default;

WasmAudioRecorder::~WasmAudioRecorder() {
  stop();
}

bool WasmAudioRecorder::start(int sampleRate, int channels) {
  if (recording_) {
    return false;
  }

  filename_ = generateTimestampFilename();
  sampleRate_ = sampleRate;
  channels_ = channels;
  buffer_.clear();
  buffer_.reserve(sampleRate * channels * 60); // Reserve 1 minute
  recording_ = true;

  EM_ASM({
    console.log('WAV Recording started: ' + UTF8ToString($0));
  }, filename_.c_str());

  return true;
}

void WasmAudioRecorder::stop() {
  if (!recording_) {
    return;
  }

  recording_ = false;
  sendToJavaScript();
  buffer_.clear();

  EM_ASM({
    console.log('WAV Recording stopped: ' + UTF8ToString($0));
  }, filename_.c_str());
}

bool WasmAudioRecorder::isRecording() const {
  return recording_;
}

void WasmAudioRecorder::writeSamples(const int16_t* samples, size_t sampleCount) {
  if (!recording_ || !samples || sampleCount == 0) {
    return;
  }

  buffer_.insert(buffer_.end(), samples, samples + sampleCount);
}

const std::string& WasmAudioRecorder::filename() const {
  return filename_;
}

std::string WasmAudioRecorder::generateTimestampFilename() const {
  char timestamp[32] = "unknown";
  std::time_t now = std::time(nullptr);
  std::tm tm{};
  
  if (localtime_r(&now, &tm) && 
      std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm) == 0) {
    std::snprintf(timestamp, sizeof(timestamp), "unknown");
  }

  std::string filename = "grooveputer_";
  filename += timestamp;
  filename += ".wav";
  return filename;
}

void WasmAudioRecorder::sendToJavaScript() {
  if (buffer_.empty()) {
    return;
  }

  // Build WAV file in memory
  std::uint32_t dataBytes = static_cast<std::uint32_t>(buffer_.size() * sizeof(int16_t));
  std::uint32_t fileSize = 44 + dataBytes;
  std::vector<std::uint8_t> wavData(fileSize);

  // Write WAV header
  std::uint8_t* header = wavData.data();
  std::memcpy(header, "RIFF", 4);
  writeLE32(header + 4, 36 + dataBytes);
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
  writeLE32(header + 40, dataBytes);

  // Copy audio data
  std::memcpy(wavData.data() + 44, buffer_.data(), dataBytes);

  // Call JavaScript function to download the file
  EM_ASM({
    if (typeof window.grooveputerDownloadRecording === 'function') {
      const ptr = $0;
      const size = $1;
      const filename = UTF8ToString($2);
      const data = new Uint8Array(Module.HEAPU8.buffer, ptr, size);
      const blob = new Blob([data], { type: 'audio/wav' });
      window.grooveputerDownloadRecording(blob, filename);
    } else {
      console.error('window.grooveputerDownloadRecording not defined');
    }
  }, wavData.data(), static_cast<int>(wavData.size()), filename_.c_str());
}

#endif // __EMSCRIPTEN__
