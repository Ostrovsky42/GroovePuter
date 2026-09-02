#pragma once

#include <cstdint>

#include "musical_event_router.h"
#include "src/audio/audio_mutation_gate.h"

class MiniAcid;

// Physical output sink for normalized musical events. Voice ownership and
// held-note priority remain outside this class in PerformanceKeyboard.
class InternalSynthOutput final : public IMusicalEventSink {
public:
    InternalSynthOutput(MiniAcid& engine, AudioMutationGate& mutationGate)
        : engine_(engine), mutationGate_(mutationGate) {}

    void handleMusicalEvent(const MusicalEvent& event) override;

private:
    static int synthIndex(MusicalEventTarget target);

    MiniAcid& engine_;
    AudioMutationGate& mutationGate_;
    // Only tracks sampler voices started by this PERFORM sink. It is not a MIDI
    // note-owner table; one bit corresponds to one normalized drum lane 0..7.
    uint8_t liveDrumPadMask_{0};
};
