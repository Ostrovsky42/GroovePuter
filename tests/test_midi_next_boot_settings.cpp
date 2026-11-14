#include <cassert>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/midi_output_route_projection.h"
#include "src/midi/midi_performance_route_projection.h"
#include "src/platform/cardputer_midi_settings_session.h"

using namespace GroovePuterMidi;

int main() {
    MidiOutputSettings active =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    active.transportClockSource = TransportClockSource::SeqtrakExternal;
    active.externalFollowEnabled = false;

    const MidiOutputSettings internalOnly =
        composeNextBootMidiOutputSettings(
            active, MidiDeviceProfile::SeqtrakNative, false);
    assert(internalOnly.profile == MidiDeviceProfile::SeqtrakNative);
    assert(!internalOnly.enabled);
    assert(internalOnly.transportClockSource ==
           TransportClockSource::SeqtrakExternal);
    assert(!internalOnly.externalFollowEnabled);

    const MidiOutputSettings genericOutput =
        composeNextBootMidiOutputSettings(
            internalOnly, MidiDeviceProfile::GenericMidi, true);
    assert(genericOutput.profile == MidiDeviceProfile::GenericMidi);
    assert(genericOutput.enabled);
    assert(genericOutput.transportClockSource ==
           TransportClockSource::SeqtrakExternal);
    assert(!genericOutput.externalFollowEnabled);

    const MidiOutputRouteProjection pattern =
        projectMidiOutputRoutes(internalOnly);
    assert(!pattern.live.enabled);
    assert(!pattern.patternSynthA.enabled);
    assert(!pattern.patternSynthB.enabled);
    for (const DrumMidiRoute& route : pattern.patternDrums) {
        assert(!route.enabled);
    }

    const MidiPerformanceRouteProjection performance =
        projectMidiPerformanceRoutes(internalOnly);
    assert(!performance.enabled);
    assert(!performance.synthA.enabled);
    assert(!performance.synthB.enabled);
    assert(!performance.dx.enabled);
    for (const DrumMidiRoute& route : performance.drums) {
        assert(!route.enabled);
    }

    const auto bytes = MidiSettingsCodec::encode(internalOnly);
    MidiOutputSettings reloaded{};
    assert(MidiSettingsCodec::decode(bytes.data(), bytes.size(), reloaded));
    assert(reloaded == internalOnly);

    GroovePuterMidi::midiDeviceProfileRuntime().initialize(active);
    assert(GroovePuterPlatform::pendingCardputerMidiOutputEnabled());
    assert(!GroovePuterPlatform::cardputerMidiSettingsRestartRequired());
    assert(GroovePuterPlatform::selectCardputerMidiOutputEnabledForNextBoot(
        false));
    assert(!GroovePuterPlatform::pendingCardputerMidiOutputEnabled());
    assert(GroovePuterPlatform::cardputerMidiSettingsRestartRequired());
    assert(GroovePuterPlatform::selectCardputerMidiDeviceProfileForNextBoot(
        MidiDeviceProfile::GenericMidi));
    assert(GroovePuterPlatform::pendingCardputerMidiDeviceProfile() ==
           MidiDeviceProfile::GenericMidi);
    assert(!GroovePuterPlatform::pendingCardputerMidiOutputEnabled());
    return 0;
}
