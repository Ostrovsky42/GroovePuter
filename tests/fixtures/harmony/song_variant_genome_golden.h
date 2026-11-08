#ifndef GROOVEPUTER_TESTS_FIXTURES_HARMONY_SONG_VARIANT_GENOME_GOLDEN_H
#define GROOVEPUTER_TESTS_FIXTURES_HARMONY_SONG_VARIANT_GENOME_GOLDEN_H

#include <array>
#include <cstdint>

namespace GroovePuterHarmonyTestFixtures {

constexpr uint16_t kGoldenModelVersion = 7;
constexpr char kCorpusModelSha256[] =
    "9a0b50e81abf7e21dd9c2b33213721ff557a54db16b623f72b22193dbe0604bf";
constexpr uint32_t kGoldenSongIdentity = 0x1234abcdu;
constexpr uint8_t kGoldenStyleId = 4;  // UK Garage.

struct GenomeGoldenVector {
  uint32_t projectSeed;
  std::array<uint8_t, 14> bytes;
};

constexpr std::array<GenomeGoldenVector, 4> kGenomeGoldenVectors{{
    {0u, {{79, 143, 83, 149, 119, 91, 42, 70, 40, 122, 116, 87, 77, 142}}},
    {1u, {{69, 182, 91, 130, 139, 85, 45, 84, 40, 115, 109, 92, 90, 115}}},
    {42u, {{80, 156, 85, 122, 140, 84, 43, 97, 47, 100, 123, 76, 88, 105}}},
    {0xffffffffu,
     {{86, 170, 109, 129, 139, 80, 42, 79, 41, 117, 123, 90, 80, 134}}},
}};

}  // namespace GroovePuterHarmonyTestFixtures

#endif  // GROOVEPUTER_TESTS_FIXTURES_HARMONY_SONG_VARIANT_GENOME_GOLDEN_H
