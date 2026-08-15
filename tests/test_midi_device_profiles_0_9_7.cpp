#include <cassert>
#include <cstdint>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_transport_capabilities.h"

using namespace GroovePuterMidi;

namespace {

const DrumMidiRoute& routeFor(const MidiOutputSettings& settings,
                              MidiDrumVoice voice) {
    return settings.drumRoutes[drumVoiceIndex(voice)];
}

void testPersistedProfileIdentityIsStable() {
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::SeqtrakNative) == 0,
                  "SeqtrakNative persisted identity changed");
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::GeneralMidi) == 1,
                  "GeneralMidi persisted identity changed");
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::Custom) == 2,
                  "Custom persisted identity changed");
}

void testSeqtrakRoutingBaseline() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);

    assert(settings.profile == MidiDeviceProfile::SeqtrakNative);
    assert(settings.liveChannel == 7);
    assert(settings.synthAChannel == 7);
    assert(settings.synthBChannel == 8);

    const DrumMidiRoute kick{true, 0, 60};
    const DrumMidiRoute snare{true, 1, 60};
    const DrumMidiRoute clap{true, 2, 60};
    const DrumMidiRoute closedHat{true, 3, 60};
    const DrumMidiRoute openHat{true, 4, 60};
    const DrumMidiRoute midTom{true, 5, 60};
    const DrumMidiRoute rim{true, 5, 60};
    const DrumMidiRoute highTom{true, 6, 60};

    assert(routeFor(settings, MidiDrumVoice::Kick) == kick);
    assert(routeFor(settings, MidiDrumVoice::Snare) == snare);
    assert(routeFor(settings, MidiDrumVoice::Clap) == clap);
    assert(routeFor(settings, MidiDrumVoice::ClosedHat) == closedHat);
    assert(routeFor(settings, MidiDrumVoice::OpenHat) == openHat);
    assert(routeFor(settings, MidiDrumVoice::MidTom) == midTom);
    assert(routeFor(settings, MidiDrumVoice::Rim) == rim);
    assert(routeFor(settings, MidiDrumVoice::HighTom) == highTom);
}

void testSeqtrakTransportClaimsStayConservative() {
    constexpr MidiTransportCapabilities caps =
        midiTransportCapabilitiesForProfile(MidiDeviceProfile::SeqtrakNative);

    static_assert(caps.clockTx);
    static_assert(caps.clockRx);
    static_assert(caps.startTx);
    static_assert(caps.startRx);
    static_assert(caps.stopTx);
    static_assert(caps.stopRx);
    static_assert(caps.continueRx);
    static_assert(!caps.continueTx);
    static_assert(!caps.songPositionTx);
    static_assert(!caps.songPositionRx);
    static_assert(caps.continueBehavior ==
                  MidiContinueBehavior::RestartFromBeginning);
}

void testCurrentGeneralMidiBehaviorIsRecordedExplicitly() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);

    for (const DrumMidiRoute& route : settings.drumRoutes) {
        assert(route.enabled);
        assert(route.channel == 9);
    }

    assert(routeFor(settings, MidiDrumVoice::Kick).note == 36);
    assert(routeFor(settings, MidiDrumVoice::Snare).note == 38);
    assert(routeFor(settings, MidiDrumVoice::ClosedHat).note == 42);
    assert(routeFor(settings, MidiDrumVoice::OpenHat).note == 46);
    assert(routeFor(settings, MidiDrumVoice::MidTom).note == 43);
    assert(routeFor(settings, MidiDrumVoice::HighTom).note == 47);
    assert(routeFor(settings, MidiDrumVoice::Rim).note == 37);
    assert(routeFor(settings, MidiDrumVoice::Clap).note == 39);

    constexpr MidiTransportCapabilities caps =
        midiTransportCapabilitiesForProfile(MidiDeviceProfile::GeneralMidi);
    static_assert(caps.clockTx);
    static_assert(!caps.clockRx);
    static_assert(caps.startTx);
    static_assert(!caps.startRx);
    static_assert(caps.stopTx);
    static_assert(!caps.stopRx);
    static_assert(caps.continueTx);
    static_assert(caps.songPositionTx);
    static_assert(caps.continueBehavior ==
                  MidiContinueBehavior::ContinueFromPosition);
}

void testApplyingProfilePreservesUserOwnedState() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);

    settings.enabled = false;
    settings.liveEnabled = false;
    settings.patternSynthAEnabled = false;
    settings.patternSynthBEnabled = true;
    settings.drumsEnabled = false;
    settings.liveTarget = MidiLiveTarget::Drums;
    settings.transportClockSource = TransportClockSource::SeqtrakExternal;
    settings.externalFollowEnabled = false;

    applyMidiDeviceProfile(MidiDeviceProfile::GeneralMidi, settings);

    assert(settings.profile == MidiDeviceProfile::GeneralMidi);
    assert(!settings.enabled);
    assert(!settings.liveEnabled);
    assert(!settings.patternSynthAEnabled);
    assert(settings.patternSynthBEnabled);
    assert(!settings.drumsEnabled);
    assert(settings.liveTarget == MidiLiveTarget::Drums);
    assert(settings.transportClockSource == TransportClockSource::SeqtrakExternal);
    assert(!settings.externalFollowEnabled);

    // Device-owned defaults are allowed to change.
    assert(settings.liveChannel == 0);
    assert(settings.synthAChannel == 0);
    assert(settings.synthBChannel == 1);
}

void testCustomPreservesCustomRouting() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);

    settings.liveChannel = 12;
    settings.synthAChannel = 13;
    settings.synthBChannel = 14;
    settings.drumGateMs = 177;
    settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::Kick)] =
        DrumMidiRoute{false, 11, 71};

    applyMidiDeviceProfile(MidiDeviceProfile::Custom, settings);

    assert(settings.profile == MidiDeviceProfile::Custom);
    assert(settings.liveChannel == 12);
    assert(settings.synthAChannel == 13);
    assert(settings.synthBChannel == 14);
    assert(settings.drumGateMs == 177);

    const DrumMidiRoute customKick{false, 11, 71};
    assert(routeFor(settings, MidiDrumVoice::Kick) == customKick);

    constexpr MidiTransportCapabilities caps =
        midiTransportCapabilitiesForProfile(MidiDeviceProfile::Custom);
    static_assert(caps.clockTx);
    static_assert(!caps.clockRx);
    static_assert(caps.startTx);
    static_assert(caps.stopTx);
    static_assert(!caps.continueTx);
    static_assert(!caps.songPositionTx);
    static_assert(caps.continueBehavior ==
                  MidiContinueBehavior::RestartFromBeginning);
}

}  // namespace

int main() {
    testPersistedProfileIdentityIsStable();
    testSeqtrakRoutingBaseline();
    testSeqtrakTransportClaimsStayConservative();
    testCurrentGeneralMidiBehaviorIsRecordedExplicitly();
    testApplyingProfilePreservesUserOwnedState();
    testCustomPreservesCustomRouting();
    return 0;
}
