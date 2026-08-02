#pragma once

#include <cstdint>

enum class ExternalMidiTransportEventType : uint8_t {
    Clock = 0,
    Start,
    Continue,
    Stop,
};

struct ExternalMidiTransportEvent {
    ExternalMidiTransportEventType type{ExternalMidiTransportEventType::Clock};
    uint8_t reserved[3]{};
    uint32_t timestampMicros{0};
    uint32_t pulseOrdinal{0};
    uint32_t receiveSequence{0};
};

inline constexpr bool externalMidiTransportEventIsCritical(
        ExternalMidiTransportEventType type) {
    return type == ExternalMidiTransportEventType::Start ||
           type == ExternalMidiTransportEventType::Continue ||
           type == ExternalMidiTransportEventType::Stop;
}

static_assert(sizeof(ExternalMidiTransportEvent) == 16,
              "external transport event must remain compact");
