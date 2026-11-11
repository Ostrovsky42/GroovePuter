// CUTOFF / AUTO SWEEP state machine. Host-only: no transport, no timer.
#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/cutoff_sweep.h"

using GroovePuterMidi::CutoffSweep;

namespace {

// Feeds bar positions in fine steps and collects every value that would be sent.
std::vector<uint8_t> runSweep(CutoffSweep& sweep, double fromBar, double toBar, double step) {
    std::vector<uint8_t> sent;
    for (double bar = fromBar; bar <= toBar + 1e-9; bar += step) {
        uint8_t v = 0;
        if (sweep.poll(bar, v)) sent.push_back(v);
    }
    return sent;
}

void testManualMapsValuesAndClamps() {
    CutoffSweep s;
    uint8_t v = 0;
    assert(s.setManual(0, v) && v == 0);
    assert(s.setManual(64, v) && v == 64);
    assert(s.setManual(110, v) && v == 110);
    assert(s.setManual(127, v) && v == 127);
    assert(s.setManual(126, v) && v == 126);
    assert(s.setManual(200, v) && v == 127);   // out of range clamps, never exceeds 127
    assert(!s.setManual(127, v));              // unchanged: no send
}

void testSweepStartsFromHeldValueAndEndsOnTarget() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 10.0, false);
    const std::vector<uint8_t> sent = runSweep(s, 10.0, 15.0, 0.01);
    assert(!sent.empty());
    assert(sent.front() >= 64 && sent.front() <= 66);  // starts from the held value
    assert(sent.back() == 110);                        // ends exactly on target
    assert(!s.active());
}

void testSweepIsMonotonicWithoutRepeats() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, false);
    const std::vector<uint8_t> sent = runSweep(s, 0.0, 5.0, 0.001);
    for (std::size_t i = 1; i < sent.size(); ++i) {
        assert(sent[i] > sent[i - 1]);  // rising target: strictly rising, no repeats
    }
    assert(sent.size() <= 110 - 64 + 1);  // bounded by the value range, never flooded
}

void testSweepDownwardAlsoMonotonic() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(110, v);
    s.begin(40, 0.0, false);
    const std::vector<uint8_t> sent = runSweep(s, 0.0, 5.0, 0.001);
    assert(!sent.empty() && sent.back() == 40);
    for (std::size_t i = 1; i < sent.size(); ++i) assert(sent[i] < sent[i - 1]);
}

void testCompletesExactlyAtFourBarsAndStaysQuiet() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, false);
    uint8_t out = 0;
    assert(s.poll(3.999, out));     // still inside the sweep
    assert(out < 110);
    assert(s.poll(4.0, out) && out == 110);  // completion sends the target once
    assert(!s.active());
    for (double bar = 4.0; bar < 12.0; bar += 0.25) {
        assert(!s.poll(bar, out));  // no further CC after completion
    }
}

void testNoSendWhenTargetEqualsStart() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(110, v);
    s.begin(110, 0.0, false);
    const std::vector<uint8_t> sent = runSweep(s, 0.0, 5.0, 0.05);
    assert(sent.empty());                        // unchanged value: no CC at all
    assert(!s.active());                         // and the sweep still finishes
}

void testManualChangeCancelsSweep() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, false);
    uint8_t out = 0;
    assert(s.poll(1.0, out));
    assert(s.setManual(90, v) && v == 90);   // manual edit takes full control
    assert(!s.active());
    for (double bar = 1.0; bar < 6.0; bar += 0.1) {
        assert(!s.poll(bar, out));           // the sweep never resumes or overrides
    }
}

void testStopCancelsAndKeepsValue() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, false);
    uint8_t out = 0;
    assert(s.poll(2.0, out));
    const uint8_t held = s.value();
    s.cancel();                              // transport stop
    assert(!s.active());
    assert(s.value() == held);               // no return to 64, no unexpected value
    for (double bar = 2.0; bar < 8.0; bar += 0.1) assert(!s.poll(bar, out));
}

void testNoAutoRestartAfterCompletion() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, false);
    uint8_t out = 0;
    s.poll(4.0, out);
    assert(!s.active());
    // A later bar position alone must not start anything.
    assert(!s.poll(8.0, out));
}

void testBeforeStartBarIsClampedToStart() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 5.0, false);
    uint8_t out = 0;
    assert(!s.poll(4.0, out) || out == 64);  // a stale bar before the start is not motion
}

}  // namespace


// ---- CutoffAutomation: musical time, stop, restart and requests ----

namespace {

using GroovePuterMidi::CutoffAutomation;
using GroovePuterMidi::CutoffRequest;
using GroovePuterMidi::CutoffTransportView;

CutoffTransportView playing(double bar, uint32_t epoch = 1) {
    CutoffTransportView t;
    t.valid = true;
    t.playing = true;
    t.epoch = epoch;
    t.bar = bar;
    return t;
}

CutoffTransportView stopped(double bar, uint32_t epoch = 1) {
    CutoffTransportView t = playing(bar, epoch);
    t.playing = false;
    return t;
}

void testAutomationSweepsOverFourBarsAndCompletes() {
    CutoffAutomation a;
    uint8_t v = 0;
    assert(a.manual(64, v) && v == 64);
    assert(a.startSweep(110, playing(20.0), false));
    std::vector<uint8_t> sent;
    for (double bar = 20.0; bar <= 25.0; bar += 0.01) {
        if (a.tick(playing(bar), v)) sent.push_back(v);
    }
    assert(!sent.empty() && sent.back() == 110);
    assert(!a.sweepActive());
}

void testStopCancelsAndHoldsValue() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0), false));
    assert(a.tick(playing(1.0), v));
    const uint8_t held = a.value();
    assert(!a.tick(stopped(1.5), v));       // stop: nothing sent
    assert(!a.sweepActive());
    assert(a.value() == held);              // no snap back to 64
    for (double bar = 1.5; bar < 8.0; bar += 0.1) assert(!a.tick(playing(bar), v));
}

void testStopCancelsAndNeverResumes() {
    // Stop (and pause) cancel; a sweep is never armed again by itself.
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0, 1), false));
    assert(a.tick(playing(1.0, 1), v));
    assert(!a.tick(stopped(1.2, 1), v));
    assert(!a.sweepActive());
    for (double bar = 1.2; bar < 8.0; bar += 0.1) assert(!a.tick(playing(bar, 2), v));
}

void testStartWhileStoppedIsRefused() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(!a.startSweep(110, stopped(0.0), false));
    assert(!a.sweepActive());
}

void testManualEditDuringSweepTakesOver() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0), false));
    assert(a.tick(playing(1.0), v));
    assert(a.manual(70, v) && v == 70);
    assert(!a.sweepActive());
    for (double bar = 1.0; bar < 6.0; bar += 0.1) assert(!a.tick(playing(bar), v));
}

void testRequestCodecRoundTrips() {
    const uint32_t w = CutoffRequest::encode(CutoffRequest::kManual, 110, 5);
    assert(CutoffRequest::op(w) == CutoffRequest::kManual);
    assert(CutoffRequest::value(w) == 110);
    assert(CutoffRequest::sequence(w) == 5);
    const uint32_t c = CutoffRequest::encode(CutoffRequest::kCancelSweep, 0, 6);
    assert(CutoffRequest::op(c) == CutoffRequest::kCancelSweep);
    assert(CutoffRequest::sequence(c) == 6);
    // A zero sequence is coerced to 1, so no encoded request equals "none".
    assert(CutoffRequest::encode(CutoffRequest::kManual, 1, 0) != 0u);
}

}  // namespace

namespace {
void testBarsElapsedAndPublishedLayout() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.barsElapsed(0.0) == 0);             // no sweep: nothing elapsed
    assert(a.startSweep(110, playing(10.0), false));
    uint8_t tick = 0;
    // The dispatcher ticks before it publishes; progress is the bars really passed.
    a.tick(playing(10.5), tick);
    assert(a.barsElapsed(0.0) == 0);
    a.tick(playing(12.2), tick);
    assert(a.barsElapsed(0.0) == 2);
    a.tick(playing(13.9), tick);
    assert(a.barsElapsed(0.0) == 3);
    a.tick(playing(14.0), tick);                 // the fourth bar completes the sweep
    assert(!a.sweepActive() && a.barsElapsed(0.0) == 0);
    assert(a.startSweep(110, playing(20.0), false));
    a.tick(playing(21.0), tick);
    a.tick(playing(99.0), tick);                 // a huge jump is a gap: not counted
    assert(a.barsElapsed(0.0) == 1);
    a.cancelSweep();
    assert(a.barsElapsed(0.0) == 0);             // cancelled: nothing elapsed

    GroovePuterMidi::CutoffControlChannel channel;
    channel.publish(96, true, 3, true, 2, true);
    assert(channel.value() == 96 && channel.sweeping() && channel.barsElapsed() == 3 && channel.known());
    assert(channel.controller() == 2 && channel.returning());
    channel.publish(40, false, 0, false, 0, false);
    assert(channel.value() == 40 && !channel.sweeping() && channel.barsElapsed() == 0 && !channel.known());
}

}  // namespace

namespace {
void testSweepNeedsAKnownValue() {
    CutoffAutomation a;
    assert(!a.known());
    assert(!a.startSweep(110, playing(0.0), false));  // a fresh device has no known value
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.known());
    assert(a.startSweep(110, playing(0.0), false));
}

}  // namespace

namespace {
void testReturnShapeGoesThereAndBack() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0, true);
    uint8_t out = 0;
    std::vector<uint8_t> sent;
    for (double bar = 0.0; bar <= 5.0; bar += 0.001) {
        if (s.poll(bar, out)) sent.push_back(out);
    }
    assert(!sent.empty() && sent.back() == 64);  // ends where it started
    uint8_t peak = 0;
    for (uint8_t x : sent) if (x > peak) peak = x;
    assert(peak == 110);                         // reaches the target at the midpoint
    // First leg rises, second leg falls, with no repeats inside a leg.
    std::size_t turn = 0;
    while (turn < sent.size() && sent[turn] < 110) ++turn;
    for (std::size_t i = 1; i < turn; ++i) assert(sent[i] > sent[i - 1]);
    for (std::size_t i = turn + 1; i < sent.size(); ++i) assert(sent[i] < sent[i - 1]);
    assert(!s.active());
    for (double bar = 4.0; bar < 9.0; bar += 0.25) assert(!s.poll(bar, out));
}

void testControllerTableAndIndex() {
    using GroovePuterMidi::controllerIndex;
    using GroovePuterMidi::kControllerCount;
    assert(kControllerCount == 7);
    assert(controllerIndex(74) == 0);       // CUTOFF is the default
    assert(controllerIndex(73) == 1);       // ATTACK
    assert(controllerIndex(71) == -1);      // RESO is not offered
    assert(controllerIndex(10) == 6);       // PAN
    assert(controllerIndex(5) == -1);       // portamento: owned by the slide path
    assert(controllerIndex(65) == -1);
    assert(controllerIndex(27) == -1);      // arpeggiator
    assert(controllerIndex(116) == -1);     // DX only
    assert(GroovePuterMidi::kControllers[6].minValue == 1);  // PAN starts at 1
    using GroovePuterMidi::kDurationCount;
    using GroovePuterMidi::kDurations;
    assert(kDurationCount == 6);
    assert(kDurations[0] == 1 && kDurations[3] == 8 && kDurations[4] == 16 && kDurations[5] == 32);
}

void testRequestCodecCarriesControllerAndShape() {
    using GroovePuterMidi::CutoffRequest;
    const uint32_t w = CutoffRequest::encode(CutoffRequest::kSelectController, 0, 9, 6);
    assert(CutoffRequest::op(w) == CutoffRequest::kSelectController);
    assert(CutoffRequest::controller(w) == 6);
    assert(CutoffRequest::sequence(w) == 9);
    const uint32_t shape = CutoffRequest::encode(CutoffRequest::kSetShape, 1, 10);
    assert(CutoffRequest::op(shape) == CutoffRequest::kSetShape && CutoffRequest::value(shape) == 1);
}

void testRestoreSelectsWithoutSending() {
    CutoffAutomation a;
    a.restore(90, true);
    assert(a.known() && a.value() == 90 && !a.sweepActive());
    a.restore(0, false);
    assert(!a.known());
    assert(!a.startSweep(110, playing(0.0), false));  // unknown controller: no sweep
}

}  // namespace

namespace {
void testSweepLengthIsChosenPerSweep() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0), false, 8));
    uint8_t out = 0;
    a.tick(playing(4.0), out);
    assert(a.barsElapsed(0.0) == 4);             // half-way through eight bars
    a.tick(playing(7.9), out);
    assert(a.barsElapsed(0.0) == 7);
    const bool reached = a.tick(playing(8.0), out);  // completes at the eighth bar, not before
    assert(reached && out == 110 && !a.sweepActive());
    // The codec and the published word carry the length index.
    using GroovePuterMidi::CutoffRequest;
    const uint32_t w = CutoffRequest::encode(CutoffRequest::kSetDuration, 3, 12);
    assert(CutoffRequest::op(w) == CutoffRequest::kSetDuration && CutoffRequest::value(w) == 3);
    GroovePuterMidi::CutoffControlChannel channel;
    channel.publish(70, true, 8, true, 1, false, 3);
    assert(channel.barsElapsed() == 8 && channel.durationIndex() == 3 && channel.controller() == 1);
    // Long passes: 16 and 32 bars, the 6-bit counter and the 3-bit duration index.
    channel.publish(90, true, 32, true, 6, true, 5, 6);
    assert(channel.barsElapsed() == 32 && channel.durationIndex() == 5 && channel.controller() == 6);
    assert(channel.owner() == 6 && channel.returning() && channel.known() && channel.sweeping());
    assert(channel.value() == 90);
    channel.publish(70, true, 16, true, 2, false, 4, 1);
    assert(channel.barsElapsed() == 16 && channel.durationIndex() == 4 && channel.owner() == 1);
    assert(!channel.returning() && channel.controller() == 2);
}

}  // namespace

namespace {
void testSweepSurvivesTheBarCounterResetting() {
    // A one-bar loop, or a transport epoch bump mid-play, can move the project's bar
    // position backwards. The sweep counts the bars it has really passed, so it must
    // keep going and reach the target, without freezing on the value it had.
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(10.0, 1), false, 2));
    uint8_t out = 0;
    // one bar of play, then the counter jumps back to the start of the project
    for (double bar = 10.0; bar < 11.0; bar += 0.05) a.tick(playing(bar, 1), out);
    a.tick(playing(0.25, 2), out);                   // reset: a new epoch, bar 0
    assert(a.sweepActive());                         // not cancelled by the epoch
    bool reachedTarget = false;
    for (double bar = 0.25; bar < 3.0; bar += 0.05) {
        if (a.tick(playing(bar, 2), out) && out == 110) reachedTarget = true;
    }
    assert(reachedTarget && !a.sweepActive());
}

}  // namespace

namespace {
using GroovePuterMidi::CutoffVoice;

// Counts the levels of the given controller index among the voice's current levels.
int levelsFor(const CutoffVoice& v, uint8_t index, uint8_t* value = nullptr) {
    CutoffVoice::Level out[2];
    const uint8_t n = v.levels(out);
    int found = 0;
    for (uint8_t i = 0; i < n; ++i) {
        if (out[i].index == index) {
            ++found;
            if (value) *value = out[i].value;
        }
    }
    return found;
}

void testSweepKeepsRunningWhenAnotherControllerIsEdited() {
    // SWEEP on CUTOFF, then the user selects ATTACK and turns it: the CUTOFF sweep must
    // keep going, and ATTACK's own value must reach the wire next to it.
    CutoffVoice v;
    v.manual(64);                                     // CUTOFF known at 64
    assert(v.startSweep(110, playing(0.0), false, 4));
    v.select(1);                                      // ATTACK: edited, not swept
    v.manual(100);
    assert(v.sweeping() && v.owner() == 0);           // the sweep was not cancelled
    assert(v.selected() == 1 && v.value() == 100 && v.known());
    uint8_t cutoff = 0;
    uint8_t reso = 0;
    v.tick(playing(2.0));
    assert(levelsFor(v, 0, &cutoff) == 1 && cutoff > 64 && cutoff < 110);
    assert(levelsFor(v, 1, &reso) == 1 && reso == 100);
    // The sweep reaches its target; CUTOFF's final value is still a level, so the
    // dispatcher delivers it, and ATTACK keeps its own value.
    v.tick(playing(4.0));
    assert(!v.sweeping());
    assert(levelsFor(v, 0, &cutoff) == 1 && cutoff == 110);
    assert(levelsFor(v, 1, &reso) == 1 && reso == 100);
}

void testManualOnTheSweptControllerStillCancels() {
    CutoffVoice v;
    v.manual(64);
    assert(v.startSweep(110, playing(0.0), false, 4));
    v.manual(20);                                     // editing the sweeping controller
    assert(!v.sweeping() && v.value() == 20);
}

void testNewSweepOnAnotherControllerKeepsTheOldValue() {
    CutoffVoice v;
    v.manual(64);
    assert(v.startSweep(110, playing(0.0), false, 4));
    v.tick(playing(2.0));
    const uint8_t mid = v.value();                    // CUTOFF mid-sweep
    v.select(1);
    v.manual(40);                                     // ATTACK set first
    assert(v.startSweep(90, playing(2.0), false, 4)); // a sweep on ATTACK takes over
    assert(v.sweeping() && v.owner() == 1);
    v.select(0);                                      // back to CUTOFF: its value was kept
    assert(v.value() == mid && v.known());
    assert(levelsFor(v, 1) == 1);                     // ATTACK sweeps, CUTOFF is held
}

void testNothingIsSentBeforeAValueIsSet() {
    CutoffVoice v;
    CutoffVoice::Level out[2];
    assert(v.levels(out) == 0);
    v.select(1);
    assert(v.levels(out) == 0);
    v.manual(30);
    assert(v.levels(out) == 1 && out[0].index == 1 && out[0].value == 30);
}

void testPublishedWordCarriesTheSweepOwner() {
    // The page shows the selected controller's value next to the owner's sweep, so
    // it must be able to tell them apart.
    GroovePuterMidi::CutoffControlChannel channel;
    channel.publish(100, true, 2, true, 1, false, 2, 0);   // ATTACK selected, CUTOFF sweeps
    assert(channel.controller() == 1 && channel.owner() == 0);
    assert(channel.value() == 100 && channel.sweeping() && channel.barsElapsed() == 2);
    assert(channel.durationIndex() == 2 && channel.known() && !channel.returning());
    channel.publish(100, false, 0, true, 6, true, 3, 6);
    assert(channel.controller() == 6 && channel.owner() == 6 && channel.durationIndex() == 3);
    channel.publish(70, true, 8, true, 5, false, 3);       // owner omitted: the selected one
    assert(channel.owner() == 5);
}

}  // namespace

int main() {
    testManualMapsValuesAndClamps();
    testSweepStartsFromHeldValueAndEndsOnTarget();
    testSweepIsMonotonicWithoutRepeats();
    testSweepDownwardAlsoMonotonic();
    testCompletesExactlyAtFourBarsAndStaysQuiet();
    testNoSendWhenTargetEqualsStart();
    testManualChangeCancelsSweep();
    testStopCancelsAndKeepsValue();
    testNoAutoRestartAfterCompletion();
    testBeforeStartBarIsClampedToStart();
    testAutomationSweepsOverFourBarsAndCompletes();
    testStopCancelsAndHoldsValue();
    testStopCancelsAndNeverResumes();
    testStartWhileStoppedIsRefused();
    testManualEditDuringSweepTakesOver();
    testRequestCodecRoundTrips();
    testBarsElapsedAndPublishedLayout();
    testSweepNeedsAKnownValue();
    testReturnShapeGoesThereAndBack();
    testControllerTableAndIndex();
    testRequestCodecCarriesControllerAndShape();
    testRestoreSelectsWithoutSending();
    testSweepLengthIsChosenPerSweep();
    testSweepSurvivesTheBarCounterResetting();
    testSweepKeepsRunningWhenAnotherControllerIsEdited();
    testManualOnTheSweptControllerStillCancels();
    testNewSweepOnAnotherControllerKeepsTheOldValue();
    testNothingIsSentBeforeAValueIsSet();
    testPublishedWordCarriesTheSweepOwner();
    return 0;
}
