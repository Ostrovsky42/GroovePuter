#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "src/dsp/miniacid_engine.h"
#include "src/input/internal_synth_output.h"
#include "src/output/output_ownership.h"

namespace {

MusicalEvent event(MusicalEventType type,
                   MusicalEventSource source,
                   MusicalEventTarget target,
                   uint8_t note = 0,
                   uint8_t velocity = 0) {
    MusicalEvent value{};
    value.type = type;
    value.source = source;
    value.target = target;
    value.note = note;
    value.velocity = velocity;
    return value;
}

std::size_t countCalls(const MiniAcid& engine,
                       MiniAcid::CallType type,
                       int voice) {
    std::size_t count = 0;
    for (const MiniAcid::Call& call : engine.calls) {
        if (call.type == type && call.voice == voice) ++count;
    }
    return count;
}

void setInternalPerformanceOutput() {
    GroovePuterOutput::setMode(
        GroovePuterOutput::Track::SynthA,
        GroovePuterOutput::Mode::Internal);
    GroovePuterOutput::setMode(
        GroovePuterOutput::Track::SynthB,
        GroovePuterOutput::Mode::Internal);
}

void testTargetScopedCleanupAndIsolation() {
    setInternalPerformanceOutput();

    MiniAcid engine;
    AudioMutationGate mutationGate;
    InternalSynthOutput sink(engine, mutationGate);
    MusicalEventRouter router;
    assert(router.addSink(sink));

    router.route(event(MusicalEventType::NoteOn,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA,
                       60,
                       100));
    router.route(event(MusicalEventType::NoteOn,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthB,
                       67,
                       100));

    assert(engine.liveNote[0] == 60);
    assert(engine.liveNote[1] == 67);
    assert(countCalls(engine, MiniAcid::CallType::NoteOn, 0) == 1);
    assert(countCalls(engine, MiniAcid::CallType::NoteOn, 1) == 1);

    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA));

    assert(engine.liveNote[0] == -1);
    assert(engine.liveNote[1] == 67);
    assert(countCalls(engine, MiniAcid::CallType::NoteOff, 0) == 1);
    assert(countCalls(engine, MiniAcid::CallType::NoteOff, 1) == 0);

    // A second target cleanup cannot release anything else: the authoritative
    // Synth A candidate and projected identity were both cleared by the first.
    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA));
    assert(countCalls(engine, MiniAcid::CallType::NoteOff, 0) == 1);
    assert(engine.liveNote[1] == 67);

    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthB));
    assert(engine.liveNote[1] == -1);
    assert(countCalls(engine, MiniAcid::CallType::NoteOff, 1) == 1);
}

void testExternalPolyDoesNotEnterInternalMonoCleanup() {
    setInternalPerformanceOutput();

    MiniAcid engine;
    AudioMutationGate mutationGate;
    InternalSynthOutput sink(engine, mutationGate);
    MusicalEventRouter router;
    assert(router.addSink(sink));

    router.route(event(MusicalEventType::NoteOn,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA,
                       60,
                       100));
    const std::size_t callCountBeforePoly = engine.calls.size();

    router.route(event(MusicalEventType::NoteOn,
                       MusicalEventSource::PerformanceKeyboardPoly,
                       MusicalEventTarget::SynthA,
                       72,
                       100));
    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboardPoly,
                       MusicalEventTarget::SynthA));

    assert(engine.calls.size() == callCountBeforePoly);
    assert(engine.liveNote[0] == 60);

    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA));
    assert(engine.liveNote[0] == -1);
}

void testCleanupWhilePatternOwnsCannotResurrectPerformanceCandidate() {
    setInternalPerformanceOutput();

    MiniAcid engine;
    AudioMutationGate mutationGate;
    InternalSynthOutput sink(engine, mutationGate);
    MusicalEventRouter router;
    assert(router.addSink(sink));

    router.route(event(MusicalEventType::NoteOn,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA,
                       62,
                       100));
    assert(engine.liveNote[0] == 62);
    assert(countCalls(engine, MiniAcid::CallType::NoteOn, 0) == 1);

    engine.patternOwned[0] = true;
    sink.syncPatternOwnership();
    assert(engine.liveNote[0] == -1);
    assert(countCalls(engine, MiniAcid::CallType::SuspendLiveProjection, 0) == 1);

    // The direct candidate is now hidden behind Pattern ownership. Cleanup must
    // delete that authoritative candidate, not merely silence a projection.
    router.route(event(MusicalEventType::AllNotesOff,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::SynthA));

    const std::size_t noteOnsBeforePatternRelease =
        countCalls(engine, MiniAcid::CallType::NoteOn, 0);
    engine.patternOwned[0] = false;
    sink.syncPatternOwnership();

    assert(countCalls(engine, MiniAcid::CallType::NoteOn, 0) ==
           noteOnsBeforePatternRelease);
    assert(engine.liveNote[0] == -1);
}

}  // namespace

int main() {
    testTargetScopedCleanupAndIsolation();
    testExternalPolyDoesNotEnterInternalMonoCleanup();
    testCleanupWhilePatternOwnsCannotResurrectPerformanceCandidate();

    std::cout << "InternalSynthOutput target cleanup: PASS\n";
    std::cout << "InternalSynthOutput Synth A/B isolation: PASS\n";
    std::cout << "PerformanceKeyboardPoly external ownership: PASS\n";
    std::cout << "InternalSynthOutput no resurrection: PASS\n";
    return 0;
}
