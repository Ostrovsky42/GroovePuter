// 0.9.17 Melody chords on the real engine: a Melody with overlapping notes
// sends every chord note to MIDI (kMusicalEventChord), each ends after its own
// duration, the monophonic internal synth follows the top note, and STOP in
// the middle of a chord releases every note.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

float readPhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

void addNote(Buffer& melody, uint16_t startTick, uint16_t ticks, uint8_t note) {
  auto& event = melody.events[melody.count++];
  event.startTick = startTick;
  event.durationSubticks =
      static_cast<uint16_t>(ticks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
}

struct Seen {
  MusicalEventType type;
  uint8_t note;
  bool chord;
  long frame;
};

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_melody_chords";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("chords", 0);
  PatternPagingService::setProjectName("chords");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  constexpr float kRate = 22050.0f;
  constexpr int kFrames = 512;
  MiniAcid engine{kRate, &storage};
  engine.init();
  engine.setSongMode(false);
  engine.setBpm(120.0f);
  engine.set303PatternIndex(0, 3);
  assert(PhraseSourceToggle::toggle(engine, nullptr, 0) !=
         PhraseSourceToggle::Result::Rejected);

  // C-E-G on beat 1 (E shorter), then D alone on beat 3.
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  addNote(melody, 0, 96, 60);
  addNote(melody, 0, 48, 64);
  addNote(melody, 0, 96, 67);
  addNote(melody, 192, 48, 62);
  assert(RuntimePhraseEdit::hasOverlappingNotes(melody));
  engine.workingMaterial_[0].storeMelody(melody);

  MusicalEventQueue queue;
  engine.setPatternEventQueue(&queue);
  queue.setPhaseReader(readPhase, &engine);

  std::vector<Seen> seen;
  std::vector<int16_t> buffer(kFrames);
  auto render = [&](uint32_t block) {
    queue.beginMidiRenderBlock(block, kFrames, engine.transportPhaseSteps(),
                               engine.bpm(), kRate, engine.isPlaying(), false,
                               true);
    engine.generateAudioBuffer(buffer.data(), kFrames);
    queue.endMidiRenderBlock();
    ScheduledMusicalEvent e{};
    while (queue.tryPop(e)) {
      if (e.event.source != MusicalEventSource::PatternPlayer ||
          e.event.target != MusicalEventTarget::SynthA) {
        continue;
      }
      if (e.event.type == MusicalEventType::AllNotesOff) continue;
      seen.push_back({e.event.type, e.event.note,
                      (e.event.flags & kMusicalEventChord) != 0,
                      static_cast<long>(e.blockSequence) * kFrames +
                          e.frameOffset});
    }
  };

  engine.start();
  // One beat at 120 BPM is 0.5 s = ~21.5 blocks; render half a beat.
  uint32_t block = 1;
  for (; block <= 10; ++block) render(block);

  int chordOns = 0;
  for (const Seen& s : seen) {
    if (s.type == MusicalEventType::NoteOn) {
      assert(s.chord);
      ++chordOns;
    }
  }
  assert(chordOns == 3);
  assert(engine.patternPlaybackState_[0].voiceCount() == 3);
  assert(engine.chordInternalNote_[0] == 67);  // the top note sounds inside

  // Past E's end (1/8 note): E released alone, C and G still held.
  for (; block <= 16; ++block) render(block);
  int offE = 0, offC = 0;
  for (const Seen& s : seen) {
    if (s.type == MusicalEventType::NoteOff && s.note == 64) ++offE;
    if (s.type == MusicalEventType::NoteOff && s.note == 60) ++offC;
  }
  assert(offE == 1 && offC == 0);
  assert(engine.patternPlaybackState_[0].voiceCount() == 2);

  // STOP mid-chord releases every held note inside, and on the wire: STOP runs
  // outside a render block, so the chord NoteOffs become a Synth A target
  // panic that MidiDispatchTask turns into AllNotesOff (which UsbMidiOutput
  // applies to the chord set; see test_pattern_midi_articulation).
  (void)queue.takePendingAllNotesOffMask();
  engine.stop();
  assert((queue.takePendingAllNotesOffMask() &
          ScheduledMusicalEventQueue::kSynthAMask) != 0);
  assert(!engine.patternPlaybackState_[0].active());
  assert(engine.chordInternalNote_[0] == -1);

  std::puts("Melody chord playback: PASS");
  return 0;
}
