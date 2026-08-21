#include "../platform_sdl/arduino_compat.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/phrase/phrase_types.h"

#include <cstdio>

SerialMock Serial;
SDMock SD;

int main() {
  using namespace GeneratedPhraseSong;
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  using namespace GroovePuterRhythm::PhraseLiveArrangementDetail;
  using namespace GroovePuterRhythm::LiveSongArrangementDetail;

  const std::size_t patternStorage =
      (2u * sizeof(SynthPattern)) + sizeof(DrumPatternSet);
  const std::size_t cPublicationFixedBss =
      sizeof(g_slots) + sizeof(g_slotState) + sizeof(g_publishedSlot) +
      sizeof(g_status) + sizeof(g_commitSerial);
  const std::size_t d2ActivationFixedBss = sizeof(g_phraseActivation);
  const std::size_t d3ActivationFixedBss = sizeof(g_songActivation);

  std::printf("E3_SIZE SynthStep=%zu\n", sizeof(SynthStep));
  std::printf("E3_SIZE SynthPattern=%zu\n", sizeof(SynthPattern));
  std::printf("E3_SIZE DrumStep=%zu\n", sizeof(DrumStep));
  std::printf("E3_SIZE DrumPattern=%zu\n", sizeof(DrumPattern));
  std::printf("E3_SIZE AutomationNode=%zu\n", sizeof(AutomationNode));
  std::printf("E3_SIZE AutomationLane=%zu\n", sizeof(AutomationLane));
  std::printf("E3_SIZE DrumPatternSet=%zu\n", sizeof(DrumPatternSet));
  std::printf("E3_SIZE PhraseBar=%zu\n", sizeof(PhraseGenerator::PhraseBar));
  std::printf("E3_SIZE PhraseSlot=%zu\n", sizeof(PhraseCore::PhraseSlot));
  std::printf("E3_SIZE PhraseBank=%zu\n", sizeof(PhraseCore::PhraseBank));
  std::printf("E3_SIZE SongPosition=%zu\n", sizeof(SongPosition));
  std::printf("E3_SIZE Song=%zu\n", sizeof(Song));
  std::printf("E3_SIZE PatternStoragePerAlignedSlot=%zu\n", patternStorage);
  std::printf("E3_SIZE PreparedPhraseArrangement=%zu\n",
              sizeof(PreparedPhraseArrangement));
  std::printf("E3_SIZE GeneratedPhraseUndoPayload=%zu\n",
              sizeof(GeneratedPhraseUndoPayload));
  std::printf("E3_SIZE PendingGeneration=%zu\n", sizeof(PendingGeneration));
  std::printf("E3_SIZE PendingGenerationDoubleBuffer=%zu\n", sizeof(g_slots));
  std::printf("E3_SIZE PhraseActivationMetadata=%zu\n",
              sizeof(PhraseActivationMetadata));
  std::printf("E3_SIZE PhraseActivationDoubleBuffer=%zu\n",
              sizeof(g_phraseActivation));
  std::printf("E3_SIZE SongActivationMetadata=%zu\n",
              sizeof(SongActivationMetadata));
  std::printf("E3_SIZE SongActivationDoubleBuffer=%zu\n",
              sizeof(g_songActivation));
  std::printf("E3_SIZE CPublicationFixedBss=%zu\n", cPublicationFixedBss);
  std::printf("E3_SIZE D2ActivationFixedBss=%zu\n", d2ActivationFixedBss);
  std::printf("E3_SIZE D3ActivationFixedBss=%zu\n", d3ActivationFixedBss);
  std::printf("E3_SIZE ExistingGenerationActivationFixedBss=%zu\n",
              cPublicationFixedBss + d2ActivationFixedBss +
                  d3ActivationFixedBss);
  std::printf("E3_SLOT PatternsPerBank=%d\n", Bank<SynthPattern>::kPatterns);
  std::printf("E3_SLOT BanksPerPage=%d\n", kBankCount);
  std::printf("E3_SLOT PatternsPerPage=%d\n", kPatternsPerPage);
  std::printf("E3_SLOT PhraseCoreSlots=%d\n", PhraseCore::kSlotCount);
  std::printf("E3_SLOT PhraseCoreMaxBars=%d\n", PhraseCore::kMaxBars);
  std::printf("E3_SLOT PhraseCoreTracks=%d\n", PhraseCore::kTrackCount);
  return 0;
}
