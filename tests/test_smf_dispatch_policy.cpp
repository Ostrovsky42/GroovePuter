#include <cassert>

#include "src/midi/smf_dispatch_policy.h"

int main() {
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
