// CALL/RESPONSE phase 5: LOOP on the grid and the REPLACE threshold.
//  - every LOOP reply starts on the window downbeat and keeps its last-step
//    note (phase 5: replays; phase 7: also the first reply after a new loop
//    phrase; phase 9: PARTNER DELAY removed, every mode is on the grid).
//  - A closed window replaces the saved loop only with enough notes
//    (REPLACE ANY / 3+ / HALF); fewer notes replay the loop and are reported
//    through service(heard).
//  - ANSWER and DUET are not affected by REPLACE; NoteOn/NoteOff stay balanced
//    and every exit releases a sounding replay note.
#include <cassert>
#include <cstdio>
#include <string_view>
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

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
        times.push_back(gNow);
    }
    std::vector<MusicalEvent> events;
    std::vector<uint32_t> times;
};

struct Reply {
    int32_t windowStart;
    uint16_t cycle;
    uint8_t heardCount;
    PhraseCapture::Phrase phrase;
    ResponsePlan plan;
};

struct Rig {
    MusicalEventRouter router;
    RecordingSink sink;
    Listener listener;
    std::vector<Reply> replies;
    double cursor{0.0};

    explicit Rig(Listener::Mode mode) {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(mode);
    }
    void serviceTo(double step) {
        gNow = at(step);
        PhraseCapture::Phrase phrase;
        PhraseCapture::Phrase heard;
        if (listener.service(gNow, TransportView{}, 120.0f, phrase, &heard) != Close::Phrase) return;
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        ResponsePlan plan;
        if (listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan)) {
            replies.push_back(Reply{phrase.windowStartStep, listener.loopCycle(), heard.count, phrase, plan});
            heardLog.push_back(heard);
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
    std::vector<double> noteOnSteps(double from, double to) const {
        std::vector<double> steps;
        for (std::size_t i = 0; i < sink.events.size(); ++i) {
            if (sink.events[i].type != MusicalEventType::NoteOn) continue;
            const double step = static_cast<double>(sink.times[i] - kT0) / kSixteenth;
            if (step >= from && step < to) steps.push_back(step);
        }
        return steps;
    }
    std::vector<PhraseCapture::Phrase> heardLog;
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

// Notes on 0, 8 and the last step 31 of a 2-bar window.
void playGridCall(Rig& rig) {
    rig.note(0.0, 2.0, 60);
    rig.note(8.0, 10.0, 64);
    rig.note(31.0, 31.6, 67);
}

void testLoopRepeatsOnTheGrid() {
    for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
        Rig rig(Listener::Mode::Loop);
        rig.listener.setLoopEvolve(evolve);
        playGridCall(rig);
        rig.runTo(127.0);
        if (rig.replies.size() != 3) std::printf("evolve=%u replies=%zu\n", evolve, rig.replies.size());
        assert(rig.replies.size() == 3);
        // First reply after the question: on the grid as well, the onset-31
        // note kept.
        const Reply& first = rig.replies[0];
        assert(first.windowStart == 0 && first.cycle == 0 && first.plan.windowSteps == 32);
        assert(first.plan.droppedTail == 0 && first.plan.count == 3);
        // Two replays in a row: on the grid, all three notes, the last on step 31.
        for (int r = 1; r <= 2; ++r) {
            const Reply& replay = rig.replies[r];
            assert(replay.cycle == r && replay.heardCount == 0);
            assert(replay.windowStart == 32 * r);
            assert(replay.plan.windowSteps == 32);
            assert(replay.plan.droppedTail == 0 && replay.plan.count == 3);
            assert(replay.plan.notes[2].step == 31 && replay.plan.notes[2].length == 1);
        }
        const auto firstOns = rig.noteOnSteps(32.0, 64.0);
        assert(firstOns.size() == 3 && firstOns[0] == 32.0 && firstOns[1] == 40.0 && firstOns[2] == 63.0);
        for (int r = 2; r <= 3; ++r) {
            const double start = 32.0 * r;
            const auto ons = rig.noteOnSteps(start, start + 32.0);
            if (evolve < 3) {
                // Onsets exactly on the call's grid (EVOLVE 3 may shift its tail).
                assert(ons.size() == 3);
                assert(ons[0] == start && ons[1] == start + 8.0 && ons[2] == start + 31.0);
            } else {
                assert(ons.size() == 3 && ons[0] == start && ons[2] == start + 31.0);
            }
        }
        rig.runTo(130.0);
        rig.listener.setMode(Listener::Mode::Off);
        assertBalanced(rig.sink.events);
    }
}

// A 4-note loop: C D E F on 0/4/8/12.
void playFourNoteLoop(Rig& rig) {
    rig.note(0.0, 2.0, 60);
    rig.note(4.0, 6.0, 62);
    rig.note(8.0, 10.0, 64);
    rig.note(12.0, 14.0, 65);
}

void testReplaceThreePlusDefault() {
    Rig rig(Listener::Mode::Loop);
    assert(rig.listener.loopReplace() == Listener::LoopReplace::ThreePlus);
    assert(std::string_view(rig.listener.loopReplaceName()) == "3+");
    playFourNoteLoop(rig);
    rig.runTo(32.0);
    assert(rig.replies.size() == 1 && rig.replies[0].cycle == 0);
    // One stray note: the loop is kept and replayed on the grid.
    rig.note(40.0, 41.0, 72);
    rig.runTo(64.0);
    assert(rig.replies.size() == 2);
    const Reply& kept = rig.replies[1];
    assert(kept.phrase.count == 4 && kept.phrase.notes[0].note == 60 && kept.phrase.windowStartStep == 32);
    assert(kept.cycle == 1 && kept.plan.windowSteps == 32);
    assert(kept.heardCount == 1 && rig.heardLog[1].notes[0].note == 72 &&
           rig.heardLog[1].notes[0].onset == 8);
    // Two notes: still kept.
    rig.note(66.0, 67.0, 72);
    rig.note(70.0, 71.0, 74);
    rig.runTo(96.0);
    assert(rig.replies.size() == 3 && rig.replies[2].phrase.notes[0].note == 60);
    assert(rig.replies[2].cycle == 2 && rig.replies[2].heardCount == 2);
    // Three notes: the new loop, its first reply (on the grid).
    rig.note(96.0, 97.0, 67);
    rig.note(100.0, 101.0, 69);
    rig.note(104.0, 105.0, 71);
    rig.runTo(128.0);
    assert(rig.replies.size() == 4);
    const Reply& replaced = rig.replies[3];
    assert(replaced.phrase.count == 3 && replaced.phrase.notes[0].note == 67);
    assert(replaced.cycle == 0 && replaced.plan.windowSteps == 32 && replaced.heardCount == 0);  // phase 7
    // ...and that is what repeats afterwards.
    rig.runTo(160.0);
    assert(rig.replies.size() == 5 && rig.replies[4].phrase.notes[0].note == 67 &&
           rig.replies[4].cycle == 1);
    rig.listener.setMode(Listener::Mode::Off);
    assertBalanced(rig.sink.events);
}

// With no loop yet, any window with notes becomes the loop (even one note).
void testFirstLoopNeedsOneNote() {
    Rig rig(Listener::Mode::Loop);
    rig.note(0.0, 2.0, 60);
    rig.runTo(32.0);
    assert(rig.listener.hasLoopPhrase() && rig.replies.size() == 1 && rig.replies[0].phrase.count == 1);
}

void testReplaceHalf() {
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 0) == 1);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 1) == 1);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 2) == 1);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 5) == 3);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 6) == 3);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Half, 32) == 16);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::ThreePlus, 1) == 3);
    assert(Listener::loopReplaceMinNotes(Listener::LoopReplace::Any, 30) == 1);

    Rig rig(Listener::Mode::Loop);
    rig.listener.setLoopReplace(Listener::LoopReplace::Half);
    for (int i = 0; i < 6; ++i) rig.note(i * 4.0, i * 4.0 + 1.0, static_cast<uint8_t>(60 + i));
    rig.runTo(32.0);
    assert(rig.replies.back().phrase.count == 6);
    rig.note(36.0, 37.0, 72);
    rig.note(40.0, 41.0, 74);  // 2 < 6/2: kept
    rig.runTo(64.0);
    assert(rig.replies.back().phrase.count == 6 && rig.replies.back().heardCount == 2);
    rig.note(64.0, 65.0, 72);
    rig.note(68.0, 69.0, 74);
    rig.note(72.0, 73.0, 76);  // 3 >= 3: replaces
    rig.runTo(96.0);
    assert(rig.replies.back().phrase.count == 3 && rig.replies.back().phrase.notes[0].note == 72);
    assert(rig.replies.back().cycle == 0);
    rig.note(100.0, 101.0, 65);
    rig.note(104.0, 105.0, 67);  // 2 >= ceil(3/2): replaces a 3-note loop
    rig.runTo(128.0);
    assert(rig.replies.back().phrase.count == 2 && rig.replies.back().phrase.notes[0].note == 65);
    rig.note(130.0, 131.0, 69);  // 1 >= ceil(2/2): replaces a 2-note loop
    rig.runTo(160.0);
    assert(rig.replies.back().phrase.count == 1 && rig.replies.back().phrase.notes[0].note == 69);
    rig.listener.setMode(Listener::Mode::Off);
    assertBalanced(rig.sink.events);
}

void testReplaceAnyAsBefore() {
    Rig rig(Listener::Mode::Loop);
    rig.listener.setLoopReplace(Listener::LoopReplace::Any);
    playFourNoteLoop(rig);
    rig.note(40.0, 41.0, 72);
    rig.runTo(64.0);
    assert(rig.replies.size() == 2);
    assert(rig.replies[1].phrase.count == 1 && rig.replies[1].phrase.notes[0].note == 72);
    assert(rig.replies[1].cycle == 0 && rig.replies[1].heardCount == 0 && rig.replies[1].plan.windowSteps == 32);  // phase 7
    rig.runTo(96.0);
    assert(rig.replies[2].cycle == 1 && rig.replies[2].plan.windowSteps == 32);
}

void testReplaceCycle() {
    Listener listener;
    assert(listener.loopReplace() == Listener::LoopReplace::ThreePlus);
    listener.cycleLoopReplace(1);
    assert(listener.loopReplace() == Listener::LoopReplace::Half);
    listener.cycleLoopReplace(1);
    assert(listener.loopReplace() == Listener::LoopReplace::Any);
    listener.cycleLoopReplace(1);
    assert(listener.loopReplace() == Listener::LoopReplace::ThreePlus);
    listener.cycleLoopReplace(-1);
    assert(listener.loopReplace() == Listener::LoopReplace::Any);
    listener.setLoopReplace(static_cast<Listener::LoopReplace>(9));
    assert(listener.loopReplace() == Listener::LoopReplace::ThreePlus);
    assert(!listener.loopRepeat());
}

// ANSWER / DUET: REPLACE changes nothing (same events at the same times).
void testAnswerAndDuetIgnoreReplace() {
    for (Listener::Mode mode : {Listener::Mode::Answer, Listener::Mode::Duet}) {
        {
            std::vector<MusicalEvent> reference;
            std::vector<uint32_t> referenceTimes;
            for (Listener::LoopReplace rule : {Listener::LoopReplace::Any, Listener::LoopReplace::ThreePlus,
                                               Listener::LoopReplace::Half}) {
                Rig rig(mode);
                rig.listener.setLoopReplace(rule);
                rig.note(0.0, 2.0, 60);   // one-note CALL
                rig.runTo(70.0);
                rig.note(70.0, 72.0, 62);  // a second one-note CALL
                rig.runTo(140.0);
                assert(rig.replies.size() >= 2);
                for (const Reply& r : rig.replies) {
                    assert(r.cycle == 0 && r.heardCount == 0 && r.plan.windowSteps == 32);
                }
                if (reference.empty()) {
                    reference = rig.sink.events;
                    referenceTimes = rig.sink.times;
                } else {
                    assert(rig.sink.events.size() == reference.size());
                    for (std::size_t i = 0; i < reference.size(); ++i) {
                        assert(rig.sink.events[i].type == reference[i].type);
                        assert(rig.sink.events[i].note == reference[i].note);
                        assert(rig.sink.events[i].velocity == reference[i].velocity);
                        assert(rig.sink.times[i] == referenceTimes[i]);
                    }
                }
                assertBalanced(rig.sink.events);
            }
        }
    }
}

// Exits during a replay release the sounding note at once; balance holds.
void testReplayCancelAndBalance() {
    // Leaving LOOP (OFF) in the middle of the second replay.
    {
        Rig rig(Listener::Mode::Loop);
        rig.note(0.0, 6.0, 60);
        rig.runTo(66.0);  // second window replays the loop (cycle 1); note 64..70 sounds
        assert(rig.replies.back().cycle == 1 && rig.listener.responder().soundingCount() == 1);
        const std::size_t before = rig.sink.events.size();
        rig.listener.setMode(Listener::Mode::Off);
        assert(!rig.listener.answering() && rig.listener.loopCycle() == 0);
        assert(rig.sink.events.size() == before + 1 &&
               rig.sink.events.back().type == MusicalEventType::NoteOff);
        assertBalanced(rig.sink.events);
    }
    // BARS change during a replay: new epoch, loop and cycle dropped.
    {
        Rig rig(Listener::Mode::Loop);
        rig.note(0.0, 6.0, 60);
        rig.runTo(98.0);
        assert(rig.replies.back().cycle == 2 && rig.listener.responder().soundingCount() == 1);
        rig.listener.cycleAnswerBars(1);
        assert(!rig.listener.answering() && !rig.listener.hasLoopPhrase() && rig.listener.loopCycle() == 0);
        assertBalanced(rig.sink.events);
    }
    // The same pitch on the last and first step: NoteOff of the replay tail
    // comes before the NoteOn of the next replay on the same downbeat.
    {
        Rig rig(Listener::Mode::Loop);
        rig.note(0.0, 1.0, 60);
        rig.note(8.0, 9.0, 62);
        rig.note(31.0, 31.6, 60);
        rig.runTo(200.0);
        assert(rig.replies.size() == 6);
        for (std::size_t r = 1; r < rig.replies.size(); ++r) assert(rig.replies[r].plan.count == 3);
        rig.listener.setMode(Listener::Mode::Off);
        assertBalanced(rig.sink.events);
    }
    // Playing a new phrase over the loop (3+ notes) never doubles a note.
    {
        Rig rig(Listener::Mode::Loop);
        playGridCall(rig);
        rig.note(64.0, 66.0, 60);
        rig.note(72.0, 74.0, 64);
        rig.note(80.0, 82.0, 67);
        rig.runTo(160.0);
        rig.listener.setMode(Listener::Mode::Off);
        assertBalanced(rig.sink.events);
    }
}

}  // namespace

int main() {
    testLoopRepeatsOnTheGrid();
    testReplaceThreePlusDefault();
    testFirstLoopNeedsOneNote();
    testReplaceHalf();
    testReplaceAnyAsBefore();
    testReplaceCycle();
    testAnswerAndDuetIgnoreReplace();
    testReplayCancelAndBalance();
    std::printf("sizeof Listener=%zu\n", sizeof(CallResponseListener));
    std::puts("cr loop grid tests passed");
    return 0;
}
