#pragma once
#ifndef GROOVEPUTER_OUTPUT_MODE_RUNTIME_H
#define GROOVEPUTER_OUTPUT_MODE_RUNTIME_H

#include "output_ownership.h"
#include "../dsp/miniacid_engine.h"

namespace GroovePuterOutput {

// Apply under the caller's existing AudioGuard/AudioMutationGate when invoked
// from UI/control code. No allocation, SD, JSON or USB work occurs here.
inline bool applyModeWithLocalCleanup(MiniAcid& engine,
                                      Track track,
                                      Mode nextMode) {
    const TrackState previous = state(track);
    if (previous.explicitMode && previous.mode == nextMode) return false;

    const bool previousInternal = allowsInternal(track, SourceClass::Pattern);
    const bool nextInternal = nextMode != Mode::Midi;

    if (!setMode(track, nextMode)) return false;

    if (previousInternal && !nextInternal) {
        switch (track) {
            case Track::SynthA:
            case Track::SynthB: {
                // PERFORM live notes have explicit identity and can be released
                // immediately. Pattern voices keep their current bounded gate /
                // release tail; new Pattern local NoteOn is already disabled by
                // OutputOwnedSynthVoiceSlot, avoiding a private-engine escape.
                const int synthIndex = track == Track::SynthB ? 1 : 0;
                const int note = engine.liveNote(synthIndex);
                if (note >= 0) {
                    engine.liveNoteOff(
                        synthIndex, static_cast<uint8_t>(note));
                }
                break;
            }
            case Track::Drums:
                // Drum synths are one-shot. Stop only active sampler voices so
                // loops cannot survive an INTERNAL/LAYER -> MIDI transition.
                // Assignments, preload state and samplerEnabled are preserved.
                if (engine.samplerTrack) engine.samplerTrack->stopAll();
                break;
            case Track::Count:
                break;
        }
    }
    return true;
}

}  // namespace GroovePuterOutput

#endif  // GROOVEPUTER_OUTPUT_MODE_RUNTIME_H
