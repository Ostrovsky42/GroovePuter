// 0.9.18 G straight into a Melody: GeneratedMelody::generate builds a 1/2/4/8
// bar phrase for one synth from the TAKE generator. Owns the claims the Alt+G
// key in the melody editor relies on.

#include "arduino_compat.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>

#include "../platform_sdl/scene_storage_sdl.h"
#include "../src/audio/pattern_paging.h"
#include "../src/dsp/generated_melody.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/generation/migration/quantized_generation_commit.h"
#include "../src/state/generation_shape_state.h"
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

// GEN panel LIVELY: across melodic genres and presses, LIVELY leads carry more notes
// than CALM ones; NORMAL is the genre untouched (the tests above run on it).
void testLivelinessMovesDensity(MiniAcid& engine) {
  using GroovePuterState::GenerationLiveliness;
  auto notesAt = [&](GenerationLiveliness liveliness) {
    GroovePuterState::setGenerationLiveliness(liveliness);
    uint32_t total = 0;
    for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
      // Reggae and TripHop play chords on Synth B: LIVELY changes their
      // rhythm (stabs, not held pads), not the event count.
      if (mode == static_cast<int>(GenerativeMode::Reggae) ||
          mode == static_cast<int>(GenerativeMode::TripHop)) continue;
      selectGenre(engine, mode);
      for (uint32_t salt = 1; salt <= 12; ++salt) {
        Buffer phrase{};
        assert(GeneratedMelody::generate(engine, 1, 4, salt, phrase) ==
               GeneratedMelody::Status::Ready);
        total += phrase.count;
      }
    }
    return total;
  };
  const uint32_t calm = notesAt(GenerationLiveliness::Calm);
  const uint32_t normal = notesAt(GenerationLiveliness::Normal);
  const uint32_t lively = notesAt(GenerationLiveliness::Lively);
  std::printf("lead notes CALM %u NORMAL %u LIVELY %u\n", calm, normal, lively);
  std::fflush(stdout);
  assert(calm < normal);
  assert(normal < lively);
  GroovePuterState::setGenerationLiveliness(GenerationLiveliness::Normal);
}

// GEN panel NOTES: the same notes, held longer, never overlapping.
void testNoteLengthHoldsWithoutOverlap(MiniAcid& engine) {
  using GroovePuterState::GenerationNoteLength;
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    selectGenre(engine, mode);
    uint64_t shortTotal = 0, mixedTotal = 0, longTotal = 0;
    for (uint32_t salt = 1; salt <= 4; ++salt) {
      Buffer shortNotes{}, mixed{}, legato{};
      GroovePuterState::setGenerationNoteLength(GenerationNoteLength::Short);
      assert(GeneratedMelody::generate(engine, 1, 4, salt, shortNotes) ==
             GeneratedMelody::Status::Ready);
      GroovePuterState::setGenerationNoteLength(GenerationNoteLength::Mixed);
      assert(GeneratedMelody::generate(engine, 1, 4, salt, mixed) ==
             GeneratedMelody::Status::Ready);
      GroovePuterState::setGenerationNoteLength(GenerationNoteLength::Long);
      assert(GeneratedMelody::generate(engine, 1, 4, salt, legato) ==
             GeneratedMelody::Status::Ready);
      for (const Buffer* b : {&mixed, &legato}) {
        assert(b->count == shortNotes.count);
        assert(RuntimePhraseEdit::validate(*b));
        assert(!RuntimePhraseEdit::hasOverlappingNotes(*b));
        for (uint16_t i = 0; i < b->count; ++i) {
          assert(b->events[i].startTick == shortNotes.events[i].startTick);
          assert(b->events[i].note == shortNotes.events[i].note);
          assert(b->events[i].durationSubticks >= shortNotes.events[i].durationSubticks);
        }
      }
      for (uint16_t i = 0; i < shortNotes.count; ++i) {
        shortTotal += shortNotes.events[i].durationSubticks;
        mixedTotal += mixed.events[i].durationSubticks;
        longTotal += legato.events[i].durationSubticks;
      }
    }
    assert(shortTotal <= mixedTotal && mixedTotal <= longTotal);
    assert(shortTotal < longTotal);
  }
  GroovePuterState::setGenerationNoteLength(GenerationNoteLength::Short);
}

bool sameDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  return std::memcmp(&a, &b, sizeof(DrumPatternSet)) == 0;
}

bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (a.steps[step].note != b.steps[step].note) return false;
  }
  return true;
}

// GEN DRUMS: a full G with KEEP leaves the slot's drums exactly as they were
// and still writes new synths; NEW changes the drums.
void testKeepDrumsRegeneratesOnlySynths(MiniAcid& engine) {
  using namespace GroovePuterRhythm;
  for (const GenerativeMode mode : {GenerativeMode::Acid, GenerativeMode::House,
                                    GenerativeMode::LoFi}) {
    selectGenre(engine, static_cast<int>(mode));
    Scene& scene = engine.sceneManager().currentScene();
    const auto target = QuantizedGenerationDetail::captureTarget(engine.sceneManager());
    auto drumsNow = [&]() {
      return scene.drumBanks[target.drumBank].patterns[target.drumSlot];
    };
    auto synthNow = [&](int voice) {
      return voice == 0 ? scene.synthABanks[target.synthBank[0]].patterns[target.synthSlot[0]]
                        : scene.synthBBanks[target.synthBank[1]].patterns[target.synthSlot[1]];
    };
    const GenreSettings settings = scene.genre;
    const GrooveboxMode grooveMode = engine.grooveboxMode();

    assert(regenerateWithQuantizedCommit(engine, settings, grooveMode, false,
                                         engine.bpm(), false) ==
           QuantizedGenerationResult::CommittedNow);
    const DrumPatternSet drums = drumsNow();
    const SynthPattern synthA = synthNow(0);
    const SynthPattern synthB = synthNow(1);

    bool synthsChanged = false;
    for (int press = 0; press < 3 && !synthsChanged; ++press) {
      assert(regenerateWithQuantizedCommit(engine, settings, grooveMode, false,
                                           engine.bpm(), true) ==
             QuantizedGenerationResult::CommittedNow);
      assert(sameDrums(drumsNow(), drums));
      synthsChanged = !sameSynth(synthNow(0), synthA) || !sameSynth(synthNow(1), synthB);
    }
    assert(synthsChanged);
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
  testLivelinessMovesDensity(engine);
  testNoteLengthHoldsWithoutOverlap(engine);
  testKeepDrumsRegeneratesOnlySynths(engine);
  std::printf("0.9.18 generated melody: OK\n");
  return 0;
}
