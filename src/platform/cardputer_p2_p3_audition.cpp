#if defined(ARDUINO_M5STACK_CARDPUTER)

#include <cstdint>

#include "../../scenes.h"
#include "../dsp/miniacid_engine.h"
#include "../generation/migration/p2_p3_hardware_audition.h"
#include "../input/cardputer_input_edges.h"
#include "../ui/miniacid_display.h"

extern MiniAcid* volatile g_miniAcid;
extern MiniAcidDisplay* g_miniDisplay;

namespace GroovePuterInput {
namespace {

constexpr uint8_t kHidO = 0x12;
constexpr uint8_t kHid1 = 0x1E;
constexpr uint8_t kHid4 = 0x21;
constexpr uint8_t kHidEscape = 0x29;
constexpr int kAuditionSynthIndex = 1;
constexpr int kAuditionSynthInstrumentId = 2;
constexpr int kAuditionBank = 0;
constexpr int kAuditionPatternSlots = 4;
constexpr int kLongToastMs = 60000;

struct P23AuditionBackup {
  bool valid = false;
  bool wasPlaying = false;
  bool songMode = false;
  bool synthBMuted = false;
  int previousUiPage = 0;
  int synthBBank = 0;
  int synthBPattern = 0;
  int songPosition = 0;
  int songPlaybackSlot = 0;
  int activeSongSlot = 0;
  SynthPattern synthBPatterns[kAuditionPatternSlots]{};
  Song activeSong{};
  float trackVolumes[static_cast<int>(VoiceId::Count)]{};
};

bool g_p23AuditionActive = false;
P23AuditionBackup g_p23Backup{};

MiniAcid* engine() {
  return g_miniAcid;
}

uint8_t noteForSource(uint8_t sourceOrdinal) {
  // Deliberately non-monotonic so accidental source advancement is easy to
  // hear. These are pitch probes only, not Harmony Atlas vocabulary.
  static constexpr uint8_t kNotes[] = {48, 55, 52, 59, 50, 57, 53, 60};
  return kNotes[sourceOrdinal % (sizeof(kNotes) / sizeof(kNotes[0]))];
}

SynthPattern emptyPattern() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
    pattern.steps[step].velocity = 112;
    pattern.steps[step].probability = 100;
  }
  return pattern;
}

SynthPattern renderBar(
    const GroovePuterRhythm::P2P3HardwareAuditionBar& bar) {
  SynthPattern pattern = emptyPattern();
  for (uint8_t step = 0; step < GroovePuterRhythm::kStepsPerBar; ++step) {
    const GroovePuterRhythm::StepMask bit = GroovePuterRhythm::stepBit(step);
    if ((bar.audibleOnsets & bit) != 0) {
      const uint8_t source = bar.sourceOrdinalByStep[step];
      if (source != GroovePuterRhythm::kNoChordRhythmSourceOrdinal) {
        pattern.steps[step].note = static_cast<int8_t>(noteForSource(source));
      }
    } else if ((bar.continuations & bit) != 0) {
      pattern.steps[step].note = -2;  // Existing SynthPattern TIE semantics.
    }
  }
  return pattern;
}

SynthPattern renderCrossBarHoldProbe(
    const GroovePuterRhythm::P2P3HardwareAuditionPlan& plan) {
  // A single 16-step pattern is intentional here: repeating it avoids Song's
  // row-boundary AllNotesOff and lets the existing monophonic gate prove that
  // TIE steps 15 -> 0 really cross a physical bar boundary.
  SynthPattern pattern = renderBar(plan.bars[0]);
  if (plan.barCount > 1) {
    const auto& incoming = plan.bars[1];
    for (uint8_t step = 0; step < GroovePuterRhythm::kStepsPerBar; ++step) {
      const GroovePuterRhythm::StepMask bit = GroovePuterRhythm::stepBit(step);
      if ((incoming.continuations & bit) != 0)
        pattern.steps[step].note = -2;
      if ((incoming.releasePoints & bit) != 0)
        pattern.steps[step].note = -1;
    }
  }
  return pattern;
}

void isolateSynthB(Scene& scene) {
  for (int i = 0; i < static_cast<int>(VoiceId::Count); ++i)
    scene.trackVolumes[i] = 0.0f;
  scene.trackVolumes[static_cast<int>(VoiceId::SynthB)] = 1.0f;
}

void saveBackup(MiniAcid& mini) {
  SceneManager& manager = mini.sceneManager();
  Scene& scene = manager.currentScene();

  g_p23Backup = P23AuditionBackup{};
  g_p23Backup.valid = true;
  g_p23Backup.wasPlaying = mini.isPlaying();
  g_p23Backup.songMode = mini.songModeEnabled();
  g_p23Backup.synthBMuted = mini.is303Muted(kAuditionSynthIndex);
  g_p23Backup.previousUiPage =
      g_miniDisplay == nullptr ? 0 : g_miniDisplay->currentPageIndex();
  g_p23Backup.synthBBank = manager.getCurrentBankIndex(kAuditionSynthInstrumentId);
  g_p23Backup.synthBPattern = manager.getCurrentSynthPatternIndex(kAuditionSynthIndex);
  g_p23Backup.songPosition = mini.currentSongPosition();
  g_p23Backup.songPlaybackSlot = mini.songPlaybackSlot();
  g_p23Backup.activeSongSlot = mini.activeSongSlot();

  for (int slot = 0; slot < kAuditionPatternSlots; ++slot)
    g_p23Backup.synthBPatterns[slot] =
        scene.synthBBanks[kAuditionBank].patterns[slot];
  g_p23Backup.activeSong = scene.songs[g_p23Backup.activeSongSlot];
  for (int i = 0; i < static_cast<int>(VoiceId::Count); ++i)
    g_p23Backup.trackVolumes[i] = scene.trackVolumes[i];
}

void restoreBackup(MiniAcid& mini) {
  if (!g_p23Backup.valid) return;

  mini.stop();
  mini.setSongMode(false);

  SceneManager& manager = mini.sceneManager();
  Scene& scene = manager.currentScene();
  for (int slot = 0; slot < kAuditionPatternSlots; ++slot)
    scene.synthBBanks[kAuditionBank].patterns[slot] =
        g_p23Backup.synthBPatterns[slot];
  scene.songs[g_p23Backup.activeSongSlot] = g_p23Backup.activeSong;
  for (int i = 0; i < static_cast<int>(VoiceId::Count); ++i)
    scene.trackVolumes[i] = g_p23Backup.trackVolumes[i];

  manager.setCurrentBankIndex(kAuditionSynthInstrumentId,
                              g_p23Backup.synthBBank);
  manager.setCurrentSynthPatternIndex(kAuditionSynthIndex,
                                      g_p23Backup.synthBPattern);
  mini.setMute303(kAuditionSynthIndex, g_p23Backup.synthBMuted);
  mini.setSongPlaybackSlot(g_p23Backup.songPlaybackSlot);
  mini.setSongPosition(g_p23Backup.songPosition);
  mini.setSongMode(g_p23Backup.songMode);
  if (g_p23Backup.songMode)
    mini.setSongPosition(g_p23Backup.songPosition);

  if (g_miniDisplay != nullptr)
    g_miniDisplay->goToPage(g_p23Backup.previousUiPage);

  const bool resume = g_p23Backup.wasPlaying;
  g_p23Backup.valid = false;
  if (resume) mini.start();
}

bool loadFixture(uint8_t fixtureNumber) {
  if (fixtureNumber < 1 || fixtureNumber > 4) return false;
  MiniAcid* mini = engine();
  if (mini == nullptr) return false;

  const auto fixture = static_cast<GroovePuterRhythm::P2P3HardwareAuditionFixture>(
      fixtureNumber);
  const GroovePuterRhythm::P2P3HardwareAuditionPlan plan =
      GroovePuterRhythm::realizeP2P3HardwareAudition(fixture);
  if (plan.status != GroovePuterRhythm::P2P3HardwareAuditionStatus::Ok)
    return false;

  mini->stop();
  mini->setSongMode(false);

  SceneManager& manager = mini->sceneManager();
  Scene& scene = manager.currentScene();
  isolateSynthB(scene);
  mini->setMute303(kAuditionSynthIndex, false);

  for (int slot = 0; slot < kAuditionPatternSlots; ++slot)
    scene.synthBBanks[kAuditionBank].patterns[slot] = emptyPattern();

  if (fixture == GroovePuterRhythm::P2P3HardwareAuditionFixture::CrossBarHold) {
    scene.synthBBanks[kAuditionBank].patterns[0] =
        renderCrossBarHoldProbe(plan);
    manager.setCurrentBankIndex(kAuditionSynthInstrumentId, kAuditionBank);
    manager.setCurrentSynthPatternIndex(kAuditionSynthIndex, 0);
  } else if (fixture !=
             GroovePuterRhythm::P2P3HardwareAuditionFixture::MultiBarNS) {
    scene.synthBBanks[kAuditionBank].patterns[0] = renderBar(plan.bars[0]);
    manager.setCurrentBankIndex(kAuditionSynthInstrumentId, kAuditionBank);
    manager.setCurrentSynthPatternIndex(kAuditionSynthIndex, 0);
  } else {
    for (uint8_t bar = 0; bar < plan.barCount; ++bar)
      scene.synthBBanks[kAuditionBank].patterns[bar] = renderBar(plan.bars[bar]);

    const int slot = mini->activeSongSlot();
    Song& song = scene.songs[slot];
    song.length = plan.barCount;
    song.reverse = false;
    for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
      for (int track = 0; track < SongPosition::kTrackCount; ++track)
        song.positions[bar].patterns[track] = -1;
      song.positions[bar].patterns[static_cast<int>(SongTrack::SynthB)] =
          static_cast<int16_t>(songPatternFromPageBankIndex(
              mini->currentPageIndex(), kAuditionBank, bar));
    }
    mini->setSongPlaybackSlot(slot);
    mini->setSongMode(true);
    mini->setSongPosition(0);
  }

  mini->start();
  if (g_miniDisplay != nullptr) {
    char message[32]{};
    snprintf(message, sizeof(message), "P23 #%u %s",
             static_cast<unsigned>(fixtureNumber),
             GroovePuterRhythm::p2P3HardwareAuditionFixtureName(fixture));
    g_miniDisplay->showToast(message, kLongToastMs);
  }
  return true;
}

void enterAudition() {
  MiniAcid* mini = engine();
  if (mini == nullptr || g_p23AuditionActive) return;
  saveBackup(*mini);
  mini->stop();
  g_p23AuditionActive = true;
  if (g_miniDisplay != nullptr) {
    g_miniDisplay->goToPage(2);  // Existing Synth B page; no new UI owner.
    g_miniDisplay->showToast("P23 AUDITION CTRL+1..4", kLongToastMs);
  }
}

void exitAudition() {
  MiniAcid* mini = engine();
  if (!g_p23AuditionActive) return;
  g_p23AuditionActive = false;
  if (mini != nullptr) restoreBackup(*mini);
  if (g_miniDisplay != nullptr)
    g_miniDisplay->showToast("P23 AUDITION OFF", 1500);
}

}  // namespace

bool p23AuditionActive() {
  return g_p23AuditionActive;
}

bool p23AuditionConsumeCardputerHid(bool alt,
                                    bool ctrl,
                                    bool shift,
                                    bool fn,
                                    uint8_t hid) {
  const bool exactEntry = ctrl && alt && !shift && !fn && hid == kHidO;
  if (!g_p23AuditionActive) {
    if (!exactEntry) return false;
    enterAudition();
    return true;
  }

  // Exact entry chord toggles the temporary mode off too.
  if (exactEntry || (!alt && !ctrl && !shift && !fn && hid == kHidEscape)) {
    exitAudition();
    return true;
  }

  if (ctrl && !alt && !shift && !fn && hid >= kHid1 && hid <= kHid4) {
    const uint8_t fixture = static_cast<uint8_t>(hid - kHid1 + 1u);
    if (!loadFixture(fixture) && g_miniDisplay != nullptr)
      g_miniDisplay->showToast("P23 FIXTURE ERROR", 1800);
    return true;
  }

  // While audition is active, Cardputer HID input belongs to the harness so
  // accidental project edits/saves cannot leak through the normal UI.
  return true;
}

}  // namespace GroovePuterInput

#endif  // ARDUINO_M5STACK_CARDPUTER
