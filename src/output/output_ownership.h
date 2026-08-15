#pragma once
#ifndef GROOVEPUTER_OUTPUT_OWNERSHIP_H
#define GROOVEPUTER_OUTPUT_OWNERSHIP_H

#include <cstdint>

#include "../input/musical_event.h"
#include "../midi/midi_realtime_word.h"

namespace GroovePuterOutput {

enum class Track : uint8_t {
    SynthA = 0,
    SynthB = 1,
    Drums = 2,
    Count = 3,
};

enum class Mode : uint8_t {
    Internal = 1,
    Midi = 2,
    Layer = 3,
};

enum class SourceClass : uint8_t {
    Pattern = 0,
    Performance = 1,
    Preview = 2,
};

struct TrackState {
    bool explicitMode{false};
    Mode mode{Mode::Layer};
};

namespace detail {

static constexpr uint32_t kBitsPerTrack = 2u;
static constexpr uint32_t kTrackMask = 0x3u;

inline MidiRealtimeWord& stateWord() {
    // Zero is intentional: it represents the versioned legacy-compat decode
    // phase, not a fourth user-visible OutputMode.
    static MidiRealtimeWord word;
    return word;
}

inline MidiRealtimeWord& midiDisableEpochWord() {
    // Three independent 8-bit counters are packed into one aligned realtime
    // word. They let each MIDI queue observe every remove-external transition
    // even if the user cycles modes faster than one dispatcher iteration.
    static MidiRealtimeWord word;
    return word;
}

inline uint32_t trackShift(Track track) {
    return static_cast<uint32_t>(track) * kBitsPerTrack;
}

inline uint8_t rawModeFromWord(uint32_t word, Track track) {
    return static_cast<uint8_t>(
        (word >> trackShift(track)) & kTrackMask);
}

inline uint8_t rawMode(Track track) {
    return rawModeFromWord(stateWord().loadAcquire(), track);
}

inline bool rawAllowsInternal(uint8_t raw, SourceClass source) {
    if (raw == 0u) {
        // Frozen <=0.9.5 compatibility behavior:
        // Pattern/Song -> local + MIDI, PERFORM -> MIDI only,
        // sampler/manual preview -> local only.
        return source != SourceClass::Performance;
    }
    return raw == static_cast<uint8_t>(Mode::Internal) ||
           raw == static_cast<uint8_t>(Mode::Layer);
}

inline bool rawAllowsMidi(uint8_t raw, SourceClass source) {
    if (raw == 0u) {
        return source != SourceClass::Preview;
    }
    return raw == static_cast<uint8_t>(Mode::Midi) ||
           raw == static_cast<uint8_t>(Mode::Layer);
}

inline void incrementMidiDisableEpoch(Track track) {
    const uint32_t shift = static_cast<uint32_t>(track) * 8u;
    uint32_t word = midiDisableEpochWord().loadAcquire();
    const uint32_t current = (word >> shift) & 0xFFu;
    const uint32_t next = (current + 1u) & 0xFFu;
    word &= ~(0xFFu << shift);
    word |= (next << shift);
    midiDisableEpochWord().storeRelease(word);
}

}  // namespace detail

inline bool trackForTarget(MusicalEventTarget target, Track& track) {
    switch (target) {
        case MusicalEventTarget::SynthA:
            track = Track::SynthA;
            return true;
        case MusicalEventTarget::SynthB:
            track = Track::SynthB;
            return true;
        case MusicalEventTarget::Drums:
            track = Track::Drums;
            return true;
        case MusicalEventTarget::Dx:
            return false;
    }
    return false;
}

inline TrackState state(Track track) {
    const uint8_t raw = detail::rawMode(track);
    if (raw == 0u) return TrackState{};
    return TrackState{true, static_cast<Mode>(raw)};
}

inline bool hasExplicitMode(Track track) {
    return detail::rawMode(track) != 0u;
}

inline Mode mode(Track track) {
    const TrackState current = state(track);
    return current.explicitMode ? current.mode : Mode::Layer;
}

inline bool allowsInternal(Track track, SourceClass source) {
    return detail::rawAllowsInternal(detail::rawMode(track), source);
}

inline bool allowsMidi(Track track, SourceClass source) {
    return detail::rawAllowsMidi(detail::rawMode(track), source);
}

inline bool allowsInternal(MusicalEventTarget target, SourceClass source) {
    Track track = Track::SynthA;
    return trackForTarget(target, track) && allowsInternal(track, source);
}

inline bool allowsMidi(MusicalEventTarget target, SourceClass source) {
    Track track = Track::SynthA;
    // DX remains outside the 0.9.6 groove-track ownership axis and preserves
    // its existing external-only route.
    return !trackForTarget(target, track) || allowsMidi(track, source);
}

inline bool setMode(Track track, Mode nextMode) {
    const uint8_t nextRaw = static_cast<uint8_t>(nextMode);
    if (nextRaw < static_cast<uint8_t>(Mode::Internal) ||
        nextRaw > static_cast<uint8_t>(Mode::Layer)) {
        return false;
    }

    uint32_t word = detail::stateWord().loadAcquire();
    const uint32_t shift = detail::trackShift(track);
    const uint8_t previousRaw = detail::rawModeFromWord(word, track);
    if (previousRaw == nextRaw) return false;

    const bool previousMidi =
        detail::rawAllowsMidi(previousRaw, SourceClass::Pattern);
    const bool nextMidi =
        detail::rawAllowsMidi(nextRaw, SourceClass::Pattern);

    word &= ~(detail::kTrackMask << shift);
    word |= (static_cast<uint32_t>(nextRaw) << shift);
    detail::stateWord().storeRelease(word);

    if (previousMidi && !nextMidi) {
        detail::incrementMidiDisableEpoch(track);
    }
    return true;
}

inline void restoreLegacyCompatibility(Track track) {
    uint32_t word = detail::stateWord().loadAcquire();
    const uint32_t shift = detail::trackShift(track);
    const uint8_t previousRaw = detail::rawModeFromWord(word, track);
    if (previousRaw == 0u) return;

    const bool previousMidi =
        detail::rawAllowsMidi(previousRaw, SourceClass::Pattern);
    const bool legacyMidi = true;

    word &= ~(detail::kTrackMask << shift);
    detail::stateWord().storeRelease(word);

    if (previousMidi && !legacyMidi) {
        detail::incrementMidiDisableEpoch(track);
    }
}

inline uint8_t midiDisableEpoch(Track track) {
    const uint32_t shift = static_cast<uint32_t>(track) * 8u;
    return static_cast<uint8_t>(
        (detail::midiDisableEpochWord().loadAcquire() >> shift) & 0xFFu);
}

inline Mode cycleMode(Mode current) {
    switch (current) {
        case Mode::Internal: return Mode::Midi;
        case Mode::Midi: return Mode::Layer;
        case Mode::Layer: return Mode::Internal;
    }
    return Mode::Internal;
}

inline const char* modeName(Mode value) {
    switch (value) {
        case Mode::Internal: return "INTERNAL";
        case Mode::Midi: return "MIDI";
        case Mode::Layer: return "LAYER";
    }
    return "INTERNAL";
}

inline const char* modeShortName(Mode value) {
    switch (value) {
        case Mode::Internal: return "INT";
        case Mode::Midi: return "MIDI";
        case Mode::Layer: return "LAYER";
    }
    return "INT";
}

}  // namespace GroovePuterOutput

#endif  // GROOVEPUTER_OUTPUT_OWNERSHIP_H
