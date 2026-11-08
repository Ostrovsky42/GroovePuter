// CALL/RESPONSE phase 7: LOOP plays without gaps.
//
// One question window, then 6 windows without human input. The rig follows
// the main-loop path of GroovePuter.ino (serviceCallResponseLog): describe,
// harmony per strong beat (optional), genre style, reply memory, cadence,
// planPerformanceReply, startAnswer, rememberStartedReply. For EVOLVE 0..3,
// BARS 1/2, with and without harmony, every genre (and none) and several
// loop shapes it checks that (phase 9: the DELAY 0/1 axis is gone - LOOP was
// on the grid for both since phase 7, and the setting no longer exists)
//   - every window after the question gets its reply (loop=0,1,2,...);
//   - the first NoteOn of each window is exactly on the grid (window start +
//     first loop onset);
//   - no loop note is lost: NoteOns per window = loop notes;
//   - every bar that holds a loop note sounds in every window;
//   - the longest silence between two answer notes over the whole run is
//     never longer than the loop's own longest rest (+ the 2-step LATE
//     ending);
//   - NoteOn/NoteOff stay balanced and OFF leaves nothing sounding.
// Also: free clock and PLAY clock, coarse main-loop passes (one pass late,
// staleDrops == 0, NoteOn within one pass of the grid), and the REPLACE 3+
// window with 1-2 human notes still replays on the downbeat.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"
#include "src/input/call_response_performance_plan.h"

using namespace GroovePuterCallResponse;
using Close = PhraseCapture::CloseResult;
using Listener = CallResponseListener;

namespace {

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint32_t kSixteenth = 125000;  // 120 BPM
constexpr uint32_t kT0 = 1000;
constexpr uint32_t kBlockMicros = 11610;
constexpr double kFastSixteenth = 15000000.0 / 132.0;  // 256 frames at 22.05 kHz
uint32_t at(double step) { return kT0 + static_cast<uint32_t>(step * kSixteenth + 0.5); }

// C F G C, one chord per bar (bit n = pitch class n).
constexpr uint16_t kChords[4] = {0x091u, 0x221u, 0x884u, 0x091u};

class RecordingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override {
        events.push_back(event);
        times.push_back(gNow);
    }
    std::vector<MusicalEvent> events;
    std::vector<uint32_t> times;
};

struct Started {
    int32_t answerWindow;  // epoch step of the reply window
    uint16_t cycle;
    uint8_t strategy;
};

struct Config {
    uint8_t evolve{1};
    uint8_t bars{2};
    bool harmony{false};
    uint8_t genre{kNoGenre};
    bool transport{false};
    uint32_t passMicros{5000};
    uint32_t tempoChangeAt{0};  // PLAY only: from this time on 132 BPM (0 = none)
};

struct Rig {
    MusicalEventRouter router;
    RecordingSink sink;
    Listener listener;
    Config config;
    std::vector<Started> started;
    uint32_t lastPass{kT0};
    uint32_t block{0};

    explicit Rig(const Config& c) : config(c) {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(Listener::Mode::Loop);
        listener.setLoopEvolve(c.evolve);
        listener.setAnswerBars(c.bars);
        gNow = kT0;
        if (c.transport) pass(kT0);  // PLAY: the clock exists before the first note
    }

    static uint32_t publishJitter(uint32_t index) {
        uint32_t h = index * 2654435761u;
        h ^= h >> 15;
        return h % (2 * kBlockMicros);
    }

    TransportView transportAt(uint32_t now) {
        TransportView view{};
        if (!config.transport) return view;
        view.playing = true;
        view.epoch = 7;
        // The timeline publishes once per audio block (11.6 ms of audio) with
        // the position of the block start. The audio task does not publish
        // exactly on time: block k becomes visible 0..2 blocks after its
        // start (scheduling, DMA refills), so the main loop sees a position
        // that is up to ~3 blocks old and re-anchors on it.
        while (true) {
            const uint32_t next = block + 1;
            const uint32_t visibleAt = kT0 + next * kBlockMicros + publishJitter(next);
            if (static_cast<int32_t>(now - visibleAt) < 0) break;
            block = next;
        }
        view.blockSequence = block + 1;
        const uint32_t blockStart = kT0 + block * kBlockMicros;
        const uint32_t change = config.tempoChangeAt;
        if (change == 0 || static_cast<int32_t>(blockStart - change) < 0) {
            view.steps = static_cast<double>(blockStart - kT0) / kSixteenth;
        } else {
            view.steps = static_cast<double>(change - kT0) / kSixteenth +
                         static_cast<double>(blockStart - change) / kFastSixteenth;
        }
        view.bpm = change != 0 && static_cast<int32_t>(now - change) >= 0 ? 132.0 : 120.0;
        return view;
    }

    // One main-loop pass, as serviceCallResponseLog().
    void pass(uint32_t now) {
        gNow = now;
        PhraseCapture::Phrase phrase;
        PhraseCapture::Phrase heard;
        if (listener.service(now, transportAt(now), 120.0f, phrase, &heard) != Close::Phrase) return;
        MotifDescriptor motif{};
        if (!describe(phrase, 0, kMajor, 7, motif)) return;
        const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
        if (!listener.canAnswer(phrase)) return;
        ResponseHarmony harmony{};
        if (config.harmony) {
            const int32_t start = phrase.windowStartStep + phrase.windowSteps;
            for (uint8_t beat = 0; beat < phrase.windowSteps / kStrongBeatSteps; ++beat) {
                const int32_t step = start + beat * kStrongBeatSteps;
                harmony.set(beat, kChords[(step / PhraseCapture::kStepsPerBar) % 4], ChordSource::Ahead);
            }
        }
        const ResponseStyle style = pickResponseStyle(config.genre, responseSeed(phrase));
        const uint32_t signature = callSignature(phrase, motif);
        ReplyStrategy strategy{};
        ResponsePlan plan;
        ResponseHarmonyReport report;
        if (!planPerformanceReply(phrase, motif, key, harmony, style, listener.replyMemory(), true, false,
                                  listener.loopEvolve(), plan, strategy, report,
                                  listener.answerCadence(), listener.loopCycle())) return;  // phase 8: pass form
        if (!listener.startAnswer(phrase, plan, now)) return;
        listener.rememberStartedReply(signature, strategy);
        started.push_back(Started{phrase.windowStartStep + phrase.windowSteps, listener.loopCycle(),
                                  static_cast<uint8_t>(strategy)});
    }

    void runTo(uint32_t until) {
        while (lastPass + config.passMicros <= until) {
            lastPass += config.passMicros;
            pass(lastPass);
        }
    }
    void human(bool on, double step, uint8_t pitch) {
        const uint32_t when = at(step);
        runTo(when);
        gNow = when;
        listener.onPerformanceInputNote(on, 'h', pitch, 100);
    }
};

struct Shape {
    const char* name;
    uint8_t onsets[32];
    uint8_t lengths[32];
    uint8_t count;
};

// Loop shapes in sixteenths of the window (built per BARS).
Shape makeShape(int kind, int window) {
    Shape s{};
    auto add = [&](int onset, int length) {
        s.onsets[s.count] = static_cast<uint8_t>(onset);
        s.lengths[s.count] = static_cast<uint8_t>(length);
        ++s.count;
    };
    switch (kind) {
        case 0:  // even quarter notes
            s.name = "quarters";
            for (int i = 0; i < window; i += 4) add(i, 2);
            break;
        case 1:  // dense eighths (the "short reply" density)
            s.name = "eighths";
            for (int i = 0; i < window; i += 2) add(i, 1);
            break;
        case 2:  // downbeat, then off-beats in every bar (genre MINIMAL keeps strong beats only)
            s.name = "offbeats";
            add(0, 2);
            for (int bar = 0; bar < window; bar += 16) {
                for (int off : {6, 10, 14}) add(bar + off, 2);
                if (bar + 16 < window) add(bar + 18 - 16 + 16, 2);  // 2 of the next bar
            }
            break;
        case 3:  // three notes
            s.name = "three";
            add(0, 3);
            add(window / 2, 3);
            add(window - 4, 3);
            break;
        case 4:  // legato quarters: no rest at all, the loop must never stop sounding
            s.name = "legato";
            for (int i = 0; i < window; i += 4) add(i, 4);
            break;
        default:  // one note
            s.name = "one";
            add(0, 4);
            break;
    }
    // Sorted, unique, inside the window.
    for (uint8_t i = 1; i < s.count; ++i) {
        for (uint8_t j = i; j > 0 && s.onsets[j - 1] > s.onsets[j]; --j) {
            std::swap(s.onsets[j - 1], s.onsets[j]);
            std::swap(s.lengths[j - 1], s.lengths[j]);
        }
    }
    uint8_t kept = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        if (s.onsets[i] >= window) continue;
        if (kept > 0 && s.onsets[kept - 1] == s.onsets[i]) continue;
        s.onsets[kept] = s.onsets[i];
        s.lengths[kept] = s.lengths[i];
        ++kept;
    }
    s.count = kept;
    return s;
}

// Longest rest of the loop played over and over (between note end and the
// next onset, the wrap included).
int loopLongestRest(const Shape& s, int window) {
    int longest = 0;
    for (uint8_t i = 0; i < s.count; ++i) {
        int end = s.onsets[i] + s.lengths[i];
        const int next = i + 1 < s.count ? s.onsets[i + 1] : window + s.onsets[0];
        if (end > next) end = next;
        if (next - end > longest) longest = next - end;
    }
    return longest;
}

int gFailures = 0;
int gPrintLimit = 12;  // CR_LOOP_GAPS_ALL=1 prints every failure
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            if (gFailures < gPrintLimit) {                     \
                std::printf("FAIL %s: ", #cond);          \
                std::printf(__VA_ARGS__);                 \
                std::printf("\n");                        \
            }                                             \
            ++gFailures;                                  \
        }                                                 \
    } while (0)

constexpr int kReplayWindows = 6;

void runOne(const Config& config, int shapeKind, long& windowsChecked) {
    const int window = config.bars * PhraseCapture::kStepsPerBar;
    const Shape shape = makeShape(shapeKind, window);
    Rig rig(config);
    // The question: window 0 (free clock: the first note is step 0).
    for (uint8_t i = 0; i < shape.count; ++i) {
        const uint8_t pitch = static_cast<uint8_t>(60 + (i * 2) % 12);
        rig.human(true, shape.onsets[i], pitch);
        double off = shape.onsets[i] + shape.lengths[i] - 0.25;
        rig.human(false, off, pitch);
    }
    const int lastWindowEnd = window * (1 + kReplayWindows);
    rig.runTo(at(lastWindowEnd) + 3 * kBlockMicros + config.passMicros);  // window 6 fully played (its tail NoteOff on the boundary)

    char tag[160];
    std::snprintf(tag, sizeof(tag), "evolve=%u bars=%u harmony=%d genre=%u shape=%s clock=%s pass=%u",
                  config.evolve, config.bars, config.harmony ? 1 : 0, config.genre, shape.name,
                  config.transport ? "PLAY" : "FREE", static_cast<unsigned>(config.passMicros));

    // A reply was started for every window, cycles 0..5, on the grid.
    CHECK(rig.started.size() >= static_cast<std::size_t>(kReplayWindows), "%s replies=%zu", tag,
          rig.started.size());
    const int rest = loopLongestRest(shape, window);
    const auto& ev = rig.sink.events;
    const auto& tm = rig.sink.times;
    double firstOnPrev = -1.0;
    for (std::size_t r = 0; r < rig.started.size() && r < static_cast<std::size_t>(kReplayWindows); ++r) {
        const Started& s = rig.started[r];
        CHECK(s.answerWindow == window * static_cast<int>(r + 1) && s.cycle == r,
              "%s reply %zu window=%d cycle=%u", tag, r, s.answerWindow, s.cycle);
        // The notes of this window: NoteOn time and length (to its NoteOff).
        const double from = window * static_cast<double>(r + 1);
        std::vector<double> ons;
        std::vector<double> lens;
        for (std::size_t i = 0; i < ev.size(); ++i) {
            if (ev[i].type != MusicalEventType::NoteOn) continue;
            const double step = static_cast<double>(tm[i] - kT0) / kSixteenth;
            if (step < from || step >= from + window) continue;
            double off = -1.0;
            for (std::size_t j = i + 1; j < ev.size(); ++j) {
                if (ev[j].type == MusicalEventType::NoteOff && ev[j].note == ev[i].note) {
                    off = static_cast<double>(tm[j] - kT0) / kSixteenth;
                    break;
                }
            }
            ons.push_back(step - from);
            lens.push_back(off - step);
        }
        // No loop note is lost: as many notes as the question.
        CHECK(ons.size() == shape.count, "%s window %zu: %zu notes, question has %u (strategy=%u)", tag, r + 1,
              ons.size(), shape.count, s.strategy);
        // A NoteOn is sent by the first pass at or after its time; under PLAY
        // the first reply's grid is a clock estimate up to one audio block late.
        const double tolerance = (static_cast<double>(config.passMicros) +
                                  (config.transport ? 3 * kBlockMicros : 0)) / kSixteenth + 1e-9;
        // Rhythm = the question's: every onset and every length in steps.
        // EVOLVE 0/1 exactly (within one main-loop pass). EVOLVE 2/3 vary the
        // rhythm on purpose (DEVELOP / tail shift: a note at most one step
        // later and one step shorter) - the first note stays on the downbeat.
        for (std::size_t i = 0; i < ons.size() && i < shape.count; ++i) {
            const bool last = i + 1 == shape.count;
            (void)last;
            const double shift = config.evolve <= 1 ? 0.0 : 1.0;
            const double grid = shape.onsets[i];
            const bool onsetOk = i == 0 ? (ons[i] >= grid - 1e-9 && ons[i] <= grid + tolerance)
                                        : (ons[i] >= grid - 1e-9 && ons[i] <= grid + shift + tolerance);
            CHECK(onsetOk, "%s window %zu note %zu: onset %.3f, question %u (strategy=%u)", tag, r + 1, i,
                  ons[i], shape.onsets[i], s.strategy);
            const double want = shape.lengths[i];
            const double shorter = config.evolve <= 1 ? 0.0 : shift;
            CHECK(lens[i] >= want - shorter - tolerance && lens[i] <= want + tolerance,
                  "%s window %zu note %zu: length %.3f, question %u", tag, r + 1, i, lens[i], shape.lengths[i]);
        }
        // Window to window: the first NoteOn of each pass is exactly one window
        // after the previous one (no micro gap / overlap at the loop boundary),
        // up to one main-loop pass when the grid is not on the pass grid.
        if (r > 0 && !ons.empty() && firstOnPrev >= 0) {
            const double period = static_cast<double>(window) * kSixteenth;
            // PLAY: the first reply's grid is an estimate, not on the pass
            // grid, so a NoteOn waits up to one pass for it.
            const double slack = !config.transport &&
                                         std::fmod(period, static_cast<double>(config.passMicros)) == 0.0
                ? 0.0 : static_cast<double>(config.passMicros);
            const double actual = (ons[0] + from) * kSixteenth - firstOnPrev;
            CHECK(std::fabs(actual - period) <= slack + 1.0,
                  "%s window %zu: downbeat %.0f us after the previous one, window is %.0f us", tag, r + 1, actual,
                  period);
        }
        if (!ons.empty()) firstOnPrev = (ons[0] + from) * kSixteenth;
        // Every bar with a loop note sounds.
        for (int bar = 0; bar < config.bars; ++bar) {
            bool loopHas = false;
            for (uint8_t i = 0; i < shape.count; ++i) if (shape.onsets[i] / 16 == bar) loopHas = true;
            if (!loopHas) continue;
            bool sounds = false;
            for (double on : ons) if (static_cast<int>(on) / 16 == bar) sounds = true;
            CHECK(sounds, "%s window %zu: bar %d silent (strategy=%u)", tag, r + 1, bar + 1, s.strategy);
        }
        ++windowsChecked;
    }

    // Longest silence over the whole run, from the first answer NoteOn to the
    // end of the last window: never longer than the loop's own longest rest
    // (+ 1 step at EVOLVE 2/3: DEVELOP / tail shift; + one main-loop pass).
    {
        int sounding = 0;
        double silentSince = -1.0;
        double longest = 0.0;
        bool begun = false;
        for (std::size_t i = 0; i < ev.size(); ++i) {
            const double step = static_cast<double>(tm[i] - kT0) / kSixteenth;
            if (ev[i].type == MusicalEventType::NoteOn) {
                if (begun && sounding == 0 && step - silentSince > longest) longest = step - silentSince;
                begun = true;
                ++sounding;
            } else if (ev[i].type == MusicalEventType::NoteOff) {
                if (--sounding == 0) silentSince = step;
            }
        }
        const double allowed = rest + (config.evolve >= 2 ? 1 : 0) +
                               static_cast<double>(config.passMicros) / kSixteenth + 1e-9;
        CHECK(longest <= allowed, "%s longest silence %.2f steps, loop rest %d", tag, longest, rest);
    }
    CHECK(rig.listener.responder().staleDrops() == 0, "%s stale drops %u", tag,
          static_cast<unsigned>(rig.listener.responder().staleDrops()));
    // Same instant: every NoteOff is sent before any NoteOn (the tail of the
    // previous note/reply is released before the next attack, in that order).
    for (std::size_t i = 1; i < ev.size(); ++i) {
        CHECK(!(tm[i] == tm[i - 1] && ev[i].type == MusicalEventType::NoteOff &&
                ev[i - 1].type == MusicalEventType::NoteOn),
              "%s NoteOff after a NoteOn at the same time %u", tag, static_cast<unsigned>(tm[i]));
    }
    // Balance: OFF releases a sounding note; every NoteOn has its NoteOff.
    rig.listener.setMode(Listener::Mode::Off);
    int open[128]{};
    for (const MusicalEvent& e : ev) {
        if (e.type == MusicalEventType::NoteOn) {
            CHECK(open[e.note] == 0, "%s double NoteOn %u", tag, e.note);
            ++open[e.note];
        } else if (e.type == MusicalEventType::NoteOff) {
            CHECK(open[e.note] == 1, "%s NoteOff without NoteOn %u", tag, e.note);
            --open[e.note];
        }
    }
    for (int n = 0; n < 128; ++n) CHECK(open[n] == 0, "%s note %d left sounding", tag, n);
}

void testLoopWithoutGaps() {
    long windows = 0;
    long runs = 0;
    for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
        for (uint8_t bars : {1, 2}) {
            for (bool harmony : {false, true}) {
                for (int genre = -1; genre < static_cast<int>(kGenreCount); ++genre) {
                    for (int shape = 0; shape < 6; ++shape) {
                        Config config;
                        config.evolve = evolve;
                        config.bars = bars;
                        config.harmony = harmony;
                        config.genre = genre < 0 ? kNoGenre : static_cast<uint8_t>(genre);
                        runOne(config, shape, windows);
                        ++runs;
                    }
                }
            }
        }
    }
    std::printf("loop gaps: %ld runs, %ld windows checked\n", runs, windows);
}

// PLAY clock (audio-block anchors published with jitter) and main-loop passes
// of 1, 5, 20 and 60 ms (below one 16th = 125 ms): one pass late at most,
// nothing dropped, and window to window exactly one window apart.
void testClocksAndPasses() {
    long windows = 0;
    for (bool transport : {false, true}) {
        for (uint32_t pass : {1000u, 5000u, 20000u, 60000u}) {
            for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
                for (uint8_t bars : {1, 2}) {
                    for (int shape = 0; shape < 6; ++shape) {
                        Config config;
                        config.evolve = evolve;
                        config.bars = bars;
                        config.harmony = true;
                        config.genre = 3;
                        config.transport = transport;
                        config.passMicros = pass;
                        runOne(config, shape, windows);
                    }
                }
            }
        }
    }
    std::printf("clocks/passes: %ld windows checked\n", windows);
}

// REPLACE 3+: a window with 1 or 2 human notes does not become the loop and
// still gets the replay, on the downbeat, with every loop note.
// PLAY tempo change (120 -> 132 BPM) in the middle of a LOOP: the chained
// grid lets go (the PLAY estimate decides again), a reply still starts in
// every window, nothing is dropped, NoteOn/NoteOff stay balanced.
void testTempoChangeResyncs() {
    for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
        Config config;
        config.evolve = evolve;
        config.transport = true;
        config.harmony = true;
        config.genre = 3;
        config.tempoChangeAt = at(80.0);
        Rig rig(config);
        for (int i = 0; i < 32; i += 4) {
            rig.human(true, i, static_cast<uint8_t>(60 + i / 4));
            rig.human(false, i + 1.75, static_cast<uint8_t>(60 + i / 4));
        }
        rig.runTo(at(220.0));
        // Windows 32, 64, 96 (at 120), then on at 132 BPM until ~step 233.
        CHECK(rig.started.size() >= 6, "tempo evolve=%u replies=%zu", evolve, rig.started.size());
        for (std::size_t r = 0; r < rig.started.size(); ++r) {
            CHECK(rig.started[r].answerWindow == 32 * static_cast<int>(r + 1) && rig.started[r].cycle == r,
                  "tempo evolve=%u reply %zu window=%d", evolve, r, rig.started[r].answerWindow);
        }
        CHECK(rig.listener.responder().staleDrops() == 0, "tempo evolve=%u stale", evolve);
        rig.listener.setMode(Listener::Mode::Off);
        int open[128]{};
        for (const MusicalEvent& e : rig.sink.events) {
            if (e.type == MusicalEventType::NoteOn) {
                CHECK(open[e.note] == 0, "tempo double NoteOn %u", e.note);
                ++open[e.note];
            } else if (e.type == MusicalEventType::NoteOff) {
                CHECK(open[e.note] == 1, "tempo NoteOff without NoteOn %u", e.note);
                --open[e.note];
            }
        }
        for (int n = 0; n < 128; ++n) CHECK(open[n] == 0, "tempo note %d left sounding", n);
    }
}

void testThinWindowStillReplays() {
    for (uint8_t evolve = 0; evolve <= 3; ++evolve) {
        Config config;
        config.evolve = evolve;
        Rig rig(config);
        const uint8_t onsets[4] = {0, 4, 8, 12};
        for (uint8_t i = 0; i < 4; ++i) {
            rig.human(true, onsets[i], static_cast<uint8_t>(60 + i));
            rig.human(false, onsets[i] + 1.75, static_cast<uint8_t>(60 + i));
        }
        rig.human(true, 40.0, 72);  // one stray note in window 1
        rig.human(false, 41.0, 72);
        rig.human(true, 66.0, 74);  // two in window 2
        rig.human(false, 67.0, 74);
        rig.human(true, 70.0, 76);
        rig.human(false, 71.0, 76);
        rig.runTo(at(128.0) - 1);
        CHECK(rig.started.size() == 3, "thin evolve=%u replies=%zu", evolve, rig.started.size());
        for (std::size_t r = 1; r < rig.started.size(); ++r) {
            CHECK(rig.started[r].cycle == r, "thin evolve=%u reply %zu cycle=%u",
                  evolve, r, rig.started[r].cycle);
            // The replay's first NoteOn sits on the window downbeat.
            const uint32_t downbeat = at(32.0 * (r + 1));
            bool found = false;
            for (std::size_t i = 0; i < rig.sink.events.size(); ++i) {
                if (rig.sink.events[i].type == MusicalEventType::NoteOn && rig.sink.times[i] == downbeat) found = true;
            }
            CHECK(found, "thin evolve=%u reply %zu: no NoteOn on the downbeat", evolve, r);
        }
    }
}

}  // namespace

int main() {
    if (std::getenv("CR_LOOP_GAPS_ALL")) gPrintLimit = 1 << 30;
    testLoopWithoutGaps();
    testClocksAndPasses();
    testThinWindowStillReplays();
    testTempoChangeResyncs();
    if (gFailures != 0) {
        std::printf("cr loop gaps: %d failures\n", gFailures);
        return 1;
    }
    std::puts("cr loop gaps tests passed");
    return 0;
}
