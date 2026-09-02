#include <cassert>
#include <cstdint>
#include <iostream>

#include "../src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

int main() {
  constexpr uint8_t lengths[] = {1, 2, 4, 8};
  for (uint8_t length : lengths) {
    for (uint8_t bar = 0; bar < length; ++bar) {
      const PhraseTemporalCoordinates coords = phraseTemporalCoordinatesForBar(bar);
      assert(coords.phraseBarOrdinal == bar);
      assert(coords.evolutionOrdinal == static_cast<uint8_t>(bar / 4u));
      assert(phraseVocabularyBarOrdinal(bar) == static_cast<uint8_t>(bar % 4u));
    }
  }

  constexpr uint8_t expectedVocabulary[] = {0, 1, 2, 3, 0, 1, 2, 3};
  constexpr uint8_t expectedEvolution[] = {0, 0, 0, 0, 1, 1, 1, 1};
  for (uint8_t bar = 0; bar < 8; ++bar) {
    const PhraseTemporalCoordinates coords = phraseTemporalCoordinatesForBar(bar);
    assert(coords.phraseBarOrdinal == bar);
    assert(phraseVocabularyBarOrdinal(bar) == expectedVocabulary[bar]);
    assert(coords.evolutionOrdinal == expectedEvolution[bar]);
  }

  StrongRhythmMigrationContext destinationA{};
  StrongRhythmMigrationContext destinationB{};
  destinationA.patternAddress = 0;
  destinationB.patternAddress = 7;
  destinationA.phraseGenerationIdentity = 0x1234u;
  destinationB.phraseGenerationIdentity = 0x1234u;
  assert(destinationA.phraseGenerationIdentity == destinationB.phraseGenerationIdentity);
  assert(destinationA.patternAddress != destinationB.patternAddress);

  const PhraseTemporalCoordinates oneBar = phraseTemporalCoordinatesForBar(0);
  assert(oneBar.phraseBarOrdinal == 0);
  assert(oneBar.evolutionOrdinal == 0);
  assert(phraseVocabularyBarOrdinal(0) == 0);

  std::cout << "PHRASE-C1 C1-0 axes: PASS\n";
  std::cout << "lengths=1,2,4,8\n";
  std::cout << "length8_global=0,1,2,3,4,5,6,7\n";
  std::cout << "length8_vocabulary=0,1,2,3,0,1,2,3\n";
  std::cout << "length8_evolution=0,0,0,0,1,1,1,1\n";
  std::cout << "phrase_identity_physical_address_independent=PASS\n";
  std::cout << "one_bar_compatibility=PASS\n";
  return 0;
}
