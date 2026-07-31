#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_companion_settings_codec.h"

using namespace GroovePuterMidi;

namespace {

class FakeStorage final : public IMidiSettingsStorage {
public:
    MidiSettingsStorageReadStatus read(uint8_t* destination,
                                       std::size_t capacity,
                                       std::size_t& bytesRead) override {
        bytesRead = 0;
        if (readStatus != MidiSettingsStorageReadStatus::Ok) {
            return readStatus;
        }
        if (storedSize > capacity) return MidiSettingsStorageReadStatus::Error;
        std::memcpy(destination, bytes.data(), storedSize);
        bytesRead = storedSize;
        return MidiSettingsStorageReadStatus::Ok;
    }

    bool write(const uint8_t* source, std::size_t size) override {
        if (!writeResult || size > bytes.size()) return false;
        std::memcpy(bytes.data(), source, size);
        storedSize = size;
        return true;
    }

    std::array<uint8_t, MidiSettingsCodec::kEncodedSize> bytes{};
    std::size_t storedSize{0};
    MidiSettingsStorageReadStatus readStatus{
        MidiSettingsStorageReadStatus::NotFound};
    bool writeResult{true};
};

const DrumMidiRoute& routeFor(const MidiOutputSettings& settings,
                              MidiDrumVoice voice) {
    return settings.drumRoutes[drumVoiceIndex(voice)];
}

void testSeqtrakDefaults() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);

    assert(settings.profile == MidiDeviceProfile::SeqtrakNative);
    assert(settings.liveChannel == 7);
    assert(settings.synthAChannel == 7);
    assert(settings.synthBChannel == 8);
    assert(routeFor(settings, MidiDrumVoice::Kick).channel == 0);
    assert(routeFor(settings, MidiDrumVoice::Snare).channel == 1);
    assert(routeFor(settings, MidiDrumVoice::Clap).channel == 2);
    assert(routeFor(settings, MidiDrumVoice::ClosedHat).channel == 3);
    assert(routeFor(settings, MidiDrumVoice::OpenHat).channel == 4);
    assert(routeFor(settings, MidiDrumVoice::MidTom).channel == 5);
    assert(routeFor(settings, MidiDrumVoice::HighTom).channel == 6);
    assert(routeFor(settings, MidiDrumVoice::Rim).channel == 5);
    for (const DrumMidiRoute& route : settings.drumRoutes) {
        assert(route.enabled);
        assert(route.note == 60);
    }
    assert(isValidMidiOutputSettings(settings));
}

void testGeneralMidiDefaults() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);

    assert(settings.profile == MidiDeviceProfile::GeneralMidi);
    for (const DrumMidiRoute& route : settings.drumRoutes) {
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
    assert(isValidMidiOutputSettings(settings));
}

void testCustomDefaultsAreSafe() {
    const MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::Custom);
    assert(settings.profile == MidiDeviceProfile::Custom);
    assert(settings.liveChannel == 7);
    assert(settings.synthAChannel == 7);
    assert(settings.synthBChannel == 8);
    assert(routeFor(settings, MidiDrumVoice::Kick).channel == 0);
    assert(routeFor(settings, MidiDrumVoice::Clap).channel == 2);
    assert(isValidMidiOutputSettings(settings));
}

void testUiChannelConversion() {
    assert(zeroBasedMidiChannelFromUi(-20) == 0);
    assert(zeroBasedMidiChannelFromUi(1) == 0);
    assert(zeroBasedMidiChannelFromUi(8) == 7);
    assert(zeroBasedMidiChannelFromUi(16) == 15);
    assert(zeroBasedMidiChannelFromUi(99) == 15);

    assert(uiMidiChannelFromZeroBased(0) == 1);
    assert(uiMidiChannelFromZeroBased(7) == 8);
    assert(uiMidiChannelFromZeroBased(15) == 16);
    assert(uiMidiChannelFromZeroBased(255) == 16);
}

void testProfileApplicationPreservesRuntimeToggles() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    settings.enabled = false;
    settings.liveEnabled = false;
    settings.patternSynthAEnabled = false;
    settings.patternSynthBEnabled = true;
    settings.drumsEnabled = false;
    settings.liveTarget = MidiLiveTarget::Drums;

    applyMidiDeviceProfile(MidiDeviceProfile::GeneralMidi, settings);

    assert(settings.profile == MidiDeviceProfile::GeneralMidi);
    assert(!settings.enabled);
    assert(!settings.liveEnabled);
    assert(!settings.patternSynthAEnabled);
    assert(settings.patternSynthBEnabled);
    assert(!settings.drumsEnabled);
    assert(settings.liveTarget == MidiLiveTarget::Drums);
    assert(settings.liveChannel == 0);
    assert(settings.synthBChannel == 1);
    assert(isValidMidiOutputSettings(settings));
}

void testCustomProfilePreservesRoutes() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    settings.liveChannel = 12;
    settings.synthAChannel = 13;
    settings.synthBChannel = 14;
    settings.liveTarget = MidiLiveTarget::Drums;
    settings.drumRoutes[0] = DrumMidiRoute{false, 11, 71};
    settings.drumGateMs = 222;

    applyMidiDeviceProfile(MidiDeviceProfile::Custom, settings);

    assert(settings.profile == MidiDeviceProfile::Custom);
    assert(settings.liveTarget == MidiLiveTarget::Drums);
    assert(settings.liveChannel == 12);
    assert(settings.synthAChannel == 13);
    assert(settings.synthBChannel == 14);
    const DrumMidiRoute expectedRoute{false, 11, 71};
    assert(settings.drumRoutes[0] == expectedRoute);
    assert(settings.drumGateMs == 222);
}

void testValidationAndSanitization() {
    MidiOutputSettings settings =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    settings.profile = static_cast<MidiDeviceProfile>(255);
    settings.liveTarget = static_cast<MidiLiveTarget>(255);
    settings.liveChannel = 200;
    settings.synthAChannel = 201;
    settings.synthBChannel = 202;
    settings.drumGateMs = 0;
    settings.drumRoutes[0].channel = 200;
    settings.drumRoutes[0].note = 200;

    assert(!isValidMidiOutputSettings(settings));
    sanitizeMidiOutputSettings(settings);
    assert(settings.profile == MidiDeviceProfile::Custom);
    assert(settings.liveTarget == MidiLiveTarget::SynthA);
    assert(settings.liveChannel == 15);
    assert(settings.synthAChannel == 15);
    assert(settings.synthBChannel == 15);
    assert(settings.drumGateMs == kDrumGateMinMs);
    assert(settings.drumRoutes[0].channel == 15);
    assert(settings.drumRoutes[0].note == 127);
    assert(isValidMidiOutputSettings(settings));
}

void testCodecRoundTrip() {
    MidiOutputSettings source =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    source.profile = MidiDeviceProfile::Custom;
    source.enabled = false;
    source.liveEnabled = true;
    source.patternSynthAEnabled = false;
    source.patternSynthBEnabled = true;
    source.drumsEnabled = true;
    source.liveTarget = MidiLiveTarget::Drums;
    source.liveChannel = 14;
    source.synthAChannel = 2;
    source.synthBChannel = 12;
    source.drumGateMs = 137;
    source.drumRoutes[3] = DrumMidiRoute{false, 11, 73};

    const auto encoded = MidiSettingsCodec::encode(source);
    MidiOutputSettings decoded =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    assert(MidiSettingsCodec::decode(encoded.data(), encoded.size(), decoded));
    assert(decoded == source);
}

void testCodecFailureDoesNotMutateOutput() {
    MidiOutputSettings sentinel =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    sentinel.liveChannel = 12;
    const MidiOutputSettings original = sentinel;

    auto encoded = MidiSettingsCodec::encode(
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative));
    encoded[10] ^= 0x7Fu;
    assert(!MidiSettingsCodec::decode(encoded.data(), encoded.size(), sentinel));
    assert(sentinel == original);

    assert(!MidiSettingsCodec::decode(encoded.data(), encoded.size() - 1, sentinel));
    assert(sentinel == original);
}

void testPersistence() {
    FakeStorage storage;
    MidiSettingsPersistence persistence(storage);
    MidiOutputSettings output =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);

    assert(persistence.load(output) ==
           MidiSettingsLoadStatus::DefaultsFromMissing);
    assert(output.profile == MidiDeviceProfile::SeqtrakNative);

    MidiOutputSettings saved =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi);
    saved.liveTarget = MidiLiveTarget::Drums;
    saved.drumGateMs = 123;
    assert(persistence.save(saved));
    storage.readStatus = MidiSettingsStorageReadStatus::Ok;
    assert(persistence.load(output) == MidiSettingsLoadStatus::Loaded);
    assert(output == saved);
    assert(output.liveTarget == MidiLiveTarget::Drums);

    storage.bytes[9] ^= 0x80u;
    assert(persistence.load(output, MidiDeviceProfile::GeneralMidi) ==
           MidiSettingsLoadStatus::DefaultsFromCorrupt);
    assert(output ==
           makeDefaultMidiOutputSettings(MidiDeviceProfile::GeneralMidi));

    const MidiOutputSettings beforeError = output;
    storage.readStatus = MidiSettingsStorageReadStatus::Error;
    assert(persistence.load(output) == MidiSettingsLoadStatus::StorageError);
    assert(output == beforeError);

    storage.writeResult = false;
    assert(!persistence.save(saved));
}

}  // namespace

int main() {
    testSeqtrakDefaults();
    testGeneralMidiDefaults();
    testCustomDefaultsAreSafe();
    testUiChannelConversion();
    testProfileApplicationPreservesRuntimeToggles();
    testCustomProfilePreservesRoutes();
    testValidationAndSanitization();
    testCodecRoundTrip();
    testCodecFailureDoesNotMutateOutput();
    testPersistence();
    return 0;
}
