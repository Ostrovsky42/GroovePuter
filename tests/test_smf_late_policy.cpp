#include <cassert>
#include <cstdint>

#include "src/midi/smf_late_policy.h"

int main() {
    constexpr uint32_t blockMicros = 23219;

    assert(smfLateDispatchAction(
               ScheduledSmfMidiEventType::NoteOn, 0, blockMicros) ==
           SmfLateDispatchAction::OnTime);
    assert(smfLateDispatchAction(
               ScheduledSmfMidiEventType::NoteOn, blockMicros, blockMicros) ==
           SmfLateDispatchAction::SendLateNoteOn);
    assert(smfLateDispatchAction(
               ScheduledSmfMidiEventType::NoteOn, blockMicros + 1, blockMicros) ==
           SmfLateDispatchAction::DropLateNoteOn);
    assert(smfLateDispatchAction(
               ScheduledSmfMidiEventType::NoteOff, blockMicros * 20, blockMicros) ==
           SmfLateDispatchAction::SendLateNoteOff);
    return 0;
}
