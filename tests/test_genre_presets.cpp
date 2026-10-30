// kGenrePresets: six named GENRE presets, each a legal (mode, recipe) pair.
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"

#include <cstdio>
#include <cstring>

SerialMock Serial;
SDMock SD;

int main() {
  int failures = 0;
  if (kGenrePresetCount != 6) {
    std::printf("FAIL count %d\n", kGenrePresetCount);
    ++failures;
  }
  for (int i = 0; i < kGenrePresetCount; ++i) {
    const GenrePreset& a = kGenrePresets[i];
    if (!a.name || !a.name[0]) { std::printf("FAIL empty name %d\n", i); ++failures; continue; }
    if (!GroovePuterRhythm::isRecipeAvailable(a.generative, a.recipe)) {
      std::printf("FAIL %s recipe unavailable\n", a.name);
      ++failures;
    }
    for (int j = i + 1; j < kGenrePresetCount; ++j) {
      const GenrePreset& b = kGenrePresets[j];
      if (b.name && std::strcmp(a.name, b.name) == 0) {
        std::printf("FAIL duplicate name %s\n", a.name);
        ++failures;
      }
      if (a.generative == b.generative && a.recipe == b.recipe) {
        std::printf("FAIL duplicate (mode,recipe) %s / %s\n", a.name, b.name);
        ++failures;
      }
    }
  }
  std::printf(failures ? "FAILED\n" : "test_genre_presets OK\n");
  return failures ? 1 : 0;
}
