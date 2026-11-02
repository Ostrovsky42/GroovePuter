// CALL/RESPONSE phase 1: turn-taking and cancellation. A person NoteOn during
// the answer cancels the future answer NoteOns, releases sounding answer notes at
// once, and becomes the new CALL. Answer NoteOn/NoteOff stay balanced; the
// PERFORM arpeggiator is not touched by the cancel.
#include <cassert>
#include <cstdio>
#include <string_view>
#include <vector>

#include "src/input/call_response_listener.h"
#include "src/input/performance_keyboard.h"

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
    std::vector<MusicalEvent> events;
};

uint32_t at(double step) { return kT0 + static_cast<uint32_t>(step * kSixteenth); }

bool isAnswer(const MusicalEvent& e) {
    return e.source == MusicalEventSource::Arpeggiator && e.target == MusicalEventTarget::Dx;
}

int count(const std::vector<MusicalEvent>& events, MusicalEventType type, std::size_t from = 0) {
    int n = 0;
    for (std::size_t i = from; i < events.size(); ++i) {
        if (isAnswer(events[i]) && events[i].type == type) ++n;
    }
    return n;
}

void assertBalanced(const std::vector<MusicalEvent>& events) {
    int sounding[128]{};
    for (const MusicalEvent& e : events) {
        if (!isAnswer(e)) continue;
        if (e.type == MusicalEventType::NoteOn) {
            assert(sounding[e.note] == 0);  // mono line: never a double NoteOn
            ++sounding[e.note];
        } else if (e.type == MusicalEventType::NoteOff) {
            assert(sounding[e.note] == 1);  // never a NoteOff without its NoteOn
            --sounding[e.note];
        }
    }
    for (int n : sounding) assert(n == 0);  // nothing left hanging
}

// Plays C(0..4) D(4..8) E(8..16) on the free clock and closes the CALL at step 32.
// Returns true when an answer was scheduled.
bool playCallAndAnswer(CallResponseListener& listener, int32_t windowStart = 0) {
    PhraseCapture::Phrase phrase;
    const TransportView stopped{};
    auto note = [&](uint16_t tag, uint8_t pitch, double on, double off) {
        gNow = at(windowStart + on);
        (void)listener.service(gNow, stopped, 120.0f, phrase);
        listener.onPerformanceInputNote(true, tag, pitch, 100);
        gNow = at(windowStart + off);
        (void)listener.service(gNow, stopped, 120.0f, phrase);
        listener.onPerformanceInputNote(false, tag, pitch, 0);
    };
    note('a', 60, 0.0, 4.0);
    note('s', 62, 4.0, 8.0);
    note('d', 64, 8.0, 16.0);
    gNow = at(windowStart + 32.0);
    if (listener.service(gNow, stopped, 120.0f, phrase) != Close::Phrase) return false;
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    return listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan);
}

void serviceUntil(CallResponseListener& listener, double step) {
    PhraseCapture::Phrase phrase;
    const TransportView stopped{};
    gNow = at(step);
    (void)listener.service(gNow, stopped, 120.0f, phrase);
}

void testHumanNoteCancelsAnswer() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);

    assert(playCallAndAnswer(listener));
    assert(listener.answering());
    assert(sink.events.empty());                   // answer starts one step after the boundary
    serviceUntil(listener, 32.9);
    assert(sink.events.empty());
    serviceUntil(listener, 33.0);                  // step 0 of the answer: C
    assert(count(sink.events, MusicalEventType::NoteOn) == 1 && sink.events[0].note == 60);
    assert(sink.events[0].velocity == 100);
    serviceUntil(listener, 37.0);                  // C off, D on
    assert(count(sink.events, MusicalEventType::NoteOn) == 2);
    assert(count(sink.events, MusicalEventType::NoteOff) == 1);
    assert(listener.responder().soundingCount() == 1);

    // The person plays during the answer: D is released now, nothing else follows.
    gNow = at(38.0);
    const std::size_t before = sink.events.size();
    listener.onPerformanceInputNote(true, 'f', 65, 110);
    assert(sink.events.size() == before + 1);
    assert(sink.events.back().type == MusicalEventType::NoteOff && sink.events.back().note == 62);
    assert(!listener.answering());
    assert(listener.humanCancels() == 1 && listener.responder().answersCancelled() == 1);
    assert(listener.responder().lastCancelPlayed() == 2);
    gNow = at(39.0);
    listener.onPerformanceInputNote(false, 'f', 65, 0);
    const std::size_t afterCancel = sink.events.size();
    serviceUntil(listener, 50.0);
    serviceUntil(listener, 63.0);
    assert(sink.events.size() == afterCancel);     // the ending E->C never sounds
    assertBalanced(sink.events);

    // The person's note became the new CALL (onset 6 of window 32..64).
    PhraseCapture::Phrase phrase;
    gNow = at(64.0);
    assert(listener.service(gNow, TransportView{}, 120.0f, phrase) == Close::Phrase);
    assert(phrase.windowStartStep == 32 && phrase.count == 1 && phrase.notes[0].onset == 6);
}

void testDuetKeepsAnswerWhenHumanPlays() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Duet);
    assert(playCallAndAnswer(listener));
    serviceUntil(listener, 33.0);
    assert(listener.answering());
    gNow = at(34.0);
    listener.onPerformanceInputNote(true, 'x', 67, 100);
    assert(listener.answering());
    assert(listener.humanCancels() == 0);
    listener.onPerformanceInputNote(false, 'x', 67, 0);
    serviceUntil(listener, 37.0);
    assert(count(sink.events, MusicalEventType::NoteOn) >= 2);
}

void testQuietAnswerWindowPlaysToTheEnd() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);
    assert(playCallAndAnswer(listener));
    for (double step = 32.0; step <= 64.0; step += 0.25) serviceUntil(listener, step);
    assert(count(sink.events, MusicalEventType::NoteOn) == 3);
    assert(count(sink.events, MusicalEventType::NoteOff) == 3);
    assert(sink.events.back().note == 60);  // E ending answered with C
    assert(!listener.answering());
    assertBalanced(sink.events);
    assert(listener.responder().answersCancelled() == 0);
}

void testPickupMeansNoAnswer() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);
    PhraseCapture::Phrase phrase;
    gNow = at(0.0);
    listener.onPerformanceInputNote(true, 'a', 60, 100);
    gNow = at(2.0);
    listener.onPerformanceInputNote(false, 'a', 60, 0);
    gNow = at(31.8);  // pickup into the next window: the person goes on
    (void)listener.service(gNow, TransportView{}, 120.0f, phrase);
    listener.onPerformanceInputNote(true, 's', 62, 100);
    gNow = at(32.0);
    assert(listener.service(gNow, TransportView{}, 120.0f, phrase) == Close::Phrase);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(!listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan));
    assert(!listener.answering());
}

void testListenModeNeverAnswers() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setEnabled(true);
    assert(listener.mode() == CallResponseListener::Mode::Listen);
    assert(!playCallAndAnswer(listener));
    for (double step = 32.0; step <= 64.0; step += 1.0) serviceUntil(listener, step);
    assert(sink.events.empty());
}

void testStopsReleaseSoundingNotes() {
    // Leaving ANSWER mode, switching PARTNER off, PLAY start (new epoch): each
    // releases the sounding answer note at once and drops the rest.
    for (int variant = 0; variant < 3; ++variant) {
        MusicalEventRouter router;
        RecordingSink sink;
        assert(router.addSink(sink));
        CallResponseListener listener;
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(CallResponseListener::Mode::Answer);
        assert(playCallAndAnswer(listener));
        serviceUntil(listener, 34.0);
        assert(listener.responder().soundingCount() == 1);
        if (variant == 0) {
            listener.setMode(CallResponseListener::Mode::Listen);
        } else if (variant == 1) {
            listener.setMode(CallResponseListener::Mode::Off);
        } else {
            TransportView play{true, 7, 1, 0.0, 120.0};
            PhraseCapture::Phrase phrase;
            gNow = at(34.5);
            (void)listener.service(gNow, play, 120.0f, phrase);
        }
        assert(!listener.answering());
        assert(sink.events.size() == 2 && sink.events[1].type == MusicalEventType::NoteOff);
        serviceUntil(listener, 60.0);
        assert(sink.events.size() == 2);
        assertBalanced(sink.events);
    }
}

void testCycleMode() {
    CallResponseListener listener;
    assert(std::string_view(listener.modeName()) == "OFF");
    listener.cycleMode(1);
    assert(listener.mode() == CallResponseListener::Mode::Answer);
    listener.cycleMode(1);
    assert(listener.mode() == CallResponseListener::Mode::Duet);
    listener.cycleMode(1);
    assert(listener.mode() == CallResponseListener::Mode::Loop);
    listener.cycleMode(1);
    assert(!listener.enabled());
    listener.cycleMode(-1);
    assert(listener.mode() == CallResponseListener::Mode::Loop);
}

void testLoopReplaysWithoutHumanInput() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Loop);
    assert(playCallAndAnswer(listener));
    for (double step = 32.0; step < 64.0; step += 0.25) serviceUntil(listener, step);
    PhraseCapture::Phrase repeated{};
    gNow = at(64.0);
    assert(listener.service(gNow, TransportView{}, 120.0f, repeated) == Close::Phrase);
    assert(repeated.count == 3 && repeated.windowStartStep == 32);
    MotifDescriptor motif{};
    assert(describe(repeated, 0, kMajor, 7, motif));
    ResponsePlan plan{};
    assert(listener.answerPhrase(repeated, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan));
    serviceUntil(listener, 65.0);
    assert(listener.answering());
    listener.setMode(CallResponseListener::Mode::Off);
    assert(!listener.answering());
}

// Through PerformanceKeyboard: the cancel triggered by a key never touches the
// arpeggiator, whose scheduled notes keep coming on its own target.
void testCancelLeavesArpeggiatorAlone() {
    MusicalEventRouter router;
    RecordingSink sink;
    assert(router.addSink(sink));
    PerformanceKeyboard keyboard(router);
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Answer);
    keyboard.setInputObserver(&listener);
    keyboard.setTempoBpm(120.0f);

    assert(playCallAndAnswer(listener));
    serviceUntil(listener, 34.0);
    assert(listener.responder().soundingCount() == 1);

    keyboard.setArpeggiatorEnabled(true);
    gNow = at(34.5);
    keyboard.service(gNow);
    const std::size_t before = sink.events.size();
    assert(keyboard.keyDown('a', 100));  // the person plays: the answer yields
    assert(!listener.answering());
    bool answerReleased = false;
    for (std::size_t i = before; i < sink.events.size(); ++i) {
        if (isAnswer(sink.events[i]) && sink.events[i].type == MusicalEventType::NoteOff)
            answerReleased = true;
    }
    assert(answerReleased);
    int arpOns = 0;
    for (double step = 34.5; step < 40.0; step += 0.125) {
        gNow = at(step);
        keyboard.service(gNow);
        serviceUntil(listener, step);
    }
    for (std::size_t i = before; i < sink.events.size(); ++i) {
        const MusicalEvent& e = sink.events[i];
        if (e.source == MusicalEventSource::Arpeggiator && e.target == MusicalEventTarget::SynthA &&
            e.type == MusicalEventType::NoteOn)
            ++arpOns;
    }
    assert(arpOns >= 4);  // the arpeggiator keeps running after the cancel
    assert(count(sink.events, MusicalEventType::NoteOn, before) == 0);
    keyboard.keyUp('a');
    assertBalanced(sink.events);
}

}  // namespace

int main() {
    testHumanNoteCancelsAnswer();
    testDuetKeepsAnswerWhenHumanPlays();
    testQuietAnswerWindowPlaysToTheEnd();
    testPickupMeansNoAnswer();
    testListenModeNeverAnswers();
    testStopsReleaseSoundingNotes();
    testCycleMode();
    testLoopReplaysWithoutHumanInput();
    testCancelLeavesArpeggiatorAlone();
    std::puts("cr response cancel tests passed");
    return 0;
}
