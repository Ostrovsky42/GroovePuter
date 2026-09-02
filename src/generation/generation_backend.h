#ifndef GROOVEPUTER_GENERATION_GENERATION_BACKEND_H
#define GROOVEPUTER_GENERATION_GENERATION_BACKEND_H

#include <cstdint>

namespace GroovePuterRhythm {

// Migration-only runtime choice. This type is deliberately independent from
// Scene and persistence; Stage 4 must not make backend selection user state.
enum class GenerationBackend : uint8_t {
  LegacyAtlas = 0,
  LegacyProcedural,
  Vocabulary,
  Count,
};

struct GenerationBackendRoute {
  GenerationBackend applyBackend = GenerationBackend::LegacyProcedural;
  GenerationBackend shadowBackend = GenerationBackend::Vocabulary;
  bool shadowEnabled = false;
};

constexpr bool validGenerationBackend(GenerationBackend backend) {
  return static_cast<uint8_t>(backend) <
         static_cast<uint8_t>(GenerationBackend::Count);
}

constexpr bool validGenerationBackendRoute(
    const GenerationBackendRoute& route) {
  return validGenerationBackend(route.applyBackend) &&
         validGenerationBackend(route.shadowBackend) &&
         (!route.shadowEnabled || route.applyBackend != route.shadowBackend);
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_GENERATION_BACKEND_H
