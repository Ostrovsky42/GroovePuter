// LOOPER > MELODY: the pure converter loopToMelody() (src/input/looper_melody_export.h).
// Pitch and length as played, shadow / held / muted-layer notes skipped, cut at the pass
// end, 1/2/4/8 bars, overflow, sorted and validated.
#include <cstdio>
#include <cstdlib>

#include "src/input/looper_melody_export.h"

using namespace GroovePuterLooper;
using PhraseRuntime::RuntimeSynthEventBuffer;

namespace {
int gChecks = 0;
#define CHECK(cond) do { ++gChecks; if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); std::abort(); } } while (0)

// Writes a closed note: onset step, length in steps (noteOff at onset + length).
void put(LoopLayer& layer, uint16_t tag, uint8_t lane, uint8_t note, uint8_t vel, uint8_t onset,
         uint8_t len, uint8_t pass) {
    CHECK(layer.noteOn(tag, lane, note, vel, onset, pass) != LoopLayer::Write::Full);
    if (len) CHECK(layer.noteOff(tag, onset + len, pass));
}

void testBasic() {
    LoopLayer layer;
    layer.reset();
    put(layer, 1, 0, 60, 100, 8, 2, 16);   // later onset written first: output is sorted
    put(layer, 2, 0, 64, 90, 0, 4, 16);
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == 2 && out.lengthTicks == 384);
    CHECK(out.events[0].startTick == 0 && out.events[0].note == 64 && out.events[0].velocity == 90);
    CHECK(out.events[0].durationSubticks == 4u * 24u * 16u);
    CHECK(out.events[1].startTick == 8u * 24u && out.events[1].note == 60);
    CHECK(out.events[1].durationSubticks == 2u * 384u);
    CHECK(out.events[0].probability == 100 && out.events[0].flags == 0);
    CHECK(RuntimePhraseEdit::validate(out));
}

void testSkips() {
    LoopLayer layer;
    layer.reset();
    put(layer, 1, 0, 60, 100, 0, 2, 16);
    put(layer, 2, 0, 62, 100, 4, 0, 16);   // still held: length 0
    put(layer, 3, 1, 67, 100, 8, 2, 16);   // layer 2
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == 2);                 // held skipped, both layers merged
    CHECK(loopToMelody(layer, 16, 1, 0x02, out) == MelodyExportResult::Ok);
    CHECK(out.count == 1 && out.events[0].note == 60);   // layer 2 muted
    CHECK(loopToMelody(layer, 16, 1, 0x01, out) == MelodyExportResult::Ok);
    CHECK(out.count == 1 && out.events[0].note == 67);   // layer 1 muted
    CHECK(loopToMelody(layer, 16, 1, 0x03, out) == MelodyExportResult::Empty);
    CHECK(out.count == 0);

    // A SHADOW note (hidden by the last undo unit) is skipped: a CLEAR hides the layer.
    CHECK(layer.clear(0));
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == 1 && out.events[0].note == 67);
    CHECK(layer.undo());
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok && out.count == 3);  // CLEAR closed the held note at 1 step

    LoopLayer empty;
    empty.reset();
    CHECK(loopToMelody(empty, 16, 1, 0, out) == MelodyExportResult::Empty);
}

void testCutAtPassEnd() {
    LoopLayer layer;
    layer.reset();
    // Saturated length (to the pass end) from step 6 of a 1-bar pass; and a length
    // running past the end is cut to the room left.
    LoopLayer l2;
    l2.reset();
    put(layer, 1, 0, 60, 100, 6, 0, 16);
    CHECK(layer.noteOn(2, 0, 62, 100, 12.0, 16) != LoopLayer::Write::Full);
    layer.closePass(16, false);            // the held notes end at the pass end
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == 2);
    CHECK(out.events[0].startTick == 6u * 24u && out.events[0].durationSubticks == 10u * 384u);
    CHECK(out.events[1].startTick == 12u * 24u && out.events[1].durationSubticks == 4u * 384u);
    for (uint16_t i = 0; i < out.count; ++i) {
        const uint32_t end = out.events[i].startTick * 16u + out.events[i].durationSubticks;
        CHECK(end <= out.lengthTicks * 16u);
    }
    // Saturated 127 steps on an 8-bar loop from onset 100: cut to 28 steps.
    put(l2, 3, 0, 65, 80, 100, 127, 128);
    CHECK(loopToMelody(l2, 128, 8, 0, out) == MelodyExportResult::Ok);
    CHECK(out.events[0].durationSubticks == 28u * 384u);
}

void testBarLengths() {
    const uint8_t bars[] = {1, 2, 4, 8};
    for (uint8_t b : bars) {
        const uint8_t pass = static_cast<uint8_t>(b * 16);
        LoopLayer layer;
        layer.reset();
        put(layer, 1, 0, 48, 100, 0, 1, pass);
        put(layer, 2, 0, 50, 100, static_cast<uint8_t>(pass - 1), 1, pass);
        RuntimeSynthEventBuffer out;
        CHECK(loopToMelody(layer, pass, b, 0, out) == MelodyExportResult::Ok);
        CHECK(out.lengthTicks == b * 384u);
        CHECK(out.count == 2);
        CHECK(out.events[1].startTick == (pass - 1u) * 24u);
        CHECK(RuntimePhraseEdit::validate(out));
    }
    LoopLayer layer;
    layer.reset();
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 3, 0, out) == MelodyExportResult::BadArgs);
    CHECK(loopToMelody(layer, 32, 1, 0, out) == MelodyExportResult::BadArgs);
}

void testSamePitchAndDuplicates() {
    LoopLayer layer;
    layer.reset();
    put(layer, 1, 0, 60, 100, 0, 8, 16);
    put(layer, 2, 0, 60, 100, 4, 2, 16);   // same pitch restarts at 4: first ends there
    put(layer, 3, 1, 60, 70, 4, 6, 16);    // same step + pitch in layer 2: exported once
    put(layer, 4, 1, 64, 70, 2, 8, 16);    // other pitch overlaps: kept (chord)
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 1, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == 3);
    CHECK(out.events[0].note == 60 && out.events[0].durationSubticks == 4u * 384u);
    CHECK(out.events[1].note == 64);
    CHECK(out.events[2].note == 60 && out.events[2].startTick == 4u * 24u);
    CHECK(RuntimePhraseEdit::validate(out));
}

void testOverflow() {
    LoopLayer layer;
    layer.reset();
    for (uint8_t i = 0; i < 6; ++i) put(layer, static_cast<uint16_t>(i + 1), 0, static_cast<uint8_t>(40 + i), 100, i, 1, 16);
    RuntimeSynthEventBuffer out;
    CHECK(loopToMelody(layer, 16, 1, 0, out, 6) == MelodyExportResult::Ok);
    CHECK(loopToMelody(layer, 16, 1, 0, out, 5) == MelodyExportResult::Overflow);
    CHECK(out.count == 0);
    // The pool (64) always fits the Melody (128).
    static_assert(kLoopCapacity <= PhraseRuntime::kMaxSynthEvents, "pool fits the Melody");
    LoopLayer full;
    full.reset();
    for (uint8_t i = 0; i < kLoopCapacity; ++i)
        put(full, static_cast<uint16_t>(i + 1), 0, static_cast<uint8_t>(i), 100, i, 1, 64);
    CHECK(loopToMelody(full, 64, 4, 0, out) == MelodyExportResult::Ok);
    CHECK(out.count == kLoopCapacity);
    CHECK(RuntimePhraseEdit::validate(out));
}
}  // namespace

int main() {
    testBasic();
    testSkips();
    testCutAtPassEnd();
    testBarLengths();
    testSamePitchAndDuplicates();
    testOverflow();
    std::printf("looper melody export: PASS (%d checks)\n", gChecks);
    return 0;
}
