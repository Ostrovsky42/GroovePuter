#include "../platform_sdl/arduino_compat.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/phrase/phrase_types.h"
#include "../src/state/undo_owner.h"

#include <cstdio>

SerialMock Serial;
SDMock SD;

int main() {
  using namespace GeneratedPhraseSong;
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  using namespace GroovePuterRhythm::PhraseLiveArrangementDetail;
  using namespace GroovePuterRhythm::LiveSongArrangementDetail;

  const std::size_t alignedPatternStorage =
      (2u * sizeof(SynthPattern)) + sizeof(DrumPatternSet);
  const std::size_t cPublicationFixedBss =
      sizeof(g_slots) + sizeof(g_slotState) + sizeof(g_publishedSlot) +
      sizeof(g_status) + sizeof(g_commitSerial);
  const std::size_t d2ActivationFixedBss = sizeof(g_phraseActivation);
  const std::size_t d3ActivationFixedBss = sizeof(g_songActivation);

  std::printf("E0_SIZE SynthStep=%zu\n", sizeof(SynthStep));
  std::printf("E0_SIZE SynthPattern=%zu\n", sizeof(SynthPattern));
  std::printf("E0_SIZE DrumStep=%zu\n", sizeof(DrumStep));
  std::printf("E0_SIZE DrumPattern=%zu\n", sizeof(DrumPattern));
  std::printf("E0_SIZE AutomationNode=%zu\n", sizeof(AutomationNode));
  std::printf("E0_SIZE AutomationLane=%zu\n", sizeof(AutomationLane));
  std::printf("E0_SIZE DrumPatternSet=%zu\n", sizeof(DrumPatternSet));
  std::printf("E0_SIZE PhraseBar=%zu\n", sizeof(PhraseGenerator::PhraseBar));
  std::printf("E0_SIZE PhraseSlot=%zu\n", sizeof(PhraseCore::PhraseSlot));
  std::printf("E0_SIZE PhraseBank=%zu\n", sizeof(PhraseCore::PhraseBank));
  std::printf("E0_SIZE SongPosition=%zu\n", sizeof(SongPosition));
  std::printf("E0_SIZE Song=%zu\n", sizeof(Song));

  std::printf("E0_SIZE PatternStoragePerAlignedSlot=%zu\n",
              alignedPatternStorage);
  for (int bars : {1, 2, 4, 8}) {
    std::printf("E0_SLOT PhraseBars=%d worst_case_unique_slots=%d storage_bytes=%zu\n",
                bars, bars, alignedPatternStorage * static_cast<std::size_t>(bars));
  }
  std::printf("E0_SLOT Reuse_A_A_Ap_Ap_A_App_App_A unique_slots=3 storage_bytes=%zu\n",
              alignedPatternStorage * 3u);

  std::printf("E0_SIZE PreparedPhraseArrangement=%zu\n",
              sizeof(PreparedPhraseArrangement));
  std::printf("E0_SIZE GeneratedPhraseUndoPayload=%zu\n",
              sizeof(GeneratedPhraseUndoPayload));
  std::printf("E0_SIZE UndoOwner=%zu\n", sizeof(GroovePuterUndo::UndoOwner));
  std::printf("E0_SIZE UndoPayloadCapacity=%zu\n",
              GroovePuterUndo::UndoOwner::payloadCapacity());
  std::printf("E0_SIZE PendingGeneration=%zu\n", sizeof(PendingGeneration));
  std::printf("E0_SIZE PendingGenerationDoubleBuffer=%zu\n", sizeof(g_slots));
  std::printf("E0_SIZE PhraseActivationMetadata=%zu\n",
              sizeof(PhraseActivationMetadata));
  std::printf("E0_SIZE PhraseActivationDoubleBuffer=%zu\n",
              sizeof(g_phraseActivation));
  std::printf("E0_SIZE SongActivationMetadata=%zu\n",
              sizeof(SongActivationMetadata));
  std::printf("E0_SIZE SongActivationDoubleBuffer=%zu\n",
              sizeof(g_songActivation));
  std::printf("E0_SIZE CPublicationFixedBss=%zu\n", cPublicationFixedBss);
  std::printf("E0_SIZE D2ActivationFixedBss=%zu\n", d2ActivationFixedBss);
  std::printf("E0_SIZE D3ActivationFixedBss=%zu\n", d3ActivationFixedBss);
  std::printf("E0_SIZE ExistingGenerationActivationFixedBss=%zu\n",
              cPublicationFixedBss + d2ActivationFixedBss +
                  d3ActivationFixedBss);

  std::printf("E0_SLOT PatternsPerBank=%d\n", Bank<SynthPattern>::kPatterns);
  std::printf("E0_SLOT BanksPerPage=%d\n", kBankCount);
  std::printf("E0_SLOT PatternsPerPage=%d\n", kPatternsPerPage);
  std::printf("E0_SLOT PhraseCoreSlots=%d\n", PhraseCore::kSlotCount);
  std::printf("E0_SLOT PhraseCoreMaxBars=%d\n", PhraseCore::kMaxBars);
  std::printf("E0_SLOT PhraseCoreTracks=%d\n", PhraseCore::kTrackCount);
  return 0;
}
