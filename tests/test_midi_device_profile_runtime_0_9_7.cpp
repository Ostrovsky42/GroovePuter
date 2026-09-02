#include <cassert>

#include "src/midi/midi_device_profile_runtime.h"

using namespace GroovePuterMidi;

namespace {

void testInitializeOwnsOneSanitizedSnapshot() {
    MidiOutputSettings input =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    input.enabled = false;
    input.liveTarget = MidiLiveTarget::Drums;
    input.transportClockSource = TransportClockSource::SeqtrakExternal;
    input.externalFollowEnabled = false;

    MidiDeviceProfileRuntime runtime;
    assert(!runtime.initialized());
    runtime.initialize(input);

    assert(runtime.initialized());
    assert(runtime.revision() == 0);
    assert(runtime.profile() == MidiDeviceProfile::GeneralMidi);
    assert(!runtime.settings().enabled);
    assert(runtime.settings().liveTarget == MidiLiveTarget::Drums);
    assert(runtime.settings().transportClockSource ==
           TransportClockSource::SeqtrakExternal);
    assert(!runtime.settings().externalFollowEnabled);
    assert(midiTransportCapabilityRuntime().deviceProfile() ==
           MidiDeviceProfile::GeneralMidi);
}

void testProfileApplyIsTransactionalAndPreservesUserIntent() {
    MidiOutputSettings input =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    input.enabled = false;
    input.liveEnabled = false;
    input.patternSynthAEnabled = false;
    input.patternSynthBEnabled = true;
    input.drumsEnabled = false;
    input.liveTarget = MidiLiveTarget::Drums;
    input.transportClockSource = TransportClockSource::SeqtrakExternal;
    input.externalFollowEnabled = false;

    MidiDeviceProfileRuntime runtime;
    runtime.initialize(input);

    assert(runtime.applyProfile(MidiDeviceProfile::GenericMidi));
    assert(runtime.revision() == 1);
    assert(runtime.profile() == MidiDeviceProfile::GenericMidi);
    assert(!runtime.settings().enabled);
    assert(!runtime.settings().liveEnabled);
    assert(!runtime.settings().patternSynthAEnabled);
    assert(runtime.settings().patternSynthBEnabled);
    assert(!runtime.settings().drumsEnabled);
    assert(runtime.settings().liveTarget == MidiLiveTarget::Drums);
    assert(runtime.settings().transportClockSource ==
           TransportClockSource::SeqtrakExternal);
    assert(!runtime.settings().externalFollowEnabled);
    for (const DrumMidiRoute& route : runtime.settings().drumRoutes) {
        assert(!route.enabled);
    }
    assert(midiTransportCapabilityRuntime().deviceProfile() ==
           MidiDeviceProfile::GenericMidi);

    // Reapplying an identical profile is a no-op and does not create a fake
    // settings revision.
    assert(!runtime.applyProfile(MidiDeviceProfile::GenericMidi));
    assert(runtime.revision() == 1);

    assert(runtime.applyProfile(MidiDeviceProfile::GeneralMidi));
    assert(runtime.revision() == 2);
    assert(runtime.profile() == MidiDeviceProfile::GeneralMidi);
    for (const DrumMidiRoute& route : runtime.settings().drumRoutes) {
        assert(route.enabled);
        assert(route.channel == 9);
    }
}

void testTransportControlSharesTheSameSnapshot() {
    MidiDeviceProfileRuntime runtime;
    runtime.initialize(
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative));

    assert(!runtime.updateTransportControl(
        TransportClockSource::GroovePuterInternal, true));
    assert(runtime.revision() == 0);

    assert(runtime.updateTransportControl(
        TransportClockSource::SeqtrakExternal, false));
    assert(runtime.revision() == 1);
    assert(runtime.settings().transportClockSource ==
           TransportClockSource::SeqtrakExternal);
    assert(!runtime.settings().externalFollowEnabled);
    assert(runtime.profile() == MidiDeviceProfile::SeqtrakNative);
}

void testApplyBeforeExplicitInitializeUsesSafeSeqtrakBaseline() {
    MidiDeviceProfileRuntime runtime;
    assert(!runtime.initialized());

    assert(runtime.applyProfile(MidiDeviceProfile::GenericMidi));
    assert(runtime.initialized());
    assert(runtime.revision() == 1);
    assert(runtime.profile() == MidiDeviceProfile::GenericMidi);
    assert(runtime.settings().synthAChannel == 0);
    assert(runtime.settings().synthBChannel == 1);
    for (const DrumMidiRoute& route : runtime.settings().drumRoutes) {
        assert(!route.enabled);
    }
}

}  // namespace

int main() {
    testInitializeOwnsOneSanitizedSnapshot();
    testProfileApplyIsTransactionalAndPreservesUserIntent();
    testTransportControlSharesTheSameSnapshot();
    testApplyBeforeExplicitInitializeUsesSafeSeqtrakBaseline();

    // Leave the global capability view on the release default for following
    // tests in a combined process.
    midiTransportCapabilityRuntime().setDeviceProfile(
        MidiDeviceProfile::SeqtrakNative);
    return 0;
}
