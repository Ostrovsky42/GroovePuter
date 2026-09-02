#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_companion_settings_codec.h"

namespace {

class MemoryStorage final : public GroovePuterMidi::IMidiSettingsStorage {
public:
    GroovePuterMidi::MidiSettingsStorageReadStatus read(
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) override {
        bytesRead = 0;
        if (!hasRecord_) {
            return GroovePuterMidi::MidiSettingsStorageReadStatus::NotFound;
        }
        if (size_ > capacity) {
            return GroovePuterMidi::MidiSettingsStorageReadStatus::Error;
        }
        std::memcpy(destination, bytes_.data(), size_);
        bytesRead = size_;
        return GroovePuterMidi::MidiSettingsStorageReadStatus::Ok;
    }

    bool write(const uint8_t* data, std::size_t size) override {
        if (size > bytes_.size()) return false;
        std::memcpy(bytes_.data(), data, size);
        size_ = size;
        hasRecord_ = true;
        return true;
    }

private:
    std::array<uint8_t, GroovePuterMidi::MidiSettingsCodec::kEncodedSize> bytes_{};
    std::size_t size_{0};
    bool hasRecord_{false};
};

void expectRoundTripProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    using namespace GroovePuterMidi;

    MemoryStorage storage;
    MidiSettingsPersistence persistence(storage);

    MidiOutputSettings active =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    active.transportClockSource = TransportClockSource::SeqtrakExternal;
    active.externalFollowEnabled = false;

    MidiOutputSettings pending = active;
    applyMidiDeviceProfile(profile, pending);
    // Profile application must not erase user-owned transport intent.
    pending.transportClockSource = active.transportClockSource;
    pending.externalFollowEnabled = active.externalFollowEnabled;
    assert(persistence.save(pending));

    MidiOutputSettings loaded =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    assert(persistence.load(loaded) == MidiSettingsLoadStatus::Loaded);
    assert(loaded.profile == profile);
    assert(loaded.transportClockSource == TransportClockSource::SeqtrakExternal);
    assert(!loaded.externalFollowEnabled);

    // Simulate a later transport-control save before reboot. The active runtime
    // is still SEQTRAK, but persistence must be recomposed with the pending
    // profile rather than silently reverting the profile byte.
    MidiOutputSettings laterActive = active;
    laterActive.transportClockSource = TransportClockSource::GroovePuterInternal;
    laterActive.externalFollowEnabled = true;

    MidiOutputSettings record = laterActive;
    if (record.profile != profile) {
        applyMidiDeviceProfile(profile, record);
    }
    record.transportClockSource = laterActive.transportClockSource;
    record.externalFollowEnabled = laterActive.externalFollowEnabled;
    assert(persistence.save(record));

    MidiOutputSettings reloaded =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    assert(persistence.load(reloaded) == MidiSettingsLoadStatus::Loaded);
    assert(reloaded.profile == profile);
    assert(reloaded.transportClockSource ==
           TransportClockSource::GroovePuterInternal);
    assert(reloaded.externalFollowEnabled);
}

}  // namespace

int main() {
    expectRoundTripProfile(GroovePuterMidi::MidiDeviceProfile::GeneralMidi);
    expectRoundTripProfile(GroovePuterMidi::MidiDeviceProfile::GenericMidi);
    expectRoundTripProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    expectRoundTripProfile(GroovePuterMidi::MidiDeviceProfile::Custom);
    return 0;
}
