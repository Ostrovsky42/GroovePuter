// CALL/RESPONSE phase 0: MotifDescriptor (rhythm mask, contour in scale steps).
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/generation/tonal/scale_catalog.h"
#include "src/input/call_response_motif.h"

using GroovePuterCallResponse::CapturedNote;
using GroovePuterCallResponse::MotifDescriptor;
using GroovePuterCallResponse::PhraseCapture;

namespace {

const int8_t* kMajor = GroovePuterRhythm::kScaleIntervalsMajor;
const int8_t* kMinor = GroovePuterRhythm::kScaleIntervalsMinor;
const int8_t* kMinorPenta = GroovePuterRhythm::kScaleIntervalsPentatonicMinor;

PhraseCapture::Phrase phraseOf(const uint8_t* onsets, const uint8_t* notes,
                               const uint8_t* velocities, uint8_t count) {
    PhraseCapture::Phrase phrase;
    for (uint8_t i = 0; i < count; ++i) {
        phrase.notes[i] = CapturedNote{onsets[i], 1, notes[i], velocities[i]};
    }
    phrase.count = count;
    return phrase;
}

void testMaskAndBasics() {
    const uint8_t onsets[] = {0, 4, 10, 16, 31};
    const uint8_t notes[] = {60, 64, 67, 72, 62};
    const uint8_t vel[] = {100, 90, 80, 70, 60};
    MotifDescriptor d;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, notes, vel, 5), 0, kMajor, 7, d));
    assert(d.onsetMask == ((1u << 0) | (1u << 4) | (1u << 10) | (1u << 16) | (1u << 31)));
    assert(d.noteCount == 5 && d.contourCount == 5);
    // C E G C' D in C major: steps 0, +2, +4, +7, +1.
    const int8_t expected[] = {0, 2, 4, 7, 1};
    for (int i = 0; i < 5; ++i) assert(d.contour[i] == expected[i]);
    assert(d.lastDegree == 1);  // D = 2nd degree (0-based 1)
    assert(d.lowNote == 60 && d.highNote == 72);
    assert(d.averageVelocity == 80);
    assert(d.outOfScaleCount == 0);

    MotifDescriptor empty;
    PhraseCapture::Phrase none;
    assert(!GroovePuterCallResponse::describe(none, 0, kMajor, 7, empty));
}

void testContourMajorMinor() {
    // Same keys in C major and A natural minor read as different scale steps only where the scales differ.
    const uint8_t onsets[] = {0, 2, 4, 6};
    const uint8_t cMajorLine[] = {60, 62, 64, 59};  // C D E B(below)
    const uint8_t vel[] = {100, 100, 100, 100};
    MotifDescriptor major;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, cMajorLine, vel, 4), 0, kMajor, 7, major));
    assert(major.contour[1] == 1 && major.contour[2] == 2 && major.contour[3] == -1);
    assert(major.lastDegree == 6);  // B = 7th degree

    // A minor: A C E G -> steps 0, +2, +4, +6; last = G = 7th degree of A minor.
    const uint8_t aMinorLine[] = {57, 60, 64, 67};
    MotifDescriptor minor;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, aMinorLine, vel, 4), 9, kMinor, 7, minor));
    assert(minor.contour[0] == 0 && minor.contour[1] == 2 && minor.contour[2] == 4 && minor.contour[3] == 6);
    assert(minor.lastDegree == 6);
    assert(minor.outOfScaleCount == 0);

    // A chromatic note (C# in C major) is snapped to the scale tone below and counted.
    const uint8_t chromatic[] = {60, 61, 62, 60};
    MotifDescriptor snapped;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, chromatic, vel, 4), 0, kMajor, 7, snapped));
    assert(snapped.contour[1] == 0 && snapped.contour[2] == 1);
    assert(snapped.outOfScaleCount == 1);

    // Pentatonic: five steps per octave.
    const uint8_t penta[] = {57, 60, 62, 69};  // A C D A' in A minor penta
    MotifDescriptor p;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, penta, vel, 4), 9, kMinorPenta, 5, p));
    assert(p.contour[1] == 1 && p.contour[2] == 2 && p.contour[3] == 5);
    assert(p.lastDegree == 0);
}

void testTransposedPhraseSameContour() {
    const uint8_t onsets[] = {0, 3, 6, 8, 12, 14};
    const uint8_t vel[] = {100, 90, 100, 90, 100, 90};
    const uint8_t inC[] = {60, 64, 67, 65, 64, 62};         // C major
    const uint8_t inD[] = {62, 66, 69, 67, 66, 64};         // same line, D major (chromatic transposition)
    const uint8_t diatonic[] = {62, 65, 69, 67, 65, 64};    // shifted one step inside C major
    const uint8_t octaveDown[] = {48, 52, 55, 53, 52, 50};

    MotifDescriptor a, b, c, d;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, inC, vel, 6), 0, kMajor, 7, a));
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, inD, vel, 6), 2, kMajor, 7, b));
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, diatonic, vel, 6), 0, kMajor, 7, c));
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, octaveDown, vel, 6), 0, kMajor, 7, d));
    for (int i = 0; i < 6; ++i) {
        assert(a.contour[i] == b.contour[i]);
        assert(a.contour[i] == c.contour[i]);
        assert(a.contour[i] == d.contour[i]);
    }
    assert(a.onsetMask == b.onsetMask && a.onsetMask == c.onsetMask);
    assert(a.lastDegree == b.lastDegree);       // same tonal function in its own key
    assert(c.lastDegree == a.lastDegree + 1);   // diatonic shift moves the landing degree
    assert(a.lowNote != d.lowNote);             // register is what differs

    // Minor key: same check.
    const uint8_t inAm[] = {57, 60, 64, 62, 60, 59};
    const uint8_t inEm[] = {64, 67, 71, 69, 67, 66};
    MotifDescriptor m1, m2;
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, inAm, vel, 6), 9, kMinor, 7, m1));
    assert(GroovePuterCallResponse::describe(phraseOf(onsets, inEm, vel, 6), 4, kMinor, 7, m2));
    for (int i = 0; i < 6; ++i) assert(m1.contour[i] == m2.contour[i]);
}

void testContourCapAndLog() {
    PhraseCapture::Phrase phrase;
    for (uint8_t i = 0; i < 12; ++i) {
        phrase.notes[i] = CapturedNote{static_cast<uint8_t>(i * 2), 1,
                                       static_cast<uint8_t>(60 + (i % 2) * 2), 100};
    }
    phrase.count = 12;
    MotifDescriptor d;
    assert(GroovePuterCallResponse::describe(phrase, 0, kMajor, 7, d));
    assert(d.noteCount == 12 && d.contourCount == MotifDescriptor::kMaxContour);
    assert(d.lastDegree == 1);  // last of 12 notes, beyond the contour cap

    char line[160];
    const int n = GroovePuterCallResponse::formatMotifLog(line, sizeof(line), 3, d, "FREE", 0);
    assert(n > 0 && n < static_cast<int>(sizeof(line)));
    assert(std::strncmp(line, "[CR] bar=3 mask=0b1010101010101010_1010101000000000 contour=[0,1,0,1,0,1,0,1] last=2 n=12",
                        std::strlen("[CR] bar=3 mask=0b1010101010101010_1010101000000000 contour=[0,1,0,1,0,1,0,1] last=2 n=12")) == 0);
    std::puts(line);
}

void testAnswerVelocity() {
    assert(GroovePuterCallResponse::kAnswerRestProbabilityPercent == 0);
    assert(GroovePuterCallResponse::answerVelocityFor(100) == 100);
    assert(GroovePuterCallResponse::answerVelocityFor(127) == 127);
    assert(GroovePuterCallResponse::answerVelocityFor(1) == 1);
    assert(GroovePuterCallResponse::answerVelocityFor(0) == 1);
}

}  // namespace

int main() {
    testMaskAndBasics();
    testContourMajorMinor();
    testTransposedPhraseSameContour();
    testContourCapAndLog();
    testAnswerVelocity();
    std::puts("cr motif descriptor tests passed");
    return 0;
}
