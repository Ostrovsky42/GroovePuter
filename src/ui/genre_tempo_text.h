#pragma once

// What a genre expects of the project tempo, as text for GENRE and MATERIAL.
//
// The corridor lives in the generation profile table (flash). Until now it
// reached the musician only through APPLY = NEW TAKE + TEMPO and was shown
// nowhere, so a new scene played every genre at its own 100 BPM without a
// word. These helpers only read and format; nothing here sets the tempo.

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "../../scenes.h"
#include "../dsp/genre_manager.h"
#include "../generation/composition/generation_profile.h"

namespace GenreTempoText {

enum class Fit : uint8_t { Unknown = 0, Below, Inside, Above };

struct Info {
  uint16_t bpmMin = 0;
  uint16_t bpmMax = 0;
  uint16_t suggestedBpm = 0;
  int bpm = 0;
  Fit fit = Fit::Unknown;
};

inline Info infoFor(const GenreSettings& settings, float bpm) {
  Info info;
  info.bpm = static_cast<int>(std::lround(bpm));
  const GroovePuterRhythm::GenerationProfileView profile =
      GroovePuterRhythm::generationProfileFor(settings);
  if (profile.corridor.suggestedBpm == 0 ||
      profile.corridor.bpmMin > profile.corridor.bpmMax) {
    return info;
  }
  info.bpmMin = profile.corridor.bpmMin;
  info.bpmMax = profile.corridor.bpmMax;
  info.suggestedBpm = profile.corridor.suggestedBpm;
  info.fit = info.bpm < info.bpmMin ? Fit::Below
           : info.bpm > info.bpmMax ? Fit::Above
                                    : Fit::Inside;
  return info;
}

inline bool outside(const Info& info) {
  return info.fit == Fit::Below || info.fit == Fit::Above;
}

inline void upperGenreName(const GenreSettings& settings, char* out, size_t size) {
  if (size == 0) return;
  const GenerativeMode mode = static_cast<GenerativeMode>(
      settings.generativeMode < kGenerativeModeCount ? settings.generativeMode : 0);
  std::snprintf(out, size, "%s", GenreCatalog::generativeModeName(mode));
  for (char* c = out; *c; ++c) {
    *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
  }
}

// GENRE row value: "House  112-128" (the name as the catalog spells it).
inline void formatGenreRow(const GenreSettings& settings, const Info& info,
                           char* out, size_t size) {
  const GenerativeMode mode = static_cast<GenerativeMode>(
      settings.generativeMode < kGenerativeModeCount ? settings.generativeMode : 0);
  if (info.fit == Fit::Unknown) {
    std::snprintf(out, size, "%s", GenreCatalog::generativeModeName(mode));
    return;
  }
  std::snprintf(out, size, "%s  %u-%u", GenreCatalog::generativeModeName(mode),
                static_cast<unsigned>(info.bpmMin), static_cast<unsigned>(info.bpmMax));
}

// One status line: "HOUSE 100 BPM (112-128)". `tail` (may be null) names the
// way out, e.g. "M:+TEMPO".
inline void formatStatus(const GenreSettings& settings, const Info& info,
                         const char* tail, char* out, size_t size) {
  char name[20];
  upperGenreName(settings, name, sizeof(name));
  if (info.fit == Fit::Unknown) {
    std::snprintf(out, size, "%s %d BPM", name, info.bpm);
    return;
  }
  std::snprintf(out, size, "%s %d BPM (%u-%u)%s%s", name, info.bpm,
                static_cast<unsigned>(info.bpmMin), static_cast<unsigned>(info.bpmMax),
                tail != nullptr && tail[0] != '\0' ? " " : "",
                tail != nullptr ? tail : "");
}

}  // namespace GenreTempoText
