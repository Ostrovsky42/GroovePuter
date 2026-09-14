#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_SETTINGS_H
#define GROOVEPUTER_MIDI_INPUT_SETTINGS_H

#include <cstdint>
#include "midi_input_dispatcher.h"

namespace GroovePuterMidi::MidiInputSettings {
constexpr uint8_t kSettingsVersion = 1u;
constexpr uint8_t kSettingsMagic = 0xA7u;

inline MidiInputRoutingConfig defaultRoutingConfig() {
    return MidiInputRoutingConfig{};
}

inline uint32_t encodeRoutingConfig(const MidiInputRoutingConfig& config) {
    const uint8_t payload = static_cast<uint8_t>(
        (config.enabled ? 0x01u : 0u) |
        (config.channelMode == MidiInputChannelMode::Single ? 0x02u : 0u) |
        ((config.channel & 0x0Fu) << 2u) |
        ((static_cast<uint8_t>(config.target) & 0x03u) << 6u));
    return (static_cast<uint32_t>(kSettingsMagic) << 24u) |
           (static_cast<uint32_t>(kSettingsVersion) << 16u) |
           payload;
}

inline bool decodeRoutingConfig(uint32_t word, MidiInputRoutingConfig& out) {
    if (static_cast<uint8_t>(word >> 24u) != kSettingsMagic ||
        static_cast<uint8_t>(word >> 16u) != kSettingsVersion ||
        (word & 0x0000FF00u) != 0u) {
        out = defaultRoutingConfig();
        return false;
    }
    const uint8_t payload = static_cast<uint8_t>(word & 0xFFu);
    MidiInputRoutingConfig candidate{};
    candidate.enabled = (payload & 0x01u) != 0u;
    candidate.channelMode = (payload & 0x02u) != 0u
        ? MidiInputChannelMode::Single : MidiInputChannelMode::Omni;
    candidate.channel = static_cast<uint8_t>((payload >> 2u) & 0x0Fu);
    const uint8_t target = static_cast<uint8_t>((payload >> 6u) & 0x03u);
    if (target > static_cast<uint8_t>(MidiInputTarget::Drums)) {
        out = defaultRoutingConfig();
        return false;
    }
    candidate.target = static_cast<MidiInputTarget>(target);
    if (!MidiInputDispatcher::isValidConfig(candidate)) {
        out = defaultRoutingConfig();
        return false;
    }
    out = candidate;
    return true;
}
}  // namespace GroovePuterMidi::MidiInputSettings

#endif
