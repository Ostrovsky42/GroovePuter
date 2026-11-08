// LOOPER phase 1: the engine (docs/design/LOOPER_PLAN.md, "Фаза 1").
//
// A rig with a mocked clock (now), a mocked project transport (published per
// audio block, 0..2 blocks late, as test_cr_loop_gaps.cpp) and a router that
// records every event. The sink checks NoteOn/NoteOff balance on every event:
// a NoteOff only for a sounding (channel, lane, pitch), never two NoteOns of
// one pitch/channel without a NoteOff between (so at a pass boundary the
// NoteOff of the previous pass always comes before the NoteOn of the next).
//
//   T1 transition table: state x {Z, C, V, Ctrl+Z, B, Del, X, Space start,
//      Space stop, BARS, page exit, pass end, first note}, HOLD locks,
//      HOLD armed in RECORD, "then DUB" in RECORD;
//   T2 clock: first note = step 0, the free clock never stops on silent
//      passes, ARMED under PLAY waits for the next bar, the bar phase is kept
//      across Space stop/start, tempo 120 -> 132 mid-loop;
//   T3 balance on every transition in the middle of a sounding note;
//   T4 no gaps: FREE/PLAY x loop pass 1/5/20/60 ms x BARS 1/2/4/8 x 8 shapes
//      (64-note shapes at BARS 8: a pass in several plans of <= 32), plus
//      10000 consecutive passes;
//   T5 overdub with chords, V removes exactly the last pass, Del + V;
//   T6 the borrowed player: nothing starts while the PARTNER is enabled;
//   T7 BARS 8: the cycle 1-2-4-8, 128-step passes, the bar phase on an
//      8-bar loop, 64 notes then FULL, every note in every pass;
//   T8 two layers (phase 3): M, DUB into layer 2 never touches layer 1,
//      undo of layer 2 only, HOLD per layer (layer 1 locked, layer 2 records
//      and clears), MUTE heard at once, CLEAR of one layer, balance;
//   T9 BARS with notes: no LoopNotEmpty any more; 1 -> 2 -> 4 -> 8 repeats
//      the loop (every pass plays the repeated notes), shorter cuts, V takes
//      it back (bars too), no room, BARS mid-note on both clocks: balance.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <tuple>
#include <vector>

#include "src/input/looper.h"

using namespace GroovePuterLooper;
using GroovePuterCallResponse::TransportView;
using State = Looper::State;
using Result = Looper::Result;

namespace {

int gChecks = 0;
#define CHECK(cond) do { ++gChecks; if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); std::abort(); } } while (0)
#define CHECK_MSG(cond, ...) do { ++gChecks; if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s :: ", __FILE__, __LINE__, #cond); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); std::abort(); } } while (0)

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }

constexpr uint32_t kSixteenth = 125000;  // 120 BPM
constexpr uint32_t kT0 = 1000;
constexpr uint32_t kBlockMicros = 11610;
constexpr double kFastSixteenth = 15000000.0 / 132.0;

struct Ev {
    MusicalEventType type;
    uint8_t channel;
    uint8_t note;
    bool lane;
    uint32_t t;
};

class BalanceSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& e) override {
        if (e.type != MusicalEventType::NoteOn && e.type != MusicalEventType::NoteOff) return;
        const bool lane = (e.flags & kMusicalEventWireChannel) != 0;
        const int key = (lane ? 4096 : 0) + e.channel * 128 + e.note;
        int& n = sounding[key];
        if (e.type == MusicalEventType::NoteOn) {
            CHECK_MSG(n == 0, "double NoteOn note=%u ch=%u t=%u", e.note, e.channel, gNow);
            ++n;
            ++total;
        } else {
            CHECK_MSG(n == 1, "NoteOff without NoteOn note=%u ch=%u t=%u", e.note, e.channel, gNow);
            --n;
            --total;
        }
        events.push_back(Ev{e.type, e.channel, e.note, lane, gNow});
    }
    int countOns(uint32_t from, uint32_t to) const {
        int c = 0;
        for (const Ev& e : events) if (e.type == MusicalEventType::NoteOn && e.t >= from && e.t < to) ++c;
        return c;
    }
    std::map<int, int> sounding;
    int total{0};
    std::vector<Ev> events;
};

struct Rig {
    MusicalEventRouter router;
    BalanceSink sink;
    ResponsePlayer player;
    Looper lp;
    bool transportOn{false};
    uint32_t transportStart{0};
    uint32_t epoch{1};
    uint32_t block{0};
    uint32_t passMicros{5000};
    uint32_t tempoChangeAt{0};
    std::vector<uint32_t> downbeats;
    std::vector<size_t> downbeatEvents;  // sink.events index at each downbeat

    Rig() {
        assert(router.addSink(sink));
        player.setRouter(&router);
        lp.setPlayer(&player);
        lp.setNowFunction(fakeNow);
        gNow = kT0;
        tick(kT0);
    }
    Rig(const Rig&) = delete;

    static uint32_t jitter(uint32_t index) {
        uint32_t h = index * 2654435761u;
        h ^= h >> 15;
        return h % (2 * kBlockMicros);
    }
    TransportView view(uint32_t now) {
        TransportView v{};
        if (!transportOn) return v;
        v.playing = true;
        v.epoch = epoch;
        while (true) {
            const uint32_t next = block + 1;
            const uint32_t visibleAt = transportStart + next * kBlockMicros + jitter(next);
            if (static_cast<int32_t>(now - visibleAt) < 0) break;
            block = next;
        }
        v.blockSequence = block + 1;
        const uint32_t blockStart = transportStart + block * kBlockMicros;
        if (tempoChangeAt == 0 || static_cast<int32_t>(blockStart - tempoChangeAt) < 0) {
            v.steps = static_cast<double>(blockStart - transportStart) / kSixteenth;
        } else {
            v.steps = static_cast<double>(tempoChangeAt - transportStart) / kSixteenth +
                      static_cast<double>(blockStart - tempoChangeAt) / kFastSixteenth;
        }
        v.bpm = tempoChangeAt != 0 && static_cast<int32_t>(now - tempoChangeAt) >= 0 ? 132.0 : 120.0;
        return v;
    }
    void tick(uint32_t now) {
        gNow = now;
        const TransportView v = view(now);
        const size_t mark = sink.events.size();
        if (lp.service(now, &v, 120.0f)) {
            downbeats.push_back(lp.passDownbeatMicros());
            downbeatEvents.push_back(mark);
        }
    }
    void runUntil(uint32_t t) {
        while (static_cast<int32_t>(t - gNow) > 0) {
            const uint32_t step = static_cast<int32_t>(t - gNow) > static_cast<int32_t>(passMicros) ? passMicros : t - gNow;
            tick(gNow + step);
        }
    }
    void runSteps(double steps) { runUntil(gNow + static_cast<uint32_t>(steps * kSixteenth)); }
    void startTransport() {
        transportOn = true;
        transportStart = gNow;
        block = 0;
        ++epoch;
        tick(gNow);
    }
    void stopTransport() {
        transportOn = false;
        tick(gNow);
    }
    uint32_t transportStepTime(double step) const {
        return transportStart + static_cast<uint32_t>(step * kSixteenth + 0.5);
    }
    void press(uint16_t tag, uint8_t note, uint8_t vel = 100) { lp.onPerformanceInputNote(true, tag, note, vel); }
    void release(uint16_t tag) { lp.onPerformanceInputNote(false, tag, 0, 0); }
    bool silent() const { return sink.total == 0 && player.soundingCount() == 0; }
};

// ---------------------------------------------------------------- T1

enum class Setup : uint8_t { Idle, Armed, Record, Play, Overdub, Stopped, Hold };
enum class Event : uint8_t { Z, C, V, CtrlZ, B, Del, X, SpaceStart, SpaceStop, Bars, PageExit, PassEnd, FirstNote };
const char* setupName(Setup s) {
    static const char* n[] = {"IDLE", "ARMED", "RECORD", "PLAY", "OVERDUB", "STOPPED", "HOLD"};
    return n[static_cast<int>(s)];
}
const char* eventName(Event e) {
    static const char* n[] = {"Z", "C", "V", "CtrlZ", "B", "Del", "X", "SpaceStart", "SpaceStop", "BARS",
                              "PageExit", "PassEnd", "FirstNote"};
    return n[static_cast<int>(e)];
}

// One note (pitch 60, steps 0..2) recorded; transport: on the next bar.
void recordOne(Rig& r) {
    CHECK(r.lp.recDub() == Result::Done);
    CHECK(r.lp.state() == State::Armed);
    if (r.transportOn) r.runUntil(r.transportStepTime(16.0) + 2000);
    r.press(1, 60);
    CHECK(r.lp.state() == State::Record);
    r.runSteps(2.0);
    r.release(1);
}

void setup(Rig& r, Setup s, bool transport) {
    if (transport) {
        r.startTransport();
        r.runSteps(1.0);
    }
    if (s == Setup::Idle) return;
    if (s == Setup::Armed) { CHECK(r.lp.recDub() == Result::Done); return; }
    recordOne(r);
    if (s == Setup::Record) return;
    r.runSteps(r.lp.passSteps() - 1.0);  // RECORD -> PLAY, 1 step into pass 2
    CHECK(r.lp.state() == State::Play);
    if (s == Setup::Play) return;
    if (s == Setup::Overdub) {
        CHECK(r.lp.recDub() == Result::Done);
        r.press(2, 64);
        r.runSteps(1.0);
        r.release(2);
        CHECK(r.lp.state() == State::Overdub && r.lp.noteCount() == 2);
        return;
    }
    if (s == Setup::Stopped) { CHECK(r.lp.playStop() == Result::Done); return; }
    if (s == Setup::Hold) { CHECK(r.lp.toggleHold() == Result::Done && r.lp.hold()); return; }
}

struct Cell {
    Setup setup;
    Event event;
    State state;
    bool hold;
    int notes;  // -1: not checked
};

void apply(Rig& r, Event e) {
    switch (e) {
        case Event::Z: (void)r.lp.recDub(); break;
        case Event::C: (void)r.lp.playStop(); break;
        case Event::V: (void)r.lp.undo(); break;
        case Event::CtrlZ: if (r.lp.canUndo()) (void)r.lp.undo(); break;  // as PerformPage
        case Event::B: (void)r.lp.toggleHold(); break;
        case Event::Del: (void)r.lp.clear(); break;
        case Event::X: (void)r.lp.panicStop(); break;
        case Event::SpaceStart: r.startTransport(); break;
        case Event::SpaceStop: r.stopTransport(); break;
        case Event::Bars: (void)r.lp.cycleBars(1); break;
        case Event::PageExit: r.lp.onPageExit(); break;
        case Event::PassEnd: r.runSteps(r.lp.passSteps()); break;
        case Event::FirstNote: r.press(9, 70); r.runSteps(1.0); r.release(9); break;
    }
}

void testTransitionTable() {
    const State I = State::Idle, A = State::Armed, R = State::Record, P = State::Play,
                O = State::Overdub, S = State::Stopped;
    const Cell cells[] = {
        // IDLE
        {Setup::Idle, Event::Z, A, false, 0}, {Setup::Idle, Event::C, I, false, 0},
        {Setup::Idle, Event::V, I, false, 0}, {Setup::Idle, Event::CtrlZ, I, false, 0},
        {Setup::Idle, Event::B, I, false, 0}, {Setup::Idle, Event::Del, I, false, 0},
        {Setup::Idle, Event::X, I, false, 0}, {Setup::Idle, Event::SpaceStart, I, false, 0},
        {Setup::Idle, Event::SpaceStop, I, false, 0}, {Setup::Idle, Event::Bars, I, false, 0},
        {Setup::Idle, Event::PageExit, I, false, 0}, {Setup::Idle, Event::PassEnd, I, false, 0},
        {Setup::Idle, Event::FirstNote, I, false, 0},
        // ARMED
        {Setup::Armed, Event::Z, I, false, 0}, {Setup::Armed, Event::C, I, false, 0},
        {Setup::Armed, Event::V, A, false, 0}, {Setup::Armed, Event::CtrlZ, A, false, 0},
        {Setup::Armed, Event::B, A, false, 0}, {Setup::Armed, Event::Del, A, false, 0},
        {Setup::Armed, Event::X, I, false, 0}, {Setup::Armed, Event::SpaceStart, R, false, 0},  // bar 1 = now
        {Setup::Armed, Event::SpaceStop, A, false, 0}, {Setup::Armed, Event::Bars, A, false, 0},
        {Setup::Armed, Event::PageExit, I, false, 0}, {Setup::Armed, Event::PassEnd, A, false, 0},
        {Setup::Armed, Event::FirstNote, R, false, 1},
        // RECORD (one note)
        {Setup::Record, Event::Z, R, false, 1}, {Setup::Record, Event::C, S, false, 1},
        {Setup::Record, Event::V, I, false, 0}, {Setup::Record, Event::CtrlZ, I, false, 0},
        {Setup::Record, Event::B, R, false, 1}, {Setup::Record, Event::Del, I, false, 0},
        {Setup::Record, Event::X, S, false, 1}, {Setup::Record, Event::SpaceStart, R, false, 0},  // dropped, again from bar 1
        {Setup::Record, Event::SpaceStop, S, false, 1}, {Setup::Record, Event::Bars, R, false, 1},
        {Setup::Record, Event::PageExit, S, false, 1}, {Setup::Record, Event::PassEnd, P, false, 1},
        {Setup::Record, Event::FirstNote, R, false, 2},
        // PLAY
        {Setup::Play, Event::Z, O, false, 1}, {Setup::Play, Event::C, S, false, 1},
        {Setup::Play, Event::V, I, false, 0}, {Setup::Play, Event::CtrlZ, I, false, 0},
        {Setup::Play, Event::B, P, true, 1}, {Setup::Play, Event::Del, I, false, 0},
        {Setup::Play, Event::X, S, false, 1}, {Setup::Play, Event::SpaceStart, P, false, 1},
        {Setup::Play, Event::SpaceStop, S, false, 1}, {Setup::Play, Event::Bars, P, false, 2},  // 2 -> 4 bars: repeated
        {Setup::Play, Event::PageExit, S, false, 1}, {Setup::Play, Event::PassEnd, P, false, 1},
        {Setup::Play, Event::FirstNote, P, false, 1},
        // OVERDUB (two notes, the second in this pass)
        {Setup::Overdub, Event::Z, P, false, 2}, {Setup::Overdub, Event::C, S, false, 2},
        {Setup::Overdub, Event::V, O, false, 1}, {Setup::Overdub, Event::CtrlZ, O, false, 1},
        {Setup::Overdub, Event::B, P, true, 2}, {Setup::Overdub, Event::Del, I, false, 0},
        {Setup::Overdub, Event::X, S, false, 2}, {Setup::Overdub, Event::SpaceStart, P, false, 2},
        {Setup::Overdub, Event::SpaceStop, S, false, 2}, {Setup::Overdub, Event::Bars, O, false, 4},
        {Setup::Overdub, Event::PageExit, S, false, 2}, {Setup::Overdub, Event::PassEnd, O, false, 2},
        {Setup::Overdub, Event::FirstNote, O, false, 3},
        // STOPPED (by C)
        {Setup::Stopped, Event::Z, O, false, 1}, {Setup::Stopped, Event::C, P, false, 1},
        {Setup::Stopped, Event::V, I, false, 0}, {Setup::Stopped, Event::CtrlZ, I, false, 0},
        {Setup::Stopped, Event::B, S, true, 1}, {Setup::Stopped, Event::Del, I, false, 0},
        {Setup::Stopped, Event::X, S, false, 1}, {Setup::Stopped, Event::SpaceStart, S, false, 1},
        {Setup::Stopped, Event::SpaceStop, S, false, 1}, {Setup::Stopped, Event::Bars, S, false, 2},
        {Setup::Stopped, Event::PageExit, S, false, 1}, {Setup::Stopped, Event::PassEnd, S, false, 1},
        {Setup::Stopped, Event::FirstNote, S, false, 1},
        // HOLD (PLAY + B): Z, V, Ctrl+Z, Del, BARS change nothing
        {Setup::Hold, Event::Z, P, true, 1}, {Setup::Hold, Event::C, S, true, 1},
        {Setup::Hold, Event::V, P, true, 1}, {Setup::Hold, Event::CtrlZ, P, true, 1},
        {Setup::Hold, Event::B, P, false, 1}, {Setup::Hold, Event::Del, P, true, 1},
        {Setup::Hold, Event::X, S, true, 1}, {Setup::Hold, Event::SpaceStart, P, true, 1},
        {Setup::Hold, Event::SpaceStop, S, true, 1}, {Setup::Hold, Event::Bars, P, true, 1},
        {Setup::Hold, Event::PageExit, S, true, 1}, {Setup::Hold, Event::PassEnd, P, true, 1},
        {Setup::Hold, Event::FirstNote, P, true, 1},
    };
    int n = 0;
    for (const Cell& c : cells) {
        Rig r;
        // Space stop needs PLAY running before; everything else runs on the
        // free clock (Space start then switches to PLAY).
        setup(r, c.setup, c.event == Event::SpaceStop);
        const uint8_t barsBefore = r.lp.bars();
        apply(r, c.event);
        CHECK_MSG(r.lp.state() == c.state && r.lp.hold() == c.hold,
                  "%s + %s -> %s hold=%d (expected %d hold=%d)", setupName(c.setup), eventName(c.event),
                  r.lp.stateName(), r.lp.hold(), static_cast<int>(c.state), c.hold);
        if (c.notes >= 0)
            CHECK_MSG(r.lp.noteCount() == c.notes, "%s + %s notes=%u expected %d", setupName(c.setup),
                      eventName(c.event), r.lp.noteCount(), c.notes);
        if (c.event == Event::Bars) {
            // Phase 3: BARS keeps the notes; refused only while recording or locked.
            const bool canChange = c.setup != Setup::Record && c.setup != Setup::Hold;
            CHECK_MSG((r.lp.bars() != barsBefore) == canChange, "%s + BARS", setupName(c.setup));
        }
        if (c.state == State::Idle || c.state == State::Armed || c.state == State::Stopped)
            CHECK_MSG(r.silent(), "%s + %s: notes still sounding", setupName(c.setup), eventName(c.event));
        // Two more passes: balance holds (checked per event), still silent when stopped.
        r.runSteps(2.0 * r.lp.passSteps());
        if (r.lp.state() == State::Idle || r.lp.state() == State::Stopped) CHECK(r.silent());
        ++n;
    }
    CHECK(n == 7 * 13);

    // HOLD locks: the refusal is reported (toast "HOLD: B TO UNLOCK").
    {
        Rig r;
        setup(r, Setup::Hold, false);
        CHECK(r.lp.recDub() == Result::HoldLocked);
        CHECK(r.lp.undo() == Result::HoldLocked);
        CHECK(r.lp.clear() == Result::HoldLocked);
        CHECK(r.lp.cycleBars(1) == Result::HoldLocked);
        CHECK(r.lp.canUndo());  // Ctrl+Z is taken by the looper and refused
        CHECK(r.lp.playStop() == Result::Done && r.lp.state() == State::Stopped && r.lp.hold());
        CHECK(r.lp.playStop() == Result::Done && r.lp.state() == State::Play && r.lp.hold());
    }
    // HOLD in OVERDUB: punch-out now, the pass's notes and their undo stay.
    {
        Rig r;
        setup(r, Setup::Overdub, false);
        CHECK(r.lp.toggleHold() == Result::Done);
        CHECK(r.lp.state() == State::Play && r.lp.hold() && r.lp.noteCount() == 2 && r.lp.canUndo());
        r.press(3, 66);  // not recorded
        r.release(3);
        CHECK(r.lp.noteCount() == 2);
        CHECK(r.lp.toggleHold() == Result::Done && !r.lp.hold());
        CHECK(r.lp.undo() == Result::Done && r.lp.noteCount() == 1);
    }
    // HOLD armed in RECORD: PLAY + HOLD at the end of the pass.
    {
        Rig r;
        setup(r, Setup::Record, false);
        CHECK(r.lp.toggleHold() == Result::Done && r.lp.holdArmed() && !r.lp.hold());
        r.runSteps(r.lp.passSteps());
        CHECK(r.lp.state() == State::Play && r.lp.hold() && !r.lp.holdArmed());
    }
    // Z in RECORD: "then DUB" -> OVERDUB at the end of the pass; Z again = then PLAY.
    {
        Rig r;
        setup(r, Setup::Record, false);
        CHECK(r.lp.recDub() == Result::Done && r.lp.dubArmed());
        r.runSteps(r.lp.passSteps());
        CHECK(r.lp.state() == State::Overdub);
        Rig q;
        setup(q, Setup::Record, false);
        CHECK(q.lp.recDub() == Result::Done && q.lp.recDub() == Result::Done && !q.lp.dubArmed());
        q.runSteps(q.lp.passSteps());
        CHECK(q.lp.state() == State::Play);
    }
    // Space stop then Space start: the loop resumes (stopped by Space), and
    // stays stopped when it was stopped by C.
    {
        Rig r;
        setup(r, Setup::Play, true);
        r.stopTransport();
        CHECK(r.lp.state() == State::Stopped && r.silent());
        r.runSteps(8.0);
        r.startTransport();
        CHECK(r.lp.state() == State::Play);
        Rig q;
        setup(q, Setup::Stopped, true);
        q.stopTransport();
        q.startTransport();
        CHECK(q.lp.state() == State::Stopped);
    }
    // Page exit under PLAY: the loop keeps sounding on any page.
    {
        Rig r;
        setup(r, Setup::Overdub, true);
        r.lp.onPageExit();
        CHECK(r.lp.state() == State::Play);
        const int before = r.sink.countOns(0, 0xFFFFFFFFu);
        r.runSteps(2.0 * r.lp.passSteps());
        CHECK(r.sink.countOns(0, 0xFFFFFFFFu) > before);
    }
    // Undo of CLEAR: back as it was - playing, or stopped.
    {
        Rig r;
        setup(r, Setup::Play, false);
        CHECK(r.lp.clear() == Result::Done && r.lp.state() == State::Idle && r.lp.canUndo());
        CHECK(r.lp.undo() == Result::Done && r.lp.state() == State::Play && r.lp.noteCount() == 1);
        Rig q;
        setup(q, Setup::Stopped, false);
        CHECK(q.lp.clear() == Result::Done);
        CHECK(q.lp.undo() == Result::Done && q.lp.state() == State::Stopped && q.lp.noteCount() == 1);
    }
    std::printf("T1 transition table: %d cells + HOLD/RECORD/Space/page/CLEAR extras\n", n);
}

// ---------------------------------------------------------------- T2

void testClock() {
    // Free clock: the first note in ARMED is step 0 and the pass downbeat.
    {
        Rig r;
        r.runSteps(3.3);
        CHECK(r.lp.recDub() == Result::Done);
        r.runSteps(1.7);
        const uint32_t t0 = gNow;
        r.press(1, 60);
        CHECK(r.lp.state() == State::Record && r.lp.clockSource() == LoopClock::Source::Free);
        CHECK(r.lp.passDownbeatMicros() == t0);
        r.runSteps(1.0);
        r.release(1);
        CHECK(r.lp.layer().note(0).onset() == 0);
        // Silent passes never stop the free clock while the loop plays.
        r.runSteps(32.0 * 20 + 1.0);
        CHECK(r.lp.state() == State::Play && r.lp.clockSource() == LoopClock::Source::Free);
        CHECK(r.downbeats.size() == 20);
        for (size_t i = 0; i < r.downbeats.size(); ++i)
            CHECK(r.downbeats[i] == t0 + (i + 1) * 32u * kSixteenth);
    }
    // Under PLAY, ARMED waits for the next project bar.
    {
        Rig r;
        r.startTransport();
        r.runUntil(r.transportStepTime(5.3));
        CHECK(r.lp.recDub() == Result::Done);
        CHECK(std::fabs(r.lp.stepsToRecord() - (16.0 - 5.3)) < 0.5);
        r.runUntil(r.transportStepTime(10.0));
        r.press(1, 60);  // before the bar: live only
        r.release(1);
        CHECK(r.lp.state() == State::Armed && r.lp.noteCount() == 0);
        // Pickup -> step 0. The PLAY estimate lags by up to two audio blocks
        // (~0.22 step), so the press is 0.03 step before the bar.
        r.runUntil(r.transportStepTime(15.97));
        r.press(2, 62);
        CHECK(r.lp.state() == State::Record);
        r.runUntil(r.transportStepTime(17.0));
        r.release(2);
        CHECK(r.lp.noteCount() == 1 && r.lp.layer().note(0).onset() == 0);
        const int32_t err = static_cast<int32_t>(r.lp.passDownbeatMicros() - r.transportStepTime(16.0));
        CHECK(err > -static_cast<int32_t>(3 * kBlockMicros) && err < static_cast<int32_t>(3 * kBlockMicros));
    }
    // Bar phase across Space stop/start: BARS 2 recorded from project bar 2
    // (phase 1). Restart at bar 1: the loop is in its second bar there.
    {
        Rig r;
        r.startTransport();
        r.runSteps(1.0);
        CHECK(r.lp.recDub() == Result::Done);
        r.runUntil(r.transportStepTime(16.0) + 1000);  // record starts at bar 2 (step 16)
        r.runUntil(r.transportStepTime(32.0));          // loop step 16 = second bar
        r.press(1, 72);
        r.runUntil(r.transportStepTime(33.0));
        r.release(1);
        CHECK(r.lp.state() == State::Record && r.lp.layer().note(0).onset() == 16);
        r.runUntil(r.transportStepTime(50.0));
        CHECK(r.lp.state() == State::Play);
        r.stopTransport();
        CHECK(r.lp.state() == State::Stopped && r.silent());
        r.runSteps(5.0);
        const size_t mark = r.sink.events.size();
        r.startTransport();  // project bar 1 = loop bar 2: the note is due now
        r.runSteps(0.5);
        bool on = false;
        for (size_t i = mark; i < r.sink.events.size(); ++i)
            if (r.sink.events[i].type == MusicalEventType::NoteOn && r.sink.events[i].note == 72) on = true;
        CHECK(on);
        CHECK(r.lp.state() == State::Play);
    }
    // Tempo 120 -> 132 under PLAY in the middle of the loop: a pass in every window.
    {
        Rig r;
        r.startTransport();
        r.runSteps(1.0);
        CHECK(r.lp.setBars(1) == Result::Done);
        CHECK(r.lp.recDub() == Result::Done);
        r.runUntil(r.transportStepTime(16.0) + 1000);
        r.press(1, 60);
        r.runSteps(1.0);
        r.release(1);
        r.runUntil(r.transportStepTime(16.0 * 4 + 8.0));
        r.tempoChangeAt = gNow;  // mid-pass
        const size_t before = r.downbeats.size();
        const uint32_t until = gNow + 20u * 16u * static_cast<uint32_t>(kFastSixteenth);
        r.runUntil(until);
        const size_t passes = r.downbeats.size() - before;
        CHECK_MSG(passes >= 19 && passes <= 21, "passes=%zu", passes);
        for (size_t i = before + 2; i < r.downbeats.size(); ++i) {
            const int32_t d = static_cast<int32_t>(r.downbeats[i] - r.downbeats[i - 1]);
            CHECK_MSG(std::abs(d - static_cast<int32_t>(16 * kFastSixteenth)) < 2000, "d=%d", d);
        }
        CHECK(r.player.staleDrops() == 0);
    }
    std::printf("T2 clock: first note, free clock, next bar, bar phase, tempo change\n");
}

// ---------------------------------------------------------------- T3

// A loop of one long note (steps 0..W-1) playing; returns with it sounding.
void longNotePlaying(Rig& r, bool transport, uint8_t channel = 0) {
    r.lp.setChannel(channel);
    if (transport) {
        r.startTransport();
        r.runSteps(1.0);
    }
    CHECK(r.lp.recDub() == Result::Done);
    if (transport) r.runUntil(r.transportStepTime(16.0) + 2000);
    r.press(1, 50);
    r.runSteps(r.lp.passSteps() - 1.0);
    r.release(1);
    r.runSteps(1.0 + 3.0);  // into pass 2, the note sounding
    CHECK(r.lp.state() == State::Play);
    CHECK(r.sink.total == 1 && r.player.soundingCount() == 1);
}

void testBalance() {
    struct Case { const char* name; bool transport; };
    // Each of these must release the sounding loop note at once.
    {
        Rig r; longNotePlaying(r, false); (void)r.lp.playStop(); CHECK(r.silent());
    }
    {
        Rig r; longNotePlaying(r, false); (void)r.lp.panicStop(); CHECK(r.silent());
    }
    {
        Rig r; longNotePlaying(r, false); (void)r.lp.clear(); CHECK(r.silent());
        (void)r.lp.undo(); r.runSteps(64.0); CHECK(r.sink.total <= 1);
    }
    {
        Rig r; longNotePlaying(r, true); r.stopTransport(); CHECK(r.silent());
        r.runSteps(10.0); CHECK(r.silent());
    }
    {
        Rig r; longNotePlaying(r, false); r.lp.onPageExit(); CHECK(r.silent());
    }
    {
        // V in PLAY removes the recording: silent now.
        Rig r; longNotePlaying(r, false); (void)r.lp.undo(); CHECK(r.silent() && r.lp.state() == State::Idle);
    }
    {
        // Live-input epoch: the main loop cancels the (shared) player.
        Rig r; longNotePlaying(r, false); (void)r.player.cancel(); CHECK(r.silent());
        r.runSteps(r.lp.passSteps());  // next pass plays again
        CHECK(r.sink.total == 1);
    }
    {
        // Space start from the free clock: the free pass is released before the
        // PLAY pass starts (balance checked per event), one note sounding after.
        Rig r; longNotePlaying(r, false);
        const size_t mark = r.sink.events.size();
        r.startTransport();
        CHECK(r.sink.events.size() >= mark + 2);
        CHECK(r.sink.events[mark].type == MusicalEventType::NoteOff);
        CHECK(r.sink.total == 1);
    }
    {
        // CHANNEL change mid-note: NoteOff on the old channel, next pass on the new.
        Rig r; longNotePlaying(r, false, 5);
        r.lp.setChannel(9);
        r.runSteps(r.lp.passSteps());
        bool offOld = false, onNew = false;
        for (const Ev& e : r.sink.events) {
            if (e.type == MusicalEventType::NoteOff && e.lane && e.channel == 4) offOld = true;
            if (e.type == MusicalEventType::NoteOn && e.lane && e.channel == 8) onNew = true;
        }
        CHECK(offOld && onNew);
        (void)r.lp.playStop();
        CHECK(r.silent());
    }
    {
        // V during PLAY with dub notes: the pass plays out, balance holds.
        Rig r; longNotePlaying(r, false);
        CHECK(r.lp.recDub() == Result::Done);
        r.press(2, 67); r.runSteps(1.0); r.release(2);
        CHECK(r.lp.recDub() == Result::Done);  // punch-out
        r.runSteps(r.lp.passSteps());
        CHECK(r.lp.undo() == Result::Done && r.lp.noteCount() == 1);
        r.runSteps(2.0 * r.lp.passSteps());
        (void)r.lp.playStop();
        CHECK(r.silent());
    }
    {
        // Every transition again in the middle of a dense, polyphonic pass.
        const Event events[] = {Event::C, Event::X, Event::Del, Event::PageExit, Event::V, Event::Z,
                                Event::B, Event::Bars, Event::SpaceStart};
        for (Event e : events) {
            Rig r;
            CHECK(r.lp.recDub() == Result::Done);
            for (int s = 0; s < 16; ++s) {
                r.press(static_cast<uint16_t>(10 + s), static_cast<uint8_t>(60 + (s % 3)));
                r.press(static_cast<uint16_t>(40 + s), static_cast<uint8_t>(70 + (s % 2)));
                r.runSteps(1.0);
                r.release(static_cast<uint16_t>(10 + s));
                r.runSteps(1.0);
                r.release(static_cast<uint16_t>(40 + s));
            }
            r.runSteps(5.5);
            CHECK(r.sink.total > 0);
            apply(r, e);
            r.runSteps(3.0 * r.lp.passSteps());
            (void)r.lp.panicStop();
            CHECK_MSG(r.silent(), "%s", eventName(e));
        }
    }
    std::printf("T3 balance: C X Del Space-stop Space-start page-exit channel V epoch + dense pass\n");
}

// ---------------------------------------------------------------- T4

using NoteSpec = std::tuple<int, int, int>;  // onset, length, pitch

std::vector<NoteSpec> shape(int index, int W) {
    std::vector<NoteSpec> s;
    switch (index) {
        case 0: s = {{0, 1, 60}}; break;
        case 1: for (int t = 0; t < W; t += 4) s.emplace_back(t, 2, 60 + (t / 4) % 5); break;
        case 2: s = {{0, 2, 60}, {W - 1, 1, 62}}; break;
        case 3: s = {{0, 4, 60}, {0, 4, 64}, {0, 4, 67}, {8, 4, 65}, {8, 4, 69}}; break;
        case 4: for (int t = 0; t < W; t += W / 16) s.emplace_back(t, 1, 60 + t % 2); break;
        case 5: s = {{0, W, 48}, {4, 1, 72}, {W / 2, 1, 74}}; break;
        // W/2 notes, two steps apart (64 at BARS 8: two plans, a clean split).
        case 6: for (int t = 0; t < W; t += 2) s.emplace_back(t, 1, 48 + ((t / 2) * 5) % 24); break;
        // A drone over the pass + notes on odd steps (64 at BARS 8): no clean
        // split, the drone goes on in the next plan (NoteOff + NoteOn there).
        default:
            s = {{0, W, 36}};
            for (int t = 1; t + 1 < W && s.size() < 64; t += 2) s.emplace_back(t, 1, 60 + (t / 2) % 12);
            break;
    }
    return s;
}

// NoteOns per pass: one per note, plus one per extra piece of a note carried
// over a forced plan boundary (shape 7 at BARS 8: the drone in 3 pieces).
int expectedOns(int shapeIndex, int W, size_t notes) {
    return static_cast<int>(notes) + (shapeIndex == 7 && W == 128 ? 2 : 0);
}

// Records the shape: free clock (first note = step 0) or the next PLAY bar.
void recordShape(Rig& r, const std::vector<NoteSpec>& notes) {
    CHECK(r.lp.recDub() == Result::Done);
    struct Act { double step; bool on; uint16_t tag; uint8_t pitch; };
    std::vector<Act> acts;
    uint16_t tag = 1;
    for (const auto& n : notes) {
        acts.push_back(Act{static_cast<double>(std::get<0>(n)), true, tag, static_cast<uint8_t>(std::get<2>(n))});
        acts.push_back(Act{static_cast<double>(std::get<0>(n) + std::get<1>(n)) - 0.05, false, tag, 0});
        ++tag;
    }
    std::stable_sort(acts.begin(), acts.end(), [](const Act& a, const Act& b) { return a.step < b.step; });
    const uint32_t origin = r.transportOn ? r.transportStepTime(16.0) : gNow;
    for (const Act& a : acts) {
        r.runUntil(origin + static_cast<uint32_t>(a.step * kSixteenth));
        if (a.on) r.press(a.tag, a.pitch); else r.release(a.tag);
    }
}

struct GapStats { long passes{0}; };

void runGapConfig(bool transport, uint32_t passMicros, uint8_t bars, int shapeIndex, int passes,
                  GapStats& stats) {
    Rig r;
    r.passMicros = passMicros;
    CHECK(r.lp.setBars(bars) == Result::Done || bars == 2);
    const int W = bars * 16;
    const std::vector<NoteSpec> notes = shape(shapeIndex, W);
    if (transport) {
        r.startTransport();
        r.runSteps(1.0);
    }
    recordShape(r, notes);
    const uint8_t expected = static_cast<uint8_t>(notes.size());
    const int expectedPerPass = expectedOns(shapeIndex, W, notes.size());
    const size_t first = r.downbeats.size();
    const uint32_t start = r.lp.passDownbeatMicros();
    // Pass by pass: the run may be hours long (micros wrap, int32 deltas).
    uint32_t target = start;
    for (int p = 0; p < passes; ++p) {
        target += static_cast<uint32_t>(W) * kSixteenth;
        r.runUntil(target);
    }
    r.runUntil(target + W * kSixteenth / 2);
    CHECK(r.lp.state() == State::Play);
    CHECK_MSG(r.lp.noteCount() == expected, "shape %d notes %u/%u", shapeIndex, r.lp.noteCount(), expected);
    const size_t got = r.downbeats.size() - first;
    CHECK_MSG(got >= static_cast<size_t>(passes), "passes %zu/%d", got, passes);
    // NoteOns per pass: events between the two downbeat marks (+ slack),
    // times compared as wrapping int32 deltas.
    const size_t passCount = r.downbeats.size() - first;
    for (size_t i = 0; i + 1 < passCount; ++i) {
        const uint32_t a = r.downbeats[first + i], b = r.downbeats[first + i + 1];
        const size_t from = r.downbeatEvents[first + i] > 8 ? r.downbeatEvents[first + i] - 8 : 0;
        const size_t to = std::min(r.sink.events.size(), r.downbeatEvents[first + i + 1] + 8);
        int ons = 0;
        uint32_t firstLate = 0xFFFFFFFFu;
        for (size_t k = from; k < to; ++k) {
            const Ev& e = r.sink.events[k];
            if (e.type != MusicalEventType::NoteOn) continue;
            if (static_cast<int32_t>(e.t - a) < 0 || static_cast<int32_t>(e.t - b) >= 0) continue;
            ++ons;
            if (e.t - a < firstLate) firstLate = e.t - a;
        }
        // downbeat of every pass exactly one window after the previous one
        CHECK_MSG(b - a == static_cast<uint32_t>(W) * kSixteenth, "tr=%d pm=%u bars=%u shape=%d d=%u",
                  transport, passMicros, bars, shapeIndex, b - a);
        // every loop note sounds in every pass, the first one within one loop pass
        CHECK_MSG(ons == expectedPerPass, "tr=%d pm=%u bars=%u shape=%d pass=%zu ons=%d",
                  transport, passMicros, bars, shapeIndex, i, ons);
        CHECK(firstLate <= passMicros);
        ++stats.passes;
    }
    CHECK(r.player.staleDrops() == 0);
    (void)r.lp.playStop();
    CHECK(r.silent());
}

void testNoGaps() {
    GapStats stats;
    for (bool transport : {false, true})
        for (uint32_t pm : {1000u, 5000u, 20000u, 60000u})
            for (uint8_t bars : {1, 2, 4, 8})
                for (int s = 0; s < 8; ++s)
                    runGapConfig(transport, pm, bars, s,
                                 bars == 1 ? 101 : bars == 2 ? 71 : bars == 4 ? 41 : 13, stats);
    std::printf("T4 no gaps: %ld passes over 256 configurations\n", stats.passes);
    std::fflush(stdout);
    CHECK(stats.passes >= 10000);
    // 10000 consecutive passes on one loop (free clock, BARS 1, 5 ms loop pass).
    GapStats longRun;
    runGapConfig(false, 5000u, 1, 3, 10000, longRun);
    std::printf("T4 no gaps: %ld consecutive passes\n", longRun.passes);
}

// ---------------------------------------------------------------- T5

std::vector<std::tuple<int, int>> planNotes(const Looper& lp) {
    ResponsePlan plan;
    lp.layer().buildPlan(plan, lp.passSteps(), lp.length());
    std::vector<std::tuple<int, int>> out;
    for (uint8_t i = 0; i < plan.count; ++i) out.emplace_back(plan.notes[i].step, plan.notes[i].note);
    std::sort(out.begin(), out.end());
    return out;
}

void testOverdub() {
    Rig r;
    CHECK(r.lp.setBars(1) == Result::Done);
    CHECK(r.lp.recDub() == Result::Done);
    const uint32_t t0 = gNow;
    // RECORD: a C major chord on step 0.
    r.press(1, 60); r.press(2, 64); r.press(3, 67);
    r.runSteps(2.0);
    r.release(1); r.release(2); r.release(3);
    CHECK(r.lp.recDub() == Result::Done && r.lp.dubArmed());  // then DUB
    auto at = [&](int pass, double step) { return t0 + static_cast<uint32_t>((pass * 16 + step) * kSixteenth); };
    // DUB pass 1: a chord on step 4; pass 2: one note on 8; pass 3: a chord on 12.
    r.runUntil(at(1, 4.0)); r.press(4, 62); r.press(5, 65); r.runSteps(1.0); r.release(4); r.release(5);
    r.runUntil(at(2, 8.0)); r.press(6, 69); r.runSteps(1.0); r.release(6);
    const auto beforeLast = planNotes(r.lp);
    r.runUntil(at(3, 12.0)); r.press(7, 71); r.press(8, 74); r.runSteps(1.0); r.release(7); r.release(8);
    CHECK(r.lp.recDub() == Result::Done && r.lp.state() == State::Play);
    const auto all = planNotes(r.lp);
    CHECK(all.size() == 8);
    // The next pass plays every pitch, chords included (polyphony).
    r.runUntil(at(4, 0.0) + 1000);
    r.runUntil(at(5, 0.0) - 1);
    CHECK(r.sink.countOns(at(4, 0.0), at(5, 0.0)) == 8);
    // V removes exactly the last DUB pass.
    CHECK(r.lp.undo() == Result::Done);
    CHECK(planNotes(r.lp) == beforeLast);
    CHECK(r.lp.undo() == Result::NothingToUndo);
    // Del + V: layer and sound as before Del.
    r.runUntil(at(6, 0.0) + 1000);
    const auto layerBefore = planNotes(r.lp);
    CHECK(r.lp.clear() == Result::Done && r.silent() && planNotes(r.lp).empty());
    r.runUntil(at(7, 0.0) + 1000);
    CHECK(r.sink.countOns(at(6, 0.5), at(7, 0.5)) == 0);
    CHECK(r.lp.undo() == Result::Done && r.lp.state() == State::Play);
    CHECK(planNotes(r.lp) == layerBefore);
    const uint32_t resumed = r.lp.passDownbeatMicros();
    r.runUntil(resumed + 16 * kSixteenth + 1000);
    r.runUntil(resumed + 32 * kSixteenth - 1);
    CHECK(r.sink.countOns(resumed + 16 * kSixteenth, resumed + 32 * kSixteenth) ==
          static_cast<int>(layerBefore.size()));
    std::printf("T5 overdub: chords, V = last pass, Del + V\n");
}

// ---------------------------------------------------------------- T6

void testBorrowedPlayer() {
    auto& partner = GroovePuterCallResponse::callResponseListener();
    CHECK(!partner.enabled());  // Off after start
    Rig r;
    partner.setEnabled(true);
    CHECK(r.lp.recDub() == Result::PartnerActive && r.lp.state() == State::Idle);
    partner.setEnabled(false);
    recordOne(r);
    r.runSteps(r.lp.passSteps());
    CHECK(r.lp.playStop() == Result::Done && r.lp.state() == State::Stopped);
    partner.setEnabled(true);
    CHECK(r.lp.playStop() == Result::PartnerActive && r.lp.state() == State::Stopped);
    CHECK(r.lp.recDub() == Result::PartnerActive && r.silent());
    partner.setEnabled(false);
    CHECK(!partner.enabled());
    // The loop channel goes to the player per pass only: the PARTNER setting
    // stored in the shared player is unchanged.
    r.player.setWireChannel(3);
    r.lp.setChannel(11);
    CHECK(r.lp.playStop() == Result::Done && r.lp.state() == State::Play);
    CHECK(r.player.wireChannel() == 3 && r.player.activeWireChannel() == 11);
    std::printf("T6 borrowed player: PARTNER active refuses Z/C, channel restored\n");
}

// ---------------------------------------------------------------- T7

void testEightBars() {
    // BARS cycle: 1 -> 2 -> 4 -> 8 -> 1, back the other way; anything else = 2.
    {
        Rig r;
        CHECK(r.lp.bars() == 2);
        CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 4);
        CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 8 && r.lp.passSteps() == 128);
        CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 1);
        CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 2);
        CHECK(r.lp.cycleBars(-1) == Result::Done && r.lp.bars() == 1);
        CHECK(r.lp.cycleBars(-1) == Result::Done && r.lp.bars() == 8);
        CHECK(r.lp.cycleBars(-1) == Result::Done && r.lp.bars() == 4);
        CHECK(r.lp.setBars(8) == Result::Done && r.lp.setBars(8) == Result::Ignored);
        CHECK(r.lp.setBars(3) == Result::Done && r.lp.bars() == 2);
    }
    // A note on loop step 120 (bar 8) is recorded and played every 128 steps.
    {
        Rig r;
        CHECK(r.lp.setBars(8) == Result::Done);
        CHECK(r.lp.recDub() == Result::Done);
        const uint32_t t0 = gNow;
        r.press(1, 60);
        r.runSteps(1.0);
        r.release(1);
        r.runUntil(t0 + 120 * kSixteenth);
        r.press(2, 72);
        r.runSteps(2.0);
        r.release(2);
        CHECK(r.lp.noteCount() == 2 && r.lp.layer().note(1).onset() == 120);
        r.runUntil(t0 + 128 * kSixteenth + 1000);
        CHECK(r.lp.state() == State::Play && r.lp.passDownbeatMicros() == t0 + 128 * kSixteenth);
        r.runUntil(t0 + 2 * 128 * kSixteenth + 1000);
        const uint32_t pass2 = t0 + 128 * kSixteenth;
        bool late = false;
        for (const Ev& e : r.sink.events)
            if (e.type == MusicalEventType::NoteOn && e.note == 72 && e.t >= pass2 + 120 * kSixteenth &&
                e.t < pass2 + 120 * kSixteenth + 6000) late = true;
        CHECK(late);
        // The strip/menu view: bar 1..8 of 8.
        CHECK(r.lp.passPositionNow() >= 0.0 && r.lp.passPositionNow() <= 128.0);
    }
    // Bar phase on an 8-bar loop: recorded from project bar 2 (phase 1);
    // restart at project bar 1 = loop bar 8, the note on loop step 116 is due
    // 4 steps after the restart.
    {
        Rig r;
        CHECK(r.lp.setBars(8) == Result::Done);
        r.startTransport();
        r.runSteps(1.0);
        CHECK(r.lp.recDub() == Result::Done);
        r.runUntil(r.transportStepTime(16.0) + 3 * kBlockMicros);  // the PLAY estimate lags
        CHECK(r.lp.state() == State::Record);
        r.runUntil(r.transportStepTime(16.0 + 116.0));
        r.press(1, 74);
        r.runUntil(r.transportStepTime(16.0 + 117.0));
        r.release(1);
        CHECK(r.lp.layer().note(0).onset() == 116);
        r.runUntil(r.transportStepTime(16.0 + 130.0));
        CHECK(r.lp.state() == State::Play);
        r.stopTransport();
        CHECK(r.lp.state() == State::Stopped && r.silent());
        r.runSteps(3.0);
        const size_t mark = r.sink.events.size();
        r.startTransport();
        r.runSteps(3.0);
        for (size_t i = mark; i < r.sink.events.size(); ++i)
            CHECK(!(r.sink.events[i].type == MusicalEventType::NoteOn && r.sink.events[i].note == 74));
        r.runSteps(2.0);
        bool on = false;
        for (size_t i = mark; i < r.sink.events.size(); ++i)
            if (r.sink.events[i].type == MusicalEventType::NoteOn && r.sink.events[i].note == 74) on = true;
        CHECK(on);
    }
    // 64 notes fill the pool: the 65th is dropped (FULL notice once), every
    // pass plays all 64 (two plans), V and Del + V as at 4 bars.
    {
        Rig r;
        CHECK(r.lp.setBars(8) == Result::Done);
        CHECK(r.lp.recDub() == Result::Done);
        const uint32_t t0 = gNow;
        for (int i = 0; i < 64; ++i) {
            r.runUntil(t0 + static_cast<uint32_t>(i * 2) * kSixteenth);
            r.press(static_cast<uint16_t>(100 + i), static_cast<uint8_t>(40 + i % 40));
            r.runSteps(0.9);
            r.release(static_cast<uint16_t>(100 + i));
        }
        CHECK(r.lp.noteCount() == 64 && r.lp.full() && !r.lp.takeFullNotice());
        r.runUntil(t0 + 127 * kSixteenth);
        r.press(999, 90);  // the 65th
        r.runSteps(0.5);
        r.release(999);
        CHECK(r.lp.noteCount() == 64 && r.lp.layer().dropped() == 1);
        CHECK(r.lp.takeFullNotice() && !r.lp.takeFullNotice());
        r.runUntil(t0 + 128 * kSixteenth + 1000);
        CHECK(r.lp.state() == State::Play);
        const uint32_t pass2 = t0 + 128 * kSixteenth;
        r.runUntil(pass2 + 128 * kSixteenth - 1);
        CHECK(r.sink.countOns(pass2, pass2 + 128 * kSixteenth) == 64);
        r.runUntil(pass2 + 2 * 128 * kSixteenth - 1);
        CHECK(r.sink.countOns(pass2 + 128 * kSixteenth, pass2 + 2 * 128 * kSixteenth) == 64);
        CHECK(r.player.staleDrops() == 0);
        CHECK(r.lp.clear() == Result::Done && r.silent());
        CHECK(r.lp.undo() == Result::Done && r.lp.state() == State::Play && r.lp.noteCount() == 64);
        CHECK(r.lp.playStop() == Result::Done && r.silent());
    }
    std::printf("T7 BARS 8: cycle, 128-step pass, bar phase, 64 notes + FULL\n");
}

// ---------------------------------------------------------------- T8

int onsIn(const Rig& r, uint32_t from, uint32_t to, uint8_t note) {
    int c = 0;
    for (const Ev& e : r.sink.events)
        if (e.type == MusicalEventType::NoteOn && e.note == note && e.t >= from && e.t < to) ++c;
    return c;
}

void testTwoLayers() {
    // Layer 1: 60 on step 0, 64 on step 4 (1 bar, free clock).
    Rig r;
    CHECK(r.lp.setBars(1) == Result::Done);
    CHECK(r.lp.activeLayer() == 0);
    CHECK(r.lp.recDub() == Result::Done);
    const uint32_t t0 = gNow;
    auto at = [&](int pass, double step) { return t0 + static_cast<uint32_t>((pass * 16 + step) * kSixteenth); };
    r.press(1, 60); r.runSteps(1.0); r.release(1);
    r.runUntil(at(0, 4.0)); r.press(2, 64); r.runSteps(1.0); r.release(2);
    CHECK(r.lp.nextLayer() == Result::Recording);  // not while the first pass records
    r.runUntil(at(1, 0.5));
    CHECK(r.lp.state() == State::Play && r.lp.layerNoteCount(0) == 2);
    // M -> layer 2; Z dubs into it: 60 on step 0 (same step and pitch as
    // layer 1) and 67 on step 8 (a long note, steps 8..14).
    CHECK(r.lp.nextLayer() == Result::Done && r.lp.activeLayer() == 1);
    CHECK(r.lp.recDub() == Result::Done && r.lp.state() == State::Overdub);
    r.runUntil(at(1, 8.0)); r.press(3, 67); r.runSteps(6.0); r.release(3);
    r.runUntil(at(2, 0.0) - 2000); r.press(4, 60); r.runSteps(1.0); r.release(4);  // step 0 of pass 2 (pickup)
    r.runUntil(at(2, 4.0)); r.press(5, 72); r.runSteps(1.0); r.release(5);
    CHECK(r.lp.layerNoteCount(0) == 2 && r.lp.layerNoteCount(1) == 3 && r.lp.noteCount() == 5);
    for (uint8_t i = 0; i < r.lp.layer().slotCount(); ++i) {
        const auto& n = r.lp.layer().note(i);
        if (n.layer() == 0) CHECK(!n.shadow() && (n.note() == 60 || n.note() == 64));
    }
    // V: the last DUB pass of layer 2 only (60 + 72); layer 1 untouched.
    CHECK(r.lp.undo() == Result::Done);
    CHECK(r.lp.layerNoteCount(0) == 2 && r.lp.layerNoteCount(1) == 1);
    CHECK(r.lp.recDub() == Result::Done && r.lp.state() == State::Play);
    // Every pass plays both layers: 60, 64 (layer 1) and 67 (layer 2).
    r.runUntil(at(4, 0.0) + 1000);
    r.runUntil(at(5, 0.0) - 1);
    CHECK(onsIn(r, at(4, 0.0), at(5, 0.0), 60) == 1 && onsIn(r, at(4, 0.0), at(5, 0.0), 64) == 1 &&
          onsIn(r, at(4, 0.0), at(5, 0.0), 67) == 1);

    // HOLD per layer: lock layer 1, layer 2 stays free.
    CHECK(r.lp.nextLayer() == Result::Done && r.lp.activeLayer() == 0);
    CHECK(r.lp.toggleHold() == Result::Done && r.lp.hold() && r.lp.layerHold(0));
    CHECK(r.lp.recDub() == Result::HoldLocked && r.lp.clear() == Result::HoldLocked);
    CHECK(r.lp.setBars(2) == Result::HoldLocked && r.lp.bars() == 1);  // BARS changes every layer
    CHECK(r.lp.nextLayer() == Result::Done && r.lp.activeLayer() == 1 && !r.lp.hold());
    CHECK(r.lp.toggleHold() == Result::Done && r.lp.layerHold(1));  // and back
    CHECK(r.lp.toggleHold() == Result::Done && !r.lp.layerHold(1) && r.lp.layerHold(0));
    CHECK(r.lp.recDub() == Result::Done && r.lp.state() == State::Overdub);
    r.runUntil(at(5, 10.0)); r.press(6, 69); r.runSteps(1.0); r.release(6);
    CHECK(r.lp.layerNoteCount(1) == 2 && r.lp.layerNoteCount(0) == 2);
    // V undoes layer 2's pass while layer 1 is locked.
    CHECK(r.lp.undo() == Result::Done && r.lp.layerNoteCount(1) == 1 && r.lp.state() == State::Overdub);
    CHECK(r.lp.recDub() == Result::Done && r.lp.state() == State::Play);
    // CLEAR of layer 2 while layer 1 is locked: the loop plays on (layer 1).
    r.runUntil(at(6, 9.0));  // 67 sounding
    CHECK(r.lp.clear() == Result::Done && r.lp.state() == State::Play);
    CHECK(r.lp.layerNoteCount(1) == 0 && r.lp.layerNoteCount(0) == 2);
    CHECK(r.sink.sounding[67] == 0);  // the cleared layer goes silent at once
    r.runUntil(at(8, 0.0) - 1);
    CHECK(onsIn(r, at(7, 0.0), at(8, 0.0), 64) == 1 && onsIn(r, at(7, 0.0), at(8, 0.0), 67) == 0);
    // Undo of that CLEAR (layer 2, not locked) is allowed.
    CHECK(r.lp.undo() == Result::Done && r.lp.layerNoteCount(1) == 1);
    // The last unit in a locked layer: V refused.
    {
        Rig q;
        CHECK(q.lp.setBars(1) == Result::Done);
        recordOne(q);
        q.runSteps(q.lp.passSteps());
        CHECK(q.lp.nextLayer() == Result::Done && q.lp.nextLayer() == Result::Done);
        CHECK(q.lp.toggleHold() == Result::Done);  // layer 1 (its RECORD pass is the last unit)
        CHECK(q.lp.nextLayer() == Result::Done && q.lp.canUndo());
        CHECK(q.lp.undo() == Result::HoldLocked && q.lp.noteCount() == 1);
        // An empty layer has nothing to hold or clear.
        CHECK(q.lp.toggleHold() == Result::NothingToHold && q.lp.clear() == Result::Empty);
    }

    // MUTE at once: layer 2's long note gets its NoteOff now, layer 1 plays on.
    r.runUntil(at(10, 0.5));
    CHECK(r.lp.activeLayer() == 1);
    r.runUntil(at(10, 9.0));
    CHECK(r.sink.sounding[67] == 1);
    CHECK(r.lp.toggleMute() == Result::Done && r.lp.layerMuted(1) && r.lp.muteMask() == 0x02u);
    CHECK(r.sink.sounding[67] == 0);
    r.runUntil(at(12, 0.0) - 1);
    CHECK(onsIn(r, at(11, 0.0), at(12, 0.0), 67) == 0 && onsIn(r, at(11, 0.0), at(12, 0.0), 60) == 1);
    // Unmute mid-pass: the step-8 note of this pass plays again.
    r.runUntil(at(12, 4.5));
    CHECK(r.lp.toggleMute() == Result::Done && !r.lp.layerMuted(1));
    r.runUntil(at(13, 0.0) - 1);
    CHECK(onsIn(r, at(12, 5.0), at(13, 0.0), 67) == 1);
    CHECK(onsIn(r, at(12, 5.0), at(13, 0.0), 64) == 0);  // no re-attack of a note already over
    // Both muted: silence; the loop keeps running. C stops: silent.
    CHECK(r.lp.nextLayer() == Result::Done);
    CHECK(r.lp.toggleMute() == Result::Done && r.lp.toggleMute() == Result::Done && r.lp.toggleMute() == Result::Done);
    CHECK(r.lp.muteMask() == 0x01u);
    CHECK(r.lp.playStop() == Result::Done && r.silent());
    std::printf("T8 two layers: isolation, undo per layer, HOLD per layer, CLEAR one layer, MUTE now\n");
}

// ---------------------------------------------------------------- T9

void testBarsWithNotes() {
    // 1 bar: 60 on 0 (2 steps), 64 on 6, 67 on 12 (to the end).
    Rig r;
    CHECK(r.lp.setBars(1) == Result::Done);
    CHECK(r.lp.recDub() == Result::Done);
    const uint32_t t0 = gNow;
    auto at = [&](double step) { return t0 + static_cast<uint32_t>(step * kSixteenth); };
    r.press(1, 60); r.runSteps(2.0); r.release(1);
    r.runUntil(at(6.0)); r.press(2, 64); r.runSteps(1.0); r.release(2);
    r.runUntil(at(12.0)); r.press(3, 67);
    r.runUntil(at(16.5));
    r.release(3);
    CHECK(r.lp.state() == State::Play && r.lp.noteCount() == 3);
    const auto bar = planNotes(r.lp);
    auto repeated = [&](int bars) {
        std::vector<std::tuple<int, int>> out;
        for (int k = 0; k < bars; ++k)
            for (const auto& t : bar) out.emplace_back(std::get<0>(t) + 16 * k, std::get<1>(t));
        std::sort(out.begin(), out.end());
        return out;
    };
    // 1 -> 2 -> 4 -> 8 mid-pass (67 sounding at the 2 -> 4 change).
    r.runUntil(at(16 + 3.0));
    CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 2);
    CHECK(planNotes(r.lp) == repeated(2) && r.lp.noteCount() == 6 && r.lp.canUndo());
    const uint32_t pass = r.lp.passDownbeatMicros();
    CHECK(pass == at(16.0));  // the running pass keeps its downbeat
    r.runUntil(pass + 32 * kSixteenth - 1);
    CHECK(r.sink.countOns(pass + 4 * kSixteenth, pass + 32 * kSixteenth) == 5);  // 64,67 | 60,64,67
    r.runUntil(pass + 32 * kSixteenth + 13 * kSixteenth);
    CHECK(r.sink.sounding[67] == 1);
    CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 4 && r.lp.noteCount() == 12);
    CHECK(planNotes(r.lp) == repeated(4));
    CHECK(r.lp.cycleBars(1) == Result::Done && r.lp.bars() == 8 && r.lp.noteCount() == 24);
    CHECK(planNotes(r.lp) == repeated(8));
    // Two full 8-bar passes: 24 notes each.
    const uint32_t p8 = r.lp.passDownbeatMicros();
    r.runUntil(p8 + 3 * 128 * kSixteenth + 1000);
    CHECK(r.sink.countOns(p8 + 128 * kSixteenth, p8 + 2 * 128 * kSixteenth) == 24);
    CHECK(r.sink.countOns(p8 + 2 * 128 * kSixteenth, p8 + 3 * 128 * kSixteenth) == 24);
    // V: back to 4 bars (bars too), the loop plays on.
    CHECK(r.lp.undo() == Result::Done && r.lp.bars() == 4 && r.lp.noteCount() == 12);
    CHECK(r.lp.state() == State::Play && !r.lp.canUndo());
    // Shorter: 4 -> 2 hides bars 3-4; the pass goes on; V brings them back.
    r.runUntil(r.lp.passDownbeatMicros() + 64 * kSixteenth + 40 * kSixteenth);  // bar 3 of a pass
    CHECK(r.lp.cycleBars(-1) == Result::Done && r.lp.bars() == 2 && r.lp.noteCount() == 6);
    CHECK(planNotes(r.lp) == repeated(2));
    const uint32_t p2 = r.lp.passDownbeatMicros();
    CHECK(gNow >= p2 && gNow < p2 + 32 * kSixteenth);  // re-placed on the 2-bar grid
    r.runUntil(p2 + 3 * 32 * kSixteenth + 1000);
    CHECK(r.sink.countOns(p2 + 32 * kSixteenth, p2 + 2 * 32 * kSixteenth) == 6);
    CHECK(r.lp.undo() == Result::Done && r.lp.bars() == 4 && r.lp.noteCount() == 12);
    CHECK(planNotes(r.lp) == repeated(4));
    // C stops: silent; BARS while stopped keeps the notes too.
    CHECK(r.lp.playStop() == Result::Done && r.silent());
    CHECK(r.lp.setBars(1) == Result::Done && r.lp.noteCount() == 3 && planNotes(r.lp) == bar);
    CHECK(r.lp.playStop() == Result::Done && r.lp.state() == State::Play);
    r.runSteps(40.0);
    CHECK(r.lp.playStop() == Result::Done && r.silent());

    // RECORD: refused (Recording), the pass records on.
    {
        Rig q;
        setup(q, Setup::Record, false);
        CHECK(q.lp.cycleBars(1) == Result::Recording && q.lp.bars() == 2 && q.lp.state() == State::Record);
    }
    // No room: 40 notes in 4 bars cannot repeat to 8.
    {
        Rig q;
        CHECK(q.lp.setBars(4) == Result::Done);
        CHECK(q.lp.recDub() == Result::Done);
        const uint32_t s0 = gNow;
        for (int i = 0; i < 40; ++i) {
            q.runUntil(s0 + static_cast<uint32_t>(i * 1.5 * kSixteenth));
            q.press(static_cast<uint16_t>(100 + i), static_cast<uint8_t>(40 + i));
            q.runSteps(0.5);
            q.release(static_cast<uint16_t>(100 + i));
        }
        q.runUntil(s0 + 64 * kSixteenth + 1000);
        CHECK(q.lp.state() == State::Play && q.lp.noteCount() == 40);
        CHECK(q.lp.setBars(8) == Result::NoRoom && q.lp.bars() == 4 && q.lp.noteCount() == 40);
        int firstHalf = 0;
        for (uint8_t i = 0; i < q.lp.layer().slotCount(); ++i) if (q.lp.layer().note(i).onset() < 32) ++firstHalf;
        CHECK(firstHalf > 0 && firstHalf < 40);
        CHECK(q.lp.setBars(2) == Result::Done && q.lp.noteCount() == firstHalf);
        q.runSteps(70.0);
        CHECK(q.lp.playStop() == Result::Done && q.silent());
    }
    // OVERDUB + BARS with a note held: the note ends, the pass goes on in DUB.
    {
        Rig q;
        setup(q, Setup::Overdub, false);
        q.press(7, 71);
        q.runSteps(1.0);
        CHECK(q.lp.cycleBars(1) == Result::Done && q.lp.bars() == 4 && q.lp.state() == State::Overdub);
        CHECK(q.lp.noteCount() == 6);
        q.release(7);
        q.runSteps(80.0);
        CHECK(q.lp.playStop() == Result::Done && q.silent());
    }
    // Under PLAY: 2 -> 4 mid-pass keeps the downbeat, the bar phase follows,
    // Space stop/start resumes on the 4-bar grid.
    {
        Rig q;
        setup(q, Setup::Play, true);
        const uint32_t down = q.lp.passDownbeatMicros();
        q.runSteps(6.0);
        CHECK(q.lp.cycleBars(1) == Result::Done && q.lp.bars() == 4 && q.lp.noteCount() == 2);
        CHECK(q.lp.passDownbeatMicros() == down);
        q.runUntil(down + 64 * kSixteenth + 2 * kSixteenth);
        CHECK(q.lp.passDownbeatMicros() >= down + 64 * kSixteenth - kSixteenth / 2 &&
              q.lp.passDownbeatMicros() <= down + 64 * kSixteenth + kSixteenth / 2);
        CHECK(q.sink.countOns(down + 64 * kSixteenth - kSixteenth, down + 64 * kSixteenth + 2 * kSixteenth) == 1);
        q.stopTransport();
        CHECK(q.silent());
        q.startTransport();
        q.runSteps(70.0);
        CHECK(q.lp.state() == State::Play);
        q.stopTransport();
        CHECK(q.silent());
    }
    std::printf("T9 BARS with notes: repeat 1-2-4-8, cut, V, no room, DUB, PLAY clock\n");
}

}  // namespace

int main() {
    testTransitionTable();
    testClock();
    testBalance();
    testNoGaps();
    testOverdub();
    testBorrowedPlayer();
    testEightBars();
    testTwoLayers();
    testBarsWithNotes();
    std::printf("looper engine tests passed (%d checks)\n", gChecks);
    return 0;
}
