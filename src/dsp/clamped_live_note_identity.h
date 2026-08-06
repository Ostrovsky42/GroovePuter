#pragma once

#include <stdint.h>

// Stores the clamped note actually sent to the synth voice while matching a
// later NoteOff against the same normalization. This keeps the public C1..B4
// playback range without leaving notes stuck when the incoming MIDI note was
// outside that range.
class ClampedLiveNoteIdentity {
public:
  static constexpr int16_t kMinNote = 24;
  static constexpr int16_t kMaxNote = 71;

  constexpr ClampedLiveNoteIdentity(int16_t value = -1) : value_(value) {}

  ClampedLiveNoteIdentity& operator=(int value) {
    value_ = static_cast<int16_t>(value);
    return *this;
  }

  constexpr operator int16_t() const { return value_; }

  constexpr bool operator!=(int16_t incomingNote) const {
    return value_ != normalize(incomingNote);
  }

  constexpr bool operator==(int16_t incomingNote) const {
    return value_ == normalize(incomingNote);
  }

  static constexpr int16_t normalize(int note) {
    if (note < kMinNote) return kMinNote;
    if (note > kMaxNote) return kMaxNote;
    return static_cast<int16_t>(note);
  }

private:
  int16_t value_;
};
