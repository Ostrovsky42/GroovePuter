#pragma once

#include "project_transport_timeline.h"
#include "scheduled_smf_midi_event.h"

namespace GroovePuterMidi {

inline constexpr bool projectSmfNoteOnStillCurrent(
        const ScheduledSmfMidiEvent& event,
        const ProjectTransportBlockSnapshot& transport) {
    if (event.projectTransportEpoch == 0) return true;
    return transport.valid && transport.playing &&
           scheduledSmfMidiEventTransportEpochIsCurrent(
               event, transport.transportEpoch);
}

}  // namespace GroovePuterMidi
