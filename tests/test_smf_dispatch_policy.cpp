#include <cassert>

#include "src/midi/smf_dispatch_policy.h"

int main() {
    static_assert(kSmfSendRetryLimit == 24,
                  "SMF retry budget must cover a full TinyUSB FIFO drain");

    ScheduledSmfMidiEvent noteOn{};
    noteOn.type = ScheduledSmfMidiEventType::NoteOn;
    ScheduledSmfMidiEvent noteOff{};
    noteOff.type = ScheduledSmfMidiEventType::NoteOff;

    assert(smfSendFailureAction(noteOn, 1) == SmfSendFailureAction::Retry);
    assert(smfSendFailureAction(noteOff, kSmfSendRetryLimit - 1) ==
           SmfSendFailureAction::Retry);
    assert(smfSendFailureAction(noteOn, kSmfSendRetryLimit) ==
           SmfSendFailureAction::DropNoteOn);
    assert(smfSendFailureAction(noteOff, kSmfSendRetryLimit) ==
           SmfSendFailureAction::BeginCleanup);
    return 0;
}
