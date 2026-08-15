#include "sample_scene_persistence.h"

#include "sample_index.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>

namespace GroovePuterSampler {
namespace {

const SampleIndex* g_scenePersistenceSampleIndex = nullptr;
SampleRef g_unresolvedSampleRefs[SamplerSceneFilter::kSamplerPadCount] = {};

struct FlatPadFields {
  bool hasId = false;
  std::size_t idStart = 0;
  std::size_t idEnd = 0;
  uint32_t id = 0;

  bool hasRef = false;
  char ref[17] = {};
  std::size_t refLen = 0;
};

bool isWs(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool parseUint32(const char* data, std::size_t begin, std::size_t end,
                 uint32_t& out) {
  if (begin >= end) return false;
  uint64_t value = 0;
  for (std::size_t i = begin; i < end; ++i) {
    const unsigned char c = static_cast<unsigned char>(data[i]);
    if (!std::isdigit(c)) return false;
    value = value * 10u + static_cast<uint64_t>(c - '0');
    if (value > std::numeric_limits<uint32_t>::max()) return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool findFlatPadFields(const char* data, std::size_t len,
                       FlatPadFields& fields) {
  if (len < 2 || data[0] != '{' || data[len - 1] != '}') return false;

  std::size_t i = 1;
  while (i + 1 < len) {
    while (i + 1 < len && (isWs(data[i]) || data[i] == ',')) ++i;
    if (i + 1 >= len || data[i] == '}') break;
    if (data[i] != '"') return false;
    ++i;

    char key[8] = {};
    std::size_t keyLen = 0;
    bool escaped = false;
    while (i < len) {
      const char c = data[i++];
      if (escaped) return false;
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == '"') break;
      if (keyLen + 1 < sizeof(key)) key[keyLen++] = c;
    }
    key[keyLen] = '\0';

    while (i < len && isWs(data[i])) ++i;
    if (i >= len || data[i++] != ':') return false;
    while (i < len && isWs(data[i])) ++i;
    if (i >= len) return false;

    if (std::strcmp(key, "id") == 0) {
      if (fields.hasId) return false;
      const std::size_t start = i;
      while (i < len &&
             std::isdigit(static_cast<unsigned char>(data[i]))) ++i;
      if (start == i) return false;
      uint32_t parsed = 0;
      if (!parseUint32(data, start, i, parsed)) return false;
      fields.hasId = true;
      fields.idStart = start;
      fields.idEnd = i;
      fields.id = parsed;
    } else if (std::strcmp(key, "ref") == 0) {
      if (fields.hasRef || data[i] != '"') return false;
      ++i;
      const std::size_t start = i;
      while (i < len && data[i] != '"') {
        if (data[i] == '\\') return false;
        ++i;
      }
      if (i >= len) return false;
      const std::size_t end = i;
      ++i;
      const std::size_t refLen = end - start;
      if (refLen > 16) return false;
      std::memcpy(fields.ref, data + start, refLen);
      fields.ref[refLen] = '\0';
      fields.refLen = refLen;
      fields.hasRef = true;
    } else {
      // Current sampler pad values are flat scalars. Unknown scalar keys are
      // preserved byte-for-byte so future pad parameters remain forward-safe.
      if (data[i] == '"') {
        ++i;
        bool valueEscape = false;
        while (i < len) {
          const char c = data[i++];
          if (valueEscape) {
            valueEscape = false;
            continue;
          }
          if (c == '\\') {
            valueEscape = true;
            continue;
          }
          if (c == '"') break;
        }
      } else {
        while (i < len && data[i] != ',' && data[i] != '}') ++i;
      }
    }

    while (i < len && isWs(data[i])) ++i;
    if (i < len && data[i] == ',') ++i;
  }

  return fields.hasId;
}

bool appendBytes(char* output, std::size_t cap, std::size_t& pos,
                 const char* data, std::size_t len) {
  if (pos + len > cap) return false;
  std::memcpy(output + pos, data, len);
  pos += len;
  return true;
}

bool appendUint32(char* output, std::size_t cap, std::size_t& pos,
                  uint32_t value) {
  char buf[16];
  const int written = std::snprintf(buf, sizeof(buf), "%u",
                                    static_cast<unsigned>(value));
  return written > 0 &&
         appendBytes(output, cap, pos, buf,
                     static_cast<std::size_t>(written));
}

bool appendRefField(char* output, std::size_t cap, std::size_t& pos,
                    SampleRef ref) {
  char refHex[17] = {};
  if (!encodeSampleRefHex(ref, refHex)) return false;
  return appendBytes(output, cap, pos, ",\"ref\":\"", 8) &&
         appendBytes(output, cap, pos, refHex, 16) &&
         appendBytes(output, cap, pos, "\"", 1);
}

}  // namespace

void setScenePersistenceSampleIndex(const SampleIndex* index) {
  g_scenePersistenceSampleIndex = index;
}

const SampleIndex* scenePersistenceSampleIndex() {
  return g_scenePersistenceSampleIndex;
}

bool encodeSampleRefHex(SampleRef ref, char out[17]) {
  if (!ref.valid() || out == nullptr) return false;
  const int written = std::snprintf(
      out, 17, "%08x%08x",
      static_cast<unsigned>(ref.value >> 32),
      static_cast<unsigned>(ref.value & 0xFFFFFFFFULL));
  return written == 16;
}

bool decodeSampleRefHex(const char* data, std::size_t len, SampleRef& out) {
  out = {};
  if (data == nullptr || len != 16) return false;

  uint64_t value = 0;
  for (std::size_t i = 0; i < len; ++i) {
    const unsigned char c = static_cast<unsigned char>(data[i]);
    uint8_t nibble = 0;
    if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
    else if (c >= 'a' && c <= 'f') {
      nibble = static_cast<uint8_t>(10 + c - 'a');
    } else if (c >= 'A' && c <= 'F') {
      nibble = static_cast<uint8_t>(10 + c - 'A');
    } else {
      return false;
    }
    value = (value << 4) | nibble;
  }
  if (value == 0) return false;
  out.value = value;
  return true;
}

SamplerSceneFilter::SamplerSceneFilter(SceneSampleFilterDirection direction,
                                       const SampleIndex* index)
    : direction_(direction),
      index_(index ? index : scenePersistenceSampleIndex()) {
  if (direction_ == SceneSampleFilterDirection::Load) {
    // A new Scene load owns the unresolved-asset sidecar. Failed loads leave
    // it empty; only finish() publishes refs from a successfully parsed Scene.
    for (auto& ref : g_unresolvedSampleRefs) ref = {};
  }
}

bool SamplerSceneFilter::appendPadChar(char c) {
  if (padLen_ >= kMaxPadObjectBytes) {
    failed_ = true;
    return false;
  }
  padBuffer_[padLen_++] = c;
  return true;
}

void SamplerSceneFilter::updateGlobalStringState(char c) {
  if (globalInString_) {
    if (globalEscape_) {
      globalEscape_ = false;
      return;
    }
    if (c == '\\') {
      globalEscape_ = true;
      return;
    }
    if (c == '"') {
      globalInString_ = false;
      candidateSamplerKey_ =
          !globalTokenOverflow_ &&
          globalTokenLen_ == std::strlen("samplerPads") &&
          std::memcmp(globalToken_, "samplerPads", globalTokenLen_) == 0;
      return;
    }
    if (globalTokenLen_ + 1 < sizeof(globalToken_)) {
      globalToken_[globalTokenLen_++] = c;
    } else {
      globalTokenOverflow_ = true;
    }
    return;
  }

  if (c == '"') {
    globalInString_ = true;
    globalEscape_ = false;
    globalTokenLen_ = 0;
    globalTokenOverflow_ = false;
  }
}

bool SamplerSceneFilter::acceptOutsidePad(char c, const char*& data,
                                          std::size_t& len) {
  single_[0] = c;
  data = single_;
  len = 1;

  if (inSamplerArray_) {
    if (c == '{') {
      bufferingPad_ = true;
      padLen_ = 0;
      padBraceDepth_ = 1;
      padInString_ = false;
      padEscape_ = false;
      len = 0;
      return appendPadChar(c);
    }
    if (c == ']') inSamplerArray_ = false;
    return true;
  }

  const bool wasInString = globalInString_;
  updateGlobalStringState(c);
  if (wasInString || globalInString_) return true;

  if (candidateSamplerKey_) {
    if (isWs(c)) return true;
    if (c == ':') {
      candidateSamplerKey_ = false;
      waitingSamplerArray_ = true;
      return true;
    }
    candidateSamplerKey_ = false;
  } else if (waitingSamplerArray_) {
    if (isWs(c)) return true;
    waitingSamplerArray_ = false;
    if (c == '[') inSamplerArray_ = true;
  }

  return true;
}

bool SamplerSceneFilter::transformPad(
    const char* input, std::size_t inputLen, char* output,
    std::size_t outputCap, std::size_t& outputLen) {
  outputLen = 0;
  FlatPadFields fields;
  if (!findFlatPadFields(input, inputLen, fields)) return false;

  if (direction_ == SceneSampleFilterDirection::Load) {
    if (!fields.hasRef) {
      if (fields.id == 0 || index_ == nullptr) {
        return appendBytes(output, outputCap, outputLen, input, inputLen);
      }

      const std::size_t matches =
          index_->legacyMatchCount(SampleId{fields.id});
      if (matches > 1) return false;
      if (matches == 0) {
        // Legacy ID-only Scenes have no stable path to retain. Preserve the
        // historical missing-ID behavior without guessing another file.
        return appendBytes(output, outputCap, outputLen, input, inputLen);
      }

      const SampleId runtime =
          index_->runtimeIdForLegacyId(SampleId{fields.id});
      if (runtime.value == 0) return false;
      if (!appendBytes(output, outputCap, outputLen,
                       input, fields.idStart)) return false;
      if (!appendUint32(output, outputCap, outputLen,
                        runtime.value)) return false;
      return appendBytes(output, outputCap, outputLen,
                         input + fields.idEnd,
                         inputLen - fields.idEnd);
    }

    SampleRef ref{};
    if (!decodeSampleRefHex(fields.ref, fields.refLen, ref)) return false;

    const SampleId runtime =
        index_ ? index_->runtimeIdForRef(ref) : SampleId{0};
    if (runtime.value == 0 && samplerPadIndex_ < kSamplerPadCount) {
      // External asset is missing, not the project. Keep the stable identity
      // in a control-side sidecar and publish a silent runtime pad.
      pendingUnresolvedRefs_[samplerPadIndex_] = ref;
    }

    if (!appendBytes(output, outputCap, outputLen,
                     input, fields.idStart)) return false;
    if (!appendUint32(output, outputCap, outputLen,
                      runtime.value)) return false;
    return appendBytes(output, outputCap, outputLen,
                       input + fields.idEnd,
                       inputLen - fields.idEnd);
  }

  // Save path: SceneManager gives us runtime IDs only. Persist an exact stable
  // ref while keeping the historical basename ID as the compatibility field.
  if (fields.hasRef) return false;

  if (fields.id == 0) {
    const SampleRef unresolved = samplerPadIndex_ < kSamplerPadCount
        ? g_unresolvedSampleRefs[samplerPadIndex_]
        : SampleRef{};
    if (!unresolved.valid()) {
      return appendBytes(output, outputCap, outputLen, input, inputLen);
    }

    if (!appendBytes(output, outputCap, outputLen,
                     input, fields.idEnd)) return false;
    if (!appendRefField(output, outputCap, outputLen, unresolved)) return false;
    return appendBytes(output, outputCap, outputLen,
                       input + fields.idEnd,
                       inputLen - fields.idEnd);
  }

  if (index_ == nullptr) {
    return appendBytes(output, outputCap, outputLen, input, inputLen);
  }

  const SampleRef ref = index_->resolveRuntimeId(SampleId{fields.id});
  const SampleFileInfo* file = index_->findByRef(ref);
  if (!ref.valid() || file == nullptr) {
    // A legacy assignment whose WAV disappeared remains legacy-only. Do not
    // invent stable identity for a path that is not in the current registry.
    return appendBytes(output, outputCap, outputLen, input, inputLen);
  }

  if (!appendBytes(output, outputCap, outputLen,
                   input, fields.idStart)) return false;
  if (!appendUint32(output, outputCap, outputLen,
                    file->id.value)) return false;
  if (!appendRefField(output, outputCap, outputLen, ref)) return false;
  return appendBytes(output, outputCap, outputLen,
                     input + fields.idEnd,
                     inputLen - fields.idEnd);
}

bool SamplerSceneFilter::acceptPad(char c, const char*& data,
                                   std::size_t& len) {
  data = nullptr;
  len = 0;
  if (!appendPadChar(c)) return false;

  if (padInString_) {
    if (padEscape_) {
      padEscape_ = false;
    } else if (c == '\\') {
      padEscape_ = true;
    } else if (c == '"') {
      padInString_ = false;
    }
  } else {
    if (c == '"') {
      padInString_ = true;
    } else if (c == '{') {
      ++padBraceDepth_;
    } else if (c == '}') {
      --padBraceDepth_;
    }
  }

  if (padBraceDepth_ != 0) return true;

  bufferingPad_ = false;
  std::size_t transformedLen = 0;
  if (!transformPad(padBuffer_, padLen_, outputBuffer_, kMaxOutputBytes,
                    transformedLen)) {
    failed_ = true;
    return false;
  }
  ++samplerPadIndex_;
  data = outputBuffer_;
  len = transformedLen;
  return true;
}

bool SamplerSceneFilter::accept(char c, const char*& data,
                                std::size_t& len) {
  if (failed_) return false;
  if (bufferingPad_) return acceptPad(c, data, len);
  return acceptOutsidePad(c, data, len);
}

bool SamplerSceneFilter::finish() {
  if (finished_) return !failed_;
  if (failed_) return false;
  if (bufferingPad_ || inSamplerArray_ || globalInString_ ||
      waitingSamplerArray_) {
    failed_ = true;
    return false;
  }

  if (direction_ == SceneSampleFilterDirection::Load) {
    for (std::size_t i = 0; i < kSamplerPadCount; ++i) {
      g_unresolvedSampleRefs[i] = pendingUnresolvedRefs_[i];
    }
  }
  finished_ = true;
  return true;
}

bool transformSamplerSceneString(const std::string& input,
                                 SceneSampleFilterDirection direction,
                                 const SampleIndex* index,
                                 std::string& output) {
  output.clear();
  output.reserve(input.size() + 16 * 32);
  SamplerSceneFilter filter(direction, index);
  for (char c : input) {
    const char* emitted = nullptr;
    std::size_t emittedLen = 0;
    if (!filter.accept(c, emitted, emittedLen)) {
      output.clear();
      return false;
    }
    if (emittedLen > 0) output.append(emitted, emittedLen);
  }
  if (!filter.finish()) {
    output.clear();
    return false;
  }
  return true;
}

}  // namespace GroovePuterSampler
