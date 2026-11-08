// CALL/RESPONSE phase 6 (music):
//  1. every answer note keeps its CALL note's velocity (at the answer level),
//     so accents survive; EVOLVE 0 is unchanged;
//  2. the ending follows the chord under it and the place in the form: the
//     1st, 3rd, ... answer is a half cadence (fifth / chord tone, never the
//     tonic over V), the 2nd, 4th, ... a full one (tonic, or the nearest chord
//     tone in the key when the chord has no tonic);
//  3. without a chord ANSWER / DUET / LOOP end exactly as before.
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"
#include "src/input/call_response_performance_plan.h"

using namespace GroovePuterCallResponse;

namespace {

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int8_t kMinor[] = {0, 2, 3, 5, 7, 8, 10};

constexpr uint16_t pcs(std::initializer_list<int> list) {
    uint16_t mask = 0;
    for (int pc : list) mask = static_cast<uint16_t>(mask | (1u << (pc % 12)));
    return mask;
}

// C major.
constexpr uint16_t kC_I = pcs({0, 4, 7});
constexpr uint16_t kC_IV = pcs({5, 9, 0});
constexpr uint16_t kC_V = pcs({7, 11, 2});
// A natural minor: i - VI - VII - v.
constexpr uint16_t kA_i = pcs({9, 0, 4});
constexpr uint16_t kA_VI = pcs({5, 9, 0});
constexpr uint16_t kA_VII = pcs({7, 11, 2});
constexpr uint16_t kA_v = pcs({4, 7, 11});

struct Lcg {
    uint32_t state;
    uint32_t next() { state = state * 1664525u + 1013904223u; return state >> 8; }
};

// One chord per half bar of a 2-bar answer: c0 | c1 | c2 | c3.
ResponseHarmony progression(uint16_t c0, uint16_t c1, uint16_t c2, uint16_t c3) {
    ResponseHarmony harmony{};
    const uint16_t chords[4] = {c0, c1, c2, c3};
    for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat)
        harmony.set(beat, chords[(beat / 2) % 4], ChordSource::Ahead);
    return harmony;
}

// Random 2-bar CALL: 2..10 notes, distinct onsets < 31, random velocity 0..127.
PhraseCapture::Phrase randomCall(Lcg& rng, uint8_t lowNote, bool flatVelocity = false) {
    PhraseCapture::Phrase phrase;
    phrase.windowStartStep = 64;
    const uint8_t want = static_cast<uint8_t>(2 + rng.next() % 9);
    uint32_t used = 0;
    while (__builtin_popcount(used) < want) used |= 1u << (rng.next() % 31u);
    uint8_t count = 0;
    for (uint8_t onset = 0; onset < 31; ++onset) {
        if ((used & (1u << onset)) == 0) continue;
        CapturedNote& n = phrase.notes[count++];
        n.onset = onset;
        n.length = static_cast<uint8_t>(1 + rng.next() % 4);
        n.note = static_cast<uint8_t>(lowNote + rng.next() % 24);
        n.velocity = flatVelocity ? 96 : static_cast<uint8_t>(rng.next() % 128);
    }
    phrase.count = count;
    return phrase;
}

bool samePlan(const ResponsePlan& a, const ResponsePlan& b) {
    if (a.count != b.count || a.droppedTail != b.droppedTail || a.windowSteps != b.windowSteps)
        return false;
    for (uint8_t i = 0; i < a.count; ++i) {
        if (a.notes[i].step != b.notes[i].step || a.notes[i].length != b.notes[i].length ||
            a.notes[i].note != b.notes[i].note || a.notes[i].velocity != b.notes[i].velocity)
            return false;
    }
    return true;
}

int pc(int note) { return ((note % 12) + 12) % 12; }

// ---- 1. velocity -------------------------------------------------------------

void testAccentSurvives() {
    // C major, accent on the downbeats: 127 80 80 80 | 127 80.
    const CapturedNote notes[] = {{0, 2, 60, 127}, {2, 2, 62, 80}, {4, 2, 64, 80},
                                  {6, 2, 65, 80}, {16, 2, 67, 127}, {18, 4, 62, 80}};
    PhraseCapture::Phrase call;
    call.windowStartStep = 0;
    call.count = 6;
    for (uint8_t i = 0; i < 6; ++i) call.notes[i] = notes[i];
    MotifDescriptor motif;
    assert(describe(call, 0, kMajor, 7, motif));
    assert(motif.averageVelocity == 96);  // (2*127 + 4*80 + 3) / 6
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    for (int mode = 0; mode < 2; ++mode) {
        ResponsePlan plan;
        if (mode == 0) {
            assert(planEchoVariant(call, motif, key, plan));
        } else {
            ResponseHarmonyReport report;
            assert(planHarmonicEcho(call, motif, key, progression(kC_I, kC_IV, kC_V, kC_I),
                                    ResponseStyle{}, plan, &report, AnswerCadence::Half));
        }
        assert(plan.count == 6);
        uint32_t sum = 0;
        for (uint8_t i = 0; i < plan.count; ++i) {
            // Per note: the CALL velocity at the answer level (100 % = identical).
            assert(plan.notes[i].velocity == answerVelocityFor(notes[i].velocity));
            sum += plan.notes[i].velocity;
        }
        // The accent stays above the mean, the rest below it.
        const uint32_t mean = (sum + 3u) / 6u;
        assert(plan.notes[0].velocity > mean && plan.notes[4].velocity > mean);
        assert(plan.notes[1].velocity < mean && plan.notes[5].velocity < mean);
        assert(plan.notes[0].velocity - plan.notes[1].velocity ==
               answerVelocityFor(127) - answerVelocityFor(80));
        // The answer mean is the answer level of the CALL mean.
        assert(mean + 1 >= answerVelocityFor(motif.averageVelocity) &&
               mean <= answerVelocityFor(motif.averageVelocity) + 1u);
        std::printf("accent: call 127/80 -> answer %u/%u (mean %u, call mean %u)\n",
                    plan.notes[0].velocity, plan.notes[1].velocity, mean, motif.averageVelocity);
    }
}

void testVelocityRangeAllModes() {
    // Velocity 0 (a malformed NoteOn), 1 and 127 included; every output 1..127.
    Lcg rng{77};
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    const ResponseHarmony harmony = progression(kC_I, kC_IV, kC_V, kC_I);
    uint32_t notesChecked = 0;
    for (int i = 0; i < 600; ++i) {
        PhraseCapture::Phrase call = randomCall(rng, static_cast<uint8_t>(20 + rng.next() % 60));
        call.windowSteps = PhraseCapture::kWindowSteps;
        call.notes[0].velocity = static_cast<uint8_t>(i % 3 == 0 ? 0 : i % 3 == 1 ? 1 : 127);
        MotifDescriptor motif;
        assert(describe(call, 0, kMajor, 7, motif));
        for (int mode = 0; mode < 6; ++mode) {
            const bool loop = mode >= 2;
            const bool duet = mode == 1;
            const uint8_t evolve = static_cast<uint8_t>(loop ? mode - 2 : 1);
            {  // phase 9: no DELAY axis (every answer on the grid)
                ResponsePlan plan;
                ReplyStrategy actual{};
                ResponseHarmonyReport report;
                if (!planPerformanceReply(call, motif, key, harmony, ResponseStyle{}, ReplyMemory{},
                                          loop, duet, evolve, plan, actual, report,
                                          answerCadenceFor(static_cast<uint16_t>(i))))
                    continue;
                for (uint8_t n = 0; n < plan.count; ++n) {
                    assert(plan.notes[n].velocity >= 1 && plan.notes[n].velocity <= 127);
                    // Every answer velocity is one of the CALL velocities at the level.
                    bool fromCall = false;
                    for (uint8_t c = 0; c < call.count; ++c) {
                        const uint8_t v = loop && evolve == 0
                            ? (call.notes[c].velocity ? call.notes[c].velocity : 1)
                            : answerVelocityFor(call.notes[c].velocity);
                        fromCall = fromCall || v == plan.notes[n].velocity;
                    }
                    assert(fromCall);
                    ++notesChecked;
                }
            }
        }
    }
    assert(answerVelocityFor(0) == 1 && answerVelocityFor(1) == 1 && answerVelocityFor(127) == 127);
    std::printf("velocity range: %u answer notes in 1..127\n", notesChecked);
}

void testShortReplyAndDevelopKeepTheirNotesVelocity() {
    PhraseCapture::Phrase call;
    call.windowSteps = 32;
    call.count = 6;
    const uint8_t vel[6] = {127, 70, 90, 50, 110, 64};
    for (uint8_t i = 0; i < 6; ++i)
        call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 4), 2, static_cast<uint8_t>(60 + i), vel[i]};
    MotifDescriptor motif;
    assert(describe(call, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    for (ReplyStrategy strategy : {ReplyStrategy::ShortReply, ReplyStrategy::Develop}) {
        PhraseCapture::Phrase transformed;
        assert(transformCall(call, strategy, transformed));
        ResponsePlan plan;
        assert(planHarmonicEcho(transformed, motif, key, ResponseHarmony{}, ResponseStyle{}, plan));
        // Same note order: note i of the answer is note i of the CALL.
        for (uint8_t i = 0; i < plan.count; ++i) assert(plan.notes[i].velocity == vel[i]);
    }
}

void testEvolveZeroUnchanged() {
    // LOOP EVOLVE 0: exact pitches, steps and velocities (accents included),
    // whatever harmony and cadence are passed.
    PhraseCapture::Phrase call;
    call.windowSteps = 32;
    call.count = 5;
    const uint8_t vel[5] = {127, 40, 80, 1, 100};
    for (uint8_t i = 0; i < 5; ++i)
        call.notes[i] = CapturedNote{static_cast<uint8_t>(i * 5), 2, static_cast<uint8_t>(48 + i * 3), vel[i]};
    MotifDescriptor motif;
    assert(describe(call, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    for (AnswerCadence cadence : {AnswerCadence::ByCall, AnswerCadence::Half, AnswerCadence::Full}) {
        ResponsePlan plan;
        ReplyStrategy actual{};
        ResponseHarmonyReport report;
        assert(planPerformanceReply(call, motif, key, progression(kC_V, kC_V, kC_V, kC_V),
                                    ResponseStyle{}, ReplyMemory{}, true, false, 0, plan, actual,
                                    report, cadence));
        assert(plan.count == call.count);
        for (uint8_t i = 0; i < plan.count; ++i) {
            assert(plan.notes[i].note == call.notes[i].note);
            assert(plan.notes[i].step == call.notes[i].onset);
            assert(plan.notes[i].velocity == call.notes[i].velocity);
        }
    }
}

// ---- 2. ending: chord and place in the form ------------------------------------

struct FinalCase {
    const char* name;
    ResponseKey key;
    uint16_t chords[4];
    uint16_t dominant;   // the V (or v) chord of the progression
};

// The expected ending pitch class over a chord (independent restatement).
int expectedEndingPc(const ResponseKey& key, uint16_t chord, AnswerCadence cadence, int callLast) {
    const int root = key.rootPitchClass;
    const uint16_t allowed = static_cast<uint16_t>(chord & absoluteKeyPcs(key));
    const int target = cadence == AnswerCadence::Full ? nearestOfPitchClass(callLast, root, 0)
                                                      : nearestOfPitchClass(callLast, root, 7);
    if (pcIn(allowed, target)) return pc(target);
    uint16_t set = allowed;
    if (cadence == AnswerCadence::Half && (allowed & ~(1u << root))) set = allowed & ~(1u << root);
    for (int d = 1; d < 12; ++d) {  // nearest, tie goes down
        if (pcIn(set, target - d)) return pc(target - d);
        if (pcIn(set, target + d)) return pc(target + d);
    }
    return -1;
}

void testFinalOverProgressions() {
    const FinalCase cases[] = {
        {"I-IV-V-I (C major)", responseKeyFromIntervals(0, kMajor, 7), {kC_I, kC_IV, kC_V, kC_I}, kC_V},
        {"i-VI-VII-v (A minor)", responseKeyFromIntervals(9, kMinor, 7), {kA_i, kA_VI, kA_VII, kA_v}, kA_v},
    };
    for (const FinalCase& c : cases) {
        const ResponseHarmony harmony = progression(c.chords[0], c.chords[1], c.chords[2], c.chords[3]);
        uint32_t overDominant[3]{}, tonicOverDominant[3]{}, total = 0;
        Lcg rng{static_cast<uint32_t>(c.key.rootPitchClass + 3)};
        for (int i = 0; i < 1500; ++i) {
            const auto call = randomCall(rng, static_cast<uint8_t>(30 + rng.next() % 30));
            MotifDescriptor motif;
            const int8_t* scale = c.key.rootPitchClass == 0 ? kMajor : kMinor;
            assert(describe(call, c.key.rootPitchClass, scale, 7, motif));
            for (uint8_t g = 0; g <= kGenreCount; ++g) {
                const ResponseStyle style = pickResponseStyle(g == kGenreCount ? kNoGenre : g,
                                                              responseSeed(call));
                for (uint8_t number = 1; number <= 4; ++number) {
                    const AnswerCadence cadence = answerCadenceFor(static_cast<uint16_t>(number - 1));
                    assert(cadence == ((number & 1) ? AnswerCadence::Half : AnswerCadence::Full));
                    ResponsePlan plan;
                    ResponseHarmonyReport report;
                    assert(planHarmonicEcho(call, motif, c.key, harmony, style, plan, &report, cadence));
                    assert(report.cadence == cadence);
                    for (uint8_t n = 0; n < plan.count; ++n) {
                        assert(inKey(c.key, plan.notes[n].note));
                        assert(plan.notes[n].note >= kAnswerMinNote && plan.notes[n].note <= kAnswerMaxNote);
                    }
                    const ResponseNote& end = plan.notes[plan.count - 1];
                    const uint16_t chord = harmony.chordAtStep(end.step);
                    // The ending is always a chord tone of its step (every chord here has key tones).
                    assert(pcIn(chord, end.note));
                    // Pitch class as specified (independent of the octave).
                    const int expectedPc = expectedEndingPc(c.key, chord, cadence, end.note);
                    assert(pc(end.note) == expectedPc);
                    const uint8_t idx = static_cast<uint8_t>(cadence);
                    ++total;
                    if (chord == c.dominant) {
                        ++overDominant[idx];
                        if (pc(end.note) == c.key.rootPitchClass) ++tonicOverDominant[idx];
                    }
                    if (chord == c.chords[0] && cadence == AnswerCadence::Full)
                        assert(pc(end.note) == c.key.rootPitchClass);  // even answer on i/I: tonic
                    const int fifthPc = (c.key.rootPitchClass + 7) % 12;
                    if (cadence == AnswerCadence::Half && pcIn(chord, fifthPc))
                        assert(pc(end.note) == fifthPc);  // odd answer: the fifth when the chord has it
                    // Odd answer never ends on the tonic here (every chord has another key tone).
                    assert(!(cadence == AnswerCadence::Half && pc(end.note) == c.key.rootPitchClass));
                }
            }
        }
        // No tonic over the dominant at all - odd or even (the even answer
        // takes the nearest chord tone: B or D over G in C, G over Em in Am).
        const uint8_t half = static_cast<uint8_t>(AnswerCadence::Half);
        const uint8_t full = static_cast<uint8_t>(AnswerCadence::Full);
        assert(overDominant[half] > 1000 && overDominant[full] > 1000);
        assert(tonicOverDominant[half] == 0 && tonicOverDominant[full] == 0);
        std::printf("%s: %u endings, over the dominant odd=%u even=%u, tonic over dominant 0\n",
                    c.name, total, overDominant[half], overDominant[full]);
    }
}

void testFinalExamples() {
    // C major, CALL C D E F | G . E D; the last note D (onset 20) sounds over V.
    const ResponseKey c = responseKeyFromIntervals(0, kMajor, 7);
    PhraseCapture::Phrase call;
    call.windowSteps = 32;
    const CapturedNote notes[] = {{0, 2, 60, 100}, {4, 2, 62, 100}, {8, 2, 64, 100},
                                  {12, 2, 65, 100}, {16, 2, 67, 100}, {20, 4, 62, 100}};
    call.count = 6;
    for (uint8_t i = 0; i < 6; ++i) call.notes[i] = notes[i];
    MotifDescriptor motif;
    assert(describe(call, 0, kMajor, 7, motif));
    const ResponseHarmony pop = progression(kC_I, kC_IV, kC_V, kC_I);
    ResponsePlan plan;
    ResponseHarmonyReport report;
    // Before phase 6 (ByCall): D -> C, the tonic over G B D.
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::ByCall));
    assert(plan.notes[5].note == 60 && report.endingHasChord && !report.endingChordTone);
    // 1st answer: half cadence on G (the fifth, in V), nearest to D62: G67 (5 up) not G55 (7 down).
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Half));
    assert(plan.notes[5].note == 67 && report.endingChordTone && report.cadence == AnswerCadence::Half);
    // 2nd answer: full - no tonic in V, the nearest chord tone in the key to C60 is B59.
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(plan.notes[5].note == 59 && report.endingChordTone);
    // The same CALL ending on onset 24 (over I): full = C, half = G.
    call.notes[5].onset = 24;
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(plan.notes[5].note == 60);
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Half));
    assert(plan.notes[5].note == 67);
    // Over IV (F A C) the half cadence avoids the tonic: G is not in IV -> F (tie with A goes down).
    call.notes[5].onset = 10;
    call.count = 6;
    call.notes[4].onset = 9; call.notes[3].onset = 8; call.notes[2].onset = 6;
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Half));
    assert(pc(plan.notes[5].note) == 5);
    assert(planHarmonicEcho(call, motif, c, pop, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(pc(plan.notes[5].note) == 0);  // C is in IV

    // A minor over v (E G B): half = E (fifth), full = G (A absent; G and B tie, down).
    const ResponseKey a = responseKeyFromIntervals(9, kMinor, 7);
    const ResponseHarmony minor = progression(kA_i, kA_VI, kA_VII, kA_v);
    PhraseCapture::Phrase m;
    m.windowSteps = 32;
    const CapturedNote mn[] = {{0, 2, 57, 100}, {8, 2, 60, 100}, {16, 2, 64, 100}, {26, 2, 59, 100}};
    m.count = 4;
    for (uint8_t i = 0; i < 4; ++i) m.notes[i] = mn[i];
    assert(describe(m, 9, kMinor, 7, motif));
    assert(planHarmonicEcho(m, motif, a, minor, ResponseStyle{}, plan, &report, AnswerCadence::Half));
    assert(pc(plan.notes[3].note) == 4);
    assert(planHarmonicEcho(m, motif, a, minor, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(pc(plan.notes[3].note) == 7);
    assert(planHarmonicEcho(m, motif, a, minor, ResponseStyle{}, plan, &report, AnswerCadence::ByCall));
    assert(pc(plan.notes[3].note) == 9);  // before: the tonic A over E G B
    std::printf("examples: C over V: before C60, odd G67, even B59; Am over v: before A, odd E, even G\n");
}

void testChordOutsideKeyFallsBack() {
    // A chord with no tone in the key (F# A# C# in C major... A# C# F# - none in C major):
    // the ending keeps the phase-1 rule.
    const ResponseKey c = responseKeyFromIntervals(0, kMajor, 7);
    const ResponseHarmony odd = progression(pcs({6, 10, 1}), pcs({6, 10, 1}), pcs({6, 10, 1}), pcs({6, 10, 1}));
    Lcg rng{404};
    for (int i = 0; i < 300; ++i) {
        const auto call = randomCall(rng, 48);
        MotifDescriptor motif;
        assert(describe(call, 0, kMajor, 7, motif));
        ResponsePlan before, after;
        ResponseHarmonyReport report;
        assert(planHarmonicEcho(call, motif, c, odd, ResponseStyle{}, before, nullptr, AnswerCadence::ByCall));
        assert(planHarmonicEcho(call, motif, c, odd, ResponseStyle{}, after, &report, AnswerCadence::Half));
        assert(samePlan(before, after) && report.cadence == AnswerCadence::ByCall);
    }
}

// ---- 3. no chord: the previous result ----------------------------------------

void testNoChordUnchanged() {
    Lcg rng{9};
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    uint32_t plans = 0;
    for (int i = 0; i < 800; ++i) {
        PhraseCapture::Phrase call = randomCall(rng, 36, /*flatVelocity=*/true);
        call.windowSteps = PhraseCapture::kWindowSteps;
        MotifDescriptor motif;
        assert(describe(call, 0, kMajor, 7, motif));
        // ANSWER (planEchoVariant is untouched apart from per-note velocity).
        ResponsePlan echo;
        assert(planEchoVariant(call, motif, key, echo));
        for (uint8_t n = 0; n < echo.count; ++n)
            assert(echo.notes[n].velocity == answerVelocityFor(motif.averageVelocity));  // flat CALL = old flat answer
        for (int mode = 0; mode < 6; ++mode) {
            const bool loop = mode >= 2;
            const bool duet = mode == 1;
            const uint8_t evolve = static_cast<uint8_t>(loop ? mode - 2 : 1);
            const uint8_t genre = static_cast<uint8_t>(i % (kGenreCount + 1));
            const ResponseStyle style = pickResponseStyle(genre == kGenreCount ? kNoGenre : genre,
                                                          responseSeed(call));
            ResponsePlan base;
            ReplyStrategy actual{};
            ResponseHarmonyReport report;
            const bool ok = planPerformanceReply(call, motif, key, ResponseHarmony{}, style, ReplyMemory{},
                                                 loop, duet, evolve, base, actual, report,
                                                 AnswerCadence::ByCall);
            for (AnswerCadence cadence : {AnswerCadence::Half, AnswerCadence::Full}) {
                ResponsePlan plan;
                assert(planPerformanceReply(call, motif, key, ResponseHarmony{}, style, ReplyMemory{},
                                            loop, duet, evolve, plan, actual, report, cadence) == ok);
                if (!ok) continue;
                assert(samePlan(base, plan));
                assert(report.cadence == AnswerCadence::ByCall);
                ++plans;
            }
            if (mode == 0 && style.genre == kNoGenre && ok) assert(samePlan(base, echo) || actual != ReplyStrategy::Echo);
        }
    }
    std::printf("no chord: %u ANSWER/DUET/LOOP plans identical to the phase-1 ending\n", plans);
}

// ---- listener: the answer number ----------------------------------------------

uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }
constexpr uint32_t kSixteenth = 125000;
uint32_t at(double step) { return 1000u + static_cast<uint32_t>(step * kSixteenth + 0.5); }

class NullSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent&) override {}
};

struct Rig {
    MusicalEventRouter router;
    NullSink sink;
    CallResponseListener listener;
    std::vector<AnswerCadence> cadences;
    std::vector<uint16_t> cycles;
    double cursor{0.0};
    explicit Rig(CallResponseListener::Mode mode) {
        assert(router.addSink(sink));
        listener.setNowFunction(fakeNow);
        listener.setOutputRouter(&router);
        listener.setMode(mode);
    }
    void runTo(double step) {
        while (cursor < step) {
            cursor += 0.125;
            gNow = at(cursor);
            PhraseCapture::Phrase phrase;
            if (listener.service(gNow, TransportView{}, 120.0f, phrase) != PhraseCapture::CloseResult::Phrase) continue;
            MotifDescriptor motif;
            assert(describe(phrase, 0, kMajor, 7, motif));
            const AnswerCadence cadence = listener.answerCadence();
            ResponsePlan plan;
            if (listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan)) {
                cadences.push_back(cadence);
                cycles.push_back(listener.loopCycle());
            }
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
};

void testListenerAnswerNumber() {
    assert(answerCadenceFor(0) == AnswerCadence::Half && answerCadenceFor(1) == AnswerCadence::Full);
    assert(answerCadenceFor(255) == AnswerCadence::Full && answerCadenceFor(256) == AnswerCadence::Half);
    {
        // ANSWER: call, answer, call, answer, call, answer -> H F H.
        Rig rig(CallResponseListener::Mode::Answer);
        assert(rig.listener.answerCadence() == AnswerCadence::Half);
        for (int k = 0; k < 3; ++k) {
            const double base = 64.0 * k;
            rig.note(base + 0.0, base + 2.0, 60);
            rig.note(base + 8.0, base + 10.0, 64);
        }
        rig.runTo(64.0 * 3);
        assert(rig.cadences.size() == 3);
        assert(rig.cadences[0] == AnswerCadence::Half && rig.cadences[1] == AnswerCadence::Full &&
               rig.cadences[2] == AnswerCadence::Half);
        // A mode change (new form) starts again from the 1st answer.
        rig.listener.setMode(CallResponseListener::Mode::Duet);
        assert(rig.listener.answerCadence() == AnswerCadence::Half);
    }
    {
        // LOOP: the first reply (cycle 0) is Half, replays alternate F H F.
        Rig rig(CallResponseListener::Mode::Loop);
        rig.note(0.0, 2.0, 60);
        rig.note(8.0, 10.0, 64);
        rig.note(16.0, 18.0, 67);
        rig.runTo(32.0 * 5);
        assert(rig.cadences.size() >= 4);
        for (std::size_t i = 0; i < rig.cadences.size(); ++i) {
            assert(rig.cycles[i] == i);
            assert(rig.cadences[i] == ((i & 1) ? AnswerCadence::Full : AnswerCadence::Half));
        }
        std::printf("listener: ANSWER H F H, LOOP %zu cycles alternate from H\n", rig.cadences.size());
    }
}

}  // namespace

int main() {
    testAccentSurvives();
    testVelocityRangeAllModes();
    testShortReplyAndDevelopKeepTheirNotesVelocity();
    testEvolveZeroUnchanged();
    testFinalOverProgressions();
    testFinalExamples();
    testChordOutsideKeyFallsBack();
    testNoChordUnchanged();
    testListenerAnswerNumber();
    std::puts("cr phase 6 music tests passed");
    return 0;
}
