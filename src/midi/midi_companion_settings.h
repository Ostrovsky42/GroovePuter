#pragma once
#ifndef GROOVEPUTER_MIDI_COMPANION_SETTINGS_H
#define GROOVEPUTER_MIDI_COMPANION_SETTINGS_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "transport_clock_source.h"

namespace GroovePuterMidi {

constexpr std::size_t kMidiDrumVoiceCount = 8;
constexpr uint8_t kMidiChannelMin = 0;
constexpr uint8_t kMidiChannelMax = 15;
constexpr uint8_t kMidiNoteMin = 0;
constexpr uint8_t kMidiNoteMax = 127;
constexpr uint16_t kDrumGateMinMs = 1;
constexpr uint16_t kDrumGateMaxMs = 500;
constexpr uint16_t kDefaultDrumGateMs = 80;

enum class MidiDeviceProfile : uint8_t {
    SeqtrakNative = 0,
    GeneralMidi = 1,
    Custom = 2,
    // Added as a distinct semantic identity in 0.9.7-R2. This is deliberately
    // not an alias for GeneralMidi: generic MIDI must not imply a GM drum map.
    // No runtime/UI selection path is introduced by R2.
    GenericMidi = 3,
};

enum class MidiLiveTarget : uint8_t {
    SynthA = 0,
    SynthB = 1,
    Drums = 2,
};

enum class MidiDrumVoice : uint8_t {
    Kick = 0,
    Snare,
    ClosedHat,
    OpenHat,
    MidTom,
    HighTom,
    Rim,
    Clap,
};

struct DrumMidiRoute {
    bool enabled{true};
    uint8_t channel{0};
    uint8_t note{60};
};

struct MidiOutputSettings {
    MidiDeviceProfile profile{MidiDeviceProfile::SeqtrakNative};

    bool enabled{true};
    bool liveEnabled{true};
    bool patternSynthAEnabled{true};
    bool patternSynthBEnabled{true};
    bool drumsEnabled{true};

    MidiLiveTarget liveTarget{MidiLiveTarget::SynthA};
    uint8_t liveChannel{7};
    uint8_t synthAChannel{7};
    uint8_t synthBChannel{8};

    std::array<DrumMidiRoute, kMidiDrumVoiceCount> drumRoutes{};
    uint16_t drumGateMs{kDefaultDrumGateMs};

    // Transport ownership is persisted alongside the existing MIDI companion
    // settings. Schema-v1 records did not contain these fields and decode to
    // the safe legacy behavior: GP MASTER with external follow enabled.
    TransportClockSource transportClockSource{
        TransportClockSource::GroovePuterInternal};
    bool externalFollowEnabled{true};
};

constexpr std::size_t drumVoiceIndex(MidiDrumVoice voice) {
    return static_cast<std::size_t>(voice);
}

const char* midiDeviceProfileName(MidiDeviceProfile profile);
const char* midiDrumVoiceName(MidiDrumVoice voice);

uint8_t zeroBasedMidiChannelFromUi(int channelOneBased);
uint8_t uiMidiChannelFromZeroBased(uint8_t zeroBasedChannel);

MidiOutputSettings makeDefaultMidiOutputSettings(MidiDeviceProfile profile);
void applyMidiDeviceProfile(MidiDeviceProfile profile,
                            MidiOutputSettings& settings);

bool isValidMidiOutputSettings(const MidiOutputSettings& settings);
void sanitizeMidiOutputSettings(MidiOutputSettings& settings);

bool operator==(const DrumMidiRoute& lhs, const DrumMidiRoute& rhs);
bool operator==(const MidiOutputSettings& lhs,
                const MidiOutputSettings& rhs);

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_COMPANION_SETTINGS_H
