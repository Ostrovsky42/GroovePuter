// GroovePuter 0.9.14 P0 -- audio of the PRODUCT cycle (host only, not firmware).
//
// Unlike m0a_render (sections stitched from separately prepared phrases), this drives the
// real transaction: GeneratedPhraseSong::generate (kept phrase A, P3) then generateCycle
// (published DEVELOP + BREAK, one Undo), then plays Song rows 0..11 through the real engine.
// Attempt ordinals are whatever the production allocator hands out (not chosen).
// Mix C: SynthA/B 1.5, drums 0.45.
#define M0A_NO_MAIN
#include "m0a_corpus.cpp"

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/state/generation_request_state.h"

namespace {
constexpr uint32_t kRate = ::kSampleRate;

void writeWav(const std::string& path, const std::vector<int16_t>& pcm) {
  std::ofstream f(path, std::ios::binary);
  auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  const uint32_t bytes = static_cast<uint32_t>(pcm.size() * 2);
  f.write("RIFF", 4); w32(36 + bytes); f.write("WAVE", 4);
  f.write("fmt ", 4); w32(16); w16(1); w16(1); w32(kRate); w32(kRate * 2); w16(2); w16(16);
  f.write("data", 4); w32(bytes);
  f.write(reinterpret_cast<const char*>(pcm.data()), bytes);
}

struct Item { const char* genre; uint16_t archetype; const char* name; int takes; };
const auto kGuard = [](auto&& body) { body(); };

uint64_t laneHash(const Scene& sc, int row, int lane) {
  const Song& song = sc.songs[0];
  const int pat = song.positions[row].patterns[static_cast<int>(SongTrack::SynthA)] % kPatternsPerPage;
  const int b = pat / Bank<SynthPattern>::kPatterns, i = pat % Bank<SynthPattern>::kPatterns;
  const DrumPatternSet& d = sc.drumBanks[b].patterns[i];
  const SynthPattern& a = sc.synthABanks[b].patterns[i];
  const SynthPattern& s = sc.synthBBanks[b].patterns[i];
  DrumPatternSet none{}; SynthPattern noneS{};
  if (lane == 0) return GeneratedPhraseP1R::canonicalBarHash(d, noneS, noneS);
  if (lane == 1) return GeneratedPhraseP1R::canonicalBarHash(none, a, noneS);
  return GeneratedPhraseP1R::canonicalBarHash(none, noneS, s);
}
}  // namespace

int main() {
  const char* o = std::getenv("P0_CYCLE_OUT");
  const std::string out = o ? o : "build/p0/product_cycle";
  std::filesystem::create_directories(out);
  const Item items[] = {
      {"Techno", 404, "broken_techno", 2}, {"Techno", 420, "machine_syncopation", 2},
      {"DnB", 413, "two_step_roll", 1},    {"DnB", 415, "sparse_fast_break", 2},
      {"UKG", 417, "classic_2step", 2},    {"UKG", 418, "skippy_2step", 1},
      {"Dub", 410, "steppers", 2},         {"Funk", 713, "funk_house_bridge", 2},
  };
  std::ofstream manifest(out + "/manifest.tsv");
  manifest << "file\tgenre\tarchetype\tstatus\tbpm\tsections(bars)\tbars differing from A per section: drums/synthA/synthB\n";
  int made = 0;
  for (const Item& it : items) {
    const GenreCase* g = findGenre(it.genre);
    if (!g) continue;
    for (int take = 0; take < it.takes; ++take) {
      const std::string file = std::string(it.genre) + "_" + it.name + "_take" + std::to_string(take + 1);
      SceneStorageSdl storage;
      MiniAcid engine(kRate, &storage);
      PatternPagingService::setProjectName(("p0-render-" + file).c_str());
      PatternPagingService::clearProjectPages();
      engine.init();
      engine.setSongMode(false);
      configure(engine, *g);
      Scene& sc = engine.sceneManager().currentScene();
      sc.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
      sc.genre.rhythmArchetypeId = it.archetype;
      for (int bnk = 0; bnk < kBankCount; ++bnk)
        for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
          sc.synthABanks[bnk].patterns[i] = SynthPattern{}; sc.synthBBanks[bnk].patterns[i] = SynthPattern{};
          sc.drumBanks[bnk].patterns[i] = DrumPatternSet{};
        }
      for (int v = 0; v < Scene::kMaterialVoices; ++v)
        for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) sc.materialSlots[v][s] = GroovePuterMaterial::MaterialSlotDescriptor{};
      GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
      const auto* def = R::ReferenceVocabulary::definitionForId(it.archetype);
      const float bpm = def ? 0.5f * (def->suggestedBpmMin + def->suggestedBpmMax) : 120.0f;
      engine.setBpm(bpm);

      const auto kept = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
      std::string status = "kept:" + std::to_string(static_cast<int>(kept.status));
      bool ok = kept.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow;
      GeneratedPhraseSong::CycleStatus cs = GeneratedPhraseSong::CycleStatus::Failed;
      if (ok) {
        cs = GeneratedPhraseSong::generateCycle(engine, kGuard).status;
        ok = cs == GeneratedPhraseSong::CycleStatus::CommittedNow;
        status += " cycle:" + std::to_string(static_cast<int>(cs));
      }
      if (!ok) { manifest << file << '\t' << it.genre << '\t' << it.name << '\t' << status << "\t-\t-\t-\n"; continue; }

      uint64_t base[4][3];
      for (int b = 0; b < 4; ++b) for (int l = 0; l < 3; ++l) base[b][l] = laneHash(sc, b, l);
      std::string diff;
      for (int sec = 1; sec <= 2; ++sec) {
        int n[3] = {0, 0, 0};
        for (int b = 0; b < 4; ++b) for (int l = 0; l < 3; ++l) n[l] += laneHash(sc, sec * 4 + b, l) != base[b][l];
        diff += (sec == 1 ? "DEVELOP " : " BREAK ") + std::to_string(n[0]) + "/" + std::to_string(n[1]) + "/" + std::to_string(n[2]);
      }

      engine.rebuildPatternRuntimeEventBank();
      engine.setTrackVolume(VoiceId::SynthA, 1.5f);
      engine.setTrackVolume(VoiceId::SynthB, 1.5f);
      for (int id = static_cast<int>(VoiceId::DrumKick); id < static_cast<int>(VoiceId::Count); ++id)
        engine.setTrackVolume(static_cast<VoiceId>(id), 0.45f);
      engine.setSongMode(true);
      engine.setSongPlaybackSlot(0);
      engine.setSongPosition(0);
      engine.start();
      const size_t perBar = static_cast<size_t>(std::llround(4.0 * 60.0 / bpm * kRate));
      const size_t total = perBar * 12 + static_cast<size_t>(kRate * 1.5f);
      std::vector<int16_t> pcm(total, 0);
      for (size_t at = 0; at < total; at += 128) engine.generateAudioBuffer(pcm.data() + at, std::min<size_t>(128, total - at));
      engine.stop();
      writeWav(out + "/" + file + ".wav", pcm);
      manifest << file << '\t' << it.genre << '\t' << it.name << '\t' << status << '\t' << bpm
               << "\tA 0:00 | DEVELOP bar5 | BREAK bar9 (Return = bar12)\t" << diff << '\n';
      std::printf("%s ok %s\n", file.c_str(), diff.c_str());
      ++made;
    }
  }
  std::printf("rendered %d\n", made);
  return 0;
}
