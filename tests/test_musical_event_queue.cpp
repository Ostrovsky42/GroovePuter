#include <cassert>
#include <cmath>
#include <cstdint>

#include "src/input/musical_event_queue.h"

namespace {

struct FakeSequencerPhase {
    float phaseSteps{0.0f};
};

float readPhase(void* context) {
    return static_cast<FakeSequencerPhase*>(context)->phaseSteps;
}

MusicalEvent event(MusicalEventType type,
                   MusicalEventTarget target,
                   uint8_t note) {
    return MusicalEvent{
        type,
        MusicalEventSource::PatternPlayer,
        target,
        0,
        note,
        100,
    };
}

void testRenderPhaseBecomesFrameOffset() {
    MusicalEventQueue queue;
    FakeSequencerPhase phase{};
    queue.setPhaseReader(readPhase, &phase);

    static_assert(MusicalEventQueue::kStorageSize == 128);
    static_assert(MusicalEventQueue::kCapacity == 127);

    constexpr float sampleRate = 22050.0f;
    constexpr float bpm = 120.0f;
    constexpr float samplesPerStep = (sampleRate * 60.0f) / (bpm * 4.0f);

    queue.beginMidiRenderBlock(9, 512, 0.0f, bpm, sampleRate, true);
    phase.phaseSteps = 33.0f / samplesPerStep;
    assert(queue.tryPush(event(MusicalEventType::NoteOn,
                               MusicalEventTarget::SynthA, 36)));
    phase.phaseSteps = 97.0f / samplesPerStep;
    assert(queue.tryPush(event(MusicalEventType::NoteOff,
                               MusicalEventTarget::SynthA, 36)));
    queue.endMidiRenderBlock();

    ScheduledMusicalEvent first{};
    ScheduledMusicalEvent second{};
    assert(queue.tryPop(first));
    assert(queue.tryPop(second));
    assert(first.blockSequence == 9);
    assert(first.frameOffset == 32);
    assert(second.frameOffset == 96);
    assert(first.publicationSequence < second.publicationSequence);
    assert(!queue.tryPop(first));
}

void testForcedTransportStartMapsToFrameZero() {
    MusicalEventQueue queue;
    FakeSequencerPhase phase{};
    queue.setPhaseReader(readPhase, &phase);

    constexpr float sampleRate = 22050.0f;
    constexpr float bpm = 100.0f;
    constexpr float samplesPerStep = (sampleRate * 60.0f) / (bpm * 4.0f);

    const float forcedStartPhase = 15.0f + (23.0f / 24.0f);
    queue.beginMidiRenderBlock(
        3, 512, forcedStartPhase, bpm, sampleRate, true);
    phase.phaseSteps = 1.0f / samplesPerStep;
    assert(queue.tryPush(event(MusicalEventType::NoteOn,
                               MusicalEventTarget::SynthA, 48)));
    queue.endMidiRenderBlock();

    ScheduledMusicalEvent scheduled{};
    assert(queue.tryPop(scheduled));
    assert(scheduled.blockSequence == 3);
    assert(scheduled.frameOffset == 0);
}

void testOutsideRenderIsSuppressedAndCleansUp() {
    MusicalEventQueue queue;
    assert(!queue.tryPush(event(MusicalEventType::NoteOn,
                                MusicalEventTarget::SynthA, 40)));
    assert(queue.suppressedNonRealtimeCount() == 1);
    assert(queue.takePendingAllNotesOffMask() == 0);

    assert(!queue.tryPush(event(MusicalEventType::NoteOff,
                                MusicalEventTarget::SynthB, 41)));
    assert(queue.suppressedNonRealtimeCount() == 2);
    assert(queue.generationFor(MusicalEventTarget::SynthB) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MusicalEventQueue::kSynthBMask);
}

void testBoundedOverflowContract() {
    MusicalEventQueue queue;
    for (std::size_t i = 0; i < MusicalEventQueue::kCapacity; ++i) {
        assert(queue.ScheduledMusicalEventQueue::tryPush(
            event(MusicalEventType::NoteOn,
                  MusicalEventTarget::SynthA,
                  static_cast<uint8_t>(24 + (i % 40))),
            20,
            static_cast<uint16_t>(i % 512)));
    }

    assert(!queue.ScheduledMusicalEventQueue::tryPush(
        event(MusicalEventType::NoteOff,
              MusicalEventTarget::SynthB, 48),
        21,
        0));
    assert(queue.droppedCount() == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MusicalEventQueue::kSynthBMask);

    queue.discardPending();
    assert(queue.approximateSize() == 0);
}

}  // namespace

int main() {
    testRenderPhaseBecomesFrameOffset();
    testForcedTransportStartMapsToFrameZero();
    testOutsideRenderIsSuppressedAndCleansUp();
    testBoundedOverflowContract();
    return 0;
}
