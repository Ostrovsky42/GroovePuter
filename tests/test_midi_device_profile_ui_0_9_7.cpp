#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/ui/midi_device_profile_ui.h"

using GroovePuterMidi::MidiDeviceProfile;
namespace ProfileUi = GroovePuterUi::MidiDeviceProfileUi;

int main() {
    assert(std::strcmp(ProfileUi::shortName(MidiDeviceProfile::SeqtrakNative),
                       "SEQTRAK") == 0);
    assert(std::strcmp(ProfileUi::shortName(MidiDeviceProfile::GeneralMidi),
                       "GM") == 0);
    assert(std::strcmp(ProfileUi::shortName(MidiDeviceProfile::GenericMidi),
                       "GENERIC") == 0);
    assert(std::strcmp(ProfileUi::shortName(MidiDeviceProfile::Custom),
                       "CUSTOM") == 0);

    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::SeqtrakNative, 1) ==
           MidiDeviceProfile::GeneralMidi);
    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::GeneralMidi, 1) ==
           MidiDeviceProfile::GenericMidi);
    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::GenericMidi, 1) ==
           MidiDeviceProfile::SeqtrakNative);

    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::SeqtrakNative, -1) ==
           MidiDeviceProfile::GenericMidi);
    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::GenericMidi, -1) ==
           MidiDeviceProfile::GeneralMidi);

    // CUSTOM is visible when loaded from persisted state but is not part of
    // the preset cycle until a custom route editor exists.
    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::Custom, 1) ==
           MidiDeviceProfile::SeqtrakNative);
    assert(ProfileUi::stepSelectableProfile(
               MidiDeviceProfile::Custom, -1) ==
           MidiDeviceProfile::GenericMidi);

    const uint8_t encoded =
        ProfileUi::encodePreview(MidiDeviceProfile::GeneralMidi);
    assert(ProfileUi::profileFromPreview(
               encoded, MidiDeviceProfile::SeqtrakNative) ==
           MidiDeviceProfile::GeneralMidi);
    assert(ProfileUi::profileFromPreview(
               ProfileUi::kUnsetPreview,
               MidiDeviceProfile::GenericMidi) ==
           MidiDeviceProfile::GenericMidi);

    return 0;
}
