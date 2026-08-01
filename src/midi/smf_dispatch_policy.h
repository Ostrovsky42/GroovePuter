#pragma once

#include <cstdint>

#include "scheduled_smf_midi_event.h"

enum class SmfSendFailureAction : uint8_t {
    Retry = 0,
    DropNoteOn,
    BeginCleanup,
};

// Retries are spaced one millisecond apart. The USB MIDI TX FIFO holds only 16
// event packets, so a chord or a catch-up burst needs several host polls to
// drain; eight attempts shed notes while the endpoint was merely busy.
inline constexpr uint8_t kSmfSendRetryLimit = 24;

inline constexpr SmfSendFailureAction smfSendFailureAction(
        const ScheduledSmfMidiEvent& event,
        uint8_t failedAttempts) {
    if (failedAttempts < kSmfSendRetryLimit) {
        return SmfSendFailureAction::Retry;
    }
    return event.type == ScheduledSmfMidiEventType::NoteOn
        ? SmfSendFailureAction::DropNoteOn
        : SmfSendFailureAction::BeginCleanup;
}
