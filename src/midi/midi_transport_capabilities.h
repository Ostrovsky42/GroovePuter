#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H
#define GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H

#include <cstdint>

namespace GroovePuterMidi {

enum class MidiContinueBehavior : uint8_t {
    Unsupported = 0,
    ContinueFromPosition,
    RestartFromBeginning,
};

struct MidiTransportCapabilities {
    bool clockTx = false;
    bool clockRx = false;
    bool startTx = false;
    bool startRx = false;
    bool stopTx = false;
    bool stopRx = false;
    bool continueTx = false;
    bool continueRx = false;
    bool songPositionTx = false;
    bool songPositionRx = false;
    bool activeSensingTx = false;
    bool activeSensingRx = false;
    MidiContinueBehavior continueBehavior = MidiContinueBehavior::Unsupported;
};

constexpr MidiTransportCapabilities genericClassCompliantTransportCapabilities() {
    MidiTransportCapabilities capabilities{};
    capabilities.clockTx = true;
    capabilities.startTx = true;
    capabilities.stopTx = true;
    capabilities.continueTx = true;
    capabilities.songPositionTx = true;
    capabilities.continueBehavior = MidiContinueBehavior::ContinueFromPosition;
    return capabilities;
}

// Yamaha SEQTRAK is deliberately conservative here. Clock/Start/Stop behavior
// has been hardware exercised by the project. Continue RX is supported by the
// existing external-follow implementation, while outbound Continue and SPP
// remain disabled until the direct Cardputer-Adv -> SEQTRAK hardware gate is
// recorded on the implementation branch.
constexpr MidiTransportCapabilities seqtrakValidatedTransportCapabilities() {
    MidiTransportCapabilities capabilities{};
    capabilities.clockTx = true;
    capabilities.clockRx = true;
    capabilities.startTx = true;
    capabilities.startRx = true;
    capabilities.stopTx = true;
    capabilities.stopRx = true;
    capabilities.continueRx = true;
    capabilities.continueBehavior = MidiContinueBehavior::RestartFromBeginning;
    return capabilities;
}

constexpr uint16_t clampSongPositionPointer(uint32_t midiBeats) {
    return midiBeats > 0x3FFFu ? 0x3FFFu : static_cast<uint16_t>(midiBeats);
}

constexpr uint8_t songPositionPointerLsb(uint16_t midiBeats) {
    return static_cast<uint8_t>(clampSongPositionPointer(midiBeats) & 0x7Fu);
}

constexpr uint8_t songPositionPointerMsb(uint16_t midiBeats) {
    return static_cast<uint8_t>((clampSongPositionPointer(midiBeats) >> 7) & 0x7Fu);
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H
