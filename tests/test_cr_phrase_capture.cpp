// CALL/RESPONSE phase 0: two-bar phrase capture and the listener clock.
#include <cassert>
#include <cstdio>

#include "src/input/call_response_listener.h"
#include "src/input/call_response_phrase_capture.h"

using GroovePuterCallResponse::CallResponseListener;
using GroovePuterCallResponse::PhraseCapture;
using GroovePuterCallResponse::TransportView;
using Close = PhraseCapture::CloseResult;

namespace {

void testQuantizeAndTwoBarWindow() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    assert(capture.closeDueWindow(0.0, phrase) == Close::None);  // arms window [0,32)
    assert(capture.windowStartStep() == 0);

    assert(capture.noteOn(1, 60, 100, 0.2));    // -> 0
    capture.noteOff(1, 1.9);                    // -> 2, length 2
    assert(capture.noteOn(2, 62, 90, 4.49));    // -> 4
    capture.noteOff(2, 5.0);
    assert(capture.noteOn(3, 64, 80, 16.6));    // second bar, -> 17
    capture.noteOff(3, 18.0);

    // Window is two bars: nothing closes during bar 2.
    assert(capture.closeDueWindow(16.0, phrase) == Close::None);
    assert(capture.closeDueWindow(31.99, phrase) == Close::None);
    assert(capture.closeDueWindow(32.0, phrase) == Close::Phrase);
    assert(phrase.windowStartStep == 0 && phrase.firstBar() == 0);
    assert(phrase.count == 3);
    assert(phrase.notes[0].onset == 0 && phrase.notes[0].length == 2 && phrase.notes[0].note == 60);
    assert(phrase.notes[1].onset == 4 && phrase.notes[1].length == 1 && phrase.notes[1].velocity == 90);
    assert(phrase.notes[2].onset == 17 && phrase.notes[2].length == 1 && phrase.notes[2].note == 64);
    assert(capture.windowStartStep() == 32);

    // Next window is empty: Empty, not Phrase (empty windows trigger nothing).
    assert(capture.closeDueWindow(64.0, phrase) == Close::Empty);
    assert(phrase.count == 0);
}

void testPickup() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    // In the last 30% of the step before the window boundary: next window, onset 0.
    assert(capture.noteOn(1, 67, 100, 31.75));
    capture.noteOff(1, 33.0);
    // 0.4 step early is not a pickup: it stays on the last step of this window.
    assert(capture.noteOn(2, 65, 100, 31.6));
    capture.noteOff(2, 31.9);
    assert(capture.closeDueWindow(32.0, phrase) == Close::Phrase);
    assert(phrase.count == 1 && phrase.notes[0].note == 65 && phrase.notes[0].onset == 31);
    assert(phrase.notes[0].length == 1);
    assert(capture.pendingCount() == 1);
    assert(capture.closeDueWindow(64.0, phrase) == Close::Phrase);
    assert(phrase.windowStartStep == 32 && phrase.firstBar() == 2);
    assert(phrase.count == 1 && phrase.notes[0].note == 67 && phrase.notes[0].onset == 0);
    assert(phrase.notes[0].length == 1);

    // Around the 30% line.
    assert(PhraseCapture::quantizeOnset(31.71) == 32);
    assert(PhraseCapture::quantizeOnset(31.69) == 31);
    // Mid-window bar line is ordinary rounding (no pickup rule inside the window).
    assert(PhraseCapture::quantizeOnset(15.6) == 16);
}

void testChordTakesTopNote() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    assert(capture.noteOn(1, 60, 100, 8.0));
    assert(capture.noteOn(2, 67, 90, 8.1));     // same onset, higher: replaces
    assert(!capture.noteOn(3, 64, 110, 7.9));   // same onset, lower: ignored
    capture.noteOff(3, 9.0);                    // unknown tag: no effect
    capture.noteOff(1, 9.0);                    // replaced note: no effect
    capture.noteOff(2, 12.0);
    assert(capture.closeDueWindow(32.0, phrase) == Close::Phrase);
    assert(phrase.count == 1);
    assert(phrase.notes[0].note == 67 && phrase.notes[0].velocity == 90);
    assert(phrase.notes[0].onset == 8 && phrase.notes[0].length == 4);
}

void testOverflow() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    for (int i = 0; i < 20; ++i) {
        const bool stored = capture.noteOn(static_cast<uint16_t>(i + 1),
                                           static_cast<uint8_t>(60 + i), 100, i * 1.5);
        assert(stored == (i < PhraseCapture::kCapacity));
        capture.noteOff(static_cast<uint16_t>(i + 1), i * 1.5 + 0.5);
    }
    assert(capture.overflowCount() == 4);
    assert(capture.closeDueWindow(32.0, phrase) == Close::Phrase);
    assert(phrase.count == PhraseCapture::kCapacity);
    assert(phrase.dropped == 4);
    for (uint8_t i = 1; i < phrase.count; ++i) assert(phrase.notes[i - 1].onset < phrase.notes[i].onset);
    // Room again after the close; dropped counter is per window.
    assert(capture.noteOn(99, 70, 100, 33.0));
    assert(capture.closeDueWindow(64.0, phrase) == Close::Phrase);
    assert(phrase.count == 1 && phrase.dropped == 0);
}

void testNoteOnWithoutNoteOff() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    assert(capture.noteOn(5, 48, 100, 28.0));   // held across the boundary
    assert(capture.hasOpenNotes());
    assert(capture.closeDueWindow(32.0, phrase) == Close::Phrase);
    assert(phrase.count == 1 && phrase.notes[0].onset == 28 && phrase.notes[0].length == 4);
    assert(!capture.hasOpenNotes() && capture.pendingCount() == 0);
    capture.noteOff(5, 40.0);                   // late NoteOff: ignored, nothing stuck
    assert(capture.pendingCount() == 0);
    assert(capture.closeDueWindow(64.0, phrase) == Close::Empty);

    // Same source attacking again closes its previous note.
    assert(capture.noteOn(7, 50, 100, 64.0));
    assert(capture.noteOn(7, 52, 100, 66.0));
    assert(capture.closeDueWindow(96.0, phrase) == Close::Phrase);
    assert(phrase.count == 2 && phrase.notes[0].length == 2 && phrase.notes[1].length == 30);
}

void testEpochReset() {
    PhraseCapture capture;
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(40.0, phrase);
    assert(capture.windowStartStep() == 32);
    assert(capture.noteOn(1, 60, 100, 41.0));
    capture.reset();
    assert(!capture.windowValid() && capture.pendingCount() == 0);
    // New epoch starts from its own position; old notes never surface.
    assert(capture.closeDueWindow(3.0, phrase) == Close::None);
    assert(capture.windowStartStep() == 0);
    assert(capture.closeDueWindow(32.0, phrase) == Close::Empty);

    // A stalled loop skips silent windows in one close.
    PhraseCapture stalled;
    (void)stalled.closeDueWindow(0.0, phrase);
    assert(stalled.noteOn(1, 60, 100, 1.0));
    assert(stalled.closeDueWindow(200.0, phrase) == Close::Phrase);
    assert(stalled.windowStartStep() == 192);
}

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }

// 120 BPM: one sixteenth = 125000 us.
constexpr uint32_t kSixteenth = 125000u;

void testListenerFreeClock() {
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    PhraseCapture::Phrase phrase;
    const TransportView stopped{};

    // Off by default: notes are ignored.
    gNow = 1000;
    listener.onPerformanceInputNote(true, 'a', 60, 100);
    assert(listener.clockSource() == CallResponseListener::ClockSource::None);
    listener.setEnabled(true);
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::None);

    // First note starts the free clock: it is bar 1, beat 1.
    gNow = 5000000;
    listener.onPerformanceInputNote(true, 'a', 60, 100);
    assert(listener.clockSource() == CallResponseListener::ClockSource::Free);
    gNow += 2 * kSixteenth;
    listener.onPerformanceInputNote(false, 'a', 60, 0);
    gNow = 5000000 + 8 * kSixteenth;
    listener.onPerformanceInputNote(true, 's', 63, 80);
    gNow += kSixteenth;
    listener.onPerformanceInputNote(false, 's', 63, 0);
    gNow = 5000000 + 31 * kSixteenth;
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::None);
    gNow = 5000000 + 32 * kSixteenth;
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::Phrase);
    assert(phrase.firstBar() == 0 && phrase.count == 2);
    assert(phrase.notes[0].onset == 0 && phrase.notes[0].length == 2);
    assert(phrase.notes[1].onset == 8 && phrase.notes[1].note == 63);

    // Two silent windows stop the free clock (next note re-anchors).
    gNow = 5000000 + 64 * kSixteenth;
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::Empty);
    gNow = 5000000 + 96 * kSixteenth;
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::Empty);
    assert(listener.clockSource() == CallResponseListener::ClockSource::None);

    // Free clock disabled: without PLAY nothing is captured.
    listener.setFreeClockEnabled(false);
    listener.onPerformanceInputNote(true, 'd', 65, 100);
    assert(listener.clockSource() == CallResponseListener::ClockSource::None);
}

void testListenerTransportEpochs() {
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setEnabled(true);
    PhraseCapture::Phrase phrase;
    TransportView play{};
    play.playing = true;
    play.epoch = 3;
    play.bpm = 120.0;
    play.blockSequence = 10;
    play.steps = 64.0;  // project bar 5 (0-based 4)
    gNow = 100;
    assert(listener.service(gNow, play, 120.0f, phrase) == Close::None);
    assert(listener.clockSource() == CallResponseListener::ClockSource::Transport);
    listener.onPerformanceInputNote(true, 'a', 60, 100);
    play.blockSequence = 11;
    play.steps = 66.0;
    gNow += 2 * kSixteenth;
    listener.onPerformanceInputNote(false, 'a', 60, 0);
    (void)listener.service(gNow, play, 120.0f, phrase);

    // New transport epoch (restart) discards the pending note: nothing logged.
    play.epoch = 4;
    play.steps = 0.0;
    play.blockSequence = 12;
    assert(listener.service(gNow, play, 120.0f, phrase) == Close::None);
    play.steps = 96.0;
    play.blockSequence = 13;
    assert(listener.service(gNow, play, 120.0f, phrase) == Close::Empty);

    // Windows follow project bars: a note at project step 96 opens the window at bar 7 (1-based).
    listener.onPerformanceInputNote(true, 'a', 60, 100);
    play.steps = 128.0;
    play.blockSequence = 14;
    assert(listener.service(gNow, play, 120.0f, phrase) == Close::Phrase);
    assert(phrase.firstBar() == 6 && phrase.notes[0].onset == 0);

    // PLAY stop is a new epoch too.
    listener.onPerformanceInputNote(true, 's', 62, 100);
    TransportView stopped{};
    assert(listener.service(gNow, stopped, 120.0f, phrase) == Close::None);
    assert(listener.clockSource() == CallResponseListener::ClockSource::None);
    assert(listener.capture().pendingCount() == 0);

    // Disabling the mode clears everything.
    listener.onPerformanceInputNote(true, 'd', 64, 100);
    assert(listener.clockSource() == CallResponseListener::ClockSource::Free);
    listener.setEnabled(false);
    assert(listener.clockSource() == CallResponseListener::ClockSource::None);
    assert(listener.capture().pendingCount() == 0);
}

}  // namespace

int main() {
    testQuantizeAndTwoBarWindow();
    testPickup();
    testChordTakesTopNote();
    testOverflow();
    testNoteOnWithoutNoteOff();
    testEpochReset();
    testListenerFreeClock();
    testListenerTransportEpochs();
    std::puts("cr phrase capture tests passed");
    return 0;
}
