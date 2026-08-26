#include <array>
#include <cstdint>
#include <cstdio>

#include "src/generation/migration/strong_rhythm_migration.h"

namespace {

using GroovePuterRhythm::PhraseTemporalCoordinates;
using GroovePuterRhythm::phraseTemporalCoordinatesForBar;
using GroovePuterRhythm::phraseVocabularyBarOrdinal;

bool checkLength(uint8_t phraseBars) {
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    const PhraseTemporalCoordinates coordinates =
        phraseTemporalCoordinatesForBar(bar);
    const uint8_t vocabularyOrdinal = phraseVocabularyBarOrdinal(bar);
    const uint8_t expectedVocabulary = static_cast<uint8_t>(bar % 4u);
    const uint8_t expectedEvolution = static_cast<uint8_t>(bar / 4u);

    if (coordinates.phraseBarOrdinal != bar ||
        vocabularyOrdinal != expectedVocabulary ||
        coordinates.evolutionOrdinal != expectedEvolution) {
      std::fprintf(
          stderr,
          "length=%u bar=%u got=(%u,%u,%u) expected=(%u,%u,%u)\n",
          static_cast<unsigned>(phraseBars),
          static_cast<unsigned>(bar),
          static_cast<unsigned>(coordinates.phraseBarOrdinal),
          static_cast<unsigned>(vocabularyOrdinal),
          static_cast<unsigned>(coordinates.evolutionOrdinal),
          static_cast<unsigned>(bar),
          static_cast<unsigned>(expectedVocabulary),
          static_cast<unsigned>(expectedEvolution));
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  constexpr std::array<uint8_t, 4> kLengths{{1, 2, 4, 8}};
  for (const uint8_t length : kLengths) {
    if (!checkLength(length)) return 1;
  }

  constexpr std::array<uint8_t, 8> kExpectedVocabulary{{
      0, 1, 2, 3, 0, 1, 2, 3,
  }};
  constexpr std::array<uint8_t, 8> kExpectedEvolution{{
      0, 0, 0, 0, 1, 1, 1, 1,
  }};

  for (uint8_t bar = 0; bar < 8; ++bar) {
    const PhraseTemporalCoordinates coordinates =
        phraseTemporalCoordinatesForBar(bar);
    if (coordinates.phraseBarOrdinal != bar ||
        phraseVocabularyBarOrdinal(bar) != kExpectedVocabulary[bar] ||
        coordinates.evolutionOrdinal != kExpectedEvolution[bar]) {
      return 2;
    }
  }

  std::puts("M4-A1 coordinates: PASS");
  std::puts("  lengths=1,2,4,8 global=0..N-1");
  std::puts("  length8_global=0,1,2,3,4,5,6,7");
  std::puts("  length8_vocabulary=0,1,2,3,0,1,2,3");
  std::puts("  length8_evolution=0,0,0,0,1,1,1,1");
  return 0;
}
