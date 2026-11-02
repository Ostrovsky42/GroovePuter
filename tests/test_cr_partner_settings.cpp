// CALL/RESPONSE phase 3: PARTNER settings (BARS, GENRE, CHANNEL, DELAY).
// Defaults are the phase-2 behaviour; each setting changes only what it names.
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
    PhraseCapture::Phrase lastPhrase{};
    ResponsePlan lastPlan{};
    int answers{0};

    Rig() {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(Listener::Mode::Answer);
    }
    void serviceTo(double step) {
        gNow = at(step);
        PhraseCapture::Phrase phrase;
        if (listener.service(gNow, TransportView{}, 120.0f, phrase) != Close::Phrase) return;
        lastPhrase = phrase;
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        ResponsePlan plan;
        if (listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan)) {
            lastPlan = plan;
            ++answers;
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
    double firstOnStep() const {
        for (std::size_t i = 0; i < sink.events.size(); ++i) {
            if (sink.events[i].type == MusicalEventType::NoteOn)
                return static_cast<double>(sink.times[i] - kT0) / kSixteenth;
        }
        return -1.0;
    }
    double cursor{0.0};
};

void testDefaultsArePhaseTwo() {
    Listener listener;
    assert(listener.answerBars() == 2);
    assert(listener.answerDelaySteps() == kAnswerDelaySteps && kAnswerDelaySteps == 1);
    assert(listener.answerGenreSetting() == Listener::kGenreAuto);
    assert(listener.answerChannel() == Listener::kAnswerChannelFollowDx);
    assert(listener.answerGenre(9) == 9);  // AUTO = project genre (House)
    assert(listener.capture().windowSteps() == PhraseCapture::kWindowSteps);
}

void testGenreSetting() {
    Listener listener;
    listener.cycleAnswerGenre(1);
    assert(listener.answerGenreSetting() == Listener::kGenreNone);
    assert(listener.answerGenre(0) == kNoGenre);
    assert(!pickResponseStyle(listener.answerGenre(0), 123).active());  // phase-1 ending
    listener.cycleAnswerGenre(1);
    assert(listener.answerGenre(9) == 0);  // explicit Acid over a House project
    listener.setAnswerGenreSetting(Listener::kGenreFirst + 13);
    assert(listener.answerGenre(0) == 13);  // UK Garage
    listener.setAnswerGenreSetting(Listener::kGenreAuto);
    listener.cycleAnswerGenre(-1);
    assert(listener.answerGenreSetting() == Listener::kGenreSettingCount - 1);
    assert(listener.answerGenre(0) == 15);  // Lo-Fi
    listener.cycleAnswerGenre(1);
    assert(listener.answerGenreSetting() == Listener::kGenreAuto);
    listener.setAnswerGenreSetting(200);
    assert(listener.answerGenreSetting() == Listener::kGenreAuto);
}

// Default rig: CALL at 0..4 and 4..8, answer one step after the 2-bar boundary.
void testTwoBarDefaultTiming() {
    Rig rig;
    rig.note(0.0, 4.0, 62);
    rig.note(4.0, 8.0, 64);
    rig.runTo(70.0);
    assert(rig.answers == 1);
    assert(rig.lastPhrase.windowSteps == 32);
    assert(rig.firstOnStep() == 33.0);
}

// BARS 1: the CALL closes after one bar and the answer fills the next bar.
void testOneBar() {
    Rig rig;
    rig.listener.setAnswerBars(1);
    assert(rig.listener.answerBars() == 1);
    rig.note(0.0, 4.0, 62);
    rig.note(4.0, 8.0, 64);
    rig.runTo(40.0);
    assert(rig.answers == 1);
    assert(rig.lastPhrase.windowStartStep == 0 && rig.lastPhrase.windowSteps == 16);
    assert(rig.firstOnStep() == 17.0);  // 1 bar + 1/16
    assert(rig.lastPlan.windowSteps == 15);
    for (std::size_t i = 0; i < rig.sink.events.size(); ++i) {
        const double step = static_cast<double>(rig.sink.times[i] - kT0) / kSixteenth;
        assert(step >= 17.0 && step <= 32.0);  // inside the ANSWER bar
    }
    // A one-note CALL on the last sixteenth of a 1-bar window is answered too.
    Rig last;
    last.listener.setAnswerBars(1);
    last.note(0.0, 0.5, 60);   // anchors the free clock (answered on step 17)
    last.note(31.0, 31.5, 62); // onset 15 of window 16..32
    last.runTo(60.0);
    assert(last.answers == 2);
    assert(last.lastPlan.count == 1 && last.lastPlan.notes[0].step == 14);
}

// DELAY 0: the answer starts on the boundary; onset 31 is no longer cut.
void testDelayZero() {
    Rig rig;
    rig.listener.setAnswerDelaySteps(0);
    rig.note(0.0, 4.0, 62);
    rig.note(31.0, 31.6, 64);
    rig.runTo(70.0);
    assert(rig.answers == 1);
    assert(rig.firstOnStep() == 32.0);
    assert(rig.lastPlan.delaySteps == 0 && rig.lastPlan.droppedTail == 0 && rig.lastPlan.count == 2);
    rig.listener.cycleAnswerDelay(1);
    assert(rig.listener.answerDelaySteps() == 1);
    rig.listener.setAnswerDelaySteps(9);  // clamped to the offered range
    assert(rig.listener.answerDelaySteps() == kAnswerMaxDelaySteps);
}

// Changing BARS while an answer sounds is a new epoch: the answer note is
// released at once and the capture starts over on the new grid.
void testBarsChangeCancels() {
    Rig rig;
    rig.note(0.0, 8.0, 62);
    rig.runTo(34.0);
    assert(rig.listener.answering() && rig.listener.responder().soundingCount() == 1);
    const std::size_t before = rig.sink.events.size();
    rig.listener.cycleAnswerBars(1);
    assert(rig.listener.answerBars() == 1);
    assert(!rig.listener.answering());
    assert(rig.sink.events.size() == before + 1 &&
           rig.sink.events.back().type == MusicalEventType::NoteOff);
    assert(rig.listener.clockSource() == Listener::ClockSource::None);
    rig.listener.setAnswerBars(7);  // anything but 1 is 2
    assert(rig.listener.answerBars() == 2);
}

void testLoopHoldAndRelease() {
    for (uint8_t bars : {uint8_t{1}, uint8_t{2}}) {
    Rig rig;
    rig.listener.setAnswerBars(bars);
    rig.listener.setMode(Listener::Mode::Loop);
    rig.note(0.0, 2.0, 60);
    rig.runTo(32.0);
    assert(rig.listener.hasLoopPhrase());
    rig.listener.setLoopHold(true);
    assert(rig.listener.loopHold());
    rig.note(36.0, 38.0, 67);
    rig.runTo(64.0);
    assert(rig.lastPhrase.notes[0].note == 60);
    rig.listener.setLoopHold(false);
    rig.note(68.0, 70.0, 69); // release window is skipped
    rig.runTo(96.0);
    assert(rig.lastPhrase.notes[0].note == 60);
    rig.note(100.0, 102.0, 64);
    rig.runTo(128.0);
    assert(rig.lastPhrase.notes[0].note == 64);
    rig.listener.setLoopHold(true);
    rig.listener.setAnswerBars(bars == 1 ? 2 : 1);
    assert(!rig.listener.loopHold() && !rig.listener.hasLoopPhrase());
    assert(rig.listener.loopEvolve() == 1);
    rig.listener.setLoopEvolve(9);
    assert(rig.listener.loopEvolve() == 3);
    }
}

}  // namespace

int main() {
    testLoopHoldAndRelease();
    testDefaultsArePhaseTwo();
    testGenreSetting();
    testTwoBarDefaultTiming();
    testOneBar();
    testDelayZero();
    testBarsChangeCancels();
    std::puts("cr partner settings tests passed");
    return 0;
}
