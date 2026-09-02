#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/input/musical_event_queue.h"
#include "src/midi/midi_control_event_queue.h"
#include "src/output/output_ownership.h"

using GroovePuterOutput::Mode;
using GroovePuterOutput::Track;

namespace {

float phaseReader(void*) { return 0.0f; }

MusicalEvent event(MusicalEventType type,
                   MusicalEventSource source,
                   MusicalEventTarget target,
                   uint8_t note = 60,
                   uint8_t channel = 0) {
    return MusicalEvent{type, source, target, channel, note, 100};
}

void resetLegacy() {
    GroovePuterOutput::restoreLegacyCompatibility(Track::SynthA);
    GroovePuterOutput::restoreLegacyCompatibility(Track::SynthB);
    GroovePuterOutput::restoreLegacyCompatibility(Track::Drums);
}

void expectControlQueuePolicy() {
    resetLegacy();
    MidiControlEventQueue queue;
    MusicalEvent popped{};

    // Frozen <=0.9.5 PERFORM remains external in legacy mode.
    auto liveOn = event(MusicalEventType::NoteOn,
                        MusicalEventSource::PerformanceKeyboard,
                        MusicalEventTarget::SynthA);
    assert(queue.tryPush(liveOn));
    assert(queue.tryPop(popped));

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    const uint32_t droppedBefore = queue.droppedNoteOnCount();
    assert(!queue.tryPush(liveOn));
    assert(queue.approximateSize() == 0);
    assert(queue.droppedNoteOnCount() == droppedBefore);

    // Cleanup is never filtered by current output intent.
    auto liveOff = event(MusicalEventType::NoteOff,
                         MusicalEventSource::PerformanceKeyboard,
                         MusicalEventTarget::SynthA);
    assert(queue.tryPush(liveOff));
    assert(queue.tryPop(popped));
    auto livePanic = event(MusicalEventType::AllNotesOff,
                           MusicalEventSource::PerformanceKeyboard,
                           MusicalEventTarget::SynthA);
    assert(queue.tryPush(livePanic));
    assert(queue.tryPop(popped));

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Midi));
    assert(queue.tryPush(liveOn));
    assert(queue.tryPop(popped));

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Layer));
    assert(queue.tryPush(liveOn));
    assert(queue.tryPop(popped));

    // Synth ownership is independent per logical track.
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    assert(GroovePuterOutput::setMode(Track::SynthB, Mode::Midi));
    auto aOn = event(MusicalEventType::NoteOn,
                     MusicalEventSource::Arpeggiator,
                     MusicalEventTarget::SynthA, 62);
    auto bOn = event(MusicalEventType::NoteOn,
                     MusicalEventSource::Arpeggiator,
                     MusicalEventTarget::SynthB, 64);
    assert(!queue.tryPush(aOn));
    assert(queue.tryPush(bOn));
    assert(queue.tryPop(popped));

    // Drums use the same explicit external truth table, lane by lane.
    auto drumOn = event(MusicalEventType::NoteOn,
                        MusicalEventSource::PerformanceKeyboard,
                        MusicalEventTarget::Drums, 60, 5);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Internal));
    assert(!queue.tryPush(drumOn));
    assert(queue.approximateSize() == 0);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Midi));
    assert(queue.tryPush(drumOn));
    assert(queue.tryPop(popped));
    assert(popped.target == MusicalEventTarget::Drums);
    assert(popped.channel == 5);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Layer));
    assert(queue.tryPush(drumOn));
    assert(queue.tryPop(popped));

    // DX is outside the ownership axis.
    auto dxOn = event(MusicalEventType::NoteOn,
                      MusicalEventSource::PerformanceKeyboard,
                      MusicalEventTarget::Dx, 67);
    assert(queue.tryPush(dxOn));
    assert(queue.tryPop(popped));
}

void expectPatternQueuePolicy() {
    resetLegacy();
    MusicalEventQueue queue;
    queue.setPhaseReader(&phaseReader, nullptr);
    queue.beginMidiRenderBlock(7, 512, 0.0f, 120.0f, 22050.0f, true);

    auto patternOn = event(MusicalEventType::NoteOn,
                           MusicalEventSource::PatternPlayer,
                           MusicalEventTarget::SynthA);
    ScheduledMusicalEvent scheduled{};

    // Legacy Pattern remains layered.
    assert(queue.tryPush(patternOn));
    assert(queue.tryPop(scheduled));

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    const uint32_t droppedBefore = queue.droppedNoteOnCount();
    assert(!queue.tryPush(patternOn));
    assert(queue.approximateSize() == 0);
    assert(queue.droppedNoteOnCount() == droppedBefore);

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Midi));
    assert(queue.tryPush(patternOn));
    assert(queue.tryPop(scheduled));

    // A NoteOff published after an ownership switch still reaches the queue.
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    auto patternOff = event(MusicalEventType::NoteOff,
                            MusicalEventSource::PatternPlayer,
                            MusicalEventTarget::SynthA);
    assert(queue.tryPush(patternOff));
    assert(queue.tryPop(scheduled));

    const uint32_t generationBefore = queue.generationFor(
        MusicalEventTarget::SynthA);
    auto patternPanic = event(MusicalEventType::AllNotesOff,
                              MusicalEventSource::PatternPlayer,
                              MusicalEventTarget::SynthA);
    assert(queue.tryPush(patternPanic));
    assert(queue.generationFor(MusicalEventTarget::SynthA) ==
           generationBefore + 1u);

    auto drumOn = event(MusicalEventType::NoteOn,
                        MusicalEventSource::PatternPlayer,
                        MusicalEventTarget::Drums, 60, 7);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Internal));
    assert(!queue.tryPush(drumOn));
    assert(queue.approximateSize() == 0);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Midi));
    assert(queue.tryPush(drumOn));
    assert(queue.tryPop(scheduled));
    assert(scheduled.event.target == MusicalEventTarget::Drums);
    assert(scheduled.event.channel == 7);
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Layer));
    assert(queue.tryPush(drumOn));
    assert(queue.tryPop(scheduled));

    queue.endMidiRenderBlock();
}

void expectControlTransitionCleanup() {
    resetLegacy();
    MidiControlEventQueue queue;
    MusicalEvent popped{};
    (void)queue.takePendingAllNotesOffMask();  // sync prior test epochs

    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Layer));
    auto noteOn = event(MusicalEventType::NoteOn,
                        MusicalEventSource::PerformanceKeyboard,
                        MusicalEventTarget::SynthA, 69);
    assert(queue.tryPush(noteOn));

    // Disable MIDI before the queued NoteOn reaches the sole USB owner.
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    const uint8_t panic = queue.takePendingAllNotesOffMask();
    assert((panic & MidiControlEventQueue::kSynthAMask) != 0u);
    assert((queue.takePendingAllNotesOffMask() &
            MidiControlEventQueue::kSynthAMask) == 0u);
    // Stale pre-switch NoteOn is discarded at consumer time.
    assert(!queue.tryPop(popped));

    // Enabling MIDI does not request cleanup. Removing it again does.
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Midi));
    (void)queue.takePendingAllNotesOffMask();
    assert(GroovePuterOutput::setMode(Track::SynthA, Mode::Internal));
    assert((queue.takePendingAllNotesOffMask() &
            MidiControlEventQueue::kSynthAMask) != 0u);
}

void expectPatternTransitionCleanup() {
    resetLegacy();
    MusicalEventQueue queue;
    queue.setPhaseReader(&phaseReader, nullptr);
    queue.beginMidiRenderBlock(11, 512, 0.0f, 120.0f, 22050.0f, true);
    ScheduledMusicalEvent scheduled{};
    (void)queue.takePendingAllNotesOffMask();  // sync prior test epochs

    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Layer));
    auto drumOn = event(MusicalEventType::NoteOn,
                        MusicalEventSource::PatternPlayer,
                        MusicalEventTarget::Drums, 60, 3);
    assert(queue.tryPush(drumOn));
    assert(GroovePuterOutput::setMode(Track::Drums, Mode::Internal));

    const uint8_t panic = queue.takePendingAllNotesOffMask();
    assert((panic & ScheduledMusicalEventQueue::kDrumsMask) != 0u);
    assert((queue.takePendingAllNotesOffMask() &
            ScheduledMusicalEventQueue::kDrumsMask) == 0u);
    assert(!queue.tryPop(scheduled));

    queue.endMidiRenderBlock();
}

}  // namespace

int main() {
    expectControlQueuePolicy();
    expectPatternQueuePolicy();
    expectControlTransitionCleanup();
    expectPatternTransitionCleanup();
    std::cout << "Output ownership queue tests: PASS\n";
    return 0;
}
