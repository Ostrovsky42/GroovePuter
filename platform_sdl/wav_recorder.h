#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

class WavRecorder {
 public:
  WavRecorder();
  ~WavRecorder();

  bool start(int sampleRate, int channels);
  void stop();
  bool isRecording() const;
  void writeSamples(const int16_t* samples, size_t sampleCount);
  const std::string& filename() const;

 private:
  std::string generateTimestampFilename() const;
  void writeHeaderPlaceholder();
  void finalizeHeader();

  std::FILE* file_ = nullptr;
  std::string filename_;
  std::uint32_t dataBytes_ = 0;
  int sampleRate_ = 0;
  int channels_ = 0;
};
