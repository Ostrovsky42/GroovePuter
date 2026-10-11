#pragma once

#include <cstdint>

// 0.9.18 GEN panel (GENRE, Tab): where plain G writes and how busy the parts
// come out. Session state like the requested phrase LENGTH it sits next to;
// NORMAL liveliness and STEPS keep G exactly as before.
namespace GroovePuterState {

enum class GenerationTarget : uint8_t {
  Steps = 0,
  Melody,
};

enum class GenerationLiveliness : uint8_t {
  Calm = 0,
  Normal,
  Lively,
};

// How long generated Melody notes ring (steps have no note length).
enum class GenerationNoteLength : uint8_t {
  Short = 0,  // the genre's gate, as before
  Mixed,      // some notes held to the next one, more before rests
  Long,       // legato: every note held to the next one
};

namespace generation_shape_detail {
inline GenerationTarget& targetStorage() {
  static GenerationTarget target = GenerationTarget::Steps;
  return target;
}
inline GenerationLiveliness& livelinessStorage() {
  static GenerationLiveliness liveliness = GenerationLiveliness::Normal;
  return liveliness;
}
inline bool& keepDrumsStorage() {
  static bool keep = false;
  return keep;
}
inline GenerationNoteLength& noteLengthStorage() {
  static GenerationNoteLength length = GenerationNoteLength::Short;
  return length;
}
inline uint8_t& melodyVoiceStorage() {
  static uint8_t voice = 0;
  return voice;
}
}  // namespace generation_shape_detail

inline GenerationTarget generationTarget() {
  return generation_shape_detail::targetStorage();
}

// The synth a G from GENRE writes a Melody for (TARGET MELODY A / B). On a
// synth's own page G always writes that synth.
inline int melodyTargetVoice() {
  return generation_shape_detail::melodyVoiceStorage();
}

// TARGET steps STEPS -> MELODY A -> MELODY B (and back with direction < 0).
inline GenerationTarget cycleGenerationTarget(int direction = 1) {
  auto& target = generation_shape_detail::targetStorage();
  auto& voice = generation_shape_detail::melodyVoiceStorage();
  int position = target == GenerationTarget::Steps ? 0 : 1 + (voice ? 1 : 0);
  position += direction < 0 ? -1 : 1;
  if (position < 0) position = 2;
  if (position > 2) position = 0;
  target = position == 0 ? GenerationTarget::Steps : GenerationTarget::Melody;
  if (position > 0) voice = static_cast<uint8_t>(position - 1);
  return target;
}

// DRUMS KEEP: a STEPS G from GENRE rebuilds only the synths, against the
// drums already there. NEW (default) regenerates the drums too.
inline bool generationKeepsDrums() {
  return generation_shape_detail::keepDrumsStorage();
}

inline bool toggleGenerationKeepsDrums() {
  bool& keep = generation_shape_detail::keepDrumsStorage();
  keep = !keep;
  return keep;
}

inline GenerationLiveliness generationLiveliness() {
  return generation_shape_detail::livelinessStorage();
}

inline void setGenerationLiveliness(GenerationLiveliness liveliness) {
  if (static_cast<uint8_t>(liveliness) > 2) return;
  generation_shape_detail::livelinessStorage() = liveliness;
}

inline GenerationLiveliness cycleGenerationLiveliness(int direction) {
  int value = static_cast<int>(generationLiveliness()) + (direction < 0 ? -1 : 1);
  if (value < 0) value = 2;
  if (value > 2) value = 0;
  setGenerationLiveliness(static_cast<GenerationLiveliness>(value));
  return generationLiveliness();
}

inline const char* generationLivelinessName(GenerationLiveliness liveliness) {
  switch (liveliness) {
    case GenerationLiveliness::Calm: return "CALM";
    case GenerationLiveliness::Normal: return "NORMAL";
    case GenerationLiveliness::Lively: return "LIVELY";
  }
  return "NORMAL";
}

inline GenerationNoteLength generationNoteLength() {
  return generation_shape_detail::noteLengthStorage();
}

inline void setGenerationNoteLength(GenerationNoteLength length) {
  if (static_cast<uint8_t>(length) > 2) return;
  generation_shape_detail::noteLengthStorage() = length;
}

inline GenerationNoteLength cycleGenerationNoteLength(int direction) {
  int value = static_cast<int>(generationNoteLength()) + (direction < 0 ? -1 : 1);
  if (value < 0) value = 2;
  if (value > 2) value = 0;
  setGenerationNoteLength(static_cast<GenerationNoteLength>(value));
  return generationNoteLength();
}

inline const char* generationNoteLengthName(GenerationNoteLength length) {
  switch (length) {
    case GenerationNoteLength::Short: return "SHORT";
    case GenerationNoteLength::Mixed: return "MIXED";
    case GenerationNoteLength::Long: return "LONG";
  }
  return "SHORT";
}


}  // namespace GroovePuterState
