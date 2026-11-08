// CALL/RESPONSE phase 3, bug "one note -> the partner answers only once".
//
// Cause: a one-note CALL whose note lands on the last sixteenth of its window
// (onset 31) had no answer at all. The answer starts one sixteenth after the
// ANSWER window boundary, so onset 31 does not fit and phase 1 dropped it
// (cut=1); with a single note nothing was left and the CALL was silently
// ignored. A note lands there whenever it is played 0.3..1.5 steps before a
// window downbeat (the pickup rule only takes the last 30 % of the step), i.e.
// a downbeat played a little early, which is how a one-note call is usually
// played. On the free clock the very first note anchors the grid (onset 0
// exactly) and is answered; every later one-note call aimed at a window
// downbeat and played a touch early is not: one answer, then silence.
// Phase 9: the 1/16 delay is gone, so the onset-31 note always fits; the
// scenarios stay as regressions.
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"

using namespace GroovePuterCallResponse;
using Close = PhraseCapture::CloseResult;

namespace {

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint32_t kSixteenth = 125000;  // 120 BPM
constexpr uint32_t kT0 = 1000;

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    int noteOns() const {
        int n = 0;
        for (const MusicalEvent& e : events) n += e.type == MusicalEventType::NoteOn;
        return n;
    }
    std::vector<MusicalEvent> events;
};

uint32_t at(double step) { return kT0 + static_cast<uint32_t>(step * kSixteenth + 0.5); }

TransportView playView(double step) {
    TransportView view{};
    view.playing = true;
    view.epoch = 7;
    view.blockSequence = static_cast<uint32_t>(step * 16.0);  // a new block every 1/16 step
    view.steps = static_cast<double>(view.blockSequence) / 16.0;
    view.bpm = 120.0;
    return view;
}

// The main-loop path of GroovePuter.ino (serviceCallResponseLog) in ANSWER mode:
// describe, plan (phase-2 planner, project genre), start.
struct Rig {
    MusicalEventRouter router;
    RecordingSink sink;
    CallResponseListener listener;
    bool play{false};
    uint8_t genre{kNoGenre};
    int calls{0};
    int answersStarted{0};

    Rig() {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(CallResponseListener::Mode::Answer);
    }

    void serviceTo(double step) {
        gNow = at(step);
        PhraseCapture::Phrase phrase;
        const TransportView view = play ? playView(step) : TransportView{};
        if (listener.service(gNow, view, 120.0f, phrase) != Close::Phrase) return;
        ++calls;
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        if (!listener.canAnswer(phrase)) return;
        const ResponseStyle style = pickResponseStyle(genre, responseSeed(phrase));
        ResponsePlan plan;
        if (!planHarmonicEcho(phrase, motif, responseKeyFromIntervals(0, kMajor, 7),
                              ResponseHarmony{}, style, plan, nullptr)) {
            return;
        }
        if (listener.startAnswer(phrase, plan, gNow)) ++answersStarted;
    }

    // Main loop every 1/8 step until `step`.
    void runTo(double step) {
        double s = lastStep;
        while (s < step) {
            s += 0.125;
            serviceTo(s);
        }
        lastStep = step;
    }

    void tap(double step, uint8_t pitch) {
        runTo(step);
        gNow = at(step);
        listener.onPerformanceInputNote(true, 'a', pitch, 100);
        runTo(step + 0.75);
        gNow = at(step + 0.75);
        listener.onPerformanceInputNote(false, 'a', pitch, 0);
    }

    double lastStep{0.0};
};

// Free clock: the first note anchors the grid; then one note before every second
// window downbeat, 0.5 step early (62 ms at 120 BPM), silence in between.
void testFreeClockEarlyDownbeatsAreAnswered() {
    Rig rig;
    rig.tap(0.0, 62);  // anchor: onset 0
    const int kCalls = 5;
    for (int i = 1; i <= kCalls; ++i) rig.tap(64.0 * i - 0.5, 62);
    rig.runTo(64.0 * kCalls + 40.0);
    // Phase 1/2: rig.sink.noteOns() == 1 (only the anchored first note).
    std::printf("free clock: calls=%d answers=%d answer NoteOns=%d\n", rig.calls,
                rig.answersStarted, rig.sink.noteOns());
    assert(rig.calls == kCalls + 1);
    assert(rig.answersStarted == kCalls + 1);
    assert(rig.sink.noteOns() == kCalls + 1);
}

// Under PLAY: one note 0.5 step before each even-bar downbeat (window start).
void testPlayEarlyDownbeatsAreAnswered() {
    Rig rig;
    rig.play = true;
    rig.genre = 0;  // project genre Acid: the phase-2 genre path too
    rig.runTo(1.0);
    const int kCalls = 4;
    for (int i = 1; i <= kCalls; ++i) rig.tap(64.0 * i - 0.5, 64);
    rig.runTo(64.0 * kCalls + 40.0);
    std::printf("play: calls=%d answers=%d answer NoteOns=%d\n", rig.calls,
                rig.answersStarted, rig.sink.noteOns());
    assert(rig.calls == kCalls);
    assert(rig.answersStarted == kCalls);
    assert(rig.sink.noteOns() == kCalls);
}

// Every onset of a one-note CALL is answered, for every genre and none, and the
// answer stays inside its window.
void testEveryOnsetEveryGenreAnswersOneNote() {
    for (int genre = -1; genre < 16; ++genre) {
        const uint8_t g = genre < 0 ? kNoGenre : static_cast<uint8_t>(genre);
        for (uint8_t onset = 0; onset < PhraseCapture::kWindowSteps; ++onset) {
            PhraseCapture::Phrase phrase{};
            phrase.windowStartStep = 64;
            phrase.count = 1;
            phrase.notes[0] = CapturedNote{onset, 1, 67, 90};
            MotifDescriptor motif;
            assert(describe(phrase, 0, kMajor, 7, motif));
            ResponsePlan plan;
            assert(planHarmonicEcho(phrase, motif, responseKeyFromIntervals(0, kMajor, 7),
                                    ResponseHarmony{}, pickResponseStyle(g, responseSeed(phrase)),
                                    plan));
            assert(plan.count == 1);
            assert(plan.notes[0].step + plan.notes[0].length <= plan.windowSteps);
        }
    }
}

// Phase 9 (was "the onset-31 note is cut"): with the answer on the grid the
// last-step note of a multi-note CALL is answered on its own step.
void testMultiNoteTailKept() {
    PhraseCapture::Phrase phrase{};
    phrase.count = 2;
    phrase.notes[0] = CapturedNote{4, 2, 62, 100};
    phrase.notes[1] = CapturedNote{31, 1, 64, 100};
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), plan));
    assert(plan.count == 2 && plan.droppedTail == 0 && plan.notes[0].step == 4);
    assert(plan.notes[1].step == 31 && plan.notes[1].length == 1);
}

}  // namespace

int main() {
    testFreeClockEarlyDownbeatsAreAnswered();
    testPlayEarlyDownbeatsAreAnswered();
    testEveryOnsetEveryGenreAnswersOneNote();
    testMultiNoteTailKept();
    std::puts("cr single note tests passed");
    return 0;
}
