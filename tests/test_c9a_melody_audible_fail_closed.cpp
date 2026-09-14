// C9A: Melody Audible Fail-Closed contract.
//
// Invariants:
// 1. Active source == MELODY => WorkingMaterialStorage.holdsMelody() &&
//                               RuntimePhraseEdit::validate(melody)
// 2. Audible path (phraseEventAt_, phraseRelativeTick_, currentPhrasePlayTick)
//    operates ONLY through readableWorkingMelody_().
// 3. Pattern -> Melody via SOURCE switch is permitted ONLY if a valid retained
//    Melody already exists in working storage; otherwise REJECTED, source remains
//    Pattern, and no Melody notes can sound.
// 4. stagePendingMaterial() validates candidate with RuntimePhraseEdit::validate().
// 5. Sentinels 0xFEFEFEFE (EMPTY) and 0xFFFFFFFF (PATTERN) never leak into
//    event playback or count/lengthTicks.

#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_synth_events.h"
#include "src/state/working_material_storage.h"
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "C9A_MELODY_FAIL_CLOSED_FAIL: %s\n", message);
  ++g_failures;
}

PhraseRuntime::RuntimeSynthEventBuffer makeValidMelody(uint8_t note = 60) {
  PhraseRuntime::RuntimeSynthEventBuffer buf{};
  buf.lengthTicks = PhraseRuntime::kTicksPerBar;
  buf.count = 1;
  buf.events[0].startTick = 0;
  buf.events[0].durationSubticks = 24 * 16;
  buf.events[0].note = note;
  buf.events[0].velocity = 100;
  buf.events[0].probability = 100;
  return buf;
}

void test_working_material_storage_sentinel_isolation() {
  using GroovePuterMaterial::WorkingMaterialStorage;
  using GroovePuterMaterial::MaterialReference;
  using GroovePuterMaterial::MaterialAddress;
  using GroovePuterMaterial::MaterialId;

  WorkingMaterialStorage storage;
  expect(storage.empty(), "default WorkingMaterialStorage is not empty");
  expect(!storage.holdsPattern(), "default storage claims to hold pattern");
  expect(!storage.holdsMelody(), "default storage claims to hold melody");
  expect(storage.melodyIfHeld() == nullptr,
         "melodyIfHeld() returned non-null for empty storage");

  // Pattern storage
  SynthPattern pat{};
  pat.steps[0].note = 48;
  const MaterialReference ref{MaterialAddress{0, 2}, MaterialId{101}};
  storage.storePattern(pat, ref);
  expect(!storage.empty(), "storage holding pattern reported empty");
  expect(storage.holdsPattern(), "storage did not report holding pattern");
  expect(!storage.holdsMelody(), "storage holding pattern claims to hold melody");
  expect(storage.melodyIfHeld() == nullptr,
         "melodyIfHeld() returned non-null for pattern storage");

  // Melody storage
  const auto valid = makeValidMelody(62);
  storage.storeMelody(valid);
  expect(!storage.empty(), "storage holding melody reported empty");
  expect(!storage.holdsPattern(), "storage holding melody claims to hold pattern");
  expect(storage.holdsMelody(), "storage did not report holding melody");
  expect(storage.melodyIfHeld() != nullptr,
         "melodyIfHeld() returned null for valid melody storage");
  if (storage.melodyIfHeld() != nullptr) {
    expect(storage.melodyIfHeld()->events[0].note == 62,
           "melodyIfHeld() did not point to the stored melody payload");
  }
}

void test_empty_working_source_toggle_rejected() {
  MiniAcid engine{44100.0f, nullptr};
  constexpr int kVoice = 0;

  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "voice did not start on Pattern");
  expect(engine.workingMaterial_[kVoice].empty(),
         "initial working material is not empty");

  // SOURCE toggle must be REJECTED when no valid melody exists.
  const auto result = PhraseSourceToggle::toggle(engine, nullptr, kVoice);
  expect(result == PhraseSourceToggle::Result::Rejected,
         "empty working storage allowed SOURCE switch to Melody");
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "voice source changed despite rejection");

  // Direct engine call must also enforce the invariant and refuse the switch.
  engine.setSequencedSource(kVoice, MiniAcid::SequencedSource::Phrase);
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "setSequencedSource(Phrase) accepted empty working storage");

  // Even if activeMaterial was maliciously corrupted to Melody, audible path must be fail-closed!
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Melody;
  expect(engine.readableWorkingMelody_(kVoice) == nullptr,
         "readableWorkingMelody_ returned non-null for empty storage");
  expect(engine.phraseEventAt_(kVoice, 0) == nullptr,
         "phraseEventAt_ produced an event from empty storage");
  expect(engine.phraseRelativeTick_(kVoice, 0) == 0,
         "phraseRelativeTick_ returned non-zero for empty storage");
  expect(engine.currentPhrasePlayTick(kVoice) == 0,
         "currentPhrasePlayTick returned non-zero for empty storage");

  // Restore active material
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Pattern;
}

void test_pattern_working_source_toggle_rejected() {
  MiniAcid engine{44100.0f, nullptr};
  constexpr int kVoice = 0;

  SynthPattern& pattern = engine.editSynthPattern(kVoice);
  pattern.steps[0].note = 36;
  pattern.steps[4].note = 43;

  // Stored as Pattern in working storage
  const GroovePuterMaterial::MaterialReference ref{
      GroovePuterMaterial::MaterialAddress{0, 1},
      GroovePuterMaterial::MaterialId{55}};
  engine.workingMaterial_[kVoice].storePattern(pattern, ref);
  expect(engine.workingMaterial_[kVoice].holdsPattern(),
         "working material does not hold pattern");

  const auto result = PhraseSourceToggle::toggle(engine, nullptr, kVoice);
  expect(result == PhraseSourceToggle::Result::Rejected,
         "toggle allowed switch to Melody when working storage only holds Pattern");
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "source moved away from Pattern");

  // Direct engine call refused
  engine.setSequencedSource(kVoice, MiniAcid::SequencedSource::Phrase);
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "setSequencedSource accepted Pattern working storage");

  // Audible path fail-closed if forced
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Melody;
  expect(engine.readableWorkingMelody_(kVoice) == nullptr,
         "readableWorkingMelody_ returned non-null for Pattern storage");
  expect(engine.phraseEventAt_(kVoice, 0) == nullptr,
         "phraseEventAt_ produced event from Pattern storage");
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Pattern;
}

void test_valid_melody_lifecycle() {
  MiniAcid engine{44100.0f, nullptr};
  constexpr int kVoice = 0;

  SynthPattern& pattern = engine.editSynthPattern(kVoice);
  pattern.steps[0].note = 48;
  pattern.steps[0].probability = 100;

  // 1. MAKE PHRASE materializes candidate, stores Melody, and switches source.
  const bool made = PhraseSourceToggle::makePhrase(engine, nullptr, kVoice);
  expect(made, "makePhrase failed");
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Phrase,
         "makePhrase did not reach Phrase source");
  expect(engine.workingMaterial_[kVoice].holdsMelody(),
         "makePhrase did not store Melody in working storage");
  expect(engine.readableWorkingMelody_(kVoice) != nullptr,
         "readableWorkingMelody_ is null after makePhrase");

  // Audible path sounds the expected note
  const auto* ev = engine.phraseEventAt_(kVoice, 0);
  expect(ev != nullptr, "phraseEventAt_ returned null for valid melody at tick 0");
  if (ev != nullptr) {
    expect(ev->note == 48, "phraseEventAt_ did not produce expected note 48");
  }

  // 2. Toggle back to PATTERN keeps melody retained in working storage.
  const auto toPattern = PhraseSourceToggle::toggle(engine, nullptr, kVoice);
  expect(toPattern == PhraseSourceToggle::Result::SwitchedToPattern,
         "toggle back to Pattern was not accepted");
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "voice is not on Pattern");
  expect(engine.workingMaterial_[kVoice].holdsMelody(),
         "returning to Pattern destroyed the retained Melody");

  // While on Pattern, readableWorkingMelody_ is fail-closed (active source is not Melody!)
  expect(engine.readableWorkingMelody_(kVoice) == nullptr,
         "readableWorkingMelody_ was active while source is Pattern");
  expect(engine.phraseEventAt_(kVoice, 0) == nullptr,
         "phraseEventAt_ produced melody event while voice is on Pattern");

  // 3. Toggle to PHRASE succeeds because valid Melody is retained!
  const auto toPhrase = PhraseSourceToggle::toggle(engine, nullptr, kVoice);
  expect(toPhrase == PhraseSourceToggle::Result::SwitchedToPhrase,
         "toggle to Phrase with valid retained Melody was rejected");
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Phrase,
         "voice did not switch to Phrase");
  expect(engine.readableWorkingMelody_(kVoice) != nullptr,
         "readableWorkingMelody_ is null after returning to Phrase");

  const auto* evAfter = engine.phraseEventAt_(kVoice, 0);
  expect(evAfter != nullptr && evAfter->note == 48,
         "notes did not sound after switching back to retained Phrase");
}

void test_corrupt_melody_fail_closed() {
  MiniAcid engine{44100.0f, nullptr};
  constexpr int kVoice = 0;

  // Store invalid melody (lengthTicks = 0, count = 999)
  PhraseRuntime::RuntimeSynthEventBuffer corrupt{};
  corrupt.lengthTicks = 0;
  corrupt.count = 999;
  engine.workingMaterial_[kVoice].storeMelody(corrupt);

  // Engine must refuse switching to Phrase
  engine.setSequencedSource(kVoice, MiniAcid::SequencedSource::Phrase);
  expect(engine.currentSequencedSource(kVoice) ==
             MiniAcid::SequencedSource::Pattern,
         "corrupt melody was accepted by setSequencedSource");

  // Even if forced, readableWorkingMelody_ rejects it
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Melody;
  expect(engine.readableWorkingMelody_(kVoice) == nullptr,
         "corrupt melody was accepted by readableWorkingMelody_");
  expect(engine.phraseEventAt_(kVoice, 0) == nullptr,
         "phraseEventAt_ produced event from corrupt melody");
  engine.activeMaterial_[kVoice].kind = GroovePuterMaterial::MaterialKind::Pattern;
}

void test_stage_pending_material_validation() {
  MiniAcid engine{44100.0f, nullptr};
  expect(engine.initPendingMaterial(), "initPendingMaterial failed");
  constexpr int kVoice = 0;

  // Null melody rejected
  expect(!engine.stagePendingMaterial(
             kVoice, 1, GroovePuterMaterial::MaterialKind::Melody, nullptr),
         "stagePendingMaterial accepted nullptr melody");

  // Invalid lengthTicks rejected
  PhraseRuntime::RuntimeSynthEventBuffer invalidLength = makeValidMelody();
  invalidLength.lengthTicks = 9999;
  expect(!engine.stagePendingMaterial(
             kVoice, 1, GroovePuterMaterial::MaterialKind::Melody, &invalidLength),
         "stagePendingMaterial accepted invalid lengthTicks");

  // Invalid count rejected
  PhraseRuntime::RuntimeSynthEventBuffer invalidCount = makeValidMelody();
  invalidCount.count = 200; // > 128
  expect(!engine.stagePendingMaterial(
             kVoice, 1, GroovePuterMaterial::MaterialKind::Melody, &invalidCount),
         "stagePendingMaterial accepted count > 128");

  // Valid melody accepted
  PhraseRuntime::RuntimeSynthEventBuffer valid = makeValidMelody(70);
  expect(engine.stagePendingMaterial(
             kVoice, 1, GroovePuterMaterial::MaterialKind::Melody, &valid),
         "stagePendingMaterial rejected valid melody");
}

}  // namespace

int main() {
  test_working_material_storage_sentinel_isolation();
  test_empty_working_source_toggle_rejected();
  test_pattern_working_source_toggle_rejected();
  test_valid_melody_lifecycle();
  test_corrupt_melody_fail_closed();
  test_stage_pending_material_validation();

  if (g_failures == 0) {
    std::puts("C9A Melody Audible Fail-Closed contract: PASS");
    return 0;
  }
  std::fprintf(stderr, "C9A Melody Audible Fail-Closed contract: %d failure(s)\n",
               g_failures);
  return 1;
}
