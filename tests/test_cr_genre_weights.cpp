// CALL/RESPONSE phase 2: genre weights from the idiom dictionary.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"

using namespace GroovePuterCallResponse;
using GenreIdiom::Ending;

namespace {

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint8_t kAcid = 0, kHouse = 9, kHipHop = 11, kUkg = 13, kDnb = 14, kLoFi = 15;

PhraseCapture::Phrase callOf(const CapturedNote* notes, uint8_t count) {
    PhraseCapture::Phrase phrase;
    phrase.windowStartStep = 32;
    phrase.count = count;
    for (uint8_t i = 0; i < count; ++i) phrase.notes[i] = notes[i];
    return phrase;
}

bool admissible(Ending ending) {
    return ending == Ending::Tonic || ending == Ending::RunUp || ending == Ending::Late ||
           ending == Ending::Minimal;
}

void testWeightsDifferByGenre() {
    uint8_t acid[kEndingCount], ukg[kEndingCount], house[kEndingCount], lofi[kEndingCount];
    responseEndingWeights(kAcid, acid);
    responseEndingWeights(kUkg, ukg);
    responseEndingWeights(kHouse, house);
    responseEndingWeights(kLoFi, lofi);
    assert(std::memcmp(acid, ukg, sizeof(acid)) != 0);
    assert(std::memcmp(acid, house, sizeof(acid)) != 0);
    assert(std::memcmp(ukg, lofi, sizeof(ukg)) != 0);
    // The idiom dictionary's lean: Acid keeps its running line (late answers
    // rare, no one-note ending); UKG and LoFi leave space.
    assert(acid[static_cast<uint8_t>(Ending::Late)] == 1);
    assert(acid[static_cast<uint8_t>(Ending::Minimal)] == 0);
    assert(ukg[static_cast<uint8_t>(Ending::Late)] > acid[static_cast<uint8_t>(Ending::Late)]);
    assert(lofi[static_cast<uint8_t>(Ending::Minimal)] > house[static_cast<uint8_t>(Ending::Minimal)]);
    for (uint8_t genre = 0; genre < kGenreCount; ++genre) {
        uint8_t w[kEndingCount];
        responseEndingWeights(genre, w);
        // The CALL decides tonic vs fifth (phase-1 rule): no separate fifth weight.
        assert(w[static_cast<uint8_t>(Ending::Fifth)] == 0);
        assert(w[static_cast<uint8_t>(Ending::Tonic)] > 0);
    }
    std::printf("weights RESOLVE/RUNUP/LATE/MINIMAL acid=%u/%u/%u/%u ukg=%u/%u/%u/%u lofi=%u/%u/%u/%u\n",
                acid[0], acid[2], acid[3], acid[4], ukg[0], ukg[2], ukg[3], ukg[4],
                lofi[0], lofi[2], lofi[3], lofi[4]);
}

void testPickedStylesFollowWeights() {
    for (uint8_t genre : {kAcid, kUkg, kHipHop, kDnb, kLoFi}) {
        uint32_t counts[kEndingCount]{};
        for (uint32_t seed = 0; seed < 4000; ++seed) {
            const ResponseStyle style = pickResponseStyle(genre, seed);
            assert(style.active());
            assert(admissible(style.ending));
            assert(style.ending == responseEndingForIdea(genre, style.idea));
            assert(style.shape == GenreIdiom::interactionShapeForIdea(style.idea));
            // Deterministic.
            const ResponseStyle again = pickResponseStyle(genre, seed);
            assert(again.idea == style.idea && again.ending == style.ending);
            ++counts[static_cast<uint8_t>(style.ending)];
        }
        uint8_t w[kEndingCount];
        responseEndingWeights(genre, w);
        uint32_t total = 0;
        for (uint8_t e = 0; e < kEndingCount; ++e) total += w[e];
        for (uint8_t e = 0; e < kEndingCount; ++e) {
            const double expected = 4000.0 * w[e] / total;
            const double got = counts[e];
            assert(got >= expected * 0.8 - 40 && got <= expected * 1.2 + 40);
        }
    }
}

void testUnknownGenreIsPhase1() {
    for (uint8_t genre : {kNoGenre, static_cast<uint8_t>(16), static_cast<uint8_t>(200)}) {
        uint8_t w[kEndingCount];
        responseEndingWeights(genre, w);
        for (uint8_t e = 0; e < kEndingCount; ++e) assert(w[e] == 0);
        assert(!pickResponseStyle(genre, 1234u).active());
    }
    const CapturedNote notes[] = {{0, 2, 60, 100}, {3, 1, 62, 100}, {8, 2, 64, 100},
                                  {18, 2, 67, 100}, {22, 1, 65, 100}, {26, 4, 62, 100}};
    const auto phrase = callOf(notes, 6);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    ResponsePlan p1, p2;
    assert(planEchoVariant(phrase, motif, key, p1));
    assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{},
                            pickResponseStyle(kNoGenre, responseSeed(phrase)), p2));
    assert(p1.count == p2.count);
    for (uint8_t i = 0; i < p1.count; ++i) {
        assert(p1.notes[i].step == p2.notes[i].step && p1.notes[i].note == p2.notes[i].note &&
               p1.notes[i].length == p2.notes[i].length);
    }
}

// Each genre ending keeps the phase-1 pitch rule and does what it names.
void testEndingShapes() {
    const CapturedNote notes[] = {{0, 2, 60, 100}, {3, 1, 62, 100}, {8, 2, 64, 100},
                                  {18, 2, 67, 100}, {21, 1, 65, 100}, {26, 4, 62, 100}};
    const auto phrase = callOf(notes, 6);
    MotifDescriptor motif;
    assert(describe(phrase, 0, kMajor, 7, motif));
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    ResponseStyle style{};
    style.genre = kUkg;

    style.ending = Ending::Tonic;
    ResponsePlan resolve;
    assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{}, style, resolve));
    assert(resolve.count == 6 && resolve.notes[5].note % 12 == 0 && resolve.notes[5].step == 26);

    style.ending = Ending::Late;
    ResponsePlan late;
    assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{}, style, late));
    assert(late.notes[5].step == 26 + kLateEndingSteps && late.notes[5].note % 12 == 0);
    assert(late.notes[5].step + late.notes[5].length <= late.windowSteps);

    style.ending = Ending::Minimal;
    ResponsePlan minimal;
    assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{}, style, minimal));
    // Bar 2 keeps its strong beats and the ending: 18 and 21 are dropped.
    assert(minimal.count == 4 && minimal.notes[3].step == 26 && minimal.notes[3].note % 12 == 0);
    for (uint8_t i = 0; i + 1 < minimal.count; ++i) {
        assert(minimal.notes[i].step < 16 || minimal.notes[i].step % 4 == 0);
    }

    style.ending = Ending::RunUp;
    ResponsePlan runUp;
    assert(planHarmonicEcho(phrase, motif, key, ResponseHarmony{}, style, runUp));
    // The off-beat note before the ending is the scale tone right under it.
    assert(runUp.notes[5].note % 12 == 0);
    assert(runUp.notes[4].note == runUp.notes[5].note - 1);  // B -> C

    // A CALL that ends on the tonic gets the fifth in every style.
    const CapturedNote toTonic[] = {{0, 2, 64, 90}, {6, 2, 62, 90}, {10, 2, 60, 90}};
    const auto closed = callOf(toTonic, 3);
    assert(describe(closed, 0, kMajor, 7, motif));
    for (Ending e : {Ending::Tonic, Ending::RunUp, Ending::Late, Ending::Minimal}) {
        style.ending = e;
        ResponsePlan plan;
        assert(planHarmonicEcho(closed, motif, key, ResponseHarmony{}, style, plan));
        assert(plan.notes[plan.count - 1].note % 12 == 7);
    }
}

void testAnswerChannelSetting() {
    CallResponseListener listener;
    assert(listener.answerChannel() == CallResponseListener::kAnswerChannelFollowDx);
    listener.setAnswerChannel(5);
    assert(listener.answerChannel() == 5);
    listener.setAnswerChannel(40);
    assert(listener.answerChannel() == 16);
    listener.cycleAnswerChannel(1);
    assert(listener.answerChannel() == 0);  // wraps to "follow DX"
    listener.cycleAnswerChannel(-1);
    assert(listener.answerChannel() == 16);
    listener.cycleAnswerChannel(1);
    listener.cycleAnswerChannel(1);
    assert(listener.answerChannel() == 1);
    // The output target stays the PERFORM DX route.
    assert(listener.responder().target() == MusicalEventTarget::Dx);
}

}  // namespace

int main() {
    testWeightsDifferByGenre();
    testPickedStylesFollowWeights();
    testUnknownGenreIsPhase1();
    testEndingShapes();
    testAnswerChannelSetting();
    std::puts("cr genre weights tests passed");
    return 0;
}
