#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H
#define GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H

#include <atomic>
#include <cstdint>

#include "midi_companion_settings.h"

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

// Historical GeneralMidi behavior. Preserve this surface in R2 so the semantic
// split does not silently alter existing profile behavior before the release
// migration decision is made.
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

// Generic MIDI makes no GM-percussion or device-specific transport promises.
// Clock/Start/Stop TX remain available as ordinary standard MIDI messages;
// Continue/SPP and all RX claims stay off until a concrete target validates them.
constexpr MidiTransportCapabilities conservativeGenericMidiTransportCapabilities() {
    MidiTransportCapabilities capabilities{};
    capabilities.clockTx = true;
    capabilities.startTx = true;
    capabilities.stopTx = true;
    capabilities.continueBehavior = MidiContinueBehavior::RestartFromBeginning;
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

// CUSTOM does not imply standards compliance. Keep the validated Clock/Start/
// Stop surface and restart fallback until explicit per-device capabilities are
// introduced; silently enabling F2/FB here would recreate the false capability
// claim this stage is intended to avoid.
constexpr MidiTransportCapabilities conservativeCustomTransportCapabilities() {
    MidiTransportCapabilities capabilities{};
    capabilities.clockTx = true;
    capabilities.startTx = true;
    capabilities.stopTx = true;
    capabilities.continueBehavior = MidiContinueBehavior::RestartFromBeginning;
    return capabilities;
}

constexpr MidiTransportCapabilities midiTransportCapabilitiesForProfile(
        MidiDeviceProfile profile) {
    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
            return seqtrakValidatedTransportCapabilities();
        case MidiDeviceProfile::GeneralMidi:
            return genericClassCompliantTransportCapabilities();
        case MidiDeviceProfile::GenericMidi:
            return conservativeGenericMidiTransportCapabilities();
        case MidiDeviceProfile::Custom:
            return conservativeCustomTransportCapabilities();
    }
    return conservativeCustomTransportCapabilities();
}

class MidiTransportCapabilityRuntime {
public:
    void setDeviceProfile(MidiDeviceProfile profile) {
        profile_.store(static_cast<uint8_t>(profile),
                       std::memory_order_release);
    }

    MidiDeviceProfile deviceProfile() const {
        const uint8_t raw = profile_.load(std::memory_order_acquire);
        if (raw > static_cast<uint8_t>(MidiDeviceProfile::GenericMidi)) {
            return MidiDeviceProfile::SeqtrakNative;
        }
        return static_cast<MidiDeviceProfile>(raw);
    }

    MidiTransportCapabilities capabilities() const {
        return midiTransportCapabilitiesForProfile(deviceProfile());
    }

private:
    std::atomic<uint8_t> profile_{
        static_cast<uint8_t>(MidiDeviceProfile::SeqtrakNative)};
};

inline MidiTransportCapabilityRuntime& midiTransportCapabilityRuntime() {
    static MidiTransportCapabilityRuntime runtime;
    return runtime;
}

constexpr uint16_t clampSongPositionPointer(uint32_t midiBeats) {
    return midiBeats > 0x3FFFu ? 0x3FFFu : static_cast<uint16_t>(midiBeats);
}

constexpr uint16_t songPositionPointerFromPpqnTicks(uint32_t ticks,
                                                    uint16_t ppqn) {
    if (ppqn == 0) return 0;
    // One MIDI Song Position Pointer beat is one sixteenth note, or one quarter
    // note divided by four. Integer truncation intentionally snaps down to the
    // last complete sixteenth-note boundary.
    return clampSongPositionPointer(
        (static_cast<uint64_t>(ticks) * 4u) / ppqn);
}

constexpr uint8_t songPositionPointerLsb(uint16_t midiBeats) {
    return static_cast<uint8_t>(clampSongPositionPointer(midiBeats) & 0x7Fu);
}

constexpr uint8_t songPositionPointerMsb(uint16_t midiBeats) {
    return static_cast<uint8_t>((clampSongPositionPointer(midiBeats) >> 7) & 0x7Fu);
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_TRANSPORT_CAPABILITIES_H
