// CALL/RESPONSE phase 1: ECHO_VARIANT answer planner (pure function).
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/input/call_response_motif.h"
#include "src/input/call_response_planner.h"
#include "src/input/call_response_harmony.h"

using namespace GroovePuterCallResponse;

namespace {

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int8_t kMinor[] = {0, 2, 3, 5, 7, 8, 10};
constexpr int8_t kLocrian[] = {0, 1, 3, 5, 6, 8, 10};

PhraseCapture::Phrase phraseOf(const CapturedNote* notes, uint8_t count) {
    PhraseCapture::Phrase phrase;
    phrase.windowStartStep = 32;
    phrase.count = count;
    for (uint8_t i = 0; i < count; ++i) phrase.notes[i] = notes[i];
    return phrase;
}

uint32_t callMask(const PhraseCapture::Phrase& phrase) {
    uint32_t mask = 0;
    for (uint8_t i = 0; i < phrase.count; ++i) mask |= 1u << phrase.notes[i].onset;
    return mask;
}

void checkInvariants(const PhraseCapture::Phrase& phrase, const ResponseKey& key,
                     const ResponsePlan& plan, uint8_t callVelocity) {
    assert(plan.count > 0 && plan.count <= ResponsePlan::kCapacity);
    assert(plan.delaySteps == kAnswerDelaySteps);
    assert(plan.windowSteps == PhraseCapture::kWindowSteps - kAnswerDelaySteps);
    // Answer rhythm = CALL rhythm (notes that fit after the delay).
    uint32_t expected = callMask(phrase) & ((1u << plan.windowSteps) - 1u);
    assert(plan.onsetMask() == expected);
    for (uint8_t i = 0; i < plan.count; ++i) {
        const ResponseNote& n = plan.notes[i];
        assert(inKey(key, n.note));
        assert(n.note >= kAnswerMinNote && n.note <= kAnswerMaxNote);
        assert(n.length >= 1);
        assert(n.step + n.length <= plan.windowSteps);  // never past the ANSWER window
        if (i + 1 < plan.count) assert(n.step + n.length <= plan.notes[i + 1].step);  // mono line
        assert(n.velocity == answerVelocityFor(callVelocity));
    }
    // The ending differs from the CALL ending.
    const uint8_t callLast = phrase.notes[plan.count - 1].note;
    assert(plan.notes[plan.count - 1].note != callLast);
}

void testEndingToTonic() {
    // C major, CALL C D E . G ... ends on D (degree 2): the answer ends on C.
    const CapturedNote notes[] = {{0, 2, 60, 100}, {4, 2, 62, 100}, {8, 2, 64, 100},
                                  {12, 4, 67, 100}, {16, 2, 65, 100}, {24, 8, 62, 100}};
    const PhraseCapture::Phrase phrase = phraseOf(notes, 6);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, key, plan));
    checkInvariants(phrase, key, plan, 100);
    assert(plan.count == 6);
    for (uint8_t i = 0; i + 1 < plan.count; ++i) assert(plan.notes[i].note == notes[i].note);
    assert(plan.notes[5].note == 60);           // D -> nearest C (tie goes down)
    assert(plan.notes[5].length == 7);          // 24 + 8 clamped to 31
    assert(plan.notes[0].velocity == 100);
}

void testTonicEndingGoesToFifth() {
    const CapturedNote notes[] = {{0, 2, 64, 90}, {6, 2, 62, 90}, {10, 2, 60, 90}};
    const PhraseCapture::Phrase phrase = phraseOf(notes, 3);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, key, plan));
    checkInvariants(phrase, key, plan, 90);
    assert(plan.notes[2].note == 55 || plan.notes[2].note == 67);  // a G
    assert(plan.notes[2].note == 55);  // C60 -> G55 is 5 below, G67 is 7 above
}

void testChromaticSnappedAndRange() {
    // A minor (root 9), high CALL with chromatic notes: everything lands in the key
    // and in 24..71; the line keeps its shape (moved by octaves).
    const CapturedNote notes[] = {{0, 1, 81, 120}, {2, 1, 82, 110}, {3, 3, 88, 100},
                                  {7, 1, 85, 100}, {20, 4, 84, 100}};
    const PhraseCapture::Phrase phrase = phraseOf(notes, 5);
    MotifDescriptor motif;
    assert(describe(phrase, 9, kMinor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(9, kMinor, 7);
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, key, plan));
    checkInvariants(phrase, key, plan, motif.averageVelocity);
    assert(plan.notes[0].note == 81 - 12 || plan.notes[0].note == 81 - 24);
    assert(plan.notes[1].note == plan.notes[0].note);  // A# snapped down to A
    assert(plan.notes[2].length == 3);
    assert(plan.notes[4].note % 12 == 9);  // C ending -> tonic A

    // Very low CALL is raised into range.
    const CapturedNote low[] = {{0, 2, 14, 100}, {8, 2, 16, 100}};
    const PhraseCapture::Phrase lowPhrase = phraseOf(low, 2);
    assert(describe(lowPhrase, 9, kMinor, 7, motif));
    assert(planEchoVariant(lowPhrase, motif, key, plan));
    checkInvariants(lowPhrase, key, plan, 100);
}

void testLastStepDroppedAfterDelay() {
    // A note on the last sixteenth (31) cannot be answered after a 1-step delay.
    const CapturedNote notes[] = {{0, 4, 62, 100}, {8, 4, 64, 100}, {31, 1, 65, 100}};
    const PhraseCapture::Phrase phrase = phraseOf(notes, 3);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, key, plan));
    assert(plan.count == 2 && plan.droppedTail == 1);
    assert(plan.notes[1].note == 60);  // E -> C ending on the last kept note
    assert(plan.notes[1].step + plan.notes[1].length <= plan.windowSteps);

    const CapturedNote only[] = {{31, 1, 65, 100}};
    const PhraseCapture::Phrase onlyLast = phraseOf(only, 1);
    assert(describe(onlyLast, 0, kMajor, 7, motif));
    // Phase 3 (bug "one note, one answer"): phase 1 returned no answer here.
    // A one-note CALL on the last step is answered on the last usable step.
    assert(planEchoVariant(onlyLast, motif, key, plan));
    assert(plan.count == 1 && plan.droppedTail == 0);
    assert(plan.notes[0].step == 30 && plan.notes[0].length == 1);
    assert(plan.notes[0].note == 60);  // F (not the tonic) is answered with C
}

void testSixteenNotesAndScaleWithoutFifth() {
    CapturedNote notes[16];
    for (uint8_t i = 0; i < 16; ++i) {
        notes[i] = CapturedNote{static_cast<uint8_t>(i * 2), 3, static_cast<uint8_t>(48 + (i % 5) * 2), 64};
    }
    PhraseCapture::Phrase phrase = phraseOf(notes, 16);
    phrase.notes[15].note = 48;  // tonic ending in Locrian (no perfect fifth)
    MotifDescriptor motif;
    assert(describe(phrase, 0, kLocrian, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kLocrian, 7);
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, key, plan));
    checkInvariants(phrase, key, plan, 64);
    assert(plan.count == 16);
    assert(plan.notes[0].length == 2);                  // trimmed to the next onset
    assert((plan.notes[15].note % 12) == 6);            // nearest tone to the fifth: b5
}

void testKeyFromContextPitchClasses() {
    // D dorian as absolute pitch classes (PerformanceContext::scalePitchClasses).
    const uint16_t abs = (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9) |
                         (1u << 11) | (1u << 0);
    const ResponseKey key = responseKeyFromPitchClasses(62, abs);
    assert(key.rootPitchClass == 2);
    assert(inKey(key, 62) && inKey(key, 60) && !inKey(key, 61));
    assert(snapDownToKey(key, 61) == 60);
}

void testLogFormat() {
    const CapturedNote notes[] = {{0, 2, 60, 100}, {4, 2, 62, 100}};
    const PhraseCapture::Phrase phrase = phraseOf(notes, 2);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    ResponsePlan plan;
    assert(planEchoVariant(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), plan));
    char line[288];
    formatResponseLog(line, sizeof(line), 5, plan);
    std::puts(line);
    assert(std::strcmp(line, "[CR-A] bar=5 at=+1 n=2 cut=0 notes=0:60:100:2,4:60:100:2") == 0);

    // 16 notes fit the glue buffer.
    CapturedNote many[16];
    for (uint8_t i = 0; i < 16; ++i) many[i] = CapturedNote{static_cast<uint8_t>(i), 1, 71, 127};
    const PhraseCapture::Phrase full = phraseOf(many, 16);
    assert(describe(full, 0, kMajor, 7, motif));
    assert(planEchoVariant(full, motif, responseKeyFromIntervals(0, kMajor, 7), plan));
    const int length = formatResponseLog(line, sizeof(line), 999, plan);
    assert(length > 0 && length < static_cast<int>(sizeof(line)));
}


// ---- Phase 2: harmony on strong beats, genre style, determinism ------------

struct Lcg {
    uint32_t state;
    uint32_t next() { state = state * 1664525u + 1013904223u; return state >> 8; }
};

// A random CALL: 3..12 notes on distinct onsets, pitches in lowNote..lowNote+23.
PhraseCapture::Phrase randomCall(Lcg& rng, uint8_t lowNote) {
    PhraseCapture::Phrase phrase;
    phrase.windowStartStep = 64;
    const uint8_t want = static_cast<uint8_t>(3 + rng.next() % 10);
    uint32_t used = 0;
    while (__builtin_popcount(used) < want) used |= 1u << (rng.next() % 31u);
    uint8_t count = 0;
    for (uint8_t onset = 0; onset < 31; ++onset) {
        if ((used & (1u << onset)) == 0) continue;
        CapturedNote& n = phrase.notes[count++];
        n.onset = onset;
        n.length = static_cast<uint8_t>(1 + rng.next() % 4);
        n.note = static_cast<uint8_t>(lowNote + rng.next() % 24);
        n.velocity = static_cast<uint8_t>(60 + rng.next() % 60);
    }
    phrase.count = count;
    return phrase;
}

// C major I | IV | V | I, one chord per half bar (2 beats each).
ResponseHarmony popHarmony() {
    ResponseHarmony harmony{};
    const uint16_t chords[4] = {0x091u /*C E G*/, 0x221u /*F A C*/, 0x884u /*G B D*/, 0x091u};
    for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat) {
        harmony.set(beat, chords[beat / 2], beat < 4 ? ChordSource::Current : ChordSource::Ahead);
    }
    return harmony;
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

void testPhase2WithoutHarmonyOrGenreIsPhase1() {
    Lcg rng{11};
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    for (int i = 0; i < 500; ++i) {
        const auto phrase = randomCall(rng, 48);
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        ResponsePlan p1, p2;
        assert(planEchoVariant(phrase, motif, key, p1));
        assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{}, ResponseStyle{}, p2));
        assert(samePlan(p1, p2));
    }
}

void testStrongBeatsOnChordTones() {
    Lcg rng{29};
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    const ResponseHarmony harmony = popHarmony();
    uint32_t strong = 0, chordTones = 0;
    for (int i = 0; i < 2000; ++i) {
        const auto phrase = randomCall(rng, 48);
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        ResponsePlan plan;
        ResponseHarmonyReport report;
        assert(planHarmonicEcho(phrase, motif, key, harmony, ResponseStyle{}, plan, &report));
        strong += report.strongNotes;
        chordTones += report.strongChordTones;
        // Every strong beat except the ending (bound by the phase-1 rule) is a chord tone.
        for (uint8_t n = 0; n + 1 < plan.count; ++n) {
            if (plan.notes[n].step % 4 != 0) continue;
            assert(pcIn(harmony.chordAtStep(plan.notes[n].step), plan.notes[n].note));
        }
        if (report.strongNotes >= 4) {
            assert(report.strongChordTones * 4u >= report.strongNotes * 3u);
        }
    }
    assert(strong > 1000);
    std::printf("phase2 strong-beat chord tones: %u/%u (%.1f%%)\n", chordTones, strong,
                100.0 * chordTones / strong);
    assert(chordTones * 4u >= strong * 3u);  // >= 75 %
}

void testPhase2InvariantsAllGenres() {
    const int8_t* scales[] = {kMajor, kMinor, kLocrian};
    for (uint8_t s = 0; s < 3; ++s) {
        for (uint8_t root = 0; root < 12; root += 5) {
            const ResponseKey key = responseKeyFromIntervals(root, scales[s], 7);
            // A chord with a tone outside the key (secondary dominant shape).
            ResponseHarmony harmony = popHarmony();
            harmony.set(3, 0x0A44u /* D F# A + Bb */, ChordSource::Ahead);
            Lcg rng{static_cast<uint32_t>(97 + s * 13 + root)};
            for (int i = 0; i < 300; ++i) {
                const uint8_t low = static_cast<uint8_t>(20 + rng.next() % 60);
                const auto phrase = randomCall(rng, low);
                MotifDescriptor motif;
                assert(describe(phrase, root, scales[s], 7, motif));
                for (uint8_t genre = 0; genre <= kGenreCount; ++genre) {
                    const uint8_t g = genre == kGenreCount ? kNoGenre : genre;
                    const uint32_t seed = responseSeed(phrase);
                    const ResponseStyle style = pickResponseStyle(g, seed);
                    ResponsePlan plan, again;
                    assert(planHarmonicEcho(phrase, motif, key, harmony, style, plan));
                    // Determinism: same input, same seed -> same answer.
                    assert(planHarmonicEcho(phrase, motif, key, harmony,
                                            pickResponseStyle(g, seed), again));
                    assert(samePlan(plan, again));
                    for (uint8_t n = 0; n < plan.count; ++n) {
                        const ResponseNote& note = plan.notes[n];
                        assert(inKey(key, note.note));
                        assert(note.note >= kAnswerMinNote && note.note <= kAnswerMaxNote);
                        assert(note.length >= 1);
                        assert(note.step + note.length <= plan.windowSteps);
                        if (n + 1 < plan.count) assert(note.step + note.length <= plan.notes[n + 1].step);
                    }
                }
            }
        }
    }
}

// Register: the answer stays where the CALL was. For a CALL inside 36..71 no
// octave shift is needed: every answer note lies within the CALL range widened
// by a half octave, and the ending is within 6 semitones of the CALL's last note.
void testPhase2RegisterNearCall() {
    Lcg rng{5};
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    const ResponseHarmony harmony = popHarmony();
    int maxOut = 0, maxEnding = 0;
    for (int i = 0; i < 1000; ++i) {
        const auto phrase = randomCall(rng, 40);  // 40..63
        MotifDescriptor motif;
        assert(describe(phrase, 0, kMajor, 7, motif));
        for (uint8_t genre = 0; genre < kGenreCount; ++genre) {
            ResponsePlan plan;
            assert(planHarmonicEcho(phrase, motif, key, harmony,
                                    pickResponseStyle(genre, responseSeed(phrase)), plan));
            for (uint8_t n = 0; n < plan.count; ++n) {
                const int below = static_cast<int>(motif.lowNote) - plan.notes[n].note;
                const int above = static_cast<int>(plan.notes[n].note) - motif.highNote;
                if (below > maxOut) maxOut = below;
                if (above > maxOut) maxOut = above;
            }
            // The ending: the nearest tonic/fifth to the CALL's last note.
            const uint8_t lastIndex = static_cast<uint8_t>(phrase.count - 1u);
            const int callLast = phrase.notes[lastIndex].note;
            const int ending = plan.notes[plan.count - 1].note;
            const int distance = ending > callLast ? ending - callLast : callLast - ending;
            if (distance > maxEnding) maxEnding = distance;
        }
    }
    std::printf("phase2 register: max %d semitones outside the CALL range, ending <= %d from the CALL's last note\n",
                maxOut, maxEnding);
    assert(maxOut <= 7);  // RunUp: a scale step under an ending 6 below
    assert(maxEnding <= 6);
}

}  // namespace

int main() {
    testEndingToTonic();
    testTonicEndingGoesToFifth();
    testChromaticSnappedAndRange();
    testLastStepDroppedAfterDelay();
    testSixteenNotesAndScaleWithoutFifth();
    testKeyFromContextPitchClasses();
    testLogFormat();
    testPhase2WithoutHarmonyOrGenreIsPhase1();
    testStrongBeatsOnChordTones();
    testPhase2InvariantsAllGenres();
    testPhase2RegisterNearCall();
    std::puts("cr response planner tests passed");
    return 0;
}
