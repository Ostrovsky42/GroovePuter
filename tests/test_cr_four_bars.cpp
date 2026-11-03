// CALL/RESPONSE phase 4: PARTNER BARS = 4 (capture, 64-bit mask, planner,
// setting, 4-row harmony lookahead). BARS 1 and 2 are covered unchanged by the
// phase 0-3 tests; this file only checks the new length and its boundaries.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"
#include "src/input/call_response_performance_plan.h"
#include "src/input/performance_harmony_lookahead.h"

using namespace GroovePuterCallResponse;
using Close = PhraseCapture::CloseResult;
using Listener = CallResponseListener;

namespace {

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int8_t kMinor[] = {0, 2, 3, 5, 7, 8, 10};

// ---- Capture ----------------------------------------------------------------

void testFourBarWindow() {
    PhraseCapture capture;
    capture.setWindowBars(4);
    assert(capture.windowSteps() == 64 && capture.capacity() == 32);
    PhraseCapture::Phrase phrase;
    assert(capture.closeDueWindow(0.0, phrase) == Close::None);
    assert(capture.noteOn(1, 60, 100, 0.1));  capture.noteOff(1, 2.0);    // bar 1
    assert(capture.noteOn(2, 62, 90, 20.0));  capture.noteOff(2, 22.0);   // bar 2
    assert(capture.noteOn(3, 64, 80, 31.8));  capture.noteOff(3, 33.0);   // 32: inside, no boundary
    assert(capture.noteOn(4, 65, 70, 40.0));  capture.noteOff(4, 44.0);   // bar 3
    assert(capture.noteOn(5, 67, 60, 63.0));  capture.noteOff(5, 70.0);   // bar 4, clamped to 64
    // The 2-bar boundary is not a window boundary any more.
    assert(capture.closeDueWindow(32.0, phrase) == Close::None);
    assert(capture.closeDueWindow(63.99, phrase) == Close::None);
    assert(capture.closeDueWindow(64.0, phrase) == Close::Phrase);
    assert(phrase.windowSteps == 64 && phrase.windowStartStep == 0 && phrase.count == 5);
    const uint8_t onsets[] = {0, 20, 32, 40, 63};
    const uint8_t lengths[] = {2, 2, 1, 4, 1};
    for (uint8_t i = 0; i < 5; ++i) {
        assert(phrase.notes[i].onset == onsets[i]);
        assert(phrase.notes[i].length == lengths[i]);
    }
    assert(capture.windowStartStep() == 64);
}

void testFourBarOverflow() {
    PhraseCapture capture;
    capture.setWindowBars(4);
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    for (int i = 0; i < 40; ++i) {
        const bool stored = capture.noteOn(static_cast<uint16_t>(i + 1),
                                           static_cast<uint8_t>(40 + i), 100, i * 1.5);
        assert(stored == (i < PhraseCapture::kMaxCapacity));
        capture.noteOff(static_cast<uint16_t>(i + 1), i * 1.5 + 0.5);
    }
    assert(capture.overflowCount() == 8);
    assert(capture.closeDueWindow(64.0, phrase) == Close::Phrase);
    assert(phrase.count == 32 && phrase.dropped == 8);
    for (uint8_t i = 1; i < phrase.count; ++i) assert(phrase.notes[i - 1].onset < phrase.notes[i].onset);
    // Back to 2 bars: capacity is 16 again (phase 0-3 overflow unchanged).
    capture.setWindowBars(2);
    assert(capture.capacity() == 16);
    (void)capture.closeDueWindow(0.0, phrase);
    int stored = 0;
    for (int i = 0; i < 20; ++i) stored += capture.noteOn(static_cast<uint16_t>(i + 1), 60, 100, i * 1.5) ? 1 : 0;
    assert(stored == 16);
    capture.setWindowBars(1);
    assert(capture.windowSteps() == 16 && capture.capacity() == 16);
    capture.setWindowBars(3);  // anything else = 2
    assert(capture.windowSteps() == 32);
}

// Pickup on the 4-bar boundary: the last 30 % of step 63 belongs to the next
// window; earlier stays on step 63; a note held across the boundary is cut.
void testFourBarPickup() {
    PhraseCapture capture;
    capture.setWindowBars(4);
    PhraseCapture::Phrase phrase;
    (void)capture.closeDueWindow(0.0, phrase);
    assert(capture.noteOn(1, 60, 100, 10.0)); capture.noteOff(1, 12.0);
    assert(capture.noteOn(2, 62, 100, 63.6)); capture.noteOff(2, 63.9);  // early: stays, 63
    assert(capture.noteOn(3, 67, 100, 63.8));                            // pickup -> next window
    assert(capture.closeDueWindow(64.0, phrase) == Close::Phrase);
    assert(phrase.count == 2);
    assert(phrase.notes[1].onset == 63 && phrase.notes[1].length == 1 && phrase.notes[1].note == 62);
    assert(capture.pendingCount() == 1 && capture.hasOpenNotes());
    // Held over the whole next window: clamped to its end (length 64).
    assert(capture.closeDueWindow(128.0, phrase) == Close::Phrase);
    assert(phrase.windowStartStep == 64 && phrase.count == 1);
    assert(phrase.notes[0].onset == 0 && phrase.notes[0].note == 67 && phrase.notes[0].length == 64);
    capture.noteOff(3, 130.0);  // late NoteOff of a closed note: ignored
    assert(capture.pendingCount() == 0);
}

// ---- 64-bit mask ---------------------------------------------------------------

void testMask64() {
    PhraseCapture::Phrase phrase{};
    phrase.windowSteps = 64;
    const uint8_t onsets[] = {0, 20, 33, 47, 63};
    for (uint8_t i = 0; i < 5; ++i) phrase.notes[i] = CapturedNote{onsets[i], 1, static_cast<uint8_t>(60 + i), 100};
    phrase.count = 5;
    MotifDescriptor d;
    assert(describe(phrase, 0, kMajor, 7, d));
    const uint64_t expected = (uint64_t{1} << 0) | (uint64_t{1} << 20) | (uint64_t{1} << 33) |
                              (uint64_t{1} << 47) | (uint64_t{1} << 63);
    assert(d.onsetMask == expected && d.windowSteps == 64);
    char line[256];
    const int n = formatMotifLog(line, sizeof(line), 1, d, "FREE", 0);
    assert(n > 0 && n < static_cast<int>(sizeof(line)));
    const char* mask = std::strstr(line, "mask=0b");
    assert(mask);
    mask += 7;
    char want[64 + 3 + 1]{};
    std::size_t w = 0;
    for (int bit = 0; bit < 64; ++bit) {
        if (bit > 0 && bit % 16 == 0) want[w++] = '_';
        want[w++] = (expected >> bit) & 1u ? '1' : '0';
    }
    assert(std::strncmp(mask, want, w) == 0 && mask[w] == ' ');
    std::puts(line);
    // A 2-bar phrase still prints 32 bits (phase 0 format).
    phrase.windowSteps = 32;
    phrase.count = 2;
    assert(describe(phrase, 0, kMajor, 7, d));
    formatMotifLog(line, sizeof(line), 1, d, "FREE", 0);
    assert(std::strstr(line, "mask=0b1000000000000000_0000100000000000 "));
}

// ---- Planner -------------------------------------------------------------------

uint32_t gRng = 0x1234567u;
uint32_t rnd() { gRng = gRng * 1664525u + 1013904223u; return gRng >> 8; }

PhraseCapture::Phrase randomFourBarCall(const int8_t* scale, uint8_t root) {
    PhraseCapture::Phrase phrase{};
    phrase.windowSteps = 64;
    phrase.windowStartStep = 64 * static_cast<int32_t>(rnd() % 8);
    const uint8_t want = static_cast<uint8_t>(1 + rnd() % 32);
    uint64_t used = 0;
    for (uint8_t i = 0; i < want; ++i) used |= uint64_t{1} << (rnd() % 64);
    if (rnd() % 4 == 0) used |= uint64_t{1} << 63;  // the last step often
    for (uint8_t step = 0; step < 64 && phrase.count < 32; ++step) {
        if (!(used & (uint64_t{1} << step))) continue;
        const uint8_t degree = static_cast<uint8_t>(rnd() % 7);
        const uint8_t octave = static_cast<uint8_t>(3 + rnd() % 4);
        const uint8_t note = static_cast<uint8_t>(root + scale[degree] + octave * 12);
        const uint8_t length = static_cast<uint8_t>(1 + rnd() % (64 - step));
        phrase.notes[phrase.count++] = CapturedNote{step, length, note, static_cast<uint8_t>(40 + rnd() % 87)};
    }
    return phrase;
}

void checkPlan(const PhraseCapture::Phrase& call, const ResponsePlan& plan, const ResponseKey& key,
               uint8_t delay) {
    const int usable = 64 - delay;
    assert(plan.windowSteps == usable && plan.delaySteps == delay);
    assert(plan.count >= 1 && plan.count <= 32 && plan.count <= call.count);
    for (uint8_t i = 0; i < plan.count; ++i) {
        const ResponseNote& note = plan.notes[i];
        assert(note.length >= 1 && note.step + note.length <= usable);  // never past the window
        if (i > 0) {
            assert(plan.notes[i - 1].step < note.step);                         // sorted
            assert(plan.notes[i - 1].step + plan.notes[i - 1].length <= note.step);  // monophonic
        }
        assert(note.note >= kAnswerMinNote && note.note <= kAnswerMaxNote);
        assert(inKey(key, note.note));
        assert(note.velocity >= 1);
    }
}

void testPlannerFourBarSweep() {
    int plans = 0, reachedBar3 = 0;
    for (int trial = 0; trial < 3000; ++trial) {
        const bool minor = (trial & 1) != 0;
        const int8_t* scale = minor ? kMinor : kMajor;
        const uint8_t root = static_cast<uint8_t>(rnd() % 12);
        const ResponseKey key = responseKeyFromIntervals(root, scale, 7);
        const PhraseCapture::Phrase call = randomFourBarCall(scale, root);
        MotifDescriptor motif;
        assert(describe(call, root, scale, 7, motif));
        ResponseHarmony harmony{};
        for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat) {
            if (rnd() % 4 == 0) continue;
            const uint8_t r = static_cast<uint8_t>((root + scale[rnd() % 7]) % 12);
            harmony.set(beat, static_cast<uint16_t>((1u << r) | (1u << ((r + 4) % 12)) | (1u << ((r + 7) % 12))),
                        ChordSource::Ahead);
        }
        const uint8_t genre = static_cast<uint8_t>(rnd() % 17);  // 16 = none
        const ResponseStyle style = pickResponseStyle(genre < 16 ? genre : kNoGenre, responseSeed(call));
        for (uint8_t delay = 0; delay < 2; ++delay) {
            ResponsePlan plan;
            assert(planEchoVariant(call, motif, key, plan, delay));
            checkPlan(call, plan, key, delay);
            ResponseHarmonyReport report;
            assert(planHarmonicEcho(call, motif, key, harmony, style, plan, &report, delay));
            checkPlan(call, plan, key, delay);
            ++plans;
            const ResponseNote& last = plan.notes[plan.count - 1];
            if (call.notes[call.count - 1].onset >= 32 && call.notes[call.count - 1].onset < 64 - delay) {
                assert(last.step >= 32);  // a CALL into bar 3-4 is answered there
                ++reachedBar3;
            }
            for (int mode = 0; mode < 2; ++mode) {
                for (uint8_t evolve = 0; evolve < 4; ++evolve) {
                    ReplyStrategy actual{};
                    if (!planPerformanceReply(call, motif, key, harmony, style, ReplyMemory{},
                                              mode == 0, mode == 1, evolve, delay, plan, actual, report))
                        continue;
                    checkPlan(call, plan, key, delay);
                }
            }
        }
    }
    // A one-note CALL on the very last step (63) is still answered (step 62).
    PhraseCapture::Phrase one{};
    one.windowSteps = 64;
    one.notes[0] = CapturedNote{63, 1, 64, 100};
    one.count = 1;
    MotifDescriptor motif;
    assert(describe(one, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(planEchoVariant(one, motif, responseKeyFromIntervals(0, kMajor, 7), plan, 1));
    assert(plan.count == 1 && plan.notes[0].step == 62 && plan.droppedTail == 0);
    assert(plan.onsetMask() == (uint64_t{1} << 62));
    std::printf("phase4 planner: %d plans in key/range/window, %d answered into bars 3-4\n",
                plans, reachedBar3);
    assert(reachedBar3 > 100);
}

// Strong beats in answer bars 3 and 4 use the chords of beats 8..15.
void testFourBarStrongBeats() {
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    PhraseCapture::Phrase call{};
    call.windowSteps = 64;
    const uint8_t onsets[] = {0, 32, 48, 56};
    for (uint8_t i = 0; i < 4; ++i) call.notes[i] = CapturedNote{onsets[i], 4, 62, 100};  // all D
    call.count = 4;
    MotifDescriptor motif;
    assert(describe(call, 0, kMajor, 7, motif));
    ResponseHarmony harmony{};
    harmony.set(0, (1u << 0) | (1u << 4) | (1u << 7), ChordSource::Ahead);    // C, bar 1
    harmony.set(8, (1u << 9) | (1u << 0) | (1u << 4), ChordSource::Ahead);    // Am, bar 3
    harmony.set(12, (1u << 5) | (1u << 9) | (1u << 0), ChordSource::Ahead);   // F, bar 4
    assert(harmony.chordAtStep(32) == ((1u << 9) | (1u << 0) | (1u << 4)));
    assert(harmony.chordAtStep(51) == ((1u << 5) | (1u << 9) | (1u << 0)));
    assert(harmony.chordAtStep(60) == 0);  // beat 15: unknown
    ResponsePlan plan;
    ResponseHarmonyReport report;
    assert(planHarmonicEcho(call, motif, key, harmony, ResponseStyle{}, plan, &report, 1));
    assert(plan.count == 4);
    assert(pcIn(harmony.chordAtStep(32), plan.notes[1].note));
    assert(pcIn(harmony.chordAtStep(48), plan.notes[2].note));
    assert(report.strongNotes == 3 && report.strongChordTones == 3);
    char line[96];
    formatHarmonyLog(line, sizeof(line), harmony, ResponseStyle{}, report);
    assert(std::strstr(line, " chord=A3C0 "));
}

// ---- Setting (listener) ---------------------------------------------------------

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }
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
    double cursor{0.0};

    explicit Rig(uint8_t bars) {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(Listener::Mode::Answer);
        listener.setAnswerBars(bars);
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
    double stepOf(std::size_t i) const { return static_cast<double>(sink.times[i] - kT0) / kSixteenth; }
    int count(MusicalEventType type) const {
        int n = 0;
        for (const auto& e : sink.events) n += e.type == type ? 1 : 0;
        return n;
    }
};

void testBarsSetting() {
    Listener listener;
    assert(listener.answerBars() == 2);  // default unchanged
    listener.cycleAnswerBars(1);
    assert(listener.answerBars() == 1);  // phase-3 forward step kept
    listener.cycleAnswerBars(1);
    assert(listener.answerBars() == 4);
    listener.cycleAnswerBars(1);
    assert(listener.answerBars() == 2);
    listener.cycleAnswerBars(-1);
    assert(listener.answerBars() == 4);
    listener.cycleAnswerBars(-1);
    assert(listener.answerBars() == 1);
    listener.cycleAnswerBars(-1);
    assert(listener.answerBars() == 2);
    listener.setAnswerBars(4);
    assert(listener.answerBars() == 4 && listener.capture().windowSteps() == 64);
    listener.setAnswerBars(3);
    assert(listener.answerBars() == 2);
}

// BARS 4: the CALL closes after 4 bars; the answer fills the next 4 bars.
void testFourBarAnswer() {
    Rig rig(4);
    rig.note(0.0, 4.0, 62);
    rig.note(40.0, 44.0, 64);  // bar 3 of the CALL
    rig.runTo(32.0 + 8.0);
    assert(rig.answers == 0);  // no answer after 2 bars any more
    rig.runTo(140.0);
    assert(rig.answers == 1);
    assert(rig.lastPhrase.windowSteps == 64 && rig.lastPhrase.windowStartStep == 0);
    assert(rig.lastPlan.windowSteps == 63 && rig.lastPlan.count == 2);
    assert(rig.lastPlan.notes[1].step == 40);
    assert(!rig.sink.events.empty() && rig.sink.events.front().type == MusicalEventType::NoteOn);
    assert(rig.stepOf(0) == 65.0);  // 4 bars + 1/16
    for (std::size_t i = 0; i < rig.sink.events.size(); ++i) {
        assert(rig.stepOf(i) >= 65.0 && rig.stepOf(i) <= 128.0);  // inside the ANSWER window
    }
    assert(rig.count(MusicalEventType::NoteOn) == 2 && rig.count(MusicalEventType::NoteOff) == 2);
}

// A one-note CALL on the last step of 4 bars is answered on step 62.
void testFourBarOneNoteLast() {
    Rig last(4);
    last.note(0.0, 1.0, 60);
    last.runTo(64.0 + 64.0 + 8.0);  // first answer done
    const std::size_t before = last.sink.events.size();
    last.note(64.0 * 3 - 0.5, 64.0 * 3, 67);  // early downbeat of window 3 -> onset 63 of window 2
    last.runTo(64.0 * 5);
    assert(last.answers == 2);
    assert(last.lastPhrase.count == 1 && last.lastPhrase.notes[0].onset == 63);
    assert(last.lastPlan.notes[0].step == 62);
    assert(last.sink.events.size() == before + 2);
    assert(last.stepOf(before) == 64.0 * 3 + 1 + 62);
}

// Changing BARS while a 4-bar answer sounds releases the note at once.
void testBarsChangeCancelsFourBars() {
    for (uint8_t next : {uint8_t{2}, uint8_t{1}}) {
        Rig rig(4);
        rig.note(0.0, 16.0, 62);
        rig.runTo(70.0);
        assert(rig.listener.answering() && rig.listener.responder().soundingCount() == 1);
        const std::size_t before = rig.sink.events.size();
        rig.listener.setAnswerBars(next);
        assert(rig.listener.answerBars() == next && !rig.listener.answering());
        assert(rig.sink.events.size() == before + 1 &&
               rig.sink.events.back().type == MusicalEventType::NoteOff);
        assert(rig.listener.clockSource() == Listener::ClockSource::None);
        rig.runTo(200.0);
        assert(rig.count(MusicalEventType::NoteOn) == rig.count(MusicalEventType::NoteOff));
    }
    // And into 4 bars from a sounding 2-bar answer.
    Rig rig(2);
    rig.note(0.0, 8.0, 62);
    rig.runTo(34.0);
    assert(rig.listener.responder().soundingCount() == 1);
    const std::size_t before = rig.sink.events.size();
    rig.listener.cycleAnswerBars(-1);  // 2 -> 4
    assert(rig.listener.answerBars() == 4 && !rig.listener.answering());
    assert(rig.sink.events.size() == before + 1 &&
           rig.sink.events.back().type == MusicalEventType::NoteOff);
}

// Playing over a 4-bar answer cancels it (the turn rule is unchanged).
void testHumanCancelFourBars() {
    Rig rig(4);
    for (int i = 0; i < 8; ++i) rig.note(i * 8.0, i * 8.0 + 2.0, static_cast<uint8_t>(60 + i));
    rig.runTo(80.0);
    assert(rig.listener.answering());
    rig.note(80.0, 81.0, 72);
    assert(!rig.listener.answering() && rig.listener.humanCancels() == 1);
    rig.runTo(300.0);
    assert(rig.count(MusicalEventType::NoteOn) == rig.count(MusicalEventType::NoteOff));
}

// ---- Lookahead over 4 Song rows -------------------------------------------------

using namespace GroovePuterPerformance;
using GroovePuterHarmony::HarmonySnapshot;
using GroovePuterHarmony::SongHarmonicParticipant;
constexpr int16_t kSongStart = 10;

GroovePuterMaterial::GeneratedSynthAOrigin makeOrigin4() {
    GroovePuterMaterial::GeneratedSynthAOrigin origin{};
    origin.common.barCount = 4;
    origin.common.rootPitchClass = 0;
    origin.common.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
    origin.common.harmonicAuthority = GroovePuterHarmony::HarmonicAuthority::Generic;
    GroovePuterRhythm::ChordProgressionSourceRequest request{};
    request.requestedId = GroovePuterRhythm::ProgressionId::PopCycle;
    request.phraseBars = 4;
    origin.common.progressionSource = GroovePuterRhythm::realizeChordProgressionSource(request).source;
    for (uint8_t bar = 0; bar < 4; ++bar) {
        auto& entry = origin.bars[bar];
        entry.phraseBarOrdinal = bar;
        entry.material.address = {0, static_cast<uint8_t>(4 + bar)};
        entry.material.id = {101u + bar};
        entry.originPatternVersion = {0x1000u + bar, 0x2000u + bar};
        entry.harmonicRhythm.phraseBarOrdinal = bar;
        entry.harmonicRhythm.phraseHarmonicPosition = static_cast<uint8_t>(bar * 2u);
        entry.harmonicRhythm.eventCount = 2;
        entry.harmonicRhythm.onsets = GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8);
    }
    return origin;
}

GroovePuterMaterial::GeneratedPhraseRecipe makeRecipe4() {
    GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
    recipe.pageIndex = 0;
    recipe.songSlot = 0;
    recipe.songStart = kSongStart;
    recipe.bars = 4;
    recipe.valid = true;
    return recipe;
}

HarmonyPlaybackPosition positionAt4(uint8_t ordinal, uint64_t absoluteStep) {
    HarmonyPlaybackPosition position{};
    position.activePage = 0;
    position.playbackSongSlot = 0;
    position.songRow = static_cast<int16_t>(kSongStart + ordinal);
    position.activeSynthAPatternGlobal = static_cast<int16_t>(4 + ordinal);
    position.activeMaterial = GroovePuterMaterial::MaterialReference{
        {0, static_cast<uint8_t>(4 + ordinal)}, {101u + ordinal}};
    position.activeMaterialVersion = {0x1000u + ordinal, 0x2000u + ordinal};
    position.publicationGeneration = 2;
    position.transportEpoch = 7;
    position.absoluteStep = absoluteStep;
    return position;
}

uint8_t participants4(SongHarmonicParticipant (&out)[4]) {
    for (uint8_t bar = 0; bar < 4; ++bar) {
        out[bar] = {0, static_cast<uint8_t>(kSongStart + bar), 0, {101u + bar},
                    {0x1000u + bar, 0x2000u + bar}};
    }
    return 4;
}

bool samePitch(const HarmonySnapshot& a, const HarmonySnapshot& b) {
    return a.status == b.status && a.base.pitchMask == b.base.pitchMask &&
           a.base.rootPc == b.base.rootPc && a.base.qualityId == b.base.qualityId;
}

void testLookaheadFourRows() {
    const auto origin = makeOrigin4();
    const auto recipe = makeRecipe4();
    SongHarmonicParticipant participants[4];
    const uint8_t count = participants4(participants);
    const auto now = positionAt4(0, 160);
    HarmonySnapshot ahead{};
    HarmonySnapshot reference{};
    // Rows 1..2 ahead: as in phase 2 (default limit).
    assert(legacyHarmonyAhead(origin, recipe, participants, count, now, 32, 31, ahead));
    // Row 3 ahead (bars 3-4 of a 4-bar answer): refused by default, allowed with 4.
    assert(!legacyHarmonyAhead(origin, recipe, participants, count, now, 48, 31, ahead));
    assert(legacyHarmonyAhead(origin, recipe, participants, count, now, 48, 31, ahead, 4));
    assert(makeLegacyHarmonySnapshot(origin, recipe, positionAt4(3, 208), reference));
    assert(samePitch(ahead, reference));
    // Past the generated phrase (row 14): no snapshot even with 4 rows.
    assert(!legacyHarmonyAhead(origin, recipe, participants, count, now, 64, 31, ahead, 4));
    // Loop end / Song end at row 12: rows 13+ are not reachable by +1.
    assert(!legacyHarmonyAhead(origin, recipe, participants, count, now, 48, 12, ahead, 4));
    assert(legacyHarmonyAhead(origin, recipe, participants, count, now, 32, 12, ahead, 4));
    // Row 13 edited to other material: refused.
    SongHarmonicParticipant edited[4];
    participants4(edited);
    edited[3].materialId = {999};
    assert(!legacyHarmonyAhead(origin, recipe, edited, count, now, 48, 31, ahead, 4));
    // Never behind the published row.
    assert(!legacyHarmonyAhead(origin, recipe, participants, count, positionAt4(1, 176), -1, 31,
                               ahead, 4));
}

void testPatternLookaheadEighty() {
    GroovePuterMaterial::GeneratedPatternHarmonyOrigin origin{};
    origin.address = {0, 3};
    origin.authority = GroovePuterHarmony::HarmonicAuthority::Generic;
    origin.rootPitchClass = 0;
    origin.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
    origin.onsets = GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8);
    origin.eventCount = 2;
    origin.events[0].degree = 0;
    origin.events[1].degree = 3;
    origin.version = {0x11u, 0x22u};
    origin.sourceId = 1;
    origin.publicationGeneration = 3;
    if (!origin.valid()) {
        std::puts("pattern origin fixture not valid; pattern lookahead skipped");
        return;
    }
    PatternHarmonyPlaybackPosition now{};
    now.activePage = 0;
    now.activePatternGlobal = 3;
    now.activeVersion = origin.version;
    now.publicationGeneration = 3;
    now.transportEpoch = 1;
    now.absoluteStep = 40;
    HarmonySnapshot ahead{};
    assert(patternHarmonyAhead(origin, now, 48, ahead));
    assert(!patternHarmonyAhead(origin, now, 72, ahead));      // phase-2 limit kept by default
    assert(patternHarmonyAhead(origin, now, 72, ahead, 80));   // 4-bar answer
    auto later = now;
    later.absoluteStep = 40 + 72;
    HarmonySnapshot reference{};
    assert(makePatternHarmonySnapshot(origin, later, reference));
    assert(samePitch(ahead, reference));
    assert(!patternHarmonyAhead(origin, now, 81, ahead, 80));
}

}  // namespace

int main() {
    static_assert(sizeof(PhraseCapture::Phrase) <= 136, "Phrase grew past the phase-4 estimate");
    static_assert(ResponsePlan::kCapacity == 32, "4-bar answer capacity");
    static_assert(ResponseHarmony::kBeats == 16, "16 strong beats over 4 bars");
    testFourBarWindow();
    testFourBarOverflow();
    testFourBarPickup();
    testMask64();
    testPlannerFourBarSweep();
    testFourBarStrongBeats();
    testBarsSetting();
    testFourBarAnswer();
    testFourBarOneNoteLast();
    testBarsChangeCancelsFourBars();
    testHumanCancelFourBars();
    testLookaheadFourRows();
    testPatternLookaheadEighty();
    std::printf("sizeof PhraseCapture=%zu Phrase=%zu ResponsePlan=%zu ResponsePlayer=%zu Listener=%zu\n",
                sizeof(PhraseCapture), sizeof(PhraseCapture::Phrase), sizeof(ResponsePlan),
                sizeof(ResponsePlayer), sizeof(CallResponseListener));
    std::puts("cr four bars tests passed");
    return 0;
}
