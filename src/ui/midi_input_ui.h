#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_UI_H
#define GROOVEPUTER_MIDI_INPUT_UI_H

#include <cstddef>
#include <cstdio>
#include "src/midi/midi_input_dispatcher.h"

namespace GroovePuterUi::MidiInputUi {
inline const char* enabledName(bool enabled) { return enabled ? "ON" : "OFF"; }

inline const char* targetName(GroovePuterMidi::MidiInputTarget target) {
    switch (target) {
        case GroovePuterMidi::MidiInputTarget::SynthA: return "SYN A";
        case GroovePuterMidi::MidiInputTarget::SynthB: return "SYN B";
        case GroovePuterMidi::MidiInputTarget::Drums: return "DRUMS";
        case GroovePuterMidi::MidiInputTarget::Perform: return "PERFORM";
    }
    return "SYN A";
}

inline const char* pitchBendName(GroovePuterMidi::PitchBendMode mode) {
    switch (mode) {
        case GroovePuterMidi::PitchBendMode::Musical: return "MUSICAL";
        case GroovePuterMidi::PitchBendMode::LegacyNudge: return "NUDGE";
        case GroovePuterMidi::PitchBendMode::Count: break;
    }
    return "MUSICAL";
}

inline void formatUsbCable(uint8_t cable, char* out, std::size_t size) {
    if (size == 0u) return;
    std::snprintf(out, size, "CABLE %u", static_cast<unsigned>(cable));
}

inline void formatChannel(const GroovePuterMidi::MidiInputRoutingConfig& config,
                          char* out, std::size_t size) {
    if (size == 0u) return;
    if (config.channelMode == GroovePuterMidi::MidiInputChannelMode::Omni) {
        std::snprintf(out, size, "OMNI");
    } else {
        std::snprintf(out, size, "CH%u", static_cast<unsigned>(config.channel + 1u));
    }
}

inline GroovePuterMidi::MidiInputRoutingConfig stepEnabled(
        GroovePuterMidi::MidiInputRoutingConfig config) {
    config.enabled = !config.enabled;
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepChannel(
        GroovePuterMidi::MidiInputRoutingConfig config, int delta) {
    int index = config.channelMode == GroovePuterMidi::MidiInputChannelMode::Omni
        ? 0 : static_cast<int>(config.channel) + 1;
    index = (index + delta) % 17;
    if (index < 0) index += 17;
    if (index == 0) {
        config.channelMode = GroovePuterMidi::MidiInputChannelMode::Omni;
        config.channel = 0;
    } else {
        config.channelMode = GroovePuterMidi::MidiInputChannelMode::Single;
        config.channel = static_cast<uint8_t>(index - 1);
    }
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepTarget(
        GroovePuterMidi::MidiInputRoutingConfig config, int delta) {
    int target = (static_cast<int>(config.target) + delta) % 4;
    if (target < 0) target += 4;
    config.target = static_cast<GroovePuterMidi::MidiInputTarget>(target);
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepPitchBend(
        GroovePuterMidi::MidiInputRoutingConfig config, int delta) {
    int value = (static_cast<int>(config.pitchBendMode) + delta) % 2;
    if (value < 0) value += 2;
    config.pitchBendMode = static_cast<GroovePuterMidi::PitchBendMode>(value);
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepUsbCable(
        GroovePuterMidi::MidiInputRoutingConfig config, int delta) {
    int cable = (static_cast<int>(config.usbCable) + delta) % 16;
    if (cable < 0) cable += 16;
    config.usbCable = static_cast<uint8_t>(cable);
    return config;
}
}  // namespace GroovePuterUi::MidiInputUi

#endif
