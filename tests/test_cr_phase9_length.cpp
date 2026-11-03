// CALL/RESPONSE phase 9: PARTNER LENGTH (replaces PARTNER DELAY).
//  - every answer note length is multiplied by 100/75/50/25 % and rounded up
//    to a whole step, never below one step; onsets, pitches and velocities
//    do not change;
//  - it applies to ANSWER, DUET and LOOP, including every LOOP replay;
//  - NoteOn/NoteOff stay balanced, and a cancel still releases a sounding
//    (shortened) note at once.
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"

using namespace GroovePuterCallResponse;
using Close = PhraseCapture::CloseResult;
using Listener = CallResponseListener;

namespace {

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint32_t kSixteenth = 125000;  // 120 BPM
constexpr uint32_t kT0 = 1000;
uint32_t at(double step) { return kT0 + static_cast<uint32_t>(step * kSixteenth + 0.5); }

constexpr AnswerLength kLengths[] = {AnswerLength::Full, AnswerLength::ThreeQuarters,
                                     AnswerLength::Half, AnswerLength::Quarter};

// The rule, written independently of the implementation.
uint8_t expectedLength(uint8_t steps, uint8_t percent) {
    const uint32_t scaled = (static_cast<uint32_t>(steps) * percent + 99u) / 100u;
    return static_cast<uint8_t>(scaled < 1u ? 1u : scaled);
}

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
        times.push_back(gNow);
    }
    std::vector<MusicalEvent> events;
    std::vector<uint32_t> times;
};

struct Rig {
    MusicalEventRouter router;
    RecordingSink sink;
    Listener listener;
    std::vector<ResponsePlan> plans;
    std::vector<uint16_t> cycles;
    double cursor{0.0};

    Rig(Listener::Mode mode, AnswerLength length) {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(mode);
        listener.setAnswerLength(length);
    }
    void serviceTo(double step) {
        gNow = at(step);
        PhraseCapture::Phrase phrase;
        if (listener.service(gNow, TransportView{}, 120.0f, phrase) != Close::Phrase) return;
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        ResponsePlan plan;
        if (listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan)) {
            plans.push_back(plan);
            cycles.push_back(listener.loopCycle());
        }
    }
    void runTo(double step) {
        while (cursor < step) {
            cursor += 0.125;
            serviceTo(cursor);
        }
    }
    void note(double on, double off, uint8_t pitch) {
        runTo(on);
        gNow = at(on);
        listener.onPerformanceInputNote(true, 'a', pitch, 100);
        runTo(off);
        gNow = at(off);
        listener.onPerformanceInputNote(false, 'a', pitch, 0);
    }
    // Long notes (lengths 7, 4, 2, 1, 5, 3 steps after the mono trim) so every
    // LENGTH rounds differently; the last one ends on the window end.
    void playCall() {
        note(0.0, 7.0, 60);
        note(8.0, 12.0, 62);
        note(12.0, 14.0, 64);
        note(16.0, 17.0, 65);
        note(20.0, 25.0, 67);
        note(28.0, 31.0, 64);
    }
    // NoteOn time -> NoteOff time pairs, in NoteOn order.
    struct Sounded { uint8_t note; uint32_t on; uint32_t off; };
    std::vector<Sounded> sounded() const {
        std::vector<Sounded> out;
        for (std::size_t i = 0; i < sink.events.size(); ++i) {
            if (sink.events[i].type != MusicalEventType::NoteOn) continue;
            Sounded s{sink.events[i].note, sink.times[i], 0};
            for (std::size_t j = i + 1; j < sink.events.size(); ++j) {
                if (sink.events[j].type == MusicalEventType::NoteOff && sink.events[j].note == s.note) {
                    s.off = sink.times[j];
                    break;
                }
            }
            out.push_back(s);
        }
        return out;
    }
};

void assertBalanced(const std::vector<MusicalEvent>& events) {
    int sounding[128]{};
    for (const MusicalEvent& e : events) {
        if (e.type == MusicalEventType::NoteOn) {
            assert(sounding[e.note] == 0);
            ++sounding[e.note];
        } else if (e.type == MusicalEventType::NoteOff) {
            assert(sounding[e.note] == 1);
            --sounding[e.note];
        }
    }
    for (int n : sounding) assert(n == 0);
}

void testRuleAndSetting() {
    assert(answerLengthPercent(AnswerLength::Full) == 100);
    assert(answerLengthPercent(AnswerLength::ThreeQuarters) == 75);
    assert(answerLengthPercent(AnswerLength::Half) == 50);
    assert(answerLengthPercent(AnswerLength::Quarter) == 25);
    for (AnswerLength length : kLengths) {
        const uint8_t percent = answerLengthPercent(length);
        for (int steps = 0; steps <= 64; ++steps) {
            const uint8_t got = scaledAnswerNoteLength(static_cast<uint8_t>(steps), percent);
            assert(got == expectedLength(static_cast<uint8_t>(steps), percent));
            assert(got >= 1);
            if (steps >= 1) assert(got <= steps);  // never longer: the line stays monophonic
        }
    }
    // Examples from the spec: 1 step stays 1; 3 -> 3/2/1; 7 -> 6/4/2.
    assert(scaledAnswerNoteLength(1, 25) == 1 && scaledAnswerNoteLength(1, 75) == 1);
    assert(scaledAnswerNoteLength(3, 75) == 3 && scaledAnswerNoteLength(3, 50) == 2 &&
           scaledAnswerNoteLength(3, 25) == 1);
    assert(scaledAnswerNoteLength(7, 75) == 6 && scaledAnswerNoteLength(7, 50) == 4 &&
           scaledAnswerNoteLength(7, 25) == 2);
    assert(scaledAnswerNoteLength(64, 25) == 16 && scaledAnswerNoteLength(64, 100) == 64);

    Listener listener;
    assert(listener.answerLength() == AnswerLength::Full && listener.answerLengthPercent() == 100);
    const uint8_t forward[] = {75, 50, 25, 100};
    for (uint8_t expected : forward) {
        listener.cycleAnswerLength(1);
        assert(listener.answerLengthPercent() == expected);
    }
    const uint8_t backward[] = {25, 50, 75, 100};
    for (uint8_t expected : backward) {
        listener.cycleAnswerLength(-1);
        assert(listener.answerLengthPercent() == expected);
    }
    listener.setAnswerLength(static_cast<AnswerLength>(9));
    assert(listener.answerLength() == AnswerLength::Full);
    // A mode change keeps LENGTH (session setting, like the other PARTNER rows).
    listener.setAnswerLength(AnswerLength::Half);
    listener.setMode(Listener::Mode::Loop);
    listener.setMode(Listener::Mode::Answer);
    assert(listener.answerLength() == AnswerLength::Half);
}

// applyAnswerLength alone: lengths by the rule, everything else untouched.
void testApplyToPlan() {
    ResponsePlan full{};
    full.windowSteps = 64;
    const uint8_t lengths[] = {1, 2, 3, 4, 5, 7, 8, 13, 16, 0};
    uint8_t step = 0;
    for (uint8_t i = 0; i < 10; ++i) {
        full.notes[i] = ResponseNote{step, lengths[i], static_cast<uint8_t>(40 + i), static_cast<uint8_t>(60 + i)};
        step = static_cast<uint8_t>(step + (lengths[i] ? lengths[i] : 1));
        ++full.count;
    }
    for (AnswerLength length : kLengths) {
        ResponsePlan plan = full;
        applyAnswerLength(plan, length);
        assert(plan.count == full.count && plan.windowSteps == full.windowSteps &&
               plan.droppedTail == full.droppedTail);
        for (uint8_t i = 0; i < plan.count; ++i) {
            assert(plan.notes[i].step == full.notes[i].step);
            assert(plan.notes[i].note == full.notes[i].note);
            assert(plan.notes[i].velocity == full.notes[i].velocity);
            assert(plan.notes[i].length == expectedLength(full.notes[i].length, answerLengthPercent(length)));
            assert(plan.notes[i].length >= 1);
        }
        assert(plan.onsetMask() == full.onsetMask());
    }
}

// ANSWER and DUET at every LENGTH against the same rig at 100 %: same reply,
// same NoteOn times; NoteOff after exactly the scaled length.
void testAnswerAndDuetEveryLength() {
    for (Listener::Mode mode : {Listener::Mode::Answer, Listener::Mode::Duet}) {
        Rig reference(mode, AnswerLength::Full);
        reference.playCall();
        reference.runTo(66.0);
        assert(reference.plans.size() == 1);
        const ResponsePlan& full = reference.plans[0];
        assert(full.count >= 3);
        const auto fullSounded = reference.sounded();
        assert(fullSounded.size() == full.count);
        for (AnswerLength length : kLengths) {
            const uint8_t percent = answerLengthPercent(length);
            Rig rig(mode, length);
            rig.playCall();
            rig.runTo(66.0);
            assert(rig.plans.size() == 1);
            const ResponsePlan& plan = rig.plans[0];
            assert(plan.count == full.count);
            bool shorter = false;
            for (uint8_t i = 0; i < plan.count; ++i) {
                assert(plan.notes[i].step == full.notes[i].step);
                assert(plan.notes[i].note == full.notes[i].note);
                assert(plan.notes[i].velocity == full.notes[i].velocity);
                assert(plan.notes[i].length == expectedLength(full.notes[i].length, percent));
                shorter = shorter || plan.notes[i].length < full.notes[i].length;
            }
            assert(shorter == (percent < 100));
            const auto sounded = rig.sounded();
            assert(sounded.size() == plan.count);
            for (uint8_t i = 0; i < plan.count; ++i) {
                assert(sounded[i].note == fullSounded[i].note);
                assert(sounded[i].on == fullSounded[i].on);  // onsets unchanged
                assert(sounded[i].off != 0);
                const double steps = static_cast<double>(sounded[i].off - sounded[i].on) / kSixteenth;
                // Within one service pass (1/8 step) of the scaled length.
                assert(steps >= plan.notes[i].length - 0.01 && steps <= plan.notes[i].length + 0.13);
            }
            assertBalanced(rig.sink.events);
            if (mode == Listener::Mode::Answer && length == AnswerLength::Half) {
                std::printf("ANSWER 50 %%: lengths");
                for (uint8_t i = 0; i < plan.count; ++i)
                    std::printf(" %u->%u", full.notes[i].length, plan.notes[i].length);
                std::printf("\n");
            }
        }
    }
}

// LOOP, LENGTH 50 %: the first reply and every replay keep the scaled length;
// onsets match the 100 % loop; NoteOn/NoteOff balanced, OFF leaves nothing.
void testLoopHalfLength() {
    for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
        Rig reference(Listener::Mode::Loop, AnswerLength::Full);
        Rig rig(Listener::Mode::Loop, AnswerLength::Half);
        for (Rig* r : {&reference, &rig}) {
            r->listener.setLoopEvolve(evolve);
            r->playCall();
            r->runTo(32.0 * 7 - 1.0);  // the reply to the question + 5 replays
        }
        assert(rig.plans.size() == 6 && reference.plans.size() == 6);
        for (std::size_t r = 0; r < rig.plans.size(); ++r) {
            assert(rig.cycles[r] == r && reference.cycles[r] == r);
            const ResponsePlan& full = reference.plans[r];
            const ResponsePlan& half = rig.plans[r];
            assert(half.count == full.count && half.count > 0);
            for (uint8_t i = 0; i < half.count; ++i) {
                assert(half.notes[i].step == full.notes[i].step);
                assert(half.notes[i].note == full.notes[i].note);
                assert(half.notes[i].velocity == full.notes[i].velocity);
                assert(half.notes[i].length == expectedLength(full.notes[i].length, 50));
            }
        }
        const auto a = reference.sounded();
        const auto b = rig.sounded();
        assert(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) assert(a[i].on == b[i].on && a[i].note == b[i].note);
        rig.listener.setMode(Listener::Mode::Off);
        reference.listener.setMode(Listener::Mode::Off);
        assertBalanced(rig.sink.events);
        assertBalanced(reference.sink.events);
        if (evolve == 1) {
            std::printf("LOOP 50 %%: %zu replies, %zu notes, pass 3 lengths", rig.plans.size(), b.size());
            for (uint8_t i = 0; i < rig.plans[3].count; ++i) std::printf(" %u", rig.plans[3].notes[i].length);
            std::printf("\n");
        }
    }
}

// Cancels at a shortened length: the sounding note is released at once
// (one NoteOff, right there), nothing pending follows, balance holds.
void testCancelAtLength() {
    // ANSWER, 25 %: the first answer note (7 steps -> 2) sounds 32..34; the
    // person plays at 33 -> NoteOff now, the rest never sounds.
    {
        Rig rig(Listener::Mode::Answer, AnswerLength::Quarter);
        rig.playCall();
        rig.runTo(33.0);
        assert(rig.plans.size() == 1 && rig.plans[0].notes[0].length == 2);
        assert(rig.listener.responder().soundingCount() == 1);
        const std::size_t before = rig.sink.events.size();
        gNow = at(33.0);
        rig.listener.onPerformanceInputNote(true, 'z', 72, 100);
        assert(rig.sink.events.size() == before + 1);
        assert(rig.sink.events.back().type == MusicalEventType::NoteOff);
        assert(rig.sink.times.back() == at(33.0));
        assert(!rig.listener.answering() && rig.listener.humanCancels() == 1);
        gNow = at(33.5);
        rig.listener.onPerformanceInputNote(false, 'z', 72, 0);
        const std::size_t afterCancel = rig.sink.events.size();
        rig.runTo(63.0);
        assert(rig.sink.events.size() == afterCancel);
        assertBalanced(rig.sink.events);
    }
    // ANSWER, 25 %: between two shortened notes nothing sounds; a cancel
    // there sends no NoteOff at all (nothing to release) and drops the rest.
    {
        Rig rig(Listener::Mode::Answer, AnswerLength::Quarter);
        rig.playCall();
        rig.runTo(35.0);  // note 0 ended at 34, note 1 starts at 40
        assert(rig.listener.answering() && rig.listener.responder().soundingCount() == 0);
        const std::size_t before = rig.sink.events.size();
        gNow = at(35.0);
        rig.listener.onPerformanceInputNote(true, 'z', 72, 100);
        assert(rig.sink.events.size() == before && !rig.listener.answering());
        gNow = at(35.5);
        rig.listener.onPerformanceInputNote(false, 'z', 72, 0);
        rig.runTo(63.0);
        assert(rig.sink.events.size() == before);
        assertBalanced(rig.sink.events);
    }
    // LOOP, 50 %: leaving LOOP during a replay note releases it at once.
    {
        Rig rig(Listener::Mode::Loop, AnswerLength::Half);
        rig.playCall();
        rig.runTo(65.0);  // replay 1: note 0 (7 -> 4 steps) sounds 64..68
        assert(rig.cycles.back() == 1 && rig.listener.responder().soundingCount() == 1);
        const std::size_t before = rig.sink.events.size();
        gNow = at(65.0);
        rig.listener.setMode(Listener::Mode::Off);
        assert(rig.sink.events.size() == before + 1 &&
               rig.sink.events.back().type == MusicalEventType::NoteOff);
        assertBalanced(rig.sink.events);
    }
    // LOOP, X (silence) on a shortened replay note.
    {
        Rig rig(Listener::Mode::Loop, AnswerLength::Half);
        rig.playCall();
        rig.runTo(97.0);
        assert(rig.listener.responder().soundingCount() == 1);
        const std::size_t before = rig.sink.events.size();
        gNow = at(97.0);
        rig.listener.silence();
        assert(rig.sink.events.size() == before + 1 &&
               rig.sink.events.back().type == MusicalEventType::NoteOff);
        rig.runTo(160.0);
        assert(rig.sink.events.size() == before + 1);  // the loop is forgotten
        assertBalanced(rig.sink.events);
    }
    // LENGTH changed while a reply sounds: the sounding reply keeps its
    // already timed NoteOffs; the next reply uses the new length.
    {
        Rig rig(Listener::Mode::Loop, AnswerLength::Full);
        rig.playCall();
        rig.runTo(33.0);
        rig.listener.cycleAnswerLength(1);  // 75 %
        rig.runTo(64.0 - 0.5);
        const auto first = rig.sounded();
        assert(first.size() == rig.plans[0].count);
        assert(first[0].off - first[0].on == static_cast<uint32_t>(rig.plans[0].notes[0].length) * kSixteenth);
        assert(rig.plans[0].notes[0].length == 7);
        rig.runTo(95.0);
        assert(rig.plans.size() == 2 && rig.plans[1].notes[0].length == expectedLength(7, 75));
        rig.listener.setMode(Listener::Mode::Off);
        assertBalanced(rig.sink.events);
    }
}

}  // namespace

int main() {
    testRuleAndSetting();
    testApplyToPlan();
    testAnswerAndDuetEveryLength();
    testLoopHalfLength();
    testCancelAtLength();
    std::printf("sizeof Listener=%zu sizeof ResponsePlan=%zu\n", sizeof(CallResponseListener),
                sizeof(ResponsePlan));
    std::puts("cr phase 9 length tests passed");
    return 0;
}
