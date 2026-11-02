// CALL/RESPONSE phase 1: answer steps -> dueMicros at 90/120/140 BPM, on the
// free clock and under PLAY, including micros() wraparound.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "src/input/call_response_listener.h"

using namespace GroovePuterCallResponse;
using Close = PhraseCapture::CloseResult;

namespace {

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }
constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};

class TimedSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
        times.push_back(gNow);
    }
    std::vector<MusicalEvent> events;
    std::vector<uint32_t> times;
};

void testSixteenthAndStepConversion() {
    struct Case { double bpm; uint32_t sixteenth; };
    const Case cases[] = {{90.0, 166667}, {120.0, 125000}, {140.0, 107143}};
    for (const Case& c : cases) {
        const uint32_t sixteenth = CallResponseListener::sixteenthMicrosFor(c.bpm);
        assert(sixteenth == c.sixteenth);
        assert(answerStepDueMicros(5000, sixteenth, 0) == 5000);
        assert(answerStepDueMicros(5000, sixteenth, 16) == 5000 + 16 * c.sixteenth);  // one bar
        // Two bars of answer stay within 1 us per step of the exact tempo.
        const double exactBar2 = 32.0 * 15000000.0 / c.bpm;
        const double got = static_cast<double>(answerStepDueMicros(0, sixteenth, 32));
        assert(std::abs(got - exactBar2) <= 32.0);
    }
    // micros() wraps every ~71.6 minutes: due times wrap with it.
    const uint32_t origin = 0xFFFFFFFFu - 100000u;
    assert(answerStepDueMicros(origin, 125000, 1) == static_cast<uint32_t>(origin + 125000u));
}

// Free clock: the first NoteOn of the answer is due at CALL start + 33 steps
// (two bars + kAnswerDelaySteps) and the following ones on their own steps.
void runFreeClock(double bpm, uint32_t t0) {
    MusicalEventRouter router;
    TimedSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);
    const uint32_t sixteenth = CallResponseListener::sixteenthMicrosFor(bpm);
    const TransportView stopped{};
    PhraseCapture::Phrase phrase;
    auto stepTime = [&](uint32_t step) { return answerStepDueMicros(t0, sixteenth, step); };

    gNow = t0 - 10;
    (void)listener.service(gNow, stopped, static_cast<float>(bpm), phrase);
    gNow = t0;
    listener.onPerformanceInputNote(true, 1, 62, 100);
    gNow = stepTime(2);
    listener.onPerformanceInputNote(false, 1, 62, 0);
    gNow = stepTime(6);
    listener.onPerformanceInputNote(true, 1, 64, 100);
    gNow = stepTime(10);
    listener.onPerformanceInputNote(false, 1, 64, 0);
    gNow = stepTime(32) + 3000;  // the loop sees the boundary a little late
    assert(listener.service(gNow, stopped, static_cast<float>(bpm), phrase) == Close::Phrase);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan));
    assert(listener.microsAtStep(33.0) == stepTime(33));
    assert(sink.events.empty());

    gNow = stepTime(33) - 1;
    (void)listener.service(gNow, stopped, static_cast<float>(bpm), phrase);
    assert(sink.events.empty());
    for (uint32_t t = stepTime(33); static_cast<int32_t>(stepTime(64) - t) > 0; t += 500) {
        gNow = t;
        (void)listener.service(gNow, stopped, static_cast<float>(bpm), phrase);
    }
    assert(sink.events.size() == 4);
    const uint32_t expectedOn[] = {stepTime(33), stepTime(33 + 6)};
    const uint32_t expectedOff[] = {stepTime(33 + 2), stepTime(33 + 6 + 4)};
    int on = 0, off = 0;
    for (std::size_t i = 0; i < sink.events.size(); ++i) {
        const uint32_t expected = sink.events[i].type == MusicalEventType::NoteOn
            ? expectedOn[on++] : expectedOff[off++];
        const int32_t late = static_cast<int32_t>(sink.times[i] - expected);
        assert(late >= 0 && late < 500);  // emitted on the first service pass at or after due
    }
    assert(on == 2 && off == 2);
}

// Under PLAY the answer is timed from the transport position, not from the CALL's first note.
void testTransportClock() {
    MusicalEventRouter router;
    TimedSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);
    const uint32_t sixteenth = CallResponseListener::sixteenthMicrosFor(140.0);
    PhraseCapture::Phrase phrase;
    const uint32_t t0 = 777000;
    TransportView play{true, 3, 1, 40.0, 140.0};  // PLAY at project step 40 (bar 3, beat 3)
    gNow = t0;
    (void)listener.service(gNow, play, 120.0f, phrase);
    auto timeOf = [&](double step) {
        return t0 + static_cast<uint32_t>((step - 40.0) * sixteenth + 0.5);
    };
    gNow = timeOf(48.0);
    listener.onPerformanceInputNote(true, 1, 67, 90);
    gNow = timeOf(52.0);
    listener.onPerformanceInputNote(false, 1, 67, 0);
    gNow = timeOf(64.0) + 1000;
    play.blockSequence = 2;
    play.steps = 64.0 + 1000.0 / sixteenth;
    assert(listener.service(gNow, play, 120.0f, phrase) == Close::Phrase);
    assert(phrase.windowStartStep == 32 && phrase.notes[0].onset == 16);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan));
    // Answer onset 16 (bar 2 of the ANSWER window) = project step 64 + 1 + 16.
    const uint32_t due = listener.microsAtStep(81.0);
    assert(std::abs(static_cast<int32_t>(due - timeOf(81.0))) <= 1);
    gNow = due - 1;
    (void)listener.service(gNow, play, 120.0f, phrase);
    assert(sink.events.empty());
    gNow = due;
    (void)listener.service(gNow, play, 120.0f, phrase);
    assert(sink.events.size() == 1 && sink.events[0].type == MusicalEventType::NoteOn);
    assert(sink.events[0].velocity == 90);
}

}  // namespace

int main() {
    testSixteenthAndStepConversion();
    runFreeClock(90.0, 1000);
    runFreeClock(120.0, 50000);
    runFreeClock(140.0, 0xFFFFFFFFu - 3000000u);  // the answer crosses the micros() wrap
    testTransportClock();
    std::puts("cr answer timing tests passed");
    return 0;
}
