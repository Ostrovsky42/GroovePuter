#include "src/sampler/ram_sample_store.h"
#include "src/sampler/sample_loader.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string pathJoin(const std::string& dir, const char* name) {
  return dir + "/" + name;
}

void expectInspectFailure(const std::string& path, WavLoadError expected) {
  WavInspectResult result{};
  WavLoadError error = WavLoadError::Ok;
  const bool ok = inspectWavFileBounded(path.c_str(), result, 1024 * 1024, &error);
  if (ok || error != expected) {
    std::fprintf(stderr, "expected inspect failure %s for %s, got ok=%d error=%s\n",
                 wavLoadErrorName(expected), path.c_str(), ok ? 1 : 0,
                 wavLoadErrorName(error));
    std::abort();
  }
}

void expectAnyInspectFailure(const std::string& path) {
  WavInspectResult result{};
  WavLoadError error = WavLoadError::Ok;
  if (inspectWavFileBounded(path.c_str(), result, 1024 * 1024, &error)) {
    std::fprintf(stderr, "expected inspect failure for %s\n", path.c_str());
    std::abort();
  }
}

std::vector<int16_t> decode(const std::string& path,
                            WavInspectResult* inspectedOut = nullptr) {
  WavInspectResult inspected{};
  WavLoadError error = WavLoadError::Ok;
  if (!inspectWavFileBounded(path.c_str(), inspected, 1024 * 1024, &error)) {
    std::fprintf(stderr, "inspect failed for %s: %s\n", path.c_str(),
                 wavLoadErrorName(error));
    std::abort();
  }
  int16_t* pcm = nullptr;
  if (!decodeWavFileBounded(path.c_str(), inspected, &pcm, 1024 * 1024, &error)) {
    std::fprintf(stderr, "decode failed for %s: %s\n", path.c_str(),
                 wavLoadErrorName(error));
    std::abort();
  }
  std::vector<int16_t> result(pcm, pcm + inspected.info.numFrames);
  std::free(pcm);
  if (inspectedOut != nullptr) *inspectedOut = inspected;
  return result;
}

void overwriteSampleRate(const std::string& path, uint32_t rate) {
  // changed_after_inspect.wav is canonical RIFF: 12-byte RIFF header,
  // 8-byte fmt header, sampleRate at byte 24 and byteRate at byte 28.
  // Keep the mutated file structurally valid so decode reaches the explicit
  // inspect-result comparison instead of failing fmt validation first.
  std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
  assert(f.good());
  const uint32_t byteRate = rate * 2u;
  const char rateBytes[4] = {
      static_cast<char>(rate & 0xff),
      static_cast<char>((rate >> 8) & 0xff),
      static_cast<char>((rate >> 16) & 0xff),
      static_cast<char>((rate >> 24) & 0xff),
  };
  const char byteRateBytes[4] = {
      static_cast<char>(byteRate & 0xff),
      static_cast<char>((byteRate >> 8) & 0xff),
      static_cast<char>((byteRate >> 16) & 0xff),
      static_cast<char>((byteRate >> 24) & 0xff),
  };
  f.seekp(24);
  f.write(rateBytes, sizeof(rateBytes));
  f.write(byteRateBytes, sizeof(byteRateBytes));
  f.flush();
  assert(f.good());
}

void testStoreAdmission(const std::string& dir) {
  RamSampleStore store;
  store.setPoolSize(8);

  const SampleId monoId{1};
  const SampleId stereoId{2};
  const SampleId malformedId{3};

  assert(store.registerFile(monoId, pathJoin(dir, "valid_mono.wav")));
  assert(store.preload(monoId));
  assert(store.freePoolBytes() == 0);

  SampleHandle monoHandle = store.acquireHandle(monoId);
  assert(monoHandle.valid());
  SampleView monoView = store.viewHandle(monoHandle);
  assert(monoView.frames == 4);
  assert(monoView.sampleRate == 22050);
  assert(monoView.pcm[0] == 1000);

  // A pinned resident sample prevents destructive admission. The incoming
  // stereo source is 16 bytes on disk but only 8 decoded mono bytes.
  assert(store.registerFile(stereoId, pathJoin(dir, "valid_stereo.wav")));
  assert(!store.preload(stereoId));
  assert(!store.viewHandle(monoHandle).empty());

  store.releaseHandle(monoHandle);
  assert(store.preload(stereoId));
  assert(store.freePoolBytes() == 0);
  SampleHandle stereoHandle = store.acquireHandle(stereoId);
  assert(stereoHandle.valid());
  SampleView stereoView = store.viewHandle(stereoHandle);
  assert(stereoView.frames == 4);
  assert(stereoView.pcm[0] == 2000);
  assert(stereoView.pcm[1] == 0);
  store.releaseHandle(stereoHandle);

  // Malformed input is rejected by inspect before LRU eviction. The current
  // resident sample must remain available after the failed preload.
  assert(store.registerFile(malformedId, pathJoin(dir, "truncated_file.wav")));
  assert(!store.preload(malformedId));
  SampleHandle stillStereo = store.acquireHandle(stereoId);
  assert(stillStereo.valid());
  assert(!store.viewHandle(stillStereo).empty());
  store.releaseHandle(stillStereo);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <wav-corpus-dir>\n", argv[0]);
    return 2;
  }
  const std::string dir = argv[1];

  WavInspectResult monoInfo{};
  const auto mono = decode(pathJoin(dir, "valid_mono.wav"), &monoInfo);
  assert(monoInfo.info.sampleRate == 22050);
  assert(monoInfo.info.channels == 1);
  assert(monoInfo.info.bitsPerSample == 16);
  assert(monoInfo.info.numFrames == 4);
  assert(monoInfo.sourceChannels == 1);
  assert(monoInfo.sourceDataBytes == 8);
  assert(monoInfo.decodedBytes == 8);
  assert((mono == std::vector<int16_t>{1000, -1000, 32767, -32768}));

  WavInspectResult stereoInfo{};
  const auto stereo = decode(pathJoin(dir, "valid_stereo.wav"), &stereoInfo);
  assert(stereoInfo.sourceChannels == 2);
  assert(stereoInfo.sourceDataBytes == 16);
  assert(stereoInfo.info.numFrames == 4);
  assert(stereoInfo.decodedBytes == 8);
  assert((stereo == std::vector<int16_t>{2000, 0, 32767, -32768}));

  WavInspectResult multiInfo{};
  const auto stereoMulti = decode(pathJoin(dir, "valid_stereo_multichunk.wav"),
                                  &multiInfo);
  assert(multiInfo.sourceChannels == 2);
  assert(multiInfo.sourceDataBytes == 1200);
  assert(multiInfo.info.numFrames == 300);
  assert(multiInfo.decodedBytes == 600);
  assert(stereoMulti.size() == 300);
  // 512-byte source scratch holds 128 stereo frames. Check values on both
  // sides of that boundary and at the final frame to prove multi-iteration
  // decode preserves frame order and averaging.
  assert(stereoMulti[0] == 0);
  assert(stereoMulti[127] == 2794);
  assert(stereoMulti[128] == 2816);
  assert(stereoMulti[299] == 6578);

  assert(decode(pathJoin(dir, "odd_junk.wav")) == mono);
  assert(decode(pathJoin(dir, "odd_list.wav")) == mono);
  assert(decode(pathJoin(dir, "fmt_extension.wav")) == mono);
  assert(decode(pathJoin(dir, "data_before_fmt.wav")) == mono);
  assert(decode(pathJoin(dir, "valid_unknown_after_data.wav")) == mono);
  assert(decode(pathJoin(dir, "physical_trailing_bytes.wav")) == mono);

  expectInspectFailure(pathJoin(dir, "invalid_riff.wav"), WavLoadError::InvalidRiff);
  expectInspectFailure(pathJoin(dir, "riff_too_short.wav"), WavLoadError::InvalidRiff);
  expectInspectFailure(pathJoin(dir, "truncated_file.wav"), WavLoadError::Truncated);
  expectInspectFailure(pathJoin(dir, "truncated_riff.wav"), WavLoadError::Truncated);
  expectInspectFailure(pathJoin(dir, "oversized_chunk.wav"), WavLoadError::Truncated);
  expectAnyInspectFailure(pathJoin(dir, "missing_odd_pad.wav"));
  expectInspectFailure(pathJoin(dir, "fmt_too_short.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "missing_fmt.wav"), WavLoadError::MissingFmt);
  expectInspectFailure(pathJoin(dir, "missing_data.wav"), WavLoadError::MissingData);
  expectInspectFailure(pathJoin(dir, "float32.wav"), WavLoadError::UnsupportedEncoding);
  expectInspectFailure(pathJoin(dir, "pcm8.wav"), WavLoadError::UnsupportedEncoding);
  expectInspectFailure(pathJoin(dir, "pcm24.wav"), WavLoadError::UnsupportedEncoding);
  expectInspectFailure(pathJoin(dir, "zero_channels.wav"), WavLoadError::UnsupportedChannels);
  expectInspectFailure(pathJoin(dir, "three_channels.wav"), WavLoadError::UnsupportedChannels);
  expectInspectFailure(pathJoin(dir, "zero_rate.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "bad_block_align.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "bad_byte_rate.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "unaligned_data.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "empty_data.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "duplicate_fmt.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "duplicate_data.wav"), WavLoadError::InvalidFormat);
  expectInspectFailure(pathJoin(dir, "malformed_trailing_chunk.wav"), WavLoadError::Truncated);

  WavInspectResult budgetInfo{};
  WavLoadError budgetError = WavLoadError::Ok;
  assert(!inspectWavFileBounded(pathJoin(dir, "valid_mono.wav").c_str(),
                                budgetInfo, 7, &budgetError));
  assert(budgetError == WavLoadError::TooLarge);

  const std::string changedPath = pathJoin(dir, "changed_after_inspect.wav");
  WavInspectResult beforeChange{};
  WavLoadError changedError = WavLoadError::Ok;
  assert(inspectWavFileBounded(changedPath.c_str(), beforeChange,
                               1024 * 1024, &changedError));
  overwriteSampleRate(changedPath, 44100);
  int16_t* changedPcm = nullptr;
  assert(!decodeWavFileBounded(changedPath.c_str(), beforeChange, &changedPcm,
                               1024 * 1024, &changedError));
  assert(changedPcm == nullptr);
  assert(changedError == WavLoadError::ChangedAfterInspect);

  WavInfo compatInfo{};
  int16_t* compatPcm = nullptr;
  assert(loadWavFileBounded(pathJoin(dir, "valid_mono.wav").c_str(),
                            compatInfo, &compatPcm, 8));
  assert(compatInfo.numFrames == 4);
  std::free(compatPcm);

  testStoreAdmission(dir);

  std::puts("sampler WAV loader 0.9.5-A: PASS");
  return 0;
}
