// One-time migration of GroovePuter data from the card root into /grooveputer.
// Runs on the host against SDMock, which maps card paths onto a temp directory.

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "src/storage/data_root_migration.h"

SerialMock Serial;
SDMock SD;

namespace fs = std::filesystem;

namespace {

fs::path g_root;

void writeCard(const char* relative, const std::string& body) {
  const fs::path path = g_root / relative;
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << body;
}

std::string readCard(const char* relative) {
  std::ifstream in(g_root / relative, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

bool cardExists(const char* relative) { return fs::exists(g_root / relative); }

void resetCard() {
  fs::remove_all(g_root);
  fs::create_directories(g_root);
  SD.setRoot(g_root);
}

void seedLegacyCard() {
  writeCard("scenes/grooveputer_scene.json", "scene-main");
  writeCard("scenes/grooveputer_scene.json.bak", "scene-backup");
  writeCard("scenes/grooveputer_scene.auto.json", "scene-auto");
  writeCard("scenes/grooveputer_scene_name.txt", "grooveputer_scene");
  writeCard("scenes/stale.json.tmp", "half-written");
  writeCard("scenes/voices/0001.raw", "voice-cache");
  writeCard("patterns/proj_a/page_00.gpp", "pattern-page");
  writeCard("patterns/proj_a/page_00_A.gpp", "pattern-slot");
  writeCard("projects/proj_a/melody/v0_g000.gpml", "melody");
  writeCard("midi/song.mid", "smf-bytes");
  writeCard("grooveputer_tilt.txt", "tilt=1");
  writeCard("grooveputer_0001.wav", "recording-stays-in-root");
}

void testFullMigration() {
  resetCard();
  seedLegacyCard();

  assert(GroovePuterStorage::migrateLegacyDataRootOnce());

  assert(readCard("grooveputer/scenes/grooveputer_scene.json") == "scene-main");
  assert(readCard("grooveputer/scenes/grooveputer_scene.json.bak") == "scene-backup");
  assert(readCard("grooveputer/scenes/grooveputer_scene.auto.json") == "scene-auto");
  assert(readCard("grooveputer/scenes/grooveputer_scene_name.txt") == "grooveputer_scene");
  assert(readCard("grooveputer/patterns/proj_a/page_00.gpp") == "pattern-page");
  assert(readCard("grooveputer/patterns/proj_a/page_00_A.gpp") == "pattern-slot");
  assert(readCard("grooveputer/projects/proj_a/melody/v0_g000.gpml") == "melody");
  assert(readCard("grooveputer/midi/song.mid") == "smf-bytes");
  assert(readCard("grooveputer/grooveputer_tilt.txt") == "tilt=1");

  // The voice cache is rebuilt in place, stale temporaries are not data, and the
  // WAV recordings stay in the card root for this step.
  assert(!cardExists("grooveputer/scenes/voices"));
  assert(!cardExists("grooveputer/scenes/stale.json.tmp"));
  assert(cardExists("grooveputer_0001.wav"));

  // Legacy data is copied, never moved or deleted.
  assert(readCard("scenes/grooveputer_scene.json") == "scene-main");
  assert(readCard("midi/song.mid") == "smf-bytes");
  assert(readCard("grooveputer_tilt.txt") == "tilt=1");

  assert(cardExists("grooveputer/.migrated"));
  assert(!cardExists("grooveputer/scenes/grooveputer_scene.json.migrating"));
  std::puts("  full migration copies, marks, and keeps legacy data");
}

void testMarkerPreventsRerunOverwrite() {
  resetCard();
  seedLegacyCard();
  assert(GroovePuterStorage::migrateLegacyDataRootOnce());

  // The user keeps working in the new root; a later legacy change must not leak in.
  writeCard("grooveputer/scenes/grooveputer_scene.json", "scene-edited-in-new-root");
  writeCard("scenes/grooveputer_scene.json", "scene-changed-on-legacy");
  assert(GroovePuterStorage::migrateLegacyDataRootOnce());
  assert(readCard("grooveputer/scenes/grooveputer_scene.json") == "scene-edited-in-new-root");
  std::puts("  marker present: no rerun, new data untouched");
}

void testRetryFillsGapsWithoutOverwrite() {
  resetCard();
  seedLegacyCard();
  // Simulate an interrupted first run: one scene already in the new root and no marker.
  writeCard("grooveputer/scenes/grooveputer_scene.json", "already-new");

  assert(GroovePuterStorage::migrateLegacyDataRootOnce());
  assert(readCard("grooveputer/scenes/grooveputer_scene.json") == "already-new");
  assert(readCard("grooveputer/midi/song.mid") == "smf-bytes");
  assert(readCard("grooveputer/projects/proj_a/melody/v0_g000.gpml") == "melody");
  assert(cardExists("grooveputer/.migrated"));
  std::puts("  retry after partial run fills gaps and keeps existing new files");
}

void testFreshCardCreatesNothingLegacy() {
  resetCard();
  assert(GroovePuterStorage::migrateLegacyDataRootOnce());
  assert(cardExists("grooveputer/.migrated"));
  assert(!cardExists("scenes"));
  assert(!cardExists("patterns"));
  std::puts("  fresh card: marker only");
}

void testReadFallbackPrefersNewAndNeverWritesLegacy() {
  resetCard();
  writeCard("scenes/only_legacy.json", "legacy-only");
  writeCard("scenes/both.json", "legacy-copy");
  writeCard("grooveputer/scenes/both.json", "new-copy");

  // New path first; legacy only when the new file is absent; plain miss otherwise.
  assert(GroovePuterStorage::readPathFor("/grooveputer/scenes/both.json") ==
         "/grooveputer/scenes/both.json");
  assert(GroovePuterStorage::readPathFor("/grooveputer/scenes/only_legacy.json") ==
         "/scenes/only_legacy.json");
  assert(GroovePuterStorage::readPathFor("/grooveputer/scenes/none.json") ==
         "/grooveputer/scenes/none.json");
  assert(readCard("scenes/only_legacy.json") == "legacy-only");
  assert(!cardExists("grooveputer/scenes/only_legacy.json"));
  std::puts("  read fallback: new first, legacy read-only");
}

}  // namespace

int main() {
  g_root = fs::temp_directory_path() / "grooveputer_data_root_migration";
  testFullMigration();
  testMarkerPreventsRerunOverwrite();
  testRetryFillsGapsWithoutOverwrite();
  testFreshCardCreatesNothingLegacy();
  testReadFallbackPrefersNewAndNeverWritesLegacy();
  fs::remove_all(g_root);
  std::puts("data root migration: ALL PASS");
  return 0;
}
