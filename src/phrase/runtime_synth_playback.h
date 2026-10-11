#pragma once

#include <cstdint>
#include <type_traits>

#include "runtime_synth_events.h"

namespace PhraseRuntime {

// Runtime-only event flag (0.9.17): set by RuntimeSynthPlaybackState on a note
// it holds as one voice of a chord (a Melody with overlapping notes). Never
// stored. Kept out of runtime_synth_events.h, whose public surface is frozen
// by the P1C contract; the bit is unused by the stored flags there.
constexpr uint8_t kEventChordVoice = 1u << 7u;
static_assert((kEventChordVoice & (kEventAccent | kEventSlide | kEventGhost)) == 0,
              "chord-voice bit must not collide with a stored event flag");

enum class RuntimeSynthPlaybackActionType : uint8_t {
  Release = 0,
  Start,
  Retrigger,
};

struct RuntimeSynthPlaybackAction {
  RuntimeSynthPlaybackActionType type = RuntimeSynthPlaybackActionType::Release;
  RuntimeSynthEvent event{};
};

// A chord onset may release every held voice and start one more.
constexpr uint8_t kMaxChordVoices = 4;

struct RuntimeSynthPlaybackActions {
  RuntimeSynthPlaybackAction values[kMaxChordVoices + 1]{};
  uint8_t count = 0;
};

// The single owner of Pattern/Melody note lifetimes for one synth.
//
// Steps and monophonic Melodies use acceptOnset(): one note, a new onset
// releases the old one (unchanged since P2). A Melody with overlapping notes
// uses acceptChordOnset(): up to kMaxChordVoices notes sound together, each
// for its own duration. Those voices carry kEventChordVoice so the engine can
// route them (MIDI chord path, internal top note). Every release path --
// releaseDue, hardBarrier, a mono onset -- releases all held voices.
class RuntimeSynthPlaybackState {
 public:
  RuntimeSynthPlaybackActions acceptOnset(const RuntimeSynthEvent& event,
                                           uint32_t absoluteStartSubtick);
  RuntimeSynthPlaybackActions acceptChordOnset(const RuntimeSynthEvent& event,
                                                uint32_t absoluteStartSubtick);
  RuntimeSynthPlaybackActions acceptRetrigger(const RuntimeSynthEvent& event);
  RuntimeSynthPlaybackActions releaseDue(uint32_t absoluteSubtick);
  RuntimeSynthPlaybackActions hardBarrier();

  bool active() const { return count_ > 0; }
  // The most recently started voice (the only one outside chords).
  uint8_t activeNote() const { return active() ? voices_[count_ - 1].event.note : 0; }
  uint32_t releaseAtSubtick() const {
    return active() ? voices_[count_ - 1].releaseAt : 0;
  }
  uint8_t voiceCount() const { return count_; }
  const RuntimeSynthEvent& voiceEvent(uint8_t index) const {
    return voices_[index].event;
  }
  // Highest held note, or -1: the one GroovePuter's monophonic synth plays.
  int topNote() const;

 private:
  struct Voice {
    RuntimeSynthEvent event{};
    uint32_t releaseAt = 0;
  };

  void removeVoice(uint8_t index);

  Voice voices_[kMaxChordVoices]{};  // oldest first
  uint8_t count_ = 0;
};

static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackAction>::value,
              "P2 playback action must remain trivially copyable");
static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackActions>::value,
              "P2 playback action batch must remain trivially copyable");
static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackState>::value,
              "P2 playback state must remain fixed/trivially copyable");
// Returned by value on the audio path: 4 chord releases + 1 start.
static_assert(sizeof(RuntimeSynthPlaybackActions) <= 64,
              "P2 playback action batch must stay bounded");

}  // namespace PhraseRuntime
