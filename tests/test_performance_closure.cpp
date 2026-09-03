#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/input/internal_synth_output.h"
#include "src/input/performance_keyboard.h"
#include "src/midi/project_transport_timeline.h"

namespace {

MusicalEvent event(MusicalEventType type,
                   MusicalEventSource source,
                   uint8_t note,
                   uint8_t velocity = 100) {
    return MusicalEvent{type,
                        source,
                        MusicalEventTarget::SynthA,
                        0,
                        note,
                        velocity};
}

void expectCandidate(const InternalSynthOutput::MonoArbitrationState::Candidate& candidate,
                     bool active,
                     MusicalEventSource source,
                     uint8_t note) {
    assert(candidate.active == active);
    if (!active) return;
    assert(candidate.source == source);
    assert(candidate.note == note);
}

void testArbitrationTransitions() {
    using State = InternalSynthOutput::MonoArbitrationState;

    // A1: GENERATED temporarily overrides DIRECT; DIRECT returns only because
    // its current candidate is still logically active.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 36));
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::Arpeggiator, 48));
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::Arpeggiator, 48);
        state.applyLiveEvent(event(MusicalEventType::NoteOff,
                                   MusicalEventSource::Arpeggiator, 48, 0));
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::PerformanceKeyboard, 36);
    }

    // A2: PATTERN suppresses projection but does not erase a still-active
    // GENERATED candidate.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::Arpeggiator, 52));
        state.setPatternOwned(true);
        expectCandidate(state.selectedCandidate(), false,
                        MusicalEventSource::MidiInput, 0);
        state.setPatternOwned(false);
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::Arpeggiator, 52);
    }

    // A3: a release received while PATTERN owns the voice must clear the
    // suppressed candidate. Releasing PATTERN must not resurrect stale DIRECT.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 40));
        state.setPatternOwned(true);
        state.applyLiveEvent(event(MusicalEventType::NoteOff,
                                   MusicalEventSource::PerformanceKeyboard, 40, 0));
        state.setPatternOwned(false);
        expectCandidate(state.selectedCandidate(), false,
                        MusicalEventSource::MidiInput, 0);
    }

    // A4: DIRECT outranks OTHER LIVE, then the still-active OTHER LIVE current
    // candidate returns after DIRECT releases.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::MidiInput, 31));
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 43));
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::PerformanceKeyboard, 43);
        state.applyLiveEvent(event(MusicalEventType::NoteOff,
                                   MusicalEventSource::PerformanceKeyboard, 43, 0));
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::MidiInput, 31);
    }

    // Candidate identity is current-state based, not a history stack: an old
    // NoteOff may not clear a newer note from the same source class.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 36));
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 38));
        state.applyLiveEvent(event(MusicalEventType::NoteOff,
                                   MusicalEventSource::PerformanceKeyboard, 36, 0));
        expectCandidate(state.selectedCandidate(), true,
                        MusicalEventSource::PerformanceKeyboard, 38);
    }

    // Manual POLY remains external-only and must not acquire an internal mono
    // candidate.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboardPoly, 60));
        expectCandidate(state.selectedCandidate(), false,
                        MusicalEventSource::MidiInput, 0);
    }

    // A5: panic clears every live candidate and PATTERN ownership. No later
    // priority re-evaluation may resurrect anything.
    {
        State state;
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::MidiInput, 31));
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::PerformanceKeyboard, 43));
        state.applyLiveEvent(event(MusicalEventType::NoteOn,
                                   MusicalEventSource::Arpeggiator, 55));
        state.setPatternOwned(true);
        state.panic();
        state.setPatternOwned(false);
        expectCandidate(state.selectedCandidate(), false,
                        MusicalEventSource::MidiInput, 0);
    }
}

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& value) override {
        events.push_back(value);
    }

    void clear() { events.clear(); }

    std::vector<MusicalEvent> events;
};

struct Fixture {
    MusicalEventRouter router;
    RecordingSink sink;
    PerformanceKeyboard keyboard;

    Fixture() : keyboard(router) {
        assert(router.addSink(sink));
    }
};

std::size_t countEvents(const std::vector<MusicalEvent>& events,
                        MusicalEventType type,
                        MusicalEventSource source) {
    std::size_t result = 0;
    for (const MusicalEvent& value : events) {
        if (value.type == type && value.source == source) ++result;
    }
    return result;
}

void testTransportCleanup() {
    // T1: delayed generated NoteOn cannot cross STOPPED -> PLAYING.
    {
        Fixture f;
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.keyboard.cycleStrum(1);  // 8 ms delayed chord voices.
        f.sink.clear();
        f.keyboard.service(1000000u);
        assert(f.keyboard.keyDown('a', 100));
        assert(countEvents(f.sink.events, MusicalEventType::NoteOn,
                           MusicalEventSource::Arpeggiator) == 1u);
        assert(f.keyboard.scheduledDepth() > 0u);

        f.keyboard.setTransportPlaying(true);
        assert(f.keyboard.scheduledDepth() == 0u);
        const std::size_t noteOns = countEvents(
            f.sink.events, MusicalEventType::NoteOn,
            MusicalEventSource::Arpeggiator);
        f.keyboard.service(1050000u);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOn,
                           MusicalEventSource::Arpeggiator) == noteOns);
    }

    // T2: generated notes already accepted by the router receive cleanup
    // NoteOff before the transport epoch changes.
    {
        Fixture f;
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 100));
        assert(countEvents(f.sink.events, MusicalEventType::NoteOn,
                           MusicalEventSource::Arpeggiator) == 3u);
        f.keyboard.setTransportPlaying(true);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOff,
                           MusicalEventSource::Arpeggiator) == 3u);
    }

    // T3: transport cleanup is generated-domain cleanup, not global panic.
    // A direct held key remains owned until its physical key-up.
    {
        Fixture f;
        assert(f.keyboard.keyDown('a', 96));
        f.sink.clear();
        f.keyboard.setTransportPlaying(true);
        assert(f.keyboard.heldCount() == 1u);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOff,
                           MusicalEventSource::PerformanceKeyboard) == 0u);
        assert(countEvents(f.sink.events, MusicalEventType::AllNotesOff,
                           MusicalEventSource::PerformanceKeyboard) == 0u);
        assert(f.keyboard.keyUp('a'));
        assert(countEvents(f.sink.events, MusicalEventType::NoteOff,
                           MusicalEventSource::PerformanceKeyboard) == 1u);
    }

    // T4: generated ARP/ratchet obligations cannot survive PLAYING -> STOPPED.
    {
        Fixture f;
        auto& timeline = GroovePuterMidi::projectTransportTimeline();
        timeline.resetPublisher();
        timeline.publishBlock(40u, 512u, 3.25f, 120.0f, 22050.0f, true);
        f.keyboard.setArpeggiatorEnabled(true);
        f.keyboard.cycleRatchet(1);  // x2
        f.keyboard.setTransportPlaying(true);
        f.sink.clear();
        f.keyboard.service(4000000u);
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.service(4000000u);

        // Advance the authoritative PROJECT timeline into the scheduler's
        // half-pulse preparation window. Advancing wall time alone does not
        // advance musical phase and therefore cannot materialize an ARP pulse.
        timeline.publishBlock(41u, 512u, 3.55f, 120.0f, 22050.0f, true);
        f.keyboard.service(4023219u);
        assert(f.keyboard.scheduledDepth() > 0u);

        // Service after the predicted 4.0-step boundary while the first x2
        // ratchet hit is active. This is a real generated NoteOn produced by
        // the production ARP path, not a synthetic event injected by the test.
        f.keyboard.service(4080000u);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOn,
                           MusicalEventSource::Arpeggiator) >= 1u);

        f.keyboard.setTransportPlaying(false);
        assert(f.keyboard.scheduledDepth() == 0u);
        assert(f.keyboard.heldCount() == 1u);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOff,
                           MusicalEventSource::Arpeggiator) >= 1u);

        // Remove live source material only after transport cleanup has been
        // observed. Otherwise later service() calls are allowed to start a new
        // standalone ARP pulse, which is fresh material rather than stale
        // transport work.
        assert(f.keyboard.keyUp('a'));
        const std::size_t afterStop = countEvents(
            f.sink.events, MusicalEventType::NoteOn,
            MusicalEventSource::Arpeggiator);
        f.keyboard.service(4300000u);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOn,
                           MusicalEventSource::Arpeggiator) == afterStop);
    }

    // T5: repeated transitions with no newly accepted generated notes must not
    // duplicate cleanup or strand a generated note.
    {
        Fixture f;
        f.keyboard.setChordMode(PerformanceChordMode::Major);
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 100));
        f.keyboard.setTransportPlaying(true);
        const std::size_t cleaned = countEvents(
            f.sink.events, MusicalEventType::NoteOff,
            MusicalEventSource::Arpeggiator);
        assert(cleaned == 3u);
        f.keyboard.setTransportPlaying(false);
        f.keyboard.setTransportPlaying(true);
        f.keyboard.setTransportPlaying(false);
        assert(countEvents(f.sink.events, MusicalEventType::NoteOff,
                           MusicalEventSource::Arpeggiator) == cleaned);
        assert(f.keyboard.scheduledDepth() == 0u);
    }
}

void testHeldPitchIdentity() {
    // ROOT: release uses the pitch resolved at keyDown; a future keyDown uses
    // the newly selected root.
    {
        Fixture f;
        f.keyboard.setRootPitchClass(0);
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 100));
        assert(f.sink.events.back().note == 36u);
        f.keyboard.setRootPitchClass(2);
        assert(f.keyboard.keyUp('a'));
        assert(f.sink.events.back().type == MusicalEventType::NoteOff);
        assert(f.sink.events.back().note == 36u);
        assert(f.keyboard.keyDown('a', 100));
        assert(f.sink.events.back().type == MusicalEventType::NoteOn);
        assert(f.sink.events.back().note == 38u);
    }

    // SCALE: same invariant for a degree whose pitch changes between NAT MINOR
    // and CHROMATIC.
    {
        Fixture f;
        f.keyboard.setRootPitchClass(0);
        f.keyboard.setScale(PerformanceScale::NaturalMinor);
        f.sink.clear();
        assert(f.keyboard.keyDown('s', 100));
        const uint8_t original = f.sink.events.back().note;
        f.keyboard.setScale(PerformanceScale::Chromatic);
        assert(f.keyboard.keyUp('s'));
        assert(f.sink.events.back().type == MusicalEventType::NoteOff);
        assert(f.sink.events.back().note == original);
        assert(f.keyboard.keyDown('s', 100));
        assert(f.sink.events.back().type == MusicalEventType::NoteOn);
        assert(f.sink.events.back().note != original);
    }

    // OCTAVE: held identity is immutable, while future presses use the new
    // octave shift.
    {
        Fixture f;
        f.sink.clear();
        assert(f.keyboard.keyDown('a', 100));
        const uint8_t original = f.sink.events.back().note;
        assert(f.keyboard.shiftOctave(1));
        assert(f.keyboard.keyUp('a'));
        assert(f.sink.events.back().type == MusicalEventType::NoteOff);
        assert(f.sink.events.back().note == original);
        assert(f.keyboard.keyDown('a', 100));
        assert(f.sink.events.back().type == MusicalEventType::NoteOn);
        assert(f.sink.events.back().note == static_cast<uint8_t>(original + 12u));
    }
}

}  // namespace

int main() {
    testArbitrationTransitions();
    testTransportCleanup();
    testHeldPitchIdentity();
    return 0;
}
