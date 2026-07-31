#pragma once

#include <cstdint>

#include "scheduled_smf_midi_event.h"

enum class SmfSendFailureAction : uint8_t {
    Retry = 0,
    DropNoteOn,
    BeginCleanup,
};

inline constexpr uint8_t kSmfSendRetryLimit = 8;

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
