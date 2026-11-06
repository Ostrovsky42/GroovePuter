#ifndef GROOVEPUTER_GENERATION_RHYTHM_BROWSER_CATALOG_H
#define GROOVEPUTER_GENERATION_RHYTHM_BROWSER_CATALOG_H

#include <cstdint>
#include "../materialization/pattern_materializer.h"

namespace RhythmBrowser {

enum class Source : uint8_t { Vocabulary, Atlas };
enum class Group : uint8_t { All, House, Techno, UkGarage, Breaks, HipHop, Acid, Dub, Electro, Count };
enum class Provenance : uint8_t { VocabularyGrammar, CompiledAtlasRightsReview };
enum class Renderability : uint8_t { RequiresMaterialization };
enum class Status : uint8_t { Ok, InvalidSource, UnknownId, InvalidVariant, Unrealizable, EmptyDrums };

struct Identity {
  Source source = Source::Vocabulary;
  uint16_t stableId = 0;
  uint8_t variant = 0;
  uint32_t previewSeed = 0;
  uint32_t contextVersion = 0;
};

struct Descriptor {
  Source source = Source::Vocabulary;
  uint16_t stableId = 0;
  const char* name = nullptr;
  uint8_t variantCount = 0;
  uint16_t bpmMin = 0;
  uint16_t bpmMax = 0;
  Provenance provenance = Provenance::VocabularyGrammar;
  bool fourFour = false;
  Renderability renderability = Renderability::RequiresMaterialization;
};

struct Workspace {
  GroovePuterRhythm::MaterializedPatterns material{};
};

struct Candidate {
  Identity identity{};
  DrumPatternSet drums{};
};

uint8_t count();
bool descriptorAt(uint8_t index, Descriptor& out);
bool lookup(Source source, uint16_t stableId, Descriptor& out);
bool matchesGroup(const Descriptor& descriptor, Group group);
// Control-side only; neither scratch nor out may be concurrently read here.
// Scratch may change on refusal; out never does. Before publication the caller
// must validate contextVersion and use the existing synchronization owner.
Status materialize(const Identity& identity, Workspace& scratch, Candidate& out);

}  // namespace RhythmBrowser

#endif  // GROOVEPUTER_GENERATION_RHYTHM_BROWSER_CATALOG_H
