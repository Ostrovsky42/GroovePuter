#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "sample_ref.h"

class SampleIndex;

namespace GroovePuterSampler {

enum class SceneSampleFilterDirection : uint8_t {
  Load,
  Save,
};

void setScenePersistenceSampleIndex(const SampleIndex* index);
const SampleIndex* scenePersistenceSampleIndex();

bool encodeSampleRefHex(SampleRef ref, char out[17]);
bool decodeSampleRefHex(const char* data, std::size_t len, SampleRef& out);

class SamplerSceneFilter {
public:
  static constexpr std::size_t kSamplerPadCount = 16;
  static constexpr std::size_t kMaxPadObjectBytes = 384;
  static constexpr std::size_t kMaxOutputBytes = 448;

  SamplerSceneFilter(SceneSampleFilterDirection direction,
                     const SampleIndex* index = nullptr);

  bool accept(char c, const char*& data, std::size_t& len);
  bool finish();
  bool failed() const { return failed_; }

private:
  bool acceptOutsidePad(char c, const char*& data, std::size_t& len);
  bool acceptPad(char c, const char*& data, std::size_t& len);
  bool transformPad(const char* input, std::size_t inputLen,
                    char* output, std::size_t outputCap,
                    std::size_t& outputLen);
  void updateGlobalStringState(char c);
  bool appendPadChar(char c);

  SceneSampleFilterDirection direction_;
  const SampleIndex* index_;

  bool failed_ = false;
  bool finished_ = false;
  bool inSamplerArray_ = false;
  bool bufferingPad_ = false;
  int padBraceDepth_ = 0;
  bool padInString_ = false;
  bool padEscape_ = false;

  bool globalInString_ = false;
  bool globalEscape_ = false;
  char globalToken_[32] = {};
  std::size_t globalTokenLen_ = 0;
  bool globalTokenOverflow_ = false;
  bool candidateSamplerKey_ = false;
  bool waitingSamplerArray_ = false;

  char single_[1] = {};
  char padBuffer_[kMaxPadObjectBytes] = {};
  std::size_t padLen_ = 0;
  char outputBuffer_[kMaxOutputBytes] = {};
  std::size_t samplerPadIndex_ = 0;
  SampleRef pendingUnresolvedRefs_[kSamplerPadCount] = {};
};

bool transformSamplerSceneString(const std::string& input,
                                 SceneSampleFilterDirection direction,
                                 const SampleIndex* index,
                                 std::string& output);

namespace detail {
template <typename Writer>
auto writeBytesImpl(Writer& writer, const char* data, std::size_t len, int)
    -> decltype(writer.write(reinterpret_cast<const uint8_t*>(data), len),
                std::size_t()) {
  return static_cast<std::size_t>(
      writer.write(reinterpret_cast<const uint8_t*>(data), len));
}

template <typename Writer>
auto writeBytesImpl(Writer& writer, const char* data, std::size_t len, long)
    -> decltype(writer.write(data, len), std::size_t()) {
  return static_cast<std::size_t>(writer.write(data, len));
}

template <typename Writer>
std::size_t writeBytes(Writer& writer, const char* data, std::size_t len) {
  return writeBytesImpl(writer, data, len, 0);
}
}  // namespace detail

template <typename Writer>
class SamplerSceneWriteFilter {
public:
  explicit SamplerSceneWriteFilter(Writer& writer,
                                   const SampleIndex* index = nullptr)
      : writer_(writer),
        filter_(SceneSampleFilterDirection::Save,
                index ? index : scenePersistenceSampleIndex()) {}

  std::size_t write(const uint8_t* data, std::size_t len) {
    std::size_t consumed = 0;
    for (; consumed < len; ++consumed) {
      const char* out = nullptr;
      std::size_t outLen = 0;
      if (!filter_.accept(static_cast<char>(data[consumed]), out, outLen)) {
        break;
      }
      if (outLen > 0 && detail::writeBytes(writer_, out, outLen) != outLen) {
        break;
      }
    }
    return consumed;
  }

  std::size_t write(const char* data, std::size_t len) {
    return write(reinterpret_cast<const uint8_t*>(data), len);
  }

  bool finish() { return filter_.finish() && !filter_.failed(); }
  bool failed() const { return filter_.failed(); }

private:
  Writer& writer_;
  SamplerSceneFilter filter_;
};

template <typename Reader>
class SamplerSceneReadFilter {
public:
  explicit SamplerSceneReadFilter(Reader& reader,
                                  const SampleIndex* index = nullptr)
      : reader_(reader),
        filter_(SceneSampleFilterDirection::Load,
                index ? index : scenePersistenceSampleIndex()) {}

  int read() {
    if (pendingPos_ < pendingLen_) {
      return static_cast<unsigned char>(pending_[pendingPos_++]);
    }
    pending_ = nullptr;
    pendingPos_ = 0;
    pendingLen_ = 0;

    while (true) {
      const int value = reader_.read();
      if (value < 0) {
        eof_ = true;
        finalizeFilter();
        return -1;
      }

      const char* out = nullptr;
      std::size_t outLen = 0;
      if (!filter_.accept(static_cast<char>(value), out, outLen)) {
        failed_ = true;
        return -1;
      }
      if (outLen == 0) continue;

      pending_ = out;
      pendingLen_ = outLen;
      pendingPos_ = 1;
      return static_cast<unsigned char>(pending_[0]);
    }
  }

  bool failed() {
    finalizeFilter();
    return failed_ || filter_.failed();
  }
  bool eof() const { return eof_; }

private:
  void finalizeFilter() {
    if (filterFinalized_) return;
    filterFinalized_ = true;
    if (!filter_.finish()) failed_ = true;
  }

  Reader& reader_;
  SamplerSceneFilter filter_;
  const char* pending_ = nullptr;
  std::size_t pendingPos_ = 0;
  std::size_t pendingLen_ = 0;
  bool failed_ = false;
  bool eof_ = false;
  bool filterFinalized_ = false;
};

}  // namespace GroovePuterSampler
