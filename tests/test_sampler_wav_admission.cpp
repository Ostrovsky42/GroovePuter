#include "../src/sampler/sample_store.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

bool loadWavFileBounded(const char* path, WavInfo& outInfo, int16_t** outPcm,
                        std::size_t maxDecodedBytes);

namespace fs = std::filesystem;

namespace {
void writeU16(std::ofstream& out, uint16_t value) {
  const char b[2] = {static_cast<char>(value & 0xFF),
                     static_cast<char>((value >> 8) & 0xFF)};
  out.write(b, sizeof(b));
}
void writeU32(std::ofstream& out, uint32_t value) {
  const char b[4] = {static_cast<char>(value & 0xFF),
                     static_cast<char>((value >> 8) & 0xFF),
                     static_cast<char>((value >> 16) & 0xFF),
                     static_cast<char>((value >> 24) & 0xFF)};
  out.write(b, sizeof(b));
}
void writePcm16Wav(const fs::path& path, uint16_t channels, uint32_t sampleRate,
                   uint32_t frames, const std::vector<int16_t>& payload) {
  const uint32_t dataBytes = frames * channels * sizeof(int16_t);
  std::ofstream out(path, std::ios::binary);
  out.write("RIFF", 4);
  writeU32(out, 36 + dataBytes);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  writeU32(out, 16);
  writeU16(out, 1);
  writeU16(out, channels);
  writeU32(out, sampleRate);
  writeU32(out, sampleRate * channels * sizeof(int16_t));
  writeU16(out, channels * sizeof(int16_t));
  writeU16(out, 16);
  out.write("data", 4);
  writeU32(out, dataBytes);
  for (int16_t sample : payload) writeU16(out, static_cast<uint16_t>(sample));
}

void testOversizedRejectsFromMetadata() {
  const fs::path path = fs::temp_directory_path() / "grooveputer_oversized.wav";
  constexpr std::size_t kPoolBytes = 32 * 1024;
  constexpr uint32_t kFrames = static_cast<uint32_t>(kPoolBytes / 2 + 1);
  writePcm16Wav(path, 1, 22050, kFrames, {});

  WavInfo info{};
  int16_t* pcm = reinterpret_cast<int16_t*>(0x1);
  assert(!loadWavFileBounded(path.string().c_str(), info, &pcm, kPoolBytes));
  assert(pcm == nullptr);
  assert(info.channels == 1);
  assert(info.numFrames == kFrames);
  fs::remove(path);
}

void testFittingMonoLoadsExactly() {
  const fs::path path = fs::temp_directory_path() / "grooveputer_mono.wav";
  const std::vector<int16_t> source = {100, -200, 300, -400};
  writePcm16Wav(path, 1, 22050, static_cast<uint32_t>(source.size()), source);

  WavInfo info{};
  int16_t* pcm = nullptr;
  assert(loadWavFileBounded(path.string().c_str(), info, &pcm,
                            source.size() * sizeof(int16_t)));
  assert(pcm != nullptr);
  assert(info.channels == 1);
  assert(info.numFrames == source.size());
  for (std::size_t i = 0; i < source.size(); ++i) assert(pcm[i] == source[i]);
  free(pcm);
  fs::remove(path);
}

void testFittingStereoDecodesIntoFinalMonoBuffer() {
  const fs::path path = fs::temp_directory_path() / "grooveputer_stereo.wav";
  const std::vector<int16_t> source = {1000, -1000, 3000, 1000};
  writePcm16Wav(path, 2, 22050, 2, source);

  WavInfo info{};
  int16_t* pcm = nullptr;
  assert(loadWavFileBounded(path.string().c_str(), info, &pcm, 4));
  assert(pcm != nullptr);
  assert(info.channels == 1);
  assert(info.numFrames == 2);
  assert(pcm[0] == 0);
  assert(pcm[1] == 2000);
  free(pcm);
  fs::remove(path);
}
}  // namespace

int main() {
  testOversizedRejectsFromMetadata();
  testFittingMonoLoadsExactly();
  testFittingStereoDecodesIntoFinalMonoBuffer();
  return 0;
}
