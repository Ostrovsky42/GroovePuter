#pragma once
#ifndef GROOVEPUTER_SRC_MIDI_GENRE_SOUND_PRESET_H
#define GROOVEPUTER_SRC_MIDI_GENRE_SOUND_PRESET_H

#include <cstddef>
#include <cstdint>

#include "cutoff_sweep.h"

// H7: a sound preset per genre for the two external synths (Synth A on MIDI
// channel 8, Synth B on channel 9). Five controllers per voice, in this order:
// CUTOFF, ATTACK, RELEASE, REVERB, DELAY. VOLUME and PAN are not part of a preset.
//
// stepsFor() gives the output as a plain list: voice, controller index (into
// GroovePuterMidi::kControllers, 0..4) and the value, already clamped to the
// controller's usable range. Voice 0 is Synth A, 1 is Synth B.
namespace GenreSound {

constexpr uint8_t kVoiceCount = 2;
constexpr uint8_t kPresetControllers = 5;  // CUTOFF ATTACK RELEASE REVERB DELAY
constexpr uint8_t kStepsPerGenre = kVoiceCount * kPresetControllers;

struct Step {
  uint8_t voice;       // 0 = Synth A (channel 8), 1 = Synth B (channel 9)
  uint8_t controller;  // index into GroovePuterMidi::kControllers
  uint8_t value;       // clamped for that controller
};

struct Preset {
  uint8_t mode;  // GenerativeMode value
  const char* name;
  const char* hint;
  uint8_t voiceA[kPresetControllers];  // CUTOFF ATTACK RELEASE REVERB DELAY
  uint8_t voiceB[kPresetControllers];
};

// Dub = Reggae (5), Techno = 10, Trip-Hop = TripHop (6), House = 9, UK garage = 13, Acid = 0.
inline constexpr Preset kPresets[] = {
    {9, "HOUSE", "A: SHORT BASS  B: PIANO/ORGAN", {70, 0, 40, 0, 0}, {90, 0, 50, 30, 10}},
    {13, "UKG", "A: SUB BASS  B: ORGAN STAB", {60, 0, 45, 0, 0}, {85, 0, 40, 25, 20}},
    {10, "TECHNO", "A: SUB  B: CHORD STAB", {55, 0, 40, 0, 0}, {60, 0, 70, 60, 70}},
    {0, "ACID", "A: SAW MONO  B: STAB", {75, 0, 30, 0, 15}, {80, 0, 35, 20, 30}},
    {5, "DUB", "A: ROUND SUB  B: SKANK ORGAN", {45, 0, 60, 0, 0}, {70, 0, 30, 50, 80}},
    {6, "TRIP-HOP", "A: DEEP BASS  B: EP/PAD", {50, 0, 60, 0, 0}, {60, 30, 80, 60, 40}},
};

inline const Preset* presetFor(uint8_t mode) {
    for (const Preset& preset : kPresets) {
        if (preset.mode == mode) return &preset;
    }
    return nullptr;
}

// Writes the 10 steps of a genre's preset to out (voice A first, then voice B).
// Returns the number of steps: 10, or 0 for a genre without a preset or a short buffer.
inline size_t stepsFor(uint8_t mode, Step* out, size_t capacity) {
    const Preset* preset = presetFor(mode);
    if (preset == nullptr || capacity < kStepsPerGenre) return 0;
    size_t n = 0;
    for (uint8_t voice = 0; voice < kVoiceCount; ++voice) {
        const uint8_t* values = voice == 0 ? preset->voiceA : preset->voiceB;
        for (uint8_t c = 0; c < kPresetControllers; ++c) {
            out[n++] = Step{voice, c, GroovePuterMidi::clampForController(c, values[c])};
        }
    }
    return n;
}

// H8: sends a genre's steps one per frame. A step is written only after the
// dispatcher has published the previous one (its controller and value), so no
// request is overwritten. kManual selects the controller it writes to; the
// controller that was selected before the send is restored at the end.
class SoundSender {
public:
    using Channels = GroovePuterMidi::CutoffControlChannel[kVoiceCount];

    // Starts a send for a genre. Returns false when the genre has no preset.
    bool start(uint8_t mode, Channels& channels) {
        count_ = static_cast<uint8_t>(stepsFor(mode, steps_, kStepsPerGenre));
        if (count_ == 0) return false;
        for (uint8_t v = 0; v < kVoiceCount; ++v) restore_[v] = channels[v].controller();
        index_ = 0;
        awaiting_ = false;
        sending_ = true;
        return true;
    }

    bool sending() const { return sending_; }
    uint8_t written() const { return index_; }

    // One call per frame. Returns true when it wrote a request this call.
    bool pump(Channels& channels) {
        if (!sending_) return false;
        if (awaiting_) {
            const Step& last = steps_[index_ - 1];
            const GroovePuterMidi::CutoffControlChannel& c = channels[last.voice];
            if (!(c.known() && c.controller() == last.controller && c.value() == last.value)) return false;
            awaiting_ = false;
        }
        if (index_ >= count_) {
            for (uint8_t v = 0; v < kVoiceCount; ++v) {
                if (channels[v].controller() != restore_[v]) {
                    channels[v].request(GroovePuterMidi::CutoffRequest::kSelectController, 0, restore_[v]);
                }
            }
            sending_ = false;
            return true;
        }
        const Step& step = steps_[index_++];
        channels[step.voice].request(GroovePuterMidi::CutoffRequest::kManual, step.value, step.controller);
        awaiting_ = true;
        return true;
    }

private:
    Step steps_[kStepsPerGenre]{};
    uint8_t count_ = 0;
    uint8_t index_ = 0;
    uint8_t restore_[kVoiceCount]{};
    bool awaiting_ = false;
    bool sending_ = false;
};

}  // namespace GenreSound

#endif  // GROOVEPUTER_SRC_MIDI_GENRE_SOUND_PRESET_H
