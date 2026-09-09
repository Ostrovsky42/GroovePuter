#pragma once
#ifndef GROOVEPUTER_STATE_SONG_CELL_H
#define GROOVEPUTER_STATE_SONG_CELL_H

#include <cstdint>
#include <type_traits>

namespace GroovePuterSong {

class SongCell {
 public:
  static constexpr int16_t kEmptyRaw = -1;
  static constexpr int16_t kRestRaw = -2;
  static constexpr int kMinPatternRef = 0;
  static constexpr int kMaxPatternRef = 255;

  constexpr SongCell() : raw_(kEmptyRaw) {}

  // Temporary compatibility for the staged O1 migration. Task 5 removes raw
  // Song semantic interpretation from consumers while keeping this bounded
  // value type as the only occurrence storage authority.
  constexpr SongCell(int raw) : raw_(sanitizeRaw(raw)) {}

  static constexpr SongCell empty() { return SongCell(kEmptyRaw); }
  static constexpr SongCell rest() { return SongCell(kRestRaw); }

  static constexpr SongCell material(int patternRef) {
    return patternRef >= kMinPatternRef && patternRef <= kMaxPatternRef
        ? SongCell(patternRef)
        : empty();
  }

  static constexpr SongCell fromRaw(int16_t raw) { return SongCell(raw); }

  constexpr bool isEmpty() const { return raw_ == kEmptyRaw; }
  constexpr bool isRest() const { return raw_ == kRestRaw; }
  constexpr bool hasMaterial() const {
    return raw_ >= kMinPatternRef && raw_ <= kMaxPatternRef;
  }
  constexpr int patternRef() const { return hasMaterial() ? raw_ : -1; }
  constexpr int16_t rawValue() const { return raw_; }

  constexpr operator int16_t() const { return raw_; }

  friend constexpr bool operator==(SongCell lhs, SongCell rhs) {
    return lhs.raw_ == rhs.raw_;
  }
  friend constexpr bool operator!=(SongCell lhs, SongCell rhs) {
    return !(lhs == rhs);
  }

 private:
  static constexpr int16_t sanitizeRaw(int raw) {
    return raw == kEmptyRaw || raw == kRestRaw ||
                   (raw >= kMinPatternRef && raw <= kMaxPatternRef)
        ? static_cast<int16_t>(raw)
        : kEmptyRaw;
  }

  int16_t raw_;
};

static_assert(sizeof(SongCell) == sizeof(int16_t),
              "O1 SongCell must remain exactly two bytes");
static_assert(std::is_trivially_copyable<SongCell>::value,
              "O1 SongCell must remain trivially copyable");

}  // namespace GroovePuterSong

#endif  // GROOVEPUTER_STATE_SONG_CELL_H
