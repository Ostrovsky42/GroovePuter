// CALL/RESPONSE phase 8 (variety):
//  A. the answer key's tonic comes from the snapshot (tonal.tonicPc), not the
//     root of the chord sounding when the CALL closes;
//  B. a phrase written by a genre template (Idiom authority) gives the partner
//     the template's chord at a step (stabs, carried over the bar line);
//  C. the genre colours the ending tone (Plain / Extended / Blue);
//  D. LOOP passes: a 4-pass form, strategy % 3 against cadence % 2, EVOLVE 3
//     weak-beat variation.
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <vector>

#include "src/input/call_response_harmony.h"
#include "src/input/call_response_listener.h"
#include "src/input/call_response_performance_plan.h"
#include "src/input/musical_event_router.h"
#include "src/input/performance_harmony_lookahead.h"
#include "src/input/performance_keyboard.h"

using namespace GroovePuterCallResponse;

namespace {

constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int8_t kMinor[] = {0, 2, 3, 5, 7, 8, 10};

constexpr uint16_t pcs(std::initializer_list<int> list) {
    uint16_t mask = 0;
    for (int pc : list) mask = static_cast<uint16_t>(mask | (1u << (pc % 12)));
    return mask;
}

int pc(int note) { return ((note % 12) + 12) % 12; }

struct Lcg {
    uint32_t state;
    uint32_t next() { state = state * 1664525u + 1013904223u; return state >> 8; }
};

PhraseCapture::Phrase randomCall(Lcg& rng, uint8_t lowNote, uint8_t windowSteps = 32) {
    PhraseCapture::Phrase phrase;
    phrase.windowStartStep = 64;
    phrase.windowSteps = windowSteps;
    const uint8_t want = static_cast<uint8_t>(2 + rng.next() % 9);
    uint64_t used = 0;
    while (__builtin_popcountll(used) < want) used |= uint64_t{1} << (rng.next() % (windowSteps - 1u));
    uint8_t count = 0;
    for (uint8_t onset = 0; onset + 1 < windowSteps; ++onset) {
        if ((used & (uint64_t{1} << onset)) == 0) continue;
        CapturedNote& n = phrase.notes[count++];
        n.onset = onset;
        n.length = static_cast<uint8_t>(1 + rng.next() % 4);
        n.note = static_cast<uint8_t>(lowNote + rng.next() % 24);
        n.velocity = static_cast<uint8_t>(30 + rng.next() % 98);
    }
    phrase.count = count;
    return phrase;
}

bool samePlan(const ResponsePlan& a, const ResponsePlan& b) {
    if (a.count != b.count || a.windowSteps != b.windowSteps) return false;
    for (uint8_t i = 0; i < a.count; ++i) {
        if (a.notes[i].step != b.notes[i].step || a.notes[i].length != b.notes[i].length ||
            a.notes[i].note != b.notes[i].note || a.notes[i].velocity != b.notes[i].velocity)
            return false;
    }
    return true;
}

// ---- A. tonic from the snapshot ------------------------------------------------

class NullSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent&) override {}
};

void testTonicFromSnapshot() {
    // C minor, the CALL closes over iv (Fm7: F Ab C Eb), published by the
    // existing pattern snapshot builder and the PERFORM keyboard.
    GroovePuterMaterial::GeneratedPatternHarmonyOrigin pattern{};
    pattern.address = {0, 5};
    pattern.version = {0x2345u, 0x6789u};
    pattern.sourceId = 102;
    pattern.publicationGeneration = 2;
    pattern.rootPitchClass = 0;
    pattern.scaleTypeValue = GroovePuterRhythm::kScaleMinor;
    pattern.authority = GroovePuterHarmony::HarmonicAuthority::Generic;
    pattern.eventCount = 1;
    pattern.onsets = GroovePuterRhythm::stepBit(0);
    pattern.events[0] = {3, GroovePuterRhythm::ChordQuality::Minor7, 0};
    GroovePuterPerformance::PatternHarmonyPlaybackPosition position{};
    position.activePage = 0;
    position.activePatternGlobal = 5;
    position.activeVersion = pattern.version;
    position.absoluteStep = 4;
    position.publicationGeneration = 2;
    GroovePuterHarmony::HarmonySnapshot snapshot{};
    assert(GroovePuterPerformance::makePatternHarmonySnapshot(pattern, position, snapshot));
    assert(snapshot.base.rootPc == 5 && snapshot.tonal.tonicPc == 0);

    MusicalEventRouter router;
    NullSink sink;
    assert(router.addSink(sink));
    PerformanceKeyboard keyboard(router);
    assert(keyboard.setHarmonySnapshot(snapshot));
    const auto context = keyboard.performanceContextSnapshot();
    assert(context.hasHarmony && context.rootMidi % 12 == 5 && context.tonicPc == 0);

    const ResponseKey before = responseKeyFromPitchClasses(context.rootMidi, context.scalePitchClasses);
    const ResponseKey key = responseKeyFromHarmony(context.tonicPc, context.rootMidi,
                                                   context.scalePitchClasses);
    assert(before.rootPitchClass == 5);           // the bug: F as the tonic
    assert(key.rootPitchClass == 0);              // now: C
    assert(absoluteKeyPcs(key) == absoluteKeyPcs(before));  // same pitch classes
    // Unknown tonic (0xFF): the old source (chord root) as a fallback.
    assert(responseKeyFromHarmony(0xFF, context.rootMidi, context.scalePitchClasses).rootPitchClass == 5);

    // CALL G Ab F | Eb D: ends on D, over Fm7 all the answer long.
    PhraseCapture::Phrase call;
    call.windowSteps = 32;
    const CapturedNote notes[] = {{0, 2, 67, 100}, {4, 2, 68, 100}, {8, 2, 65, 100},
                                  {16, 2, 63, 100}, {24, 4, 62, 100}};
    call.count = 5;
    for (uint8_t i = 0; i < 5; ++i) call.notes[i] = notes[i];
    MotifDescriptor motif;
    assert(describe(call, 0, kMinor, 7, motif));
    ResponseHarmony harmony{};
    for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat)
        harmony.set(beat, context.harmonyPitchClasses, ChordSource::Current, context.rootMidi % 12u);
    ResponsePlan plan;
    ResponseHarmonyReport report;
    assert(planHarmonicEcho(call, motif, key, harmony, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(pc(plan.notes[plan.count - 1].note) == 0);   // full cadence on C
    assert(planHarmonicEcho(call, motif, before, harmony, ResponseStyle{}, plan, &report, AnswerCadence::Full));
    assert(pc(plan.notes[plan.count - 1].note) == 5);   // before: on F (the chord root)
    // Half cadence: the key's fifth (G) is not in Fm7 -> nearest non-tonic chord tone.
    assert(planHarmonicEcho(call, motif, key, harmony, ResponseStyle{}, plan, &report, AnswerCadence::Half));
    assert(pc(plan.notes[plan.count - 1].note) != 0 && pcIn(context.harmonyPitchClasses, plan.notes[plan.count - 1].note));
    std::printf("A tonic: Fm7 in C minor -> key root C (was F); full cadence C (was F)\n");
}

// ---- B. template chords --------------------------------------------------------

const GenreIdiom::IdiomVariant& variantNamed(const GenreIdiom::IdiomVariant& v) { return v; }

uint16_t chordAt(const GenreIdiom::IdiomVariant& v, uint8_t level, uint8_t root, uint8_t scale,
                 uint8_t bar, uint8_t barCount, uint8_t step, uint8_t* rootOut = nullptr) {
    uint16_t out = 0;
    uint8_t r = 0xFF;
    const bool ok = GenreIdiom::idiomChordAt(v, level, root, scale, bar, 0, barCount, step, out, r);
    assert(ok == (out != 0));
    if (rootOut) *rootOut = r;
    return out;
}

void testTemplateChordAtStep() {
    using GroovePuterRhythm::kScaleMinor;
    const auto& house = variantNamed(GenreIdiom::k_house_deep_offbeat);
    // House P1 stabs (C minor): step 3 Cm9, 7 Cm9, 11 Fm9 (root 5).
    const uint16_t cm9 = pcs({0, 3, 7, 10, 2});
    const uint16_t fm9 = pcs({5, 8, 0, 3, 7});
    uint8_t root = 0;
    for (uint8_t step = 3; step < 11; ++step) {
        assert(chordAt(house, 0, 0, kScaleMinor, 0, 4, step, &root) == cm9);
        assert(root == 0);
    }
    for (uint8_t step = 11; step < 16; ++step) {
        assert(chordAt(house, 0, 0, kScaleMinor, 0, 4, step, &root) == fm9);
        assert(root == 5);
    }
    // Before the first stab: the previous bar's last stab (bar 1 <- bar 0,
    // bar 0 <- the phrase's last bar, a single looping bar <- itself).
    for (uint8_t step = 0; step < 3; ++step) {
        assert(chordAt(house, 0, 0, kScaleMinor, 1, 4, step) == fm9);
        assert(chordAt(house, 0, 0, kScaleMinor, 0, 4, step) == fm9);
        assert(chordAt(house, 0, 0, kScaleMinor, GenreIdiom::kNoBarOrdinal, 1, step) == fm9);
    }
    // UKG classic 2-step P1 (C minor): 2 Cm7, 6 Abmaj7, 10 Bb, 14 Gm7.
    const auto& ukg = GenreIdiom::k_ukg_classic_2step;
    assert(chordAt(ukg, 0, 0, kScaleMinor, 0, 4, 2) == pcs({0, 3, 7, 10}));
    assert(chordAt(ukg, 0, 0, kScaleMinor, 0, 4, 6) == pcs({8, 0, 3, 7}));
    assert(chordAt(ukg, 0, 0, kScaleMinor, 0, 4, 12) == pcs({10, 2, 5}));
    assert(chordAt(ukg, 0, 0, kScaleMinor, 0, 4, 15, &root) == pcs({7, 10, 2, 5}) && root == 7);
    assert(chordAt(ukg, 0, 0, kScaleMinor, 2, 4, 0) == pcs({7, 10, 2, 5}));  // carried over the bar line
    // The project key moves the chord degree by degree: A minor = C minor + 9.
    assert(chordAt(ukg, 0, 9, kScaleMinor, 0, 4, 2, &root) == pcs({9, 0, 4, 7}) && root == 9);
    // Stab-less template (Outrun): the bar's triad from the progression {0, 5, 2, 6}.
    const auto& outrun = GenreIdiom::k_outrun_drive;
    assert(chordAt(outrun, 0, 0, kScaleMinor, 0, 4, 0) == pcs({0, 3, 7}));
    // role 1: +5 degrees folded to -2: Ab major (Ab C Eb).
    assert(chordAt(outrun, 0, 0, kScaleMinor, 1, 4, 9) == pcs({8, 0, 3}));

    // Every template x level x key x scale x bar x step: deterministic, the
    // root is a chord tone, chord tones (nearly all) in the key; count the
    // out-of-key ones (chromatic template notes).
    uint32_t chords = 0, outside = 0;
    for (uint8_t v = 0; v < GenreIdiom::kIdiomVariantCount; ++v) {
        const auto* variant = GenreIdiom::idiomVariantAt(v);
        assert(variant != nullptr && GenreIdiom::idiomVariantIndex(variant) == v);
        for (uint8_t level = 0; level < 3; ++level)
            for (uint8_t key = 0; key < 12; ++key)
                for (uint8_t scale : {GroovePuterRhythm::kScaleMinor, GroovePuterRhythm::kScaleMajor,
                                      GroovePuterRhythm::kScaleDorian})
                    for (uint8_t bar = 0; bar < 8; ++bar)
                        for (uint8_t step = 0; step < 16; ++step) {
                            uint8_t r1 = 0, r2 = 0;
                            const uint16_t a = chordAt(*variant, level, key, scale, bar, 8, step, &r1);
                            const uint16_t b = chordAt(*variant, level, key, scale, bar, 8, step, &r2);
                            assert(a == b && r1 == r2 && a != 0);
                            assert(r1 < 12 && (a & (1u << r1)));
                            const auto def = GroovePuterRhythm::scaleDefinitionFor(scale);
                            const ResponseKey k = responseKeyFromIntervals(
                                key, reinterpret_cast<const int8_t*>(def.intervals), def.count);
                            ++chords;
                            if (a & ~absoluteKeyPcs(k)) ++outside;
                        }
    }
    assert(GenreIdiom::idiomVariantAt(GenreIdiom::kNoIdiomVariant) == nullptr);
    for (uint8_t v = 0; v < GenreIdiom::kIdiomVariantCount; ++v)
        for (uint8_t level = 0; level < 3; ++level) {
            uint8_t back = 9;
            const uint8_t packed = GenreIdiom::packIdiomTemplate(v, level);
            assert(packed != GenreIdiom::kNoIdiomTemplate);
            assert(GenreIdiom::idiomTemplateVariant(packed, back) == GenreIdiom::idiomVariantAt(v) && back == level);
        }
    uint8_t unused = 0;
    assert(GenreIdiom::idiomTemplateVariant(GenreIdiom::kNoIdiomTemplate, unused) == nullptr);
    assert(GenreIdiom::packIdiomTemplate(GenreIdiom::kNoIdiomVariant, 0) == GenreIdiom::kNoIdiomTemplate);
    assert(GenreIdiom::idiomVariantIndex(nullptr) == GenreIdiom::kNoIdiomVariant);
    std::printf("B template chords: %u chords, %u with a tone outside the key\n", chords, outside);
}

// The chord we read is the chord the template actually plays: every Synth B
// stab onset written by GenreIdiom::apply() is a tone of idiomChordAt().
void testTemplateChordMatchesGeneratedLead() {
    uint32_t checked = 0, matched = 0;
    const uint8_t modes[] = {0, 1, 2, 3, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (uint8_t mode : modes) {
        for (uint32_t salt = 0; salt < 6; ++salt) {
            for (uint8_t level = 0; level < 3; ++level) {
                for (uint8_t bar = 0; bar < 4; ++bar) {
                    GenreIdiom::Request request{};
                    request.generativeMode = mode;
                    request.recipe = 0;
                    request.rootPitchClass = static_cast<uint8_t>((salt * 5) % 12);
                    request.scale = salt % 2 ? GroovePuterRhythm::kScaleMinor : GroovePuterRhythm::kScaleDorian;
                    request.level = level;
                    request.barOrdinal = bar;
                    request.salt = salt * 977u + 13u;
                    request.press = salt;
                    const auto* variant = GenreIdiom::variantForRequest(request);
                    if (variant == nullptr) continue;
                    const GenreIdiom::IdiomLevel& lv = variant->levels[level];
                    if (lv.stabCount == 0) continue;  // no stabs to compare
                    SynthPattern bass{}, lead{};
                    if (!GenreIdiom::apply(request, bass, lead)) continue;
                    for (uint8_t i = 0; i < lv.stabCount; ++i) {
                        const uint8_t step = lv.stabs[i].step;
                        bool melodyStep = false;
                        for (uint8_t m = 0; m < lv.melodyCount; ++m) melodyStep |= lv.melody[m].step == step;
                        const SynthStep& s = lead.steps[step];
                        if (melodyStep || s.note < 0 || s.slide) continue;
                        // Bar 4 (the answer bar) ends with a melodic gesture from
                        // step 12 (tonic / fifth / run-up), not with a stab.
                        if (bar % 4 == 3 && step >= 12) continue;
                        const uint16_t chord = chordAt(*variant, level, request.rootPitchClass,
                                                       request.scale, bar, 4, step);
                        ++checked;
                        if (pcIn(chord, s.note)) ++matched;
                    }
                }
            }
        }
    }
    assert(checked > 300);
    assert(matched == checked);
    std::printf("B template chord = generated Synth B stab: %u/%u stab onsets\n", matched, checked);
}

// Lookahead: the origin names the template; the chord of a later step.
GroovePuterMaterial::GeneratedSynthAOrigin idiomOrigin(uint8_t variant) {
    GroovePuterMaterial::GeneratedSynthAOrigin origin{};
    origin.common.barCount = 2;
    origin.common.rootPitchClass = 0;
    origin.common.scaleTypeValue = GroovePuterRhythm::kScaleMinor;
    origin.common.harmonicAuthority = GroovePuterHarmony::HarmonicAuthority::Idiom;
    origin.common.idiomTemplate = GenreIdiom::packIdiomTemplate(variant, 0);
    const uint32_t ids[2] = {101, 102};
    const GroovePuterMaterial::MaterialVersionToken versions[2] = {{0x1234u, 0x5678u}, {0x2345u, 0x6789u}};
    for (uint8_t bar = 0; bar < 2; ++bar) {
        auto& entry = origin.bars[bar];
        entry.phraseBarOrdinal = bar;
        entry.material.address = {0, static_cast<uint8_t>(4 + bar)};
        entry.material.id = {ids[bar]};
        entry.originPatternVersion = versions[bar];
    }
    return origin;
}

GroovePuterPerformance::HarmonyPlaybackPosition songPosition(uint8_t ordinal, uint64_t step) {
    GroovePuterPerformance::HarmonyPlaybackPosition position{};
    position.activePage = 0;
    position.playbackSongSlot = 0;
    position.songRow = static_cast<int16_t>(10 + ordinal);
    position.activeSynthAPatternGlobal = static_cast<int16_t>(4 + ordinal);
    position.activeMaterial = ordinal == 0 ? GroovePuterMaterial::MaterialReference{{0, 4}, {101}}
                                           : GroovePuterMaterial::MaterialReference{{0, 5}, {102}};
    position.activeMaterialVersion = ordinal == 0 ? GroovePuterMaterial::MaterialVersionToken{0x1234u, 0x5678u}
                                                  : GroovePuterMaterial::MaterialVersionToken{0x2345u, 0x6789u};
    position.publicationGeneration = 2;
    position.absoluteStep = step;
    return position;
}

void testIdiomLookahead() {
    const uint8_t house = GenreIdiom::idiomVariantIndex(&GenreIdiom::k_house_deep_offbeat);
    auto origin = idiomOrigin(house);
    GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
    recipe.pageIndex = 0;
    recipe.songSlot = 0;
    recipe.songStart = 10;
    recipe.bars = 2;
    recipe.valid = true;
    GroovePuterHarmony::SongHarmonicParticipant participants[2];
    participants[0] = {0, 10, 0, {101}, {0x1234u, 0x5678u}};
    participants[1] = {0, 11, 0, {102}, {0x2345u, 0x6789u}};
    // The Legacy snapshot builder refuses Idiom (that is why the partner had no chord).
    GroovePuterHarmony::HarmonySnapshot snapshot{};
    assert(!GroovePuterPerformance::makeLegacyHarmonySnapshot(origin, recipe, songPosition(0, 160), snapshot));
    for (uint64_t now = 0; now < 16; ++now) {
        for (int32_t offset = 0; offset <= 16 + static_cast<int32_t>(15 - now); ++offset) {
            uint16_t chord = 0;
            uint8_t root = 0xFF;
            assert(GroovePuterPerformance::idiomChordAhead(origin, recipe, participants, 2,
                                                          songPosition(0, 160 + now), offset, 31, chord, root));
            const uint64_t target = now + offset;
            const uint8_t ordinal = static_cast<uint8_t>(target / 16);
            uint8_t expectedRoot = 0;
            assert(chord == chordAt(GenreIdiom::k_house_deep_offbeat, 0, 0, GroovePuterRhythm::kScaleMinor,
                                    ordinal, 2, static_cast<uint8_t>(target % 16), &expectedRoot));
            assert(root == expectedRoot);
        }
    }
    uint16_t chord = 0;
    uint8_t root = 0;
    // Past the phrase, an edited Song row, no participants, another authority,
    // no template recorded: no chord (fallback).
    assert(!GroovePuterPerformance::idiomChordAhead(origin, recipe, participants, 2, songPosition(1, 176), 16, 31, chord, root));
    GroovePuterHarmony::SongHarmonicParticipant edited[2] = {participants[0], participants[1]};
    edited[1].version.low ^= 1u;
    assert(!GroovePuterPerformance::idiomChordAhead(origin, recipe, edited, 2, songPosition(0, 160), 16, 31, chord, root));
    assert(!GroovePuterPerformance::idiomChordAhead(origin, recipe, nullptr, 0, songPosition(0, 160), 16, 31, chord, root));
    assert(!GroovePuterPerformance::idiomChordAhead(origin, recipe, participants, 2, songPosition(0, 160), 16, 10, chord, root));
    auto noTemplate = origin;
    noTemplate.common.idiomTemplate = GenreIdiom::kNoIdiomTemplate;
    assert(!GroovePuterPerformance::idiomChordAhead(noTemplate, recipe, participants, 2, songPosition(0, 160), 4, 31, chord, root));
    auto generic = origin;
    generic.common.harmonicAuthority = GroovePuterHarmony::HarmonicAuthority::Generic;
    assert(!GroovePuterPerformance::idiomChordAhead(generic, recipe, participants, 2, songPosition(0, 160), 4, 31, chord, root));
    auto stale = songPosition(0, 160);
    stale.activeMaterialVersion.low ^= 1u;
    assert(!GroovePuterPerformance::idiomChordAhead(origin, recipe, participants, 2, stale, 4, 31, chord, root));

    // Pattern mode: the single looping bar, any later step.
    GroovePuterMaterial::GeneratedPatternHarmonyOrigin pattern{};
    pattern.address = {0, 5};
    pattern.version = {0x2345u, 0x6789u};
    pattern.sourceId = 102;
    pattern.publicationGeneration = 2;
    pattern.rootPitchClass = 2;  // D minor
    pattern.scaleTypeValue = GroovePuterRhythm::kScaleMinor;
    pattern.authority = GroovePuterHarmony::HarmonicAuthority::Idiom;
    pattern.idiomTemplate = GenreIdiom::packIdiomTemplate(
        GenreIdiom::idiomVariantIndex(&GenreIdiom::k_ukg_classic_2step), 1);
    GroovePuterPerformance::PatternHarmonyPlaybackPosition position{};
    position.activePage = 0;
    position.activePatternGlobal = 5;
    position.activeVersion = pattern.version;
    position.publicationGeneration = 2;
    position.absoluteStep = 37;
    for (int32_t offset = 0; offset <= 48; ++offset) {
        assert(GroovePuterPerformance::idiomPatternChordAhead(pattern, position, offset, chord, root));
        assert(chord == chordAt(GenreIdiom::k_ukg_classic_2step, 1, 2, GroovePuterRhythm::kScaleMinor,
                                GenreIdiom::kNoBarOrdinal, 1, static_cast<uint8_t>((37 + offset) % 16)));
    }
    position.activeVersion.low ^= 1u;
    assert(!GroovePuterPerformance::idiomPatternChordAhead(pattern, position, 4, chord, root));
    std::printf("B lookahead: Idiom Song chords = template chords at +0..+31; refusals; pattern mode\n");
}

// Answers over template chords: in the key, 24..71, strong beats on chord
// tones, deterministic.
void testAnswersOverTemplateChords() {
    Lcg rng{8};
    uint32_t plans = 0, strong = 0, strongCt = 0;
    for (uint8_t v = 0; v < GenreIdiom::kIdiomVariantCount; ++v) {
        const auto& variant = *GenreIdiom::idiomVariantAt(v);
        for (uint8_t key : {uint8_t{0}, uint8_t{7}, uint8_t{9}}) {
            const ResponseKey k = responseKeyFromIntervals(key, kMinor, 7);
            ResponseHarmony harmony{};
            for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat) {
                uint8_t root = 0xFF;
                const uint8_t step = static_cast<uint8_t>(beat * 4);
                const uint16_t chord = chordAt(variant, 0, key, GroovePuterRhythm::kScaleMinor,
                                               static_cast<uint8_t>(step / 16), 4, step % 16, &root);
                harmony.set(beat, chord, ChordSource::Ahead, root);
            }
            for (int i = 0; i < 40; ++i) {
                const auto call = randomCall(rng, static_cast<uint8_t>(30 + rng.next() % 30));
                MotifDescriptor motif;
                assert(describe(call, key, kMinor, 7, motif));
                for (uint8_t g = 0; g <= kGenreCount; ++g) {
                    const ResponseStyle style = pickResponseStyle(g == kGenreCount ? kNoGenre : g, responseSeed(call));
                    for (AnswerCadence cadence : {AnswerCadence::Half, AnswerCadence::Full}) {
                        ResponsePlan plan, again;
                        ResponseHarmonyReport report;
                        assert(planHarmonicEcho(call, motif, k, harmony, style, plan, &report, cadence));
                        assert(planHarmonicEcho(call, motif, k, harmony, style, again, nullptr, cadence));
                        assert(samePlan(plan, again));
                        for (uint8_t n = 0; n < plan.count; ++n) {
                            assert(inKey(k, plan.notes[n].note));
                            assert(plan.notes[n].note >= kAnswerMinNote && plan.notes[n].note <= kAnswerMaxNote);
                        }
                        strong += report.strongNotes;
                        strongCt += report.strongChordTones;
                        ++plans;
                    }
                }
            }
        }
    }
    assert(strongCt * 100u >= strong * 95u);
    std::printf("B answers over template chords: %u plans, strong-beat chord tones %u/%u\n", plans, strongCt, strong);
}

// ---- C. genre ending colour ---------------------------------------------------

int ending(const ResponseKey& key, uint16_t chord, uint8_t root, uint8_t genre, AnswerCadence cadence,
           int callLast = 62) {
    return pc(cadenceEndingNote(key, callLast, chord, cadence, nullptr, root, genre));
}

void testEndingColourTable() {
    using G = uint8_t;
    const G acid = 0, reggae = 5, house = 9, techno = 10, ukg = 13, lofi = 15, outrun = 1;
    assert(endingColorForGenre(acid) == EndingColor::Plain && endingColorForGenre(techno) == EndingColor::Plain);
    assert(endingColorForGenre(house) == EndingColor::Extended && endingColorForGenre(lofi) == EndingColor::Extended &&
           endingColorForGenre(reggae) == EndingColor::Extended);
    assert(endingColorForGenre(ukg) == EndingColor::Blue);
    assert(endingColorForGenre(outrun) == EndingColor::Default && endingColorForGenre(kNoGenre) == EndingColor::Default);

    const ResponseKey cMinor = responseKeyFromIntervals(0, kMinor, 7);
    const ResponseKey cMajor = responseKeyFromIntervals(0, kMajor, 7);
    const uint16_t cm9 = pcs({0, 3, 7, 10, 2});
    const uint16_t cm7 = pcs({0, 3, 7, 10});
    const uint16_t cm = pcs({0, 3, 7});
    const uint16_t gm7 = pcs({7, 10, 2, 5});
    const uint16_t gMaj = pcs({7, 11, 2});
    const uint16_t am7 = pcs({9, 0, 4, 7});
    // House over Cm9: half = a colour tone (Bb, the 7th, nearest G), full = C.
    assert(ending(cMinor, cm9, 0, house, AnswerCadence::Half) == 10);
    assert(ending(cMinor, cm9, 0, house, AnswerCadence::Full) == 0);
    // House over Gm7 (no C): full = root/5th/7th nearest C -> D (G B? no: G Bb D F; D is 2 up).
    assert(ending(cMinor, gm7, 7, house, AnswerCadence::Full) == 2);
    // House over a plain triad: as phase 6 (the fifth).
    assert(ending(cMinor, cm, 0, house, AnswerCadence::Half) == 7);
    // UKG: b3 / b7. Over Cm7 half = Bb (3 up from G) not Eb (4 down); over Cm half = Eb.
    assert(ending(cMinor, cm7, 0, ukg, AnswerCadence::Half) == 10);
    assert(ending(cMinor, cm, 0, ukg, AnswerCadence::Half) == 3);
    assert(ending(cMinor, cm7, 0, ukg, AnswerCadence::Full) == 0);  // the tonic wins the full cadence
    assert(ending(cMinor, gm7, 7, ukg, AnswerCadence::Full) == 10);  // Bb (b3 of Gm), 2 below C
    // Acid / Techno: root or fifth, no extensions.
    assert(ending(cMajor, gMaj, 7, acid, AnswerCadence::Full) == 2);   // D, not the leading tone B
    assert(ending(cMajor, gMaj, 7, kNoGenre, AnswerCadence::Full) == 11);  // phase 6: B
    assert(ending(cMajor, am7, 9, techno, AnswerCadence::Half) == 9);  // G is Am7's 7th -> A
    assert(ending(cMajor, am7, 9, kNoGenre, AnswerCadence::Half) == 7);  // phase 6: G
    // Default genres and unknown roots: phase 6 exactly.
    Lcg rng{5};
    uint32_t same = 0;
    const uint16_t chords[] = {cm9, cm7, cm, gm7, gMaj, am7, pcs({5, 8, 0, 3}), pcs({2, 5, 9, 0})};
    const uint8_t roots[] = {0, 0, 0, 7, 7, 9, 5, 2};
    for (int i = 0; i < 3000; ++i) {
        const int last = static_cast<int>(36 + rng.next() % 36);
        const uint8_t c = static_cast<uint8_t>(rng.next() % 8);
        const ResponseKey& key = (i & 1) ? cMajor : cMinor;
        for (AnswerCadence cadence : {AnswerCadence::Half, AnswerCadence::Full, AnswerCadence::ByCall}) {
            const int base = cadenceEndingNote(key, last, chords[c], cadence);
            for (uint8_t g = 0; g <= kGenreCount; ++g) {
                const uint8_t genre = g == kGenreCount ? kNoGenre : g;
                assert(cadenceEndingNote(key, last, chords[c], cadence, nullptr, 0xFF, genre) == base);
                if (endingColorForGenre(genre) == EndingColor::Default)
                    assert(cadenceEndingNote(key, last, chords[c], cadence, nullptr, roots[c], genre) == base);
                ++same;
            }
        }
    }
    std::printf("C colour table: examples OK; %u endings without root / default genre == phase 6\n", same);
}

// Phase-6 invariants hold for every genre with known roots: the ending is a
// chord tone in the key, the full cadence lands on the tonic whenever the
// chord has it, the half cadence never on the tonic.
void testEndingColourKeepsTheForm() {
    struct Chord { uint16_t pcs; uint8_t root; };
    const ResponseKey cMinor = responseKeyFromIntervals(0, kMinor, 7);
    const Chord progression[4] = {{pcs({0, 3, 7, 10, 2}), 0}, {pcs({5, 8, 0, 3, 7}), 5},
                                  {pcs({8, 0, 3, 7}), 8}, {pcs({7, 10, 2, 5}), 7}};
    ResponseHarmony harmony{};
    for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat)
        harmony.set(beat, progression[(beat / 2) % 4].pcs, ChordSource::Ahead, progression[(beat / 2) % 4].root);
    Lcg rng{31};
    uint32_t endings = 0, coloured = 0;
    for (int i = 0; i < 1500; ++i) {
        const auto call = randomCall(rng, static_cast<uint8_t>(30 + rng.next() % 30));
        MotifDescriptor motif;
        assert(describe(call, 0, kMinor, 7, motif));
        for (uint8_t g = 0; g <= kGenreCount; ++g) {
            const ResponseStyle style = pickResponseStyle(g == kGenreCount ? kNoGenre : g, responseSeed(call));
            for (AnswerCadence cadence : {AnswerCadence::Half, AnswerCadence::Full}) {
                ResponsePlan plan;
                ResponseHarmonyReport report;
                assert(planHarmonicEcho(call, motif, cMinor, harmony, style, plan, &report, cadence));
                const ResponseNote& end = plan.notes[plan.count - 1];
                const uint16_t chord = harmony.chordAtStep(end.step);
                assert(pcIn(chord, end.note) && inKey(cMinor, end.note));
                assert(end.note >= kAnswerMinNote && end.note <= kAnswerMaxNote);
                if (cadence == AnswerCadence::Full && pcIn(chord, 0)) assert(pc(end.note) == 0);
                if (cadence == AnswerCadence::Half) assert(pc(end.note) != 0);
                const uint8_t root = harmony.rootAtStep(end.step);
                const int rel = (pc(end.note) - root + 12) % 12;
                if (endingColorForGenre(style.genre) == EndingColor::Plain && cadence == AnswerCadence::Half)
                    assert(rel == 0 || rel == 7 || rel == 3 || rel == 4);  // never a 7th / 9th
                if (rel == 10 || rel == 11 || rel == 2) ++coloured;
                ++endings;
            }
        }
    }
    assert(coloured > 0);
    std::printf("C form kept: %u endings (17 styles), %u on a 7th/9th\n", endings, coloured);
}

// ---- D. LOOP passes -------------------------------------------------------------

void testLoopPeriods() {
    // Strategy % 3 and cadence % 2 drift apart: DEVELOP meets both cadences.
    bool developHalf = false, developFull = false;
    for (uint16_t cycle = 0; cycle < 6; ++cycle) {
        const ReplyStrategy s = loopStrategyFor(cycle);
        assert(s == loopStrategyFor(static_cast<uint16_t>(cycle + 3)));
        if (s == ReplyStrategy::Develop) {
            if (answerCadenceFor(cycle) == AnswerCadence::Half) developHalf = true;
            else developFull = true;
        }
    }
    assert(developHalf && developFull);
    assert(loopStrategyFor(0) == ReplyStrategy::Echo);  // pass 0 = the loop as played
    for (uint16_t cycle = 0; cycle < 16; ++cycle) {
        assert(loopPassTransposed(cycle) == (cycle % 4 == 2));
        assert(loopPassVaried(cycle) == (cycle % 4 == 1 || cycle % 4 == 2));
    }
}

// The 4-pass form with planPerformanceReply (as GroovePuter.ino calls it).
void testLoopForm() {
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    const uint16_t I = pcs({0, 4, 7}), IV = pcs({5, 9, 0}), V = pcs({7, 11, 2});
    const uint16_t chords[4] = {I, IV, V, I};
    const uint8_t roots[4] = {0, 5, 7, 0};
    ResponseHarmony harmony{};
    for (uint8_t beat = 0; beat < ResponseHarmony::kBeats; ++beat)
        harmony.set(beat, chords[(beat / 2) % 4], ChordSource::Ahead, roots[(beat / 2) % 4]);
    Lcg rng{77};
    uint32_t passes = 0, transposedNotes = 0, variedPitch = 0, variedVelocity = 0;
    for (int i = 0; i < 400; ++i) {
        const auto call = randomCall(rng, static_cast<uint8_t>(36 + rng.next() % 24));
        MotifDescriptor motif;
        assert(describe(call, 0, kMajor, 7, motif));
        const uint8_t genre = static_cast<uint8_t>(i % (kGenreCount + 1));
        const ResponseStyle style = pickResponseStyle(genre == kGenreCount ? kNoGenre : genre, responseSeed(call));
        for (bool withHarmony : {false, true}) {
            const ResponseHarmony& h = withHarmony ? harmony : ResponseHarmony{};
            for (uint8_t evolve = 1; evolve <= 3; ++evolve) {
                ResponsePlan plans[12];
                for (uint16_t cycle = 0; cycle < 12; ++cycle) {
                    ReplyStrategy actual{};
                    ResponseHarmonyReport report;
                    assert(planPerformanceReply(call, motif, key, h, style, ReplyMemory{}, true, false, evolve,
                                                plans[cycle], actual, report, answerCadenceFor(cycle), cycle));
                    const ResponsePlan& plan = plans[cycle];
                    ++passes;
                    // Strategy by the pass; the cadence is still the phase-6 parity.
                    if (evolve >= 2) assert(actual == loopStrategyFor(cycle) || actual == ReplyStrategy::Echo);
                    else assert(actual == ReplyStrategy::Echo);
                    if (withHarmony) {
                        assert(report.cadence == answerCadenceFor(cycle));
                        assert(report.strongChordTones == report.strongNotes);  // also the transposed pass
                        const ResponseNote& end = plan.notes[plan.count - 1];
                        if (report.cadence == AnswerCadence::Half) assert(pc(end.note) != 0);
                        if (report.cadence == AnswerCadence::Full && pcIn(h.chordAtStep(end.step), 0))
                            assert(pc(end.note) == 0);
                    }
                    assert(plan.count == call.count);
                    for (uint8_t n = 0; n < plan.count; ++n) {
                        assert(inKey(key, plan.notes[n].note));
                        assert(plan.notes[n].note >= kAnswerMinNote && plan.notes[n].note <= kAnswerMaxNote);
                        assert(plan.notes[n].velocity >= 1 && plan.notes[n].velocity <= 127);
                    }
                    // Deterministic.
                    ResponsePlan again;
                    assert(planPerformanceReply(call, motif, key, h, style, ReplyMemory{}, true, false, evolve,
                                                again, actual, report, answerCadenceFor(cycle), cycle));
                    assert(samePlan(plans[cycle], again));
                }
                // Pass 2 = pass 0 a scale degree higher (no harmony, EVOLVE 1:
                // same strategy, same cadence; every note but the ending).
                if (!withHarmony && evolve == 1) {
                    // (RunUp re-pitches the note before the ending: skip it.)
                    const uint8_t tail = style.active() && style.ending == GenreIdiom::Ending::RunUp ? 2 : 1;
                    for (uint8_t n = 0; n + tail < plans[0].count; ++n) {
                        const int a = plans[0].notes[n].note, b = plans[2].notes[n].note;
                        const int up = GroovePuterCallResponse::detail::scaleToneAbove(key, a);
                        assert(b == up || b == up - 12 || b == up + 12);
                        ++transposedNotes;
                    }
                    // Pass 1 = pass 3 (both: the loop with the full cadence).
                    assert(samePlan(plans[1], plans[3]));
                    // Period 4 at EVOLVE 1.
                    assert(samePlan(plans[0], plans[4]) && samePlan(plans[2], plans[6]));
                }
                // EVOLVE 2: period 12 (form 4 x strategy 3): pass 0 and 12 equal.
                if (evolve == 2) {
                    ResponsePlan p12;
                    ReplyStrategy actual{};
                    ResponseHarmonyReport report;
                    assert(planPerformanceReply(call, motif, key, h, style, ReplyMemory{}, true, false, 2,
                                                p12, actual, report, answerCadenceFor(12), 12));
                    assert(samePlan(plans[0], p12));
                }
                // EVOLVE 3 variation: pass 1 vs pass 7 differ only by it (same
                // strategy % 3, cadence % 2, form not transposed, 7 not varied).
                if (evolve == 3) {
                    const ResponsePlan& varied = plans[1];
                    ResponsePlan plain;
                    ReplyStrategy actual{};
                    ResponseHarmonyReport report;
                    assert(planPerformanceReply(call, motif, key, h, style, ReplyMemory{}, true, false, 3,
                                                plain, actual, report, answerCadenceFor(7), 7));
                    assert(plain.count == varied.count);
                    for (uint8_t n = 0; n < plain.count; ++n) {
                        const ResponseNote& a = plain.notes[n];
                        const ResponseNote& b = varied.notes[n];
                        assert(a.step == b.step && a.length == b.length);
                        const bool fixed = n == 0 || n + 1 == plain.count || (a.step % kStrongBeatSteps) == 0;
                        if (fixed) {
                            assert(a.note == b.note && a.velocity == b.velocity);
                            continue;
                        }
                        const int dn = static_cast<int>(b.note) - a.note;
                        assert(dn >= -2 && dn <= 2);
                        const int dv = static_cast<int>(b.velocity) - a.velocity;
                        assert(dv >= -8 && dv <= 8);
                        if (dn) ++variedPitch;
                        if (dv) ++variedVelocity;
                    }
                }
            }
        }
    }
    assert(transposedNotes > 500 && variedPitch > 50 && variedVelocity > 200);
    std::printf("D form: %u passes; pass 2 a degree up (%u notes); EVOLVE 3 varied %u pitches, %u velocities\n",
                passes, transposedNotes, variedPitch, variedVelocity);
}

// ANSWER and DUET ignore the loop pass.
void testAnswerDuetIgnoreLoopCycle() {
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    Lcg rng{3};
    for (int i = 0; i < 300; ++i) {
        const auto call = randomCall(rng, 48);
        MotifDescriptor motif;
        assert(describe(call, 0, kMajor, 7, motif));
        for (bool duet : {false, true}) {
            ResponsePlan base;
            ReplyStrategy a0{};
            ResponseHarmonyReport report;
            const bool ok = planPerformanceReply(call, motif, key, ResponseHarmony{}, ResponseStyle{}, ReplyMemory{},
                                                 false, duet, 3, base, a0, report, AnswerCadence::Half, 0);
            for (uint16_t cycle = 1; cycle < 12; ++cycle) {
                ResponsePlan plan;
                ReplyStrategy a1{};
                assert(planPerformanceReply(call, motif, key, ResponseHarmony{}, ResponseStyle{}, ReplyMemory{},
                                            false, duet, 3, plan, a1, report, AnswerCadence::Half, cycle) == ok);
                if (ok) assert(samePlan(base, plan) && a0 == a1);
            }
        }
    }
}

// The listener passes its loop cycle: LOOP replay 2 is the loop a degree up.
uint32_t gNow = 0;
uint32_t fakeNow() { return gNow; }
uint32_t at(double step) { return 1000u + static_cast<uint32_t>(step * 125000.0 + 0.5); }

void testListenerLoopForm() {
    MusicalEventRouter router;
    NullSink sink;
    assert(router.addSink(sink));
    CallResponseListener listener;
    listener.setNowFunction(fakeNow);
    listener.setOutputRouter(&router);
    listener.setMode(CallResponseListener::Mode::Loop);
    std::vector<ResponsePlan> plans;
    double cursor = 0.0;
    auto runTo = [&](double step) {
        while (cursor < step) {
            cursor += 0.125;
            gNow = at(cursor);
            PhraseCapture::Phrase phrase;
            if (listener.service(gNow, TransportView{}, 120.0f, phrase) != PhraseCapture::CloseResult::Phrase) continue;
            MotifDescriptor motif;
            assert(describe(phrase, 0, kMajor, 7, motif));
            ResponsePlan plan;
            if (listener.answerPhrase(phrase, motif, responseKeyFromIntervals(0, kMajor, 7), gNow, plan))
                plans.push_back(plan);
        }
    };
    auto note = [&](double on, double off, uint8_t pitch) {
        runTo(on);
        gNow = at(on);
        listener.onPerformanceInputNote(true, 'a', pitch, 100);
        runTo(off);
        gNow = at(off);
        listener.onPerformanceInputNote(false, 'a', pitch, 0);
    };
    note(0.0, 2.0, 60);
    note(8.0, 10.0, 64);
    note(16.0, 18.0, 67);
    runTo(32.0 * 6);
    assert(plans.size() >= 5);
    const ResponseKey key = responseKeyFromIntervals(0, kMajor, 7);
    for (uint8_t n = 0; n + 1 < plans[0].count; ++n)
        assert(pc(plans[2].notes[n].note) == pc(GroovePuterCallResponse::detail::scaleToneAbove(key, plans[0].notes[n].note)));
    assert(samePlan(plans[0], plans[4]));
    std::printf("D listener: LOOP pass 2 a degree up, pass 4 = pass 0 (%zu passes)\n", plans.size());
}

}  // namespace

int main() {
    testTonicFromSnapshot();
    testTemplateChordAtStep();
    testTemplateChordMatchesGeneratedLead();
    testIdiomLookahead();
    testAnswersOverTemplateChords();
    testEndingColourTable();
    testEndingColourKeepsTheForm();
    testLoopPeriods();
    testLoopForm();
    testAnswerDuetIgnoreLoopCycle();
    testListenerLoopForm();
    std::puts("cr phase 8 variety tests passed");
    return 0;
}
