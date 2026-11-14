#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/midi_song_export.h"
#include "src/midi/smf_document.h"
#include "src/midi/smf_export_writer.h"

namespace {

class VectorSink final : public GroovePuterMidi::ISmfExportByteSink {
 public:
  bool write(const uint8_t* data, std::size_t length) override {
    bytes.insert(bytes.end(), data, data + length);
    return true;
  }
  std::vector<uint8_t> bytes;
};

class FakeMaterials final : public GroovePuterMidi::ISongExportMaterialSource {
 public:
  bool loadPage(uint8_t page, const Scene*& out) override {
    ++pageReads;
    if (!pageAvailable || page != 0) return false;
    out = &scene;
    return true;
  }

  bool loadMelody(uint8_t voice, uint8_t globalSlot,
                  PhraseRuntime::RuntimeSynthEventBuffer& out) override {
    ++melodyReads;
    if (!melodyAvailable || voice != melodyVoice || globalSlot != melodySlot) {
      return false;
    }
    out = melody;
    return true;
  }

  Scene scene{};
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  uint8_t melodyVoice{0};
  uint8_t melodySlot{0};
  int pageReads{0};
  int melodyReads{0};
  bool melodyAvailable{false};
  bool pageAvailable{true};
};

GroovePuterMidi::SmfDocument writeAndParse(
    const GroovePuterMidi::MidiSongExportSnapshot& snapshot,
    FakeMaterials& materials,
    GroovePuterMidi::SongExportLane lane,
    GroovePuterMidi::SongExportSummary& summary) {
  VectorSink trackBytes;
  GroovePuterMidi::SmfExportTrackWriter track;
  assert(GroovePuterMidi::writeSongExportTrack(
      snapshot, materials, lane, trackBytes, track, summary));
  VectorSink file;
  assert(GroovePuterMidi::writeSmfHeader(file, 1,
      GroovePuterMidi::kSongExportPpqn));
  assert(GroovePuterMidi::writeSmfTrackHeader(file, trackBytes.bytes.size()));
  assert(file.write(trackBytes.bytes.data(), trackBytes.bytes.size()));
  const auto parsed = GroovePuterMidi::SmfParser::parse(
      file.bytes.data(), file.bytes.size());
  assert(parsed.ok());
  return parsed.document;
}

GroovePuterMidi::MidiSongExportSnapshot onePatternSnapshot() {
  GroovePuterMidi::MidiSongExportSnapshot snapshot{};
  snapshot.song.length = 1;
  snapshot.song.positions[0].patterns[0] = 0;
  snapshot.patternBars = 1;
  snapshot.synthSettings[0].synthIndex = 0;
  snapshot.synthSettings[1].synthIndex = 1;
  return snapshot;
}

std::vector<GroovePuterMidi::SmfEvent> noteEvents(
    const GroovePuterMidi::SmfDocument& document) {
  std::vector<GroovePuterMidi::SmfEvent> result;
  for (const auto& event : document.events) {
    if (event.kind == GroovePuterMidi::SmfEventKind::NoteOn ||
        event.kind == GroovePuterMidi::SmfEventKind::NoteOff) {
      result.push_back(event);
    }
  }
  return result;
}

}  // namespace

int main() {
  using namespace GroovePuterMidi;
  MidiSongExportSnapshot snapshot{};
  snapshot.song.length = 2;
  snapshot.song.reverse = true;
  snapshot.song.positions[0].patterns[0] = 0;
  snapshot.song.positions[1].patterns[0] = 1;
  snapshot.song.setStepsOverride(0, 0, true);
  snapshot.patternBars = 2;
  snapshot.tempoMicrosecondsPerQuarter = 500000;
  snapshot.synthSettings[0] = PhraseRuntime::PatternProjectionSettings{};
  snapshot.synthSettings[0].synthIndex = 0;
  snapshot.synthSettings[0].gateLengthRatio = 0.5f;
  snapshot.synthSettings[1] = PhraseRuntime::PatternProjectionSettings{};
  snapshot.synthSettings[1].synthIndex = 1;
  snapshot.routes.patternSynthA.channel = 4;
  snapshot.routes.patternSynthA.enabled = false;  // export ignores live gate

  FakeMaterials materials;
  materials.scene.materialSlots[0][0].kind =
      GroovePuterMaterial::MaterialKind::Melody;
  auto& first = materials.scene.synthABanks[0].patterns[0];
  first.steps[0].note = 60;
  first.steps[0].velocity = 100;
  first.steps[4].note = 62;
  first.steps[4].velocity = 100;
  first.steps[4].accent = 1;
  first.steps[4].probability = 50;
  auto& second = materials.scene.synthABanks[0].patterns[1];
  second.steps[0].note = 67;
  second.steps[0].velocity = 90;
  materials.melodyAvailable = false;

  SongExportSummary synthSummary{};
  const auto synth = writeAndParse(snapshot, materials,
                                   SongExportLane::SynthA, synthSummary);
  assert(synthSummary.has(SongExportWarning::ConditionalFlattened));
  assert(materials.melodyReads == 0);  // STEPS override beats Melody descriptor
  const auto synthNotes = noteEvents(synth);
  assert(synthNotes.size() == 12);  // six onsets, each with its own Note Off
  assert(synthSummary.lanes[static_cast<uint8_t>(SongExportLane::SynthA)]
             .eventCount == synthNotes.size());
  assert(synth.tracks[0].name == "Synth A");
  assert(synthNotes[0].tick == 0 && synthNotes[0].kind == SmfEventKind::NoteOn &&
         synthNotes[0].data1 == 67 && synthNotes[0].channel == 4);
  assert(synthNotes[2].tick == 6144 && synthNotes[2].kind == SmfEventKind::NoteOn &&
         synthNotes[2].data1 == 67);
  assert(synthNotes[4].tick == 12288 && synthNotes[4].kind == SmfEventKind::NoteOn &&
         synthNotes[4].data1 == 60);
  assert(synthNotes[6].tick == 13824 && synthNotes[6].kind == SmfEventKind::NoteOn &&
         synthNotes[6].data1 == 62 && synthNotes[6].data2 == 127);
  assert(synthNotes[10].tick == 19968 && synthNotes[10].kind == SmfEventKind::NoteOn &&
         synthNotes[10].data1 == 62);

  snapshot.song = Song{};
  snapshot.song.length = 1;
  snapshot.song.positions[0].patterns[0] = 0;
  snapshot.song.reverse = false;
  snapshot.patternBars = 2;
  materials.scene.materialSlots[0][0].kind =
      GroovePuterMaterial::MaterialKind::Melody;
  materials.melodyAvailable = true;
  materials.melodyVoice = 0;
  materials.melodySlot = 0;
  materials.melody = PhraseRuntime::RuntimeSynthEventBuffer{};
  materials.melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  materials.melody.count = 2;
  materials.melody.events[0] = {0, 180, 72, 88, 100, 0, 0, 0};
  materials.melody.events[1] = {6, 120, 72, 92, 100, 0, 0, 0};
  SongExportSummary melodySummary{};
  const auto melody = writeAndParse(snapshot, materials,
                                    SongExportLane::SynthA, melodySummary);
  const auto melodyNotes = noteEvents(melody);
  assert(melodyNotes.size() == 8);
  assert(melodyNotes[0].tick == 0 && melodyNotes[0].data1 == 72);
  assert(melodyNotes[1].tick == 96 && melodyNotes[1].data1 == 72);
  assert(melodyNotes[2].tick == 180 && melodyNotes[2].kind == SmfEventKind::NoteOff);
  assert(melodyNotes[3].tick == 216 && melodyNotes[3].kind == SmfEventKind::NoteOff);
  assert(melodyNotes[4].tick == 6144 && melodyNotes[4].data1 == 72);
  assert(melodyNotes[5].tick == 6240 && melodyNotes[5].data1 == 72);
  assert(melodyNotes[6].tick == 6324 && melodyNotes[6].kind == SmfEventKind::NoteOff);
  assert(melodyNotes[7].tick == 6360 && melodyNotes[7].kind == SmfEventKind::NoteOff);

  snapshot.song = Song{};
  snapshot.song.length = 1;
  snapshot.song.positions[0].patterns[0] = 0;
  snapshot.patternBars = 8;
  materials.scene.materialSlots[0][0].kind =
      GroovePuterMaterial::MaterialKind::Pattern;
  materials.scene.synthABanks[0].patterns[0] = SynthPattern{};
  materials.scene.synthABanks[0].patterns[0].steps[0].note = 55;
  SongExportSummary eightBarSummary{};
  const auto eightBars = writeAndParse(snapshot, materials,
      SongExportLane::SynthA, eightBarSummary);
  assert(eightBars.endTick == 8u * 384u * 16u);
  assert(noteEvents(eightBars).size() == 16);

  snapshot.patternBars = 4;
  SongExportSummary fourBarSummary{};
  const auto fourBars = writeAndParse(snapshot, materials,
      SongExportLane::SynthA, fourBarSummary);
  assert(fourBars.endTick == 4u * 384u * 16u);
  assert(noteEvents(fourBars).size() == 8);

  snapshot.song = Song{};
  snapshot.song.length = 1;
  snapshot.song.positions[0].patterns[2] = 0;
  snapshot.patternBars = 1;
  materials.scene.drumBanks[0].patterns[0].voices[0].steps[0].hit = 1;
  materials.scene.drumBanks[0].patterns[0].voices[0].steps[0].velocity = 110;
  materials.scene.drumBanks[0].patterns[0].voices[0].steps[0].probability = 50;
  materials.scene.drumBanks[0].patterns[0].voices[0].steps[0].fx = DRUM_FX_FLAM;
  materials.scene.drumBanks[0].patterns[0].lanes[0].nodeCount = 1;
  snapshot.routes.patternDrums[0] = DrumMidiRoute{false, 3, 36};
  SongExportSummary drumSummary{};
  const auto drums = writeAndParse(snapshot, materials,
                                   SongExportLane::Drums, drumSummary);
  const auto drumNotes = noteEvents(drums);
  assert(drumNotes.size() == 2);
  assert(drumNotes[0].kind == SmfEventKind::NoteOn && drumNotes[0].channel == 3 &&
         drumNotes[0].data1 == 36 && drumNotes[0].data2 == 110);
  assert(drumNotes[1].kind == SmfEventKind::NoteOff && drumNotes[1].tick == 246);
  assert(drumSummary.has(SongExportWarning::ConditionalFlattened));
  assert(drumSummary.has(SongExportWarning::DrumFxOmitted));
  assert(drumSummary.has(SongExportWarning::DrumAutomationOmitted));

  snapshot.song.positions[0].patterns[3] = 4;
  VectorSink conductorBytes;
  SmfExportTrackWriter conductorWriter;
  assert(writeSongExportConductor(snapshot, drumSummary,
                                  conductorBytes, conductorWriter));
  assert(drumSummary.has(SongExportWarning::VoiceOmitted));
  VectorSink conductorFile;
  assert(writeSmfHeader(conductorFile, 1, kSongExportPpqn));
  assert(writeSmfTrackHeader(conductorFile, conductorBytes.bytes.size()));
  assert(conductorFile.write(conductorBytes.bytes.data(),
                             conductorBytes.bytes.size()));
  const auto conductorParse = SmfParser::parse(
      conductorFile.bytes.data(), conductorFile.bytes.size());
  assert(conductorParse.ok());
  assert(conductorParse.document.tracks[0].name == "Conductor");
  bool hasTempo = false;
  bool hasSignature = false;
  for (const auto& event : conductorParse.document.events) {
    if (event.kind == SmfEventKind::Tempo && event.value == 500000) hasTempo = true;
    if (event.kind == SmfEventKind::TimeSignature && event.data1 == 4 &&
        event.data2 == 2) hasSignature = true;
  }
  assert(hasTempo && hasSignature);

  MidiSongExportSnapshot missingPageSnapshot = onePatternSnapshot();
  FakeMaterials missingPage;
  missingPage.pageAvailable = false;
  VectorSink missingPageBytes;
  SmfExportTrackWriter missingPageWriter;
  SongExportSummary missingPageSummary{};
  SongExportError missingPageError = SongExportError::None;
  assert(!writeSongExportTrack(missingPageSnapshot, missingPage,
      SongExportLane::SynthA, missingPageBytes, missingPageWriter,
      missingPageSummary, &missingPageError));
  assert(missingPageError == SongExportError::PageUnavailable);

  MidiSongExportSnapshot missingMelodySnapshot = onePatternSnapshot();
  FakeMaterials missingMelody;
  missingMelody.scene.materialSlots[0][0].kind =
      GroovePuterMaterial::MaterialKind::Melody;
  VectorSink missingMelodyBytes;
  SmfExportTrackWriter missingMelodyWriter;
  SongExportSummary missingMelodySummary{};
  SongExportError missingMelodyError = SongExportError::None;
  assert(!writeSongExportTrack(missingMelodySnapshot, missingMelody,
      SongExportLane::SynthA, missingMelodyBytes, missingMelodyWriter,
      missingMelodySummary, &missingMelodyError));
  assert(missingMelodyError == SongExportError::MelodyUnavailable);

  MidiSongExportSnapshot emptySnapshot{};
  assert(!validSongExportSnapshot(emptySnapshot));
  return 0;
}
