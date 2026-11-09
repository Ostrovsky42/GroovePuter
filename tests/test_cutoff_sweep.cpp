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
    s.begin(110, 10.0);
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
    s.begin(110, 0.0);
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
    s.begin(40, 0.0);
    const std::vector<uint8_t> sent = runSweep(s, 0.0, 5.0, 0.001);
    assert(!sent.empty() && sent.back() == 40);
    for (std::size_t i = 1; i < sent.size(); ++i) assert(sent[i] < sent[i - 1]);
}

void testCompletesExactlyAtFourBarsAndStaysQuiet() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0);
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
    s.begin(110, 0.0);
    const std::vector<uint8_t> sent = runSweep(s, 0.0, 5.0, 0.05);
    assert(sent.empty());                        // unchanged value: no CC at all
    assert(!s.active());                         // and the sweep still finishes
}

void testManualChangeCancelsSweep() {
    CutoffSweep s;
    uint8_t v = 0;
    s.setManual(64, v);
    s.begin(110, 0.0);
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
    s.begin(110, 0.0);
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
    s.begin(110, 0.0);
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
    s.begin(110, 5.0);
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
    assert(a.startSweep(110, playing(20.0)));
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
    assert(a.startSweep(110, playing(0.0)));
    assert(a.tick(playing(1.0), v));
    const uint8_t held = a.value();
    assert(!a.tick(stopped(1.5), v));       // stop: nothing sent
    assert(!a.sweepActive());
    assert(a.value() == held);              // no snap back to 64
    for (double bar = 1.5; bar < 8.0; bar += 0.1) assert(!a.tick(playing(bar), v));
}

void testRestartEpochCancelsAndNeverResumes() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0, 1)));
    assert(a.tick(playing(1.0, 1), v));
    assert(!a.tick(playing(1.2, 2), v));    // new Start/Continue epoch: cancelled
    for (double bar = 1.2; bar < 8.0; bar += 0.1) assert(!a.tick(playing(bar, 2), v));
}

void testStartWhileStoppedIsRefused() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(!a.startSweep(110, stopped(0.0)));
    assert(!a.sweepActive());
}

void testManualEditDuringSweepTakesOver() {
    CutoffAutomation a;
    uint8_t v = 0;
    a.manual(64, v);
    assert(a.startSweep(110, playing(0.0)));
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
    testRestartEpochCancelsAndNeverResumes();
    testStartWhileStoppedIsRefused();
    testManualEditDuringSweepTakesOver();
    testRequestCodecRoundTrips();
    return 0;
}
