#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/midi_device_capabilities.h"
#include "src/midi/midi_transport_capabilities.h"

using namespace GroovePuterMidi;

namespace {

void testProfileIdentityExtensionIsAdditive() {
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::SeqtrakNative) == 0);
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::GeneralMidi) == 1);
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::Custom) == 2);
    static_assert(static_cast<uint8_t>(MidiDeviceProfile::GenericMidi) == 3);

    // R2 extends the existing one-byte profile identity but deliberately does
    // not change the persisted record shape. Runtime selection/persistence
    // policy remains a later 0.9.7 integration step.
    static_assert(MidiSettingsCodec::kSchemaVersion == 2);
    static_assert(MidiSettingsCodec::kPayloadSize == 34);
    static_assert(MidiSettingsCodec::kEncodedSize == 46);

    assert(std::strcmp(midiDeviceProfileName(MidiDeviceProfile::GeneralMidi),
                       "GENERAL MIDI") == 0);
    assert(std::strcmp(midiDeviceProfileName(MidiDeviceProfile::GenericMidi),
                       "GENERIC MIDI") == 0);
}

void testGenericDefaultsDoNotClaimGeneralMidiDrums() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GenericMidi);

    assert(settings.profile == MidiDeviceProfile::GenericMidi);
    assert(settings.liveChannel == 0);
    assert(settings.synthAChannel == 0);
    assert(settings.synthBChannel == 1);
    for (const DrumMidiRoute& route : settings.drumRoutes) {
        assert(!route.enabled);
    }
    assert(isValidMidiOutputSettings(settings));
}

void testGeneralMidiLegacyDefaultsRemainUnchanged() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);

    assert(settings.profile == MidiDeviceProfile::GeneralMidi);
    for (const DrumMidiRoute& route : settings.drumRoutes) {
        assert(route.enabled);
        assert(route.channel == 9);
    }
    assert(settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::Kick)].note == 36);
    assert(settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::Snare)].note == 38);
    assert(settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::ClosedHat)].note == 42);
    assert(settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::OpenHat)].note == 46);
}

void testCapabilityModelSeparatesProfiles() {
    constexpr MidiDeviceCapabilities seqtrak =
        midiDeviceCapabilitiesForProfile(MidiDeviceProfile::SeqtrakNative);
    static_assert(seqtrak.drumMapping == MidiDrumMappingKind::SeqtrakNative);
    static_assert(seqtrak.fixedSynthChannelDefaults);
    static_assert(seqtrak.vendorSpecificControls);
    static_assert(seqtrak.transport.clockTx);
    static_assert(seqtrak.transport.clockRx);
    static_assert(seqtrak.transport.continueRx);
    static_assert(!seqtrak.transport.continueTx);
    static_assert(!seqtrak.transport.songPositionTx);

    constexpr MidiDeviceCapabilities gm =
        midiDeviceCapabilitiesForProfile(MidiDeviceProfile::GeneralMidi);
    static_assert(gm.drumMapping == MidiDrumMappingKind::GeneralMidiPercussion);
    static_assert(gm.fixedSynthChannelDefaults);
    static_assert(!gm.vendorSpecificControls);
    // R2 preserves the historical GeneralMidi transport surface. Tightening it
    // is a migration decision, not part of the semantic split itself.
    static_assert(gm.transport.continueTx);
    static_assert(gm.transport.songPositionTx);

    constexpr MidiDeviceCapabilities generic =
        midiDeviceCapabilitiesForProfile(MidiDeviceProfile::GenericMidi);
    static_assert(generic.drumMapping == MidiDrumMappingKind::None);
    static_assert(!generic.fixedSynthChannelDefaults);
    static_assert(!generic.vendorSpecificControls);
    static_assert(generic.transport.clockTx);
    static_assert(generic.transport.startTx);
    static_assert(generic.transport.stopTx);
    static_assert(!generic.transport.clockRx);
    static_assert(!generic.transport.continueTx);
    static_assert(!generic.transport.continueRx);
    static_assert(!generic.transport.songPositionTx);
    static_assert(!generic.transport.songPositionRx);

    constexpr MidiDeviceCapabilities custom =
        midiDeviceCapabilitiesForProfile(MidiDeviceProfile::Custom);
    static_assert(custom.drumMapping == MidiDrumMappingKind::UserDefined);
    static_assert(!custom.fixedSynthChannelDefaults);
    static_assert(!custom.vendorSpecificControls);
}

void testApplyingGenericPreservesUserOwnedIntent() {
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

    applyMidiDeviceProfile(MidiDeviceProfile::GenericMidi, settings);

    assert(settings.profile == MidiDeviceProfile::GenericMidi);
    assert(!settings.enabled);
    assert(!settings.liveEnabled);
    assert(!settings.patternSynthAEnabled);
    assert(settings.patternSynthBEnabled);
    assert(!settings.drumsEnabled);
    assert(settings.liveTarget == MidiLiveTarget::Drums);
    assert(settings.transportClockSource == TransportClockSource::SeqtrakExternal);
    assert(!settings.externalFollowEnabled);

    for (const DrumMidiRoute& route : settings.drumRoutes) {
        assert(!route.enabled);
    }
}

void testTransportRuntimeCanRepresentGenericWithoutChangingDefault() {
    MidiTransportCapabilityRuntime runtime;
    assert(runtime.deviceProfile() == MidiDeviceProfile::SeqtrakNative);

    runtime.setDeviceProfile(MidiDeviceProfile::GenericMidi);
    assert(runtime.deviceProfile() == MidiDeviceProfile::GenericMidi);
    const MidiTransportCapabilities generic = runtime.capabilities();
    assert(generic.clockTx);
    assert(!generic.continueTx);
    assert(!generic.songPositionTx);
}

}  // namespace

int main() {
    testProfileIdentityExtensionIsAdditive();
    testGenericDefaultsDoNotClaimGeneralMidiDrums();
    testGeneralMidiLegacyDefaultsRemainUnchanged();
    testCapabilityModelSeparatesProfiles();
    testApplyingGenericPreservesUserOwnedIntent();
    testTransportRuntimeCanRepresentGenericWithoutChangingDefault();
    return 0;
}
