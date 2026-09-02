#include <cassert>

#include "src/midi/midi_output_route_projection.h"

using namespace GroovePuterMidi;

namespace {

const DrumMidiRoute& drumRoute(const MidiOutputRouteProjection& projection,
                               MidiDrumVoice voice) {
    return projection.patternDrums[drumVoiceIndex(voice)];
}

void testSeqtrakProjection() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    const MidiOutputRouteProjection projection = projectMidiOutputRoutes(settings);

    assert(projection.profile == MidiDeviceProfile::SeqtrakNative);
    assert(projection.patternSynthA.enabled);
    assert(projection.patternSynthA.channel == 7);
    assert(projection.patternSynthB.enabled);
    assert(projection.patternSynthB.channel == 8);
    assert(drumRoute(projection, MidiDrumVoice::Kick).channel == 0);
    assert(drumRoute(projection, MidiDrumVoice::Snare).channel == 1);
    assert(drumRoute(projection, MidiDrumVoice::Clap).channel == 2);
    assert(drumRoute(projection, MidiDrumVoice::ClosedHat).channel == 3);
    assert(drumRoute(projection, MidiDrumVoice::OpenHat).channel == 4);
    assert(drumRoute(projection, MidiDrumVoice::MidTom).channel == 5);
    assert(drumRoute(projection, MidiDrumVoice::Rim).channel == 5);
    assert(drumRoute(projection, MidiDrumVoice::HighTom).channel == 6);
    for (const DrumMidiRoute& route : projection.patternDrums) {
        assert(route.enabled);
        assert(route.note == 60);
    }
    assert(projection.receiverModeControl == MidiReceiverModeControl::SeqtrakCc26);
    assert(!projection.completePerformanceTargetTable);
}

void testGeneralMidiProjectionCarriesPhysicalNotes() {
    const MidiOutputRouteProjection projection = projectMidiOutputRoutes(
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi));

    for (const DrumMidiRoute& route : projection.patternDrums) {
        assert(route.enabled);
        assert(route.channel == 9);
    }
    assert(drumRoute(projection, MidiDrumVoice::Kick).note == 36);
    assert(drumRoute(projection, MidiDrumVoice::Snare).note == 38);
    assert(drumRoute(projection, MidiDrumVoice::ClosedHat).note == 42);
    assert(drumRoute(projection, MidiDrumVoice::OpenHat).note == 46);
    assert(drumRoute(projection, MidiDrumVoice::MidTom).note == 43);
    assert(drumRoute(projection, MidiDrumVoice::HighTom).note == 47);
    assert(drumRoute(projection, MidiDrumVoice::Rim).note == 37);
    assert(drumRoute(projection, MidiDrumVoice::Clap).note == 39);
    assert(projection.receiverModeControl == MidiReceiverModeControl::None);
}

void testGenericMidiProjectionMakesNoDrumAssumption() {
    const MidiOutputRouteProjection projection = projectMidiOutputRoutes(
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GenericMidi));

    assert(projection.profile == MidiDeviceProfile::GenericMidi);
    for (const DrumMidiRoute& route : projection.patternDrums) {
        assert(!route.enabled);
    }
    assert(projection.receiverModeControl == MidiReceiverModeControl::None);
}

void testGlobalAndPerRouteEnableAreComposed() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    settings.patternSynthAEnabled = false;
    settings.drumRoutes[drumVoiceIndex(MidiDrumVoice::Snare)].enabled = false;

    MidiOutputRouteProjection projection = projectMidiOutputRoutes(settings);
    assert(!projection.patternSynthA.enabled);
    assert(projection.patternSynthB.enabled);
    assert(drumRoute(projection, MidiDrumVoice::Kick).enabled);
    assert(!drumRoute(projection, MidiDrumVoice::Snare).enabled);

    settings.enabled = false;
    projection = projectMidiOutputRoutes(settings);
    assert(!projection.live.enabled);
    assert(!projection.patternSynthA.enabled);
    assert(!projection.patternSynthB.enabled);
    for (const DrumMidiRoute& route : projection.patternDrums) {
        assert(!route.enabled);
    }
}

void testSelectedLiveRouteIsProjectedWithoutInventingTargetTable() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    settings.liveTarget = MidiLiveTarget::SynthB;
    settings.liveChannel = 12;

    MidiOutputRouteProjection projection = projectMidiOutputRoutes(settings);
    assert(projection.live.enabled);
    assert(projection.live.target == MidiLiveTarget::SynthB);
    assert(projection.live.channel == 12);
    assert(!projection.live.usesPerVoiceDrumRoutes);
    assert(!projection.completePerformanceTargetTable);

    settings.liveTarget = MidiLiveTarget::Drums;
    projection = projectMidiOutputRoutes(settings);
    assert(projection.live.usesPerVoiceDrumRoutes);
}

void testCustomProjectionPreservesUserWireMap() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    settings.profile = MidiDeviceProfile::Custom;
    settings.synthAChannel = 4;
    settings.synthBChannel = 11;
    settings.drumRoutes[0] = DrumMidiRoute{true, 12, 71};
    settings.drumRoutes[1] = DrumMidiRoute{false, 13, 72};

    const MidiOutputRouteProjection projection = projectMidiOutputRoutes(settings);
    assert(projection.profile == MidiDeviceProfile::Custom);
    assert(projection.patternSynthA.channel == 4);
    assert(projection.patternSynthB.channel == 11);
    assert(projection.patternDrums[0].enabled);
    assert(projection.patternDrums[0].channel == 12);
    assert(projection.patternDrums[0].note == 71);
    assert(!projection.patternDrums[1].enabled);
    assert(projection.patternDrums[1].channel == 13);
    assert(projection.patternDrums[1].note == 72);
}

}  // namespace

int main() {
    testSeqtrakProjection();
    testGeneralMidiProjectionCarriesPhysicalNotes();
    testGenericMidiProjectionMakesNoDrumAssumption();
    testGlobalAndPerRouteEnableAreComposed();
    testSelectedLiveRouteIsProjectedWithoutInventingTargetTable();
    testCustomProjectionPreservesUserWireMap();
    return 0;
}
