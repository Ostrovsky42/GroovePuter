#pragma once

#include <cstdint>
#include <string>

namespace GroovePuterSampler {

// Stable control-side identity for a sample file.
//
// SampleRef deliberately stays out of the audio-thread slot ABI in 0.9.3-B.
// It is derived from a canonical logical path, so directory enumeration order
// and the Cardputer "/sd/" mount alias do not affect identity.
struct SampleRef {
  uint64_t value = 0;

  bool valid() const { return value != 0; }
  bool operator==(const SampleRef& other) const { return value == other.value; }
  bool operator!=(const SampleRef& other) const { return value != other.value; }
};

static_assert(sizeof(SampleRef) == sizeof(uint64_t),
              "SampleRef must remain a compact 64-bit value");

inline std::string canonicalSampleKey(const std::string& path) {
  std::string normalized;
  normalized.reserve(path.size());

  bool previousSlash = false;
  for (char raw : path) {
    const char c = raw == '\\' ? '/' : raw;
    if (c == '/') {
      if (previousSlash) continue;
      previousSlash = true;
    } else {
      previousSlash = false;
    }
    normalized.push_back(c);
  }

  while (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }

  // Cardputer code historically probes both /sd/... and /... for the same
  // logical SD content. Treat the leading sd/ component as a mount alias.
  if (normalized.rfind("sd/", 0) == 0) {
    normalized.erase(0, 3);
  }

  while (normalized.rfind("./", 0) == 0) {
    normalized.erase(0, 2);
  }

  std::size_t dotSegment = std::string::npos;
  while ((dotSegment = normalized.find("/./")) != std::string::npos) {
    normalized.erase(dotSegment, 2);
  }

  if (normalized == ".") normalized.clear();
  return normalized;
}

inline uint64_t fnv1a64(const std::string& value) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  // Zero is reserved for "no sample" throughout the sampler subsystem.
  return hash == 0 ? 1 : hash;
}

inline SampleRef stableSampleRefForPath(const std::string& path) {
  const std::string key = canonicalSampleKey(path);
  if (key.empty()) return {};
  return {fnv1a64(key)};
}

}  // namespace GroovePuterSampler
