#include "cardputer_ui_session.h"

#include <cstddef>
#include <cstdint>

#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
#include <Preferences.h>
#endif

namespace GroovePuterPlatform {
namespace {

constexpr uint32_t kSessionMagic = 0x47505331u;  // GPS1
constexpr uint16_t kSessionSchema = 1;
constexpr const char* kSessionNamespace = "gp-session";
constexpr const char* kSessionKey = "state";

struct UiSessionRecord {
    uint32_t magic{kSessionMagic};
    uint16_t schema{kSessionSchema};
    uint16_t payloadSize{sizeof(GroovePuterState::UiSessionState)};
    GroovePuterState::UiSessionState state{};
    uint32_t checksum{0};
};

static_assert(sizeof(UiSessionRecord) <= 32,
              "NVS UI session record must remain compact");

uint32_t checksumRecord(const UiSessionRecord& record) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
    constexpr size_t checksumOffset = offsetof(UiSessionRecord, checksum);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < checksumOffset; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

bool validRecord(const UiSessionRecord& record) {
    return record.magic == kSessionMagic &&
           record.schema == kSessionSchema &&
           record.payloadSize == sizeof(GroovePuterState::UiSessionState) &&
           record.checksum == checksumRecord(record);
}

}  // namespace

bool loadCardputerUiSession(GroovePuterState::UiSessionState& state) {
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (!preferences.begin(kSessionNamespace, true)) return false;

    UiSessionRecord record{};
    const size_t storedSize = preferences.getBytesLength(kSessionKey);
    const size_t read = storedSize == sizeof(record)
        ? preferences.getBytes(kSessionKey, &record, sizeof(record))
        : 0;
    preferences.end();

    if (read != sizeof(record) || !validRecord(record)) return false;
    state = record.state;
    GroovePuterState::sanitizeUiSessionState(state);
    return true;
#else
    (void)state;
    return false;
#endif
}

bool saveCardputerUiSession(const GroovePuterState::UiSessionState& state) {
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    UiSessionRecord record{};
    record.state = state;
    GroovePuterState::sanitizeUiSessionState(record.state);
    record.checksum = checksumRecord(record);

    Preferences preferences;
    if (!preferences.begin(kSessionNamespace, false)) return false;
    const size_t written =
        preferences.putBytes(kSessionKey, &record, sizeof(record));
    preferences.end();
    return written == sizeof(record);
#else
    // Desktop/WASM currently has no device-session backend. Treat the operation
    // as a successful no-op so the UI does not retry forever every five seconds.
    (void)state;
    return true;
#endif
}

}  // namespace GroovePuterPlatform
