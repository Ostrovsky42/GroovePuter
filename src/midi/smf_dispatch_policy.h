#pragma once

#include <cstdint>

#include "scheduled_smf_midi_event.h"

enum class SmfSendFailureAction : uint8_t {
    Retry = 0,
    // Historical name retained for source compatibility. The action means the
    // event never reached the wire and owns no cleanup responsibility; this is
    // valid for a NoteOn and for a Song Position Pointer transport intent.
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
    return event.type == ScheduledSmfMidiEventType::NoteOff
        ? SmfSendFailureAction::BeginCleanup
        : SmfSendFailureAction::DropNoteOn;
}
