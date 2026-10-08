#pragma once

#include <cstdint>

// 0.9.18 genre idioms: one template = bass + chord stabs + melody, written as a
// pair so the parts interlock (see genre_idiom.h).
namespace GenreIdiom {

enum : uint8_t {
  kAccent = 1,
  kSlideOut = 2,  // 303 convention: glide from this note into the next one
};

// step 0..15; semi relative to C2 (bass) or C4 (melody), in C minor.
struct IdiomNote {
  uint8_t step;
  int8_t semi;
  uint8_t len;
  uint8_t velocity;
  uint8_t flags;
};

// root relative to C; intervalMask bit n = chord tone n semitones above root.
struct IdiomStab {
  uint8_t step;
  int8_t root;
  uint32_t intervalMask;
  uint8_t len;
  uint8_t velocity;
};

struct IdiomLevel {
  const IdiomNote* bass;
  uint8_t bassCount;
  const IdiomStab* stabs;
  uint8_t stabCount;
  const IdiomNote* melody;
  uint8_t melodyCount;
};

// levels[0..2] = the dataset's P1 BASE, P2 VARIATION, P3 sparse drop.
struct IdiomVariant {
  const char* name;
  IdiomLevel levels[3];
};

}  // namespace GenreIdiom
