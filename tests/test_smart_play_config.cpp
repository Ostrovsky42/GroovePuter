#include <cassert>
#include <cstdint>
#include <cstddef>
#include "src/input/performance_keyboard.h"
#include "src/input/performance_keyboard_config.h"
#include "src/input/performance_config_save_timer.h"

int main() {
  MusicalEventRouter router{};
  PerformanceKeyboard keyboard{router};
  const PerformanceKeyboardConfig defaults = keyboard.exportConfiguration();
  assert(validPerformanceKeyboardConfig(defaults));
  PerformanceKeyboardConfig missingDestination{};
  assert(!PerformanceKeyboardConfigCodec::decode(nullptr, 0, missingDestination));
  assert(performanceKeyboardConfigEqual(defaults, keyboard.exportConfiguration()));

  PerformanceKeyboardConfig config{};
  config.role = GroovePuterPerformance::PerformanceRole::Lead;
  config.leadMode = GroovePuterPerformance::LeadMode::VoiceLead;
  config.arpPitchSource = GroovePuterPerformance::ArpPitchSource::Harmony;
  config.harmonyChangePolicy = GroovePuterPerformance::HarmonyChangePolicy::Restart;
  config.arpDirection = PerformanceArpDirection::DownUp;
  config.arpRate = PerformanceRate::ThirtySecond;
  config.arpOctaves = 4;
  config.gatePercent = 87;
  config.chordMode = PerformanceChordMode::Major7;
  config.chordInversion = 2;
  config.chordSpread = PerformanceSpread::Wide;
  config.voiceLeading = PerformanceVoiceLeading::Off;
  config.rootPitchClass = 7;
  config.scale = PerformanceScale::Dorian;
  config.velocity = 112;
  config.octaveShift = -1;
  config.arpEnabled = true;
  config.latchEnabled = true;
  config.ratchetCount = 4;
  config.euclideanLength = 12;
  config.euclideanPulses = 5;
  config.euclideanRotation = 3;
  config.strumMs = 0;
  config.strumDirection = PerformanceStrumDirection::AsPlayed;
  config.mutationSeed = 0x12345678u;
  config.mutationSkipPercent = 12;
  config.mutationOctaveJumpPercent = 23;
  config.mutationDeviatePercent = 34;

  assert(keyboard.applyConfiguration(config));
  assert(performanceKeyboardConfigEqual(keyboard.exportConfiguration(), config));
  assert(!keyboard.configChanged());

  uint8_t encoded[PerformanceKeyboardConfigCodec::kCurrentRecordSize]{};
  assert(PerformanceKeyboardConfigCodec::encode(config, encoded, sizeof(encoded)));
  PerformanceKeyboardConfig decoded{};
  assert(PerformanceKeyboardConfigCodec::decode(encoded, sizeof(encoded), decoded));
  assert(performanceKeyboardConfigEqual(decoded, config));

  // The pre-v2 preference payload was the exact 18-byte field sequence.
  const uint8_t legacy[18] = {
      2, 3, 2, 2, 3, 5, 4, 87, 9, 2, 1, 0, 7, 4, 112,
      static_cast<uint8_t>(-1), 1, 1};
  PerformanceKeyboardConfig migrated{};
  assert(PerformanceKeyboardConfigCodec::decode(legacy, sizeof(legacy), migrated));
  assert(migrated.role == GroovePuterPerformance::PerformanceRole::Lead);
  assert(migrated.arpDirection == PerformanceArpDirection::DownUp);
  assert(migrated.arpRate == PerformanceRate::ThirtySecond);
  assert(migrated.arpEnabled && migrated.latchEnabled);

  // The previous persisted format adds a version byte and CRC to the same
  // legacy 18-byte field list. It must migrate all-or-nothing as well.
  uint8_t legacyV1[PerformanceKeyboardConfigCodec::kLegacyV1RecordSize] = {
      1, 2, 3, 2, 2, 3, 5, 4, 87, 9, 2, 1, 0, 7, 4, 112,
      static_cast<uint8_t>(-1), 1, 1, 0, 0};
  uint16_t crc = 0xffffu;
  for (std::size_t i = 0; i < 19; ++i) {
    crc ^= static_cast<uint16_t>(legacyV1[i]) << 8u;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1u) ^ 0x1021u)
                           : static_cast<uint16_t>(crc << 1u);
  }
  legacyV1[19] = static_cast<uint8_t>(crc & 0xffu);
  legacyV1[20] = static_cast<uint8_t>(crc >> 8u);
  PerformanceKeyboardConfig migratedV1{};
  assert(PerformanceKeyboardConfigCodec::decode(legacyV1, sizeof(legacyV1), migratedV1));
  assert(migratedV1.role == GroovePuterPerformance::PerformanceRole::Lead);
  assert(migratedV1.arpEnabled && migratedV1.latchEnabled);

  PerformanceKeyboardConfig invalid = config;
  invalid.role = GroovePuterPerformance::PerformanceRole::Count;
  const auto before = keyboard.exportConfiguration();
  assert(!keyboard.applyConfiguration(invalid));
  assert(performanceKeyboardConfigEqual(keyboard.exportConfiguration(), before));

  keyboard.markConfigPersisted();
  keyboard.setArpeggiatorEnabled(false);
  assert(keyboard.configChanged());
  // A failed Preferences write must not clear dirty state; only the successful
  // save path calls markConfigPersisted().
  assert(keyboard.configChanged());
  keyboard.markConfigPersisted();
  assert(!keyboard.configChanged());

  PerformanceKeyboard intentKeyboard{router};
  PerformanceIntent intent{};
  intent.kind = PerformanceIntentKind::SetLatchEnabled;
  intent.value = 1;
  assert(!intentKeyboard.dispatchIntent(intent));
  assert(!intentKeyboard.latchEnabled());
  intent.kind = PerformanceIntentKind::SetArpEnabled;
  intent.value = 1;
  assert(intentKeyboard.dispatchIntent(intent));
  assert(intentKeyboard.arpeggiatorEnabled());
  assert(intentKeyboard.dispatchIntent(intent));
  assert(intentKeyboard.arpeggiatorEnabled());
  intent.kind = PerformanceIntentKind::SetLatchEnabled;
  intent.value = 1;
  assert(intentKeyboard.dispatchIntent(intent));
  intent.kind = PerformanceIntentKind::SetArpEnabled;
  intent.value = 0;
  assert(intentKeyboard.dispatchIntent(intent));
  assert(!intentKeyboard.latchEnabled());
  intent.kind = PerformanceIntentKind::SetLatchEnabled;
  intent.value = 2;
  assert(!intentKeyboard.dispatchIntent(intent));

  PerformanceConfigSaveTimer saveTimer;
  saveTimer.changed(1000);
  assert(!saveTimer.due(1699));
  saveTimer.changed(1699);
  assert(!saveTimer.due(2398));
  assert(saveTimer.due(2399));
  saveTimer.retryAfter(2399);
  assert(!saveTimer.due(3098));
  assert(saveTimer.due(3099));
  saveTimer.saved();
  assert(!saveTimer.due(3100));

  // Corrupt data must not partially modify the caller's destination.
  encoded[5] ^= 0x40u;
  const auto unchanged = decoded;
  assert(!PerformanceKeyboardConfigCodec::decode(encoded, sizeof(encoded), decoded));
  assert(performanceKeyboardConfigEqual(decoded, unchanged));
  return 0;
}
