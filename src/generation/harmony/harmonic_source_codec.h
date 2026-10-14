#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_CODEC_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_CODEC_H

#include <cstddef>
#include <cstdint>

#include "song_harmonic_state.h"

namespace GroovePuterHarmony {

constexpr size_t kMaxSongHarmonicStateBytes = 2048;
constexpr uint16_t kSongHarmonicStateSchemaVersion = 1;

enum class SongHarmonicCodecStatus : uint8_t {
  Ok = 0,
  InvalidInput,
  InvalidData,
  Corrupt,
  UnsupportedVersion,
  BufferTooSmall,
};

SongHarmonicCodecStatus encodeSongHarmonicState(
    const SongHarmonicState& state, uint8_t* destination,
    size_t capacity, size_t& written);
SongHarmonicCodecStatus decodeSongHarmonicState(
    const uint8_t* source, size_t size, SongHarmonicState& out);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_CODEC_H
