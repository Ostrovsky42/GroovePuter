#pragma once

#include <cstdint>

#include "scheduled_smf_midi_event.h"

enum class SmfLateDispatchAction : uint8_t {
    OnTime = 0,
    SendLateNoteOn,
    DropLateNoteOn,
    SendLateNoteOff,
};

inline constexpr SmfLateDispatchAction smfLateDispatchAction(
        ScheduledSmfMidiEventType type,
        uint32_t latenessMicros,
        uint32_t noteOnLimitMicros) {
    if (latenessMicros == 0) return SmfLateDispatchAction::OnTime;
    if (type == ScheduledSmfMidiEventType::NoteOff ||
        type == ScheduledSmfMidiEventType::SongPositionPointer) {
        // NoteOff and transport-position intents are corrective lifecycle data.
        // They must be delivered late rather than discarded as stale NoteOn.
        return SmfLateDispatchAction::SendLateNoteOff;
    }
    return latenessMicros > noteOnLimitMicros
        ? SmfLateDispatchAction::DropLateNoteOn
        : SmfLateDispatchAction::SendLateNoteOn;
}
