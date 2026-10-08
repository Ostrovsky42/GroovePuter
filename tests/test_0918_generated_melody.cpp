// 0.9.18 G straight into a Melody: GeneratedMelody::generate builds a 1/2/4/8
// bar phrase for one synth from the TAKE generator. Owns the claims the Alt+G
// key in the melody editor relies on.

#include "arduino_compat.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <set>

#include "../platform_sdl/scene_storage_sdl.h"
#include "../src/audio/pattern_paging.h"
#include "../src/dsp/generated_melody.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/platform/cardputer_material_publication_session.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

size_t distinctPitches(const Buffer& buffer) {
  std::set<int> pitches;
  for (uint16_t i = 0; i < buffer.count; ++i) pitches.insert(buffer.events[i].note);
  return pitches.size();
}

void selectGenre(MiniAcid& engine, int mode) {
  engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
  engine.genreManager().setRecipe(0);
}

// Every genre, both synths, the lengths Alt+G offers: a valid, non-empty,
// overlap-free phrase of exactly that length, the same for the same press and
// different for the next press.
void testEveryGenreGivesAPlayablePhrase(MiniAcid& engine) {
  const uint8_t lengths[] = {2, 4};
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    selectGenre(engine, mode);
    for (int voice = 0; voice < 2; ++voice) {
      for (const uint8_t bars : lengths) {
        Buffer first{}, again{}, next{};
        assert(GeneratedMelody::generate(engine, voice, bars, 1, first) ==
               GeneratedMelody::Status::Ready);
        assert(GeneratedMelody::generate(engine, voice, bars, 1, again) ==
               GeneratedMelody::Status::Ready);
        assert(GeneratedMelody::generate(engine, voice, bars, 2, next) ==
               GeneratedMelody::Status::Ready);
        assert(first.lengthTicks == bars * PhraseRuntime::kTicksPerBar);
        assert(RuntimePhraseEdit::validate(first));
        assert(!RuntimePhraseEdit::hasOverlappingNotes(first));
        assert(first.count >= bars);
        assert(RuntimePhraseEdit::same(first, again));
        assert(!RuntimePhraseEdit::same(first, next));
        for (uint16_t i = 0; i < first.count; ++i) {
          const auto& event = first.events[i];
          assert(event.durationSubticks > 0);
          // A note never runs past its own bar.
          const uint32_t end = static_cast<uint32_t>(event.startTick) *
                                   PhraseRuntime::kSubticksPerTick +
                               event.durationSubticks;
          const uint32_t barEnd =
              (event.startTick / PhraseRuntime::kTicksPerBar + 1u) *
              PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick;
          assert(end <= barEnd);
        }
      }
    }
  }
}

// The lead of a melodic genre is a tune across presses, not one repeated note.
void testLeadsAreMelodies(MiniAcid& engine) {
  const GenerativeMode leads[] = {
      GenerativeMode::Acid, GenerativeMode::Outrun, GenerativeMode::Darksynth,
      GenerativeMode::Rave, GenerativeMode::Chip, GenerativeMode::House,
      GenerativeMode::HipHop, GenerativeMode::LoFi};
  for (const GenerativeMode mode : leads) {
    selectGenre(engine, static_cast<int>(mode));
    for (uint32_t salt = 1; salt <= 12; ++salt) {
      Buffer phrase{};
      assert(GeneratedMelody::generate(engine, 1, 4, salt, phrase) ==
             GeneratedMelody::Status::Ready);
      assert(distinctPitches(phrase) >= 2);
    }
  }
}

void testInvalidRequestsLeaveNothingToCommit(MiniAcid& engine) {
  selectGenre(engine, 0);
  Buffer phrase{};
  assert(GeneratedMelody::generate(engine, 2, 4, 1, phrase) ==
         GeneratedMelody::Status::InvalidRequest);
  assert(GeneratedMelody::generate(engine, 0, 3, 1, phrase) ==
         GeneratedMelody::Status::InvalidRequest);
  assert(GeneratedMelody::generate(engine, 0, 0, 1, phrase) ==
         GeneratedMelody::Status::InvalidRequest);
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_0918_melody";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("melody0918", 0);
  PatternPagingService::setProjectName("melody0918");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100, &storage);
  engine.init();
  engine.setSongMode(false);

  testEveryGenreGivesAPlayablePhrase(engine);
  testLeadsAreMelodies(engine);
  testInvalidRequestsLeaveNothingToCommit(engine);
  std::printf("0.9.18 generated melody: OK\n");
  return 0;
}
