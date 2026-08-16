#include "../src/sampler/ram_sample_store.h"
#include "../src/sampler/sample_loader.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void writeU16(std::ofstream& out, uint16_t value) {
  const char bytes[2] = {
      static_cast<char>(value & 0xFFu),
      static_cast<char>((value >> 8) & 0xFFu),
  };
  out.write(bytes, sizeof(bytes));
}

void writeU32(std::ofstream& out, uint32_t value) {
  const char bytes[4] = {
      static_cast<char>(value & 0xFFu),
      static_cast<char>((value >> 8) & 0xFFu),
      static_cast<char>((value >> 16) & 0xFFu),
      static_cast<char>((value >> 24) & 0xFFu),
  };
  out.write(bytes, sizeof(bytes));
}

void writePcm16Wav(const fs::path& path, uint16_t channels,
                   uint32_t sampleRate, uint32_t frames) {
  assert(channels == 1 || channels == 2);
  const uint16_t blockAlign = static_cast<uint16_t>(channels * 2u);
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint32_t dataBytes = frames * blockAlign;
  const uint32_t riffSize = 36u + dataBytes;

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  assert(out.good());
  out.write("RIFF", 4);
  writeU32(out, riffSize);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  writeU32(out, 16);
  writeU16(out, 1);  // PCM
  writeU16(out, channels);
  writeU32(out, sampleRate);
  writeU32(out, byteRate);
  writeU16(out, blockAlign);
  writeU16(out, 16);
  out.write("data", 4);
  writeU32(out, dataBytes);

  for (uint32_t frame = 0; frame < frames; ++frame) {
    const int16_t left = static_cast<int16_t>(1000 + (frame % 101));
    writeU16(out, static_cast<uint16_t>(left));
    if (channels == 2) {
      const int16_t right = static_cast<int16_t>(3000 + (frame % 101));
      writeU16(out, static_cast<uint16_t>(right));
    }
  }
  out.close();
  assert(out.good());
}

int16_t expectedMono(uint32_t frame) {
  return static_cast<int16_t>(1000 + (frame % 101));
}

int16_t expectedStereoDownmix(uint32_t frame) {
  return static_cast<int16_t>(2000 + (frame % 101));
}

void testFixedCacheContract() {
  static_assert(kSamplerStreamPageBytes == 512);
  static_assert(kSamplerStreamPageFrames == 256);
  static_assert(kSamplerStreamPageCount == 8);
  static_assert(kSamplerStreamCacheBytes == 4096);
  static_assert(kSamplerStreamIoHandleCount == 4);
  static_assert(kMaxSampleSlots == 64);
}

void testResidentFastPath(const fs::path& root) {
  const fs::path wav = root / "tiny.wav";
  writePcm16Wav(wav, 1, 22050, 120);  // 240 decoded bytes

  RamSampleStore store;
  assert(store.beginStreamingCache());
  assert(store.streamingCacheReady());
  assert(store.streamingCacheBytes() == 4096);

  const SampleId id{11};
  assert(store.registerFile(id, wav.string()));
  assert(store.preload(id));

  const SampleHandle handle = store.acquireHandle(id);
  assert(handle.valid());
  const SampleSourceInfo info = store.sourceInfoHandle(handle);
  assert(info.valid());
  assert(info.storage == SampleStorageKind::Resident);
  assert(info.frames == 120);
  assert(info.sampleRate == 22050);

  const SampleView view = store.viewHandle(handle);
  assert(!view.empty());
  assert(view.pcm[0] == expectedMono(0));
  assert(view.pcm[119] == expectedMono(119));

  int16_t frame = 0;
  assert(store.readFrameHandle(handle, 57, frame));
  assert(frame == expectedMono(57));
  store.releaseHandle(handle);
}

void testMonoStreamingAndRequestDedup(const fs::path& root) {
  const fs::path wav = root / "mono_long.wav";
  writePcm16Wav(wav, 1, 22050, 1600);  // 3200 decoded bytes -> streamed

  RamSampleStore store;
  assert(store.beginStreamingCache());
  const SampleId id{21};
  assert(store.registerFile(id, wav.string()));
  assert(store.preload(id));

  const SampleHandle handle = store.acquireHandle(id);
  assert(handle.valid());
  const SampleSourceInfo info = store.sourceInfoHandle(handle);
  assert(info.valid());
  assert(info.storage == SampleStorageKind::Streamed);
  assert(info.frames == 1600);
  assert(info.sampleRate == 22050);
  assert(store.viewHandle(handle).pcm == nullptr);

  int16_t frame = 0;
  assert(store.readFrameHandle(handle, 0, frame));
  assert(frame == expectedMono(0));
  assert(store.readFrameHandle(handle, 255, frame));
  assert(frame == expectedMono(255));
  assert(!store.readFrameHandle(handle, 300, frame));

  const SamplerStreamStats before = store.streamStats();
  assert(before.pagesLoaded == 1);  // synchronous head page

  for (int i = 0; i < 100; ++i) {
    assert(store.requestFrameHandle(handle, 300));
  }
  store.serviceIo(1);

  const SamplerStreamStats after = store.streamStats();
  assert(after.pagesLoaded == before.pagesLoaded + 1);
  assert(after.requestDrops == 0);
  assert(store.readFrameHandle(handle, 300, frame));
  assert(frame == expectedMono(300));

  assert(store.requestFrameHandle(handle, 900));
  store.serviceIo(1);
  assert(store.readFrameHandle(handle, 900, frame));
  assert(frame == expectedMono(900));

  store.releaseHandle(handle);
}

void testStereoStreamingDownmix(const fs::path& root) {
  const fs::path wav = root / "stereo_long.wav";
  writePcm16Wav(wav, 2, 22050, 1600);  // decoded mono 3200 bytes

  RamSampleStore store;
  assert(store.beginStreamingCache());
  const SampleId id{31};
  assert(store.registerFile(id, wav.string()));
  assert(store.preload(id));

  const SampleHandle handle = store.acquireHandle(id);
  assert(handle.valid());
  const SampleSourceInfo info = store.sourceInfoHandle(handle);
  assert(info.storage == SampleStorageKind::Streamed);

  int16_t frame = 0;
  assert(store.readFrameHandle(handle, 0, frame));
  assert(frame == expectedStereoDownmix(0));

  assert(store.requestFrameHandle(handle, 300));
  store.serviceIo(1);
  assert(store.readFrameHandle(handle, 300, frame));
  assert(frame == expectedStereoDownmix(300));

  store.releaseHandle(handle);
}

void testStreamDescriptorSurvivesResidentEviction(const fs::path& root) {
  const fs::path longWav = root / "long.wav";
  const fs::path tinyWav = root / "tiny2.wav";
  writePcm16Wav(longWav, 1, 22050, 1600);
  writePcm16Wav(tinyWav, 1, 22050, 120);

  RamSampleStore store;
  assert(store.beginStreamingCache());
  store.setPoolSize(512);
  const SampleId longId{41};
  const SampleId tinyId{42};
  assert(store.registerFile(longId, longWav.string()));
  assert(store.registerFile(tinyId, tinyWav.string()));
  assert(store.preload(longId));
  assert(store.preload(tinyId));

  const SampleHandle streamHandle = store.acquireHandle(longId);
  assert(streamHandle.valid());
  assert(store.sourceInfoHandle(streamHandle).storage ==
         SampleStorageKind::Streamed);

  // Resident pool accounting is independent from streamed descriptors/cache.
  assert(store.freePoolBytes() == 512u - 240u);
  store.releaseHandle(streamHandle);
}

}  // namespace

int main() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sampler_stream_cache";
  fs::remove_all(root);
  fs::create_directories(root);

  testFixedCacheContract();
  testResidentFastPath(root);
  testMonoStreamingAndRequestDedup(root);
  testStereoStreamingDownmix(root);
  testStreamDescriptorSurvivesResidentEviction(root);

  fs::remove_all(root);
  return 0;
}
