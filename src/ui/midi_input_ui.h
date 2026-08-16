#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_UI_H
#define GROOVEPUTER_MIDI_INPUT_UI_H

#include <cstddef>
#include <cstdio>

#include "src/input/midi_input_router.h"

namespace GroovePuterUi::MidiInputUi {

inline const char* enabledName(bool enabled) {
    return enabled ? "ON" : "OFF";
}

inline const char* targetName(MidiInputTarget target) {
    switch (target) {
        case MidiInputTarget::SynthA: return "SYN A";
        case MidiInputTarget::SynthB: return "SYN B";
        case MidiInputTarget::Drums: return "DRUMS";
    }
    return "SYN A";
}

inline void formatChannel(const MidiInputRoutingConfig& config,
                          char* out,
                          std::size_t outBytes) {
    if (out == nullptr || outBytes == 0u) return;
    if (config.channelMode == MidiInputChannelMode::Omni) {
        std::snprintf(out, outBytes, "OMNI");
        return;
    }
    std::snprintf(out, outBytes, "%u",
                  static_cast<unsigned>(config.channel) + 1u);
}

inline MidiInputRoutingConfig stepEnabled(MidiInputRoutingConfig config) {
    config.enabled = !config.enabled;
    return config;
}

// One compact cycle covers the user-visible selector:
// OMNI <-> 1 <-> 2 ... <-> 16 <-> OMNI.
inline MidiInputRoutingConfig stepChannel(MidiInputRoutingConfig config,
                                          int delta) {
    int index = config.channelMode == MidiInputChannelMode::Omni
        ? 0
        : static_cast<int>(config.channel) + 1;
    const int direction = delta < 0 ? -1 : 1;
    index = (index + direction + 17) % 17;
    if (index == 0) {
        config.channelMode = MidiInputChannelMode::Omni;
        config.channel = 0u;
    } else {
        config.channelMode = MidiInputChannelMode::Single;
        config.channel = static_cast<uint8_t>(index - 1);
    }
    return config;
}

inline MidiInputRoutingConfig stepTarget(MidiInputRoutingConfig config,
                                         int delta) {
    int index = static_cast<int>(config.target);
    const int direction = delta < 0 ? -1 : 1;
    index = (index + direction + 3) % 3;
    config.target = static_cast<MidiInputTarget>(index);
    return config;
}

}  // namespace GroovePuterUi::MidiInputUi

#endif
