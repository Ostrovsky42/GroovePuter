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

// P0_CYCLE_DIAG=1: why does DEVELOP change nothing? Read-only; rebuilds the DEVELOP plan from
// the recipe exactly as generateCycle does and prints the plan per bar and the ghost-add room.
void diagnoseDevelop(MiniAcid& engine, uint16_t archetypeId, const std::string& file) {
  const auto* rc = engine.generatedPhraseRecipe();
  if (!rc) { std::printf("DIAG %s: no recipe\n", file.c_str()); return; }
  R::PreparedPhraseExecution ex{};
  R::PhraseExecutionScratch scr{};
  if (R::preparePhraseExecution(rc->genre, rc->materialization, rc->phraseGenerationIdentity, 4, scr, ex) !=
      R::PhraseExecutionStatus::Ready) { std::printf("DIAG %s: prepare failed\n", file.c_str()); return; }
  const auto st = R::applyPhraseLawToExecution(ex, R::PhraseEvolutionLawId::DevelopReturn);
  std::printf("DIAG %s ordinal=%u law-status=%d trajectory=%u\n", file.c_str(),
              rc->materialization.generationAttemptOrdinal, static_cast<int>(st), static_cast<unsigned>(ex.phraseTrajectory));
  const char* roleName[] = {"K", "Bb", "CH", "OH", "Pc"};
  for (int b = 0; b < ex.phrasePlan.barCount; ++b) {
    std::printf("  bar%d %-16s", b, barFnName(ex.phrasePlan.bars[b].function));
    for (int r = 0; r < 5; ++r) {
      const auto& a = ex.phrasePlan.bars[0].roles[r];
      const auto& c = ex.phrasePlan.bars[b].roles[r];
      const bool same = a.structural == c.structural && a.secondary == c.secondary && a.ghosts == c.ghosts;
      std::printf(" %s:%s(g=%04x)", roleName[r], same ? "=" : "CHG", static_cast<unsigned>(c.ghosts));
    }
    std::printf("\n");
  }
  {
    // Physical view: Loop vs DevelopReturn through the real materializer, per drum voice.
    R::PreparedPhraseExecution loop{};
    if (R::preparePhraseExecution(rc->genre, rc->materialization, rc->phraseGenerationIdentity, 4, scr, loop) ==
        R::PhraseExecutionStatus::Ready) {
      for (int b = 0; b < 4; ++b) {
        PhraseGenerator::PhraseBar l{}, d{};
        GeneratedPhraseP1R::materializeOneBar(engine, loop, static_cast<uint8_t>(b), 0, l);
        GeneratedPhraseP1R::materializeOneBar(engine, ex, static_cast<uint8_t>(b), 0, d);
        std::printf("  phys bar%d loopFn=%s: drum voices differing:", b, barFnName(loop.phrasePlan.barCount ? loop.phrasePlan.bars[b].function : R::BarFunction::Statement));
        bool any = false;
        for (int v = 0; v < 8; ++v) {
          uint16_t lm = 0, dm = 0;
          for (int st = 0; st < 16; ++st) {
            lm |= (l.drums.voices[v].steps[st].hit ? 1u : 0u) << st;
            dm |= (d.drums.voices[v].steps[st].hit ? 1u : 0u) << st;
          }
          if (std::memcmp(&l.drums.voices[v], &d.drums.voices[v], sizeof(DrumPattern)) != 0) { std::printf(" v%d(%04x->%04x)", v, lm, dm); any = true; }
        }
        std::printf("%s\n", any ? "" : " none");
      }
    }
  }
  const auto& cat = R::ReferenceVocabulary::phraseEvolutionCatalog();
  for (uint16_t i = 0; i < cat.archetypeCount; ++i) {
    const auto& a = cat.archetypes[i];
    if (a.id != archetypeId) continue;
    const auto& bud = a.mutation.level[static_cast<int>(R::RealizationLevel::P3Transformation)];
    std::printf("  P3 budget maxAdds=%u maxDrops=%u flags=0x%x optionalAdds=%d ghostConv=%d  density.ornamentMax=%u\n",
                bud.maxAdds, bud.maxDrops, bud.flags, !!(bud.flags & R::AllowOptionalAdds), !!(bud.flags & R::AllowGhostConversion),
                a.density.ornamentMax);
    for (uint8_t l = 0; l < a.laneCount; ++l) {
      const auto& lane = a.lanes[l];
      const auto& role = ex.phrasePlan.bars[1].roles[static_cast<int>(lane.role)];
      const uint16_t occ = role.structural | role.secondary | role.ghosts;
      const uint16_t room = (lane.preferred | lane.optional) & ~occ & ~lane.forbidden;
      std::printf("  lane %d ornamentMax=%u ghosts=%d free(pref|opt minus onsets,forbidden; protected NOT removed)=%04x\n",
                  static_cast<int>(lane.role), lane.ornamentMax, __builtin_popcount(role.ghosts), room);
    }
  }
}

// P0_CYCLE_SURVEY=1: which law is the kept phrase's OWN (natural) law, and does the requested
// DEVELOP / BREAK law coincide with it (then the section repeats A)? Identities 0..7, P3.
void survey() {
  struct S { const char* genre; uint16_t id; const char* name; };
  const S items[] = {{"Techno", 404, "broken_techno"}, {"Techno", 420, "machine_syncopation"}, {"DnB", 413, "two_step_roll"},
                     {"DnB", 415, "sparse_fast_break"}, {"UKG", 417, "classic_2step"}, {"UKG", 418, "skippy_2step"},
                     {"Dub", 410, "steppers"}, {"Funk", 713, "funk_house_bridge"}};
  const char* lawName[] = {"Loop", "RepeatReply", "DevelopReturn", "SparseDrift"};
  int total = 0, devDup = 0, brkDup = 0, natural[4] = {0, 0, 0, 0};
  for (const S& it : items) {
    const GenreCase* g = findGenre(it.genre);
    std::printf("SURVEY %-8s %-20s natural law by ordinal 0..7:", it.genre, it.name);
    for (uint32_t o = 0; o < 8; ++o) {
      MiniAcid engine(kRate, nullptr);
      configure(engine, *g);
      Scene& sc = engine.sceneManager().currentScene();
      sc.genre.rhythmSelectionMode = static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
      sc.genre.rhythmArchetypeId = it.id;
      GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
      GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
      const bool ok = GeneratedPhraseSong::prepareWithGenerationAttempt(engine, 4, 0, o, true, prepared);
      GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation);
      if (!ok || !prepared.useP1RRoute) { std::printf(" -"); continue; }
      const R::PreparedPhraseExecution nat = prepared.p1rExecution;
      const int law = static_cast<int>(nat.selection.composition.phraseLaw);
      R::PreparedPhraseExecution d = nat, b = nat;
      const bool dOk = R::applyPhraseLawToExecution(d, R::PhraseEvolutionLawId::DevelopReturn) == R::PhraseLawApplyStatus::Applied;
      const bool bOk = R::applyPhraseLawToExecution(b, R::PhraseEvolutionLawId::SparseDrift) == R::PhraseLawApplyStatus::Applied;
      const bool dDup = dOk && d.phraseTrajectory == nat.phraseTrajectory;
      const bool bDup = bOk && b.phraseTrajectory == nat.phraseTrajectory;
      ++total; ++natural[law]; devDup += dDup; brkDup += bDup;
      std::printf(" %c%c%c", "LRDS"[law], dDup ? '!' : '.', bDup ? '!' : '.');
    }
    std::printf("\n");
  }
  std::printf("SURVEY total=%d natural: Loop=%d RepeatReply=%d DevelopReturn=%d SparseDrift=%d | DEVELOP repeats A: %d, BREAK repeats A: %d\n",
              total, natural[0], natural[1], natural[2], natural[3], devDup, brkDup);
  (void)lawName;
}
}  // namespace

int main() {
  if (std::getenv("P0_CYCLE_SURVEY")) { survey(); return 0; }
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
      if (ok && std::getenv("P0_CYCLE_DIAG")) diagnoseDevelop(engine, it.archetype, file);
      GeneratedPhraseSong::CycleStatus cs = GeneratedPhraseSong::CycleStatus::Failed;
      int cycleBars = 8;
      bool developSkipped = false;
      if (ok) {
        const auto cycle = GeneratedPhraseSong::generateCycle(engine, kGuard);
        cs = cycle.status;
        cycleBars = cycle.bars;
        developSkipped = cycle.developSkipped;
        ok = cs == GeneratedPhraseSong::CycleStatus::CommittedNow;
        status += " cycle:" + std::to_string(static_cast<int>(cs));
      }
      if (!ok) { manifest << file << '\t' << it.genre << '\t' << it.name << '\t' << status << "\t-\t-\t-\n"; continue; }

      uint64_t base[4][3];
      for (int b = 0; b < 4; ++b) for (int l = 0; l < 3; ++l) base[b][l] = laneHash(sc, b, l);
      std::string diff;
      for (int sec = 1; sec * 4 < 4 + cycleBars; ++sec) {
        int n[3] = {0, 0, 0};
        for (int b = 0; b < 4; ++b) for (int l = 0; l < 3; ++l) n[l] += laneHash(sc, sec * 4 + b, l) != base[b][l];
        const char* name = (developSkipped || sec == 2) ? " BREAK " : "DEVELOP ";
        diff += name + std::to_string(n[0]) + "/" + std::to_string(n[1]) + "/" + std::to_string(n[2]);
      }
      if (developSkipped) diff = "(DEVELOP skipped: A already has that law) " + diff;
      const int totalBars = 4 + cycleBars;

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
      const size_t total = perBar * totalBars + static_cast<size_t>(kRate * 1.5f);
      std::vector<int16_t> pcm(total, 0);
      for (size_t at = 0; at < total; at += 128) engine.generateAudioBuffer(pcm.data() + at, std::min<size_t>(128, total - at));
      engine.stop();
      writeWav(out + "/" + file + ".wav", pcm);
      manifest << file << '\t' << it.genre << '\t' << it.name << '\t' << status << '\t' << bpm
               << (developSkipped ? "\tA 0:00 | BREAK bar5 (Return = bar8)\t" : "\tA 0:00 | DEVELOP bar5 | BREAK bar9 (Return = bar12)\t") << diff << '\n';
      std::printf("%s ok %s\n", file.c_str(), diff.c_str());
      ++made;
    }
  }
  std::printf("rendered %d\n", made);
  return 0;
}
