// LOOPER phase 0: the layer core (docs/design/LOOPER_PLAN.md, "Фаза 0").
// Pure logic: bit packing, quantization and pickup, NoteOff by tag, pass
// close, same onset+pitch (SHADOW / replace), overflow, undo (incl. 5000
// random sessions against a naive copy of the layer), CLEAR + undo, and the
// pass plan (held notes, same-pitch overlap, SHADOW, LENGTH, <= 32 notes).
// BARS 8 / 192 events: onsets 0..127 in 4 bytes, the length saturating at 127
// (= to the pass end), overflow at 192, and a pass of up to 192 notes played as
// consecutive plans of <= 32 (segments, carry across a forced boundary).
// Phase 3: two layers in the shared pool (a write never replaces or hides a
// note of the other layer; CLEAR and the undo mask per layer; MUTE in the
// plan) and resize (BARS with notes: repeat longer, hide past the end
// shorter, one undo unit with the bars before it).
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <vector>

#include "src/input/looper_layer.h"
#include "src/input/performance_input_observer.h"

using namespace GroovePuterLooper;
using Write = LoopLayer::Write;

namespace {

int gChecks = 0;
#define CHECK(cond) do { ++gChecks; if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); std::abort(); } } while (0)
#define CHECK_MSG_PASS(cond, session) do { ++gChecks; if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s (session %d)\n", __FILE__, __LINE__, #cond, session); std::abort(); } } while (0)

using Tuple = std::tuple<int, int, int, int>;  // onset, pitch, length, velocity

std::vector<Tuple> visible(const LoopLayer& layer) {
    std::vector<Tuple> out;
    for (uint8_t i = 0; i < layer.slotCount(); ++i) {
        const LoopNote& n = layer.note(i);
        if (n.shadow()) continue;
        out.emplace_back(n.onset(), n.note(), n.length(), n.velocity());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<Tuple> planTuples(const ResponsePlan& plan) {
    std::vector<Tuple> out;
    for (uint8_t i = 0; i < plan.count; ++i)
        out.emplace_back(plan.notes[i].step, plan.notes[i].note, plan.notes[i].length, plan.notes[i].velocity);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<Tuple> planOf(const LoopLayer& layer, uint8_t steps, AnswerLength len = AnswerLength::Full,
                          uint8_t from = 0) {
    ResponsePlan plan;
    layer.buildPlan(plan, steps, len, from);
    return planTuples(plan);
}

void testBitPacking() {
    static_assert(sizeof(LoopNote) == 4, "4 bytes per event");
    for (int onset = 0; onset < 128; ++onset)
        for (int layer = 0; layer < 4; ++layer)
            for (int pitch = 0; pitch < 128; pitch += 3)
                for (int vel = 1; vel < 128; vel += 7)
                    for (int flags = 0; flags < 4; ++flags) {
                        const bool isNew = flags & 1, shadow = flags & 2;
                        const int length = (onset * 7 + pitch) % 128;  // 0 = held .. 127
                        LoopNote n = LoopNote::make(onset, layer, static_cast<uint8_t>(length),
                                                    pitch, vel, isNew, shadow);
                        CHECK(n.onset() == onset && n.layer() == layer && n.note() == pitch);
                        CHECK(n.length() == length && n.held() == (length == 0));
                        CHECK(n.velocity() == vel && n.isNew() == isNew && n.shadow() == shadow);
                        n.setNew(!isNew);
                        n.setShadow(!shadow);
                        CHECK(n.note() == pitch && n.velocity() == vel && n.length() == length);
                        CHECK(n.isNew() != isNew && n.shadow() != shadow);
                        n.setVelocity(static_cast<uint8_t>(128 - vel));
                        CHECK(n.velocity() == 128 - vel && n.shadow() != shadow);
                        n.setLength(static_cast<uint8_t>(127 - length));
                        CHECK(n.length() == 127 - length && n.layer() == layer && n.onset() == onset);
                        CHECK(n.isNew() != isNew && n.velocity() == 128 - vel);
                    }
    // Out-of-range input never leaks into the flag bits; a length past 126
    // saturates to "to the pass end".
    LoopNote n = LoopNote::make(0xFF, 7, 0xFF, 0xFF, 0xFF, false, false);
    CHECK(n.onset() == 0x7F && n.layer() == 3 && n.length() == LoopNote::kLengthToEnd);
    CHECK(n.note() == 0x7F && n.velocity() == 0x7F && !n.isNew() && !n.shadow());
    n.setLength(128);
    CHECK(n.length() == LoopNote::kLengthToEnd && n.layer() == 3);
}

void testQuantizeAndPickup() {
    for (uint8_t W : {16, 32, 64, 128}) {
        struct Case { double rel; int onset; bool pickup; };
        const Case cases[] = {
            {W - 0.31, W - 1, false}, {W - 0.29, 0, true}, {W - 0.01, 0, true},
            {0.49, 0, false}, {0.51, 1, false}, {-0.2, 0, false}, {W + 0.4, 0, true}, {W + 0.6, 1, true},
        };
        for (const Case& c : cases) {
            LoopLayer layer;
            layer.reset();
            layer.beginUnit();
            CHECK(layer.noteOn(7, 0, 60, 100, c.rel, W) == Write::Stored);
            CHECK(layer.note(0).onset() == c.onset);
            // A pickup is a note of the next pass: a pass that does not
            // record drops it, a recording pass keeps it (still held).
            LoopLayer copy = layer;
            copy.closePass(W, false);
            CHECK(copy.slotCount() == (c.pickup ? 0 : 1));
            if (!c.pickup) CHECK(copy.note(0).length() == std::min(W - c.onset, 127));
            layer.closePass(W, true);
            CHECK(layer.slotCount() == 1);
            CHECK(layer.note(0).held() == c.pickup);
            CHECK(layer.note(0).isNew());
        }
    }
    // Pickup: NoteOff before the downbeat -> one step; after it -> rounded.
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 62, 90, 15.8, 16) == Write::Stored);
    CHECK(layer.noteOff(1, 15.95, 16));
    CHECK(layer.note(0).length() == 1 && layer.note(0).onset() == 0);
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 62, 90, 15.8, 16) == Write::Stored);
    layer.closePass(16, true);
    CHECK(layer.noteOff(1, 2.6, 16));
    CHECK(layer.note(0).length() == 3);
    // A pickup over a note the loop already plays on step 0 is not stored.
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(2, 0, 64, 90, 0.1, 16) == Write::Stored);
    CHECK(layer.noteOff(2, 2.0, 16));
    CHECK(layer.noteOn(3, 0, 64, 90, 15.9, 16) == Write::Duplicate);
    CHECK(layer.slotCount() == 1);
}

void testNoteOffByTag() {
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn('a', 0, 60, 100, 0.0, 16) == Write::Stored);
    CHECK(layer.noteOn('s', 0, 62, 100, 2.0, 16) == Write::Stored);
    // The keyboard's NoteOff carries the interpreted pitch: matched by tag.
    CHECK(layer.noteOff('a', 4.1, 16));   // closes pitch 60 (tag a)
    CHECK(!layer.noteOff('a', 5.0, 16));  // already closed
    CHECK(!layer.noteOff('z', 5.0, 16));  // never opened
    CHECK(layer.note(0).length() == 4 && layer.note(1).held());
    CHECK(layer.noteOff('s', 3.0, 16));
    CHECK(layer.note(1).length() == 1);
    // The same tag attacking again closes its previous note first.
    CHECK(layer.noteOn('d', 0, 64, 100, 6.0, 16) == Write::Stored);
    CHECK(layer.noteOn('d', 0, 65, 100, 9.0, 16) == Write::Stored);
    CHECK(layer.note(2).length() == 3 && layer.note(3).held());
    // External MIDI tags (bit 15) work the same.
    const uint16_t ext = performanceExternalInputTag(1, 2, 70);
    CHECK(layer.noteOn(ext, 0, 70, 100, 10.0, 16) == Write::Stored);
    CHECK(layer.noteOff(ext, 12.0, 16));
    CHECK(layer.note(4).length() == 2);
}

void testPassCloseAndLateNoteOff() {
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 60, 100, 10.0, 16) == Write::Stored);  // held over the end
    CHECK(layer.noteOn(2, 0, 67, 100, 15.8, 16) == Write::Stored);  // pickup
    layer.closePass(16, false);
    CHECK(layer.slotCount() == 1 && layer.note(0).length() == 6);
    CHECK(!layer.noteOff(1, 17.0, 16));  // late NoteOff ignored
    CHECK(!layer.noteOff(2, 17.0, 16));
    CHECK(layer.openCount() == 0);
    // NoteOff past the pass end (close not serviced yet) is clamped to it.
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 60, 100, 12.0, 16) == Write::Stored);
    CHECK(layer.noteOff(1, 19.0, 16));
    CHECK(layer.note(0).length() == 4);
    // closeOpenAt (punch-out): held notes end now, pickups dropped.
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 60, 100, 3.0, 16) == Write::Stored);
    CHECK(layer.noteOn(2, 0, 61, 100, 15.9, 16) == Write::Stored);
    layer.closeOpenAt(7.2, 16);
    CHECK(layer.slotCount() == 1 && layer.note(0).length() == 4 && layer.openCount() == 0);
}

void testSameOnsetSamePitch() {
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 60, 100, 4.0, 16) == Write::Stored);
    CHECK(layer.noteOff(1, 6.0, 16));
    // Same unit: replaced in place (new velocity, new length).
    CHECK(layer.noteOn(2, 0, 60, 50, 4.2, 16) == Write::Replaced);
    CHECK(layer.slotCount() == 1 && layer.note(0).velocity() == 50 && layer.note(0).held());
    CHECK(layer.noteOff(2, 5.0, 16));
    CHECK(layer.note(0).length() == 1);
    // Different pitch on the same onset: both (polyphony).
    CHECK(layer.noteOn(3, 0, 64, 90, 4.0, 16) == Write::Stored);
    CHECK(layer.noteOff(3, 8.0, 16));
    layer.closePass(16, true);
    // Next unit: the older 60@4 becomes SHADOW, the new one is added.
    CHECK(layer.noteOn(4, 0, 60, 120, 3.8, 16) == Write::Stored);
    CHECK(layer.noteOff(4, 6.0, 16));
    CHECK(layer.slotCount() == 3);
    CHECK(layer.note(0).shadow() && !layer.note(0).isNew());
    CHECK(layer.visibleCount() == 2);
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{4, 60, 2, 120}, {4, 64, 4, 90}}));
    CHECK(layer.undo());
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{4, 60, 1, 50}, {4, 64, 4, 90}}));
}

void testOverflow() {
    static_assert(kLoopCapacity == 192, "192 events");
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 192; ++i) {
        const int step = i % 128;
        const uint8_t pitch = static_cast<uint8_t>(40 + i / 128);
        CHECK(layer.noteOn(i, 0, pitch, 100, step, 128) == Write::Stored);
        CHECK(layer.noteOff(i, step + 1.0, 128));
    }
    CHECK(layer.full() && layer.dropped() == 0);
    CHECK(layer.noteOn(999, 0, 100, 100, 5.0, 128) == Write::Full);
    CHECK(layer.dropped() == 1 && layer.slotCount() == 192 && layer.openCount() == 0);
    CHECK(layer.noteOn(1000, 0, 40, 77, 2.0, 128) == Write::Replaced);
    CHECK(layer.noteOff(1000, 3.0, 128));
    layer.closePass(128, true);
    CHECK(layer.noteOn(1001, 0, 40, 77, 4.0, 128) == Write::Full);
    CHECK(layer.visibleCount() == 192 && layer.dropped() == 2);
    CHECK(layer.clear(0));
    CHECK(layer.slotCount() == 192 && layer.visibleCount() == 0);
    CHECK(layer.noteOn(1002, 0, 60, 100, 0.0, 128) == Write::Stored);
    CHECK(layer.slotCount() == 1);
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 191; ++i) {
        CHECK(layer.noteOn(i, 0, static_cast<uint8_t>(40 + i / 128), 90, i % 128, 128) == Write::Stored);
        CHECK(layer.noteOff(i, i % 128 + 1.0, 128));
    }
    CHECK(layer.noteOn(2000, 0, 100, 90, 127.9, 128) == Write::Stored);
    CHECK(layer.slotCount() == 192 && layer.note(191).held() && layer.note(191).onset() == 0);
    layer.closePass(128, true);
    CHECK(layer.noteOff(2000, 3.0, 128));
    CHECK(layer.note(191).length() == 3 && layer.openCount() == 0);

    // An overdub at a high index retains the shadow and restores it on undo.
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 191; ++i) {
        CHECK(layer.noteOn(i, 0, static_cast<uint8_t>(40 + i / 128), 90, i % 128, 128) == Write::Stored);
        CHECK(layer.noteOff(i, i % 128 + 1.0, 128));
    }
    layer.closePass(128, true);
    CHECK(layer.noteOn(3000, 0, 41, 110, 62.0, 128) == Write::Stored);
    CHECK(layer.noteOff(3000, 63.0, 128));
    CHECK(layer.note(190).shadow() && layer.note(191).velocity() == 110);
    CHECK(layer.undo() && !layer.note(190).shadow() && layer.note(190).velocity() == 90);
}

void testUndoBasics() {
    // After RECORD: undo -> empty pool.
    LoopLayer layer;
    layer.reset();
    CHECK(!layer.canUndo() && !layer.undo());
    layer.beginUnit();
    for (int i = 0; i < 4; ++i) {
        CHECK(layer.noteOn(i, 0, 60 + i, 100, i * 4.0, 16) == Write::Stored);
        CHECK(layer.noteOff(i, i * 4.0 + 2.0, 16));
    }
    layer.closePass(16, false);
    CHECK(layer.canUndo());
    CHECK(layer.undo());
    CHECK(layer.slotCount() == 0 && !layer.canUndo());
    CHECK(!layer.undo());  // second undo in a row does nothing

    // A DUB pass with notes: undo -> the plan before that pass, byte for byte.
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 4; ++i) {
        CHECK(layer.noteOn(i, 0, 60 + i, 100, i * 4.0, 16) == Write::Stored);
        CHECK(layer.noteOff(i, i * 4.0 + 2.0, 16));
    }
    layer.closePass(16, true);  // RECORD -> OVERDUB
    const auto before = planOf(layer, 16);
    // Empty DUB passes do not touch undo (it still undoes the RECORD pass).
    layer.closePass(16, true);
    layer.closePass(16, true);
    CHECK(layer.canUndo() && layer.newCount() == 4);
    CHECK(layer.noteOn(10, 0, 72, 100, 1.0, 16) == Write::Stored);  // commit of RECORD here
    CHECK(layer.newCount() == 1);
    CHECK(layer.noteOff(10, 3.0, 16));
    CHECK(layer.noteOn(11, 0, 60, 30, 0.0, 16) == Write::Stored);   // hides 60@0
    CHECK(layer.noteOff(11, 1.0, 16));
    layer.closePass(16, true);
    CHECK(planOf(layer, 16) != before);
    CHECK(layer.undo());
    CHECK(planOf(layer, 16) == before);
    CHECK(!layer.undo());

    // Undo of CLEAR brings the layer back; the next unit commits CLEAR.
    const auto all = planOf(layer, 16);
    CHECK(layer.clear(0));
    CHECK(planOf(layer, 16).empty() && layer.canUndo());
    CHECK(!layer.clear(0));  // nothing visible
    CHECK(layer.undo());
    CHECK(planOf(layer, 16) == all);
    CHECK(layer.clear(0));
    layer.beginUnit();
    CHECK(layer.noteOn(20, 0, 50, 100, 2.0, 16) == Write::Stored);
    CHECK(layer.slotCount() == 1);  // hidden notes deleted by the commit
    CHECK(layer.undo());
    CHECK(layer.slotCount() == 0);
    CHECK(!layer.undo());  // one level: CLEAR is gone

    // dropUndo: no undo, hidden notes gone.
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 60, 100, 0.0, 16) == Write::Stored);
    layer.closePass(16, false);
    CHECK(layer.clear(0));
    layer.dropUndo();
    CHECK(layer.slotCount() == 0 && !layer.canUndo());
}

// Naive copy of the layer: visible notes only, with the unit that wrote them.
struct ModelNote { int onset, pitch, length, velocity, unit; };

std::vector<Tuple> modelTuples(const std::vector<ModelNote>& model) {
    std::vector<Tuple> out;
    for (const auto& n : model) out.emplace_back(n.onset, n.pitch, n.length, n.velocity);
    std::sort(out.begin(), out.end());
    return out;
}

void testRandomSessionsAgainstModel() {
    std::srand(12345);
    for (int session = 0; session < 5000; ++session) {
        const uint8_t W = static_cast<uint8_t>(16 << (std::rand() % 4));
        LoopLayer layer;
        layer.reset();
        std::vector<ModelNote> model, snapshot;
        bool undoAvailable = false;
        uint16_t tag = 1;
        const int units = 1 + std::rand() % 6;
        for (int unit = 0; unit < units; ++unit) {
            const int action = std::rand() % 10;
            if (action == 0 && !model.empty()) {  // CLEAR
                snapshot = model;
                model.clear();
                undoAvailable = true;
                CHECK(layer.clear(0));
            } else if (action <= 2) {  // V
                CHECK(layer.undo() == undoAvailable);
                if (undoAvailable) model = snapshot;
                undoAvailable = false;
            } else {  // a recording pass with 0..5 notes
                layer.beginUnit();
                const int notes = std::rand() % 6;
                bool wrote = false;
                for (int k = 0; k < notes; ++k) {
                    const int onset = std::rand() % (W / 4);  // dense: collisions
                    const int pitch = 60 + std::rand() % 4;
                    const int vel = 1 + std::rand() % 127;
                    const double jitter = ((std::rand() % 41) - 20) / 100.0;
                    const int maxLen = W - onset;
                    const int len = 1 + std::rand() % maxLen;
                    const int stored = std::min(len, 127);  // saturates (to the pass end)
                    if (!wrote) { snapshot = model; wrote = true; undoAvailable = true; }
                    const Write w = layer.noteOn(tag, 0, pitch, vel, onset + jitter, W);
                    CHECK(w == Write::Stored || w == Write::Replaced);
                    CHECK(layer.noteOff(tag, onset + len + jitter / 2.0, W));
                    ++tag;
                    auto it = std::find_if(model.begin(), model.end(), [&](const ModelNote& n) {
                        return n.onset == onset && n.pitch == pitch;
                    });
                    if (it != model.end() && it->unit == unit) {
                        CHECK(w == Write::Replaced);
                        it->velocity = vel;
                        it->length = stored;
                    } else {
                        CHECK(w == Write::Stored);
                        if (it != model.end()) model.erase(it);
                        model.push_back(ModelNote{onset, pitch, stored, vel, unit});
                    }
                }
                layer.closePass(W, false);
            }
            CHECK(visible(layer) == modelTuples(model));
            CHECK(layer.canUndo() == undoAvailable);
            CHECK(layer.slotCount() <= kLoopCapacity);
        }
        // Undo once more against the snapshot (plan byte-identical).
        if (undoAvailable) {
            std::vector<ModelNote> ref = snapshot;
            LoopLayer refLayer;
            refLayer.reset();
            refLayer.beginUnit();
            uint16_t t = 1;
            for (const auto& n : ref) {
                (void)refLayer.noteOn(t, 0, n.pitch, n.velocity, n.onset, W);
                (void)refLayer.noteOff(t, n.onset + n.length, W);
                ++t;
            }
            CHECK(layer.undo());
            CHECK(planOf(layer, W) == planOf(refLayer, W));
        }
    }
}

void testPlan() {
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    // 60@0 len 8, 60@4 len 2 -> the first is cut at 4.
    CHECK(layer.noteOn(1, 0, 60, 100, 0.0, 16) == Write::Stored);
    CHECK(layer.noteOff(1, 8.0, 16));
    CHECK(layer.noteOn(2, 0, 60, 90, 4.0, 16) == Write::Stored);
    CHECK(layer.noteOff(2, 6.0, 16));
    CHECK(layer.noteOn(3, 0, 64, 80, 0.0, 16) == Write::Stored);  // held: not played
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{0, 60, 4, 100}, {4, 60, 2, 90}}));
    CHECK(layer.noteOff(3, 16.0, 16));
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{0, 60, 4, 100}, {0, 64, 16, 80}, {4, 60, 2, 90}}));
    // LENGTH scales the plan only, never the pool.
    CHECK(planOf(layer, 16, AnswerLength::Half) == (std::vector<Tuple>{{0, 60, 2, 100}, {0, 64, 8, 80}, {4, 60, 1, 90}}));
    CHECK(planOf(layer, 16, AnswerLength::Quarter) == (std::vector<Tuple>{{0, 60, 1, 100}, {0, 64, 4, 80}, {4, 60, 1, 90}}));
    CHECK(planOf(layer, 16, AnswerLength::ThreeQuarters) == (std::vector<Tuple>{{0, 60, 3, 100}, {0, 64, 12, 80}, {4, 60, 2, 90}}));
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{0, 60, 4, 100}, {0, 64, 16, 80}, {4, 60, 2, 90}}));
    // Partial plan (resume mid-pass): onsets >= from, shifted.
    CHECK(planOf(layer, 16, AnswerLength::Full, 2) == (std::vector<Tuple>{{2, 60, 2, 90}}));
    // SHADOW notes do not sound.
    layer.closePass(16, true);
    CHECK(layer.noteOn(4, 0, 64, 10, 0.0, 16) == Write::Stored);
    CHECK(layer.noteOff(4, 1.0, 16));
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{0, 60, 4, 100}, {0, 64, 1, 10}, {4, 60, 2, 90}}));
    // Mute mask (phase 3 layers): layer 0 muted -> nothing.
    ResponsePlan plan;
    CHECK(layer.buildPlan(plan, 16, AnswerLength::Full, 0, 0x01u) == 0);
    // Every step + length stays inside the pass.
    CHECK(layer.buildPlan(plan, 16, AnswerLength::Full) == 3);
    for (uint8_t i = 0; i < plan.count; ++i) CHECK(plan.notes[i].step + plan.notes[i].length <= 16);
    CHECK(plan.windowSteps == 16);
}

// Plays a whole pass as the engine does: plan from 0, then each next plan
// from *nextStep with carry. Returns the notes in pass steps (absolute),
// counts the plans; checks every plan's capacity and window.
struct PassNote { int start, end, pitch, velocity; bool operator<(const PassNote& o) const {
    return std::tie(start, pitch, end, velocity) < std::tie(o.start, o.pitch, o.end, o.velocity); }
    bool operator==(const PassNote& o) const {
    return std::tie(start, pitch, end, velocity) == std::tie(o.start, o.pitch, o.end, o.velocity); } };

std::vector<PassNote> playPass(const LoopLayer& layer, uint8_t W, AnswerLength len, int& plans,
                               std::vector<int>* bounds = nullptr) {
    std::vector<PassNote> out;
    plans = 0;
    uint8_t from = 0;
    bool carry = false;
    while (true) {
        ResponsePlan plan;
        uint8_t next = 0;
        layer.buildPlan(plan, W, len, from, 0, carry, &next);
        ++plans;
        CHECK(plan.count <= ResponsePlan::kCapacity);
        const int end = next ? next : W;
        CHECK(plan.windowSteps == end - from);
        for (uint8_t i = 0; i < plan.count; ++i) {
            const ResponseNote& n = plan.notes[i];
            CHECK(n.length >= 1 && n.step + n.length <= plan.windowSteps);
            out.push_back(PassNote{from + n.step, from + n.step + n.length, n.note, n.velocity});
        }
        if (next == 0) break;
        CHECK(next > from);
        if (bounds) bounds->push_back(next);
        from = next;
        carry = true;
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Joins the pieces of a note cut at a plan boundary (same pitch, end == start).
std::vector<PassNote> joined(std::vector<PassNote> notes) {
    std::vector<PassNote> out;
    std::sort(notes.begin(), notes.end(), [](const PassNote& a, const PassNote& b) {
        return std::tie(a.pitch, a.start) < std::tie(b.pitch, b.start); });
    for (const PassNote& n : notes) {
        if (!out.empty() && out.back().pitch == n.pitch && out.back().end == n.start &&
            out.back().velocity == n.velocity) out.back().end = n.end;
        else out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void testSegments() {
    // <= 32 notes: one plan, exactly the old single plan.
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 32; ++i) {
        CHECK(layer.noteOn(static_cast<uint16_t>(i), 0, static_cast<uint8_t>(50 + i % 12), 100, i * 4.0, 128) == Write::Stored);
        CHECK(layer.noteOff(static_cast<uint16_t>(i), i * 4 + 2.0, 128));
    }
    int plans = 0;
    CHECK(playPass(layer, 128, AnswerLength::Full, plans).size() == 32 && plans == 1);
    // A whole-pass note (length 128 saturates to 127 = to the pass end).
    layer.reset();
    layer.beginUnit();
    CHECK(layer.noteOn(1, 0, 40, 100, 0.0, 128) == Write::Stored);
    layer.closePass(128, false);
    CHECK(layer.note(0).length() == LoopNote::kLengthToEnd);
    CHECK(planOf(layer, 128) == (std::vector<Tuple>{{0, 40, 128, 100}}));
    CHECK(planOf(layer, 128, AnswerLength::Half) == (std::vector<Tuple>{{0, 40, 64, 100}}));
    CHECK(planOf(layer, 128, AnswerLength::Full, 1).empty());  // resume: onsets >= from only

    // 64 notes, 2 steps apart, one step long: two plans of 32, split on a
    // step nothing sounds across (64).
    layer.reset();
    layer.beginUnit();
    std::vector<PassNote> expected;
    for (int i = 0; i < 64; ++i) {
        const uint8_t pitch = static_cast<uint8_t>(48 + (i * 5) % 24);
        CHECK(layer.noteOn(static_cast<uint16_t>(i), 0, pitch, 64 + i, i * 2.0, 128) == Write::Stored);
        CHECK(layer.noteOff(static_cast<uint16_t>(i), i * 2 + 1.0, 128));
        expected.push_back(PassNote{i * 2, i * 2 + 1, pitch, 64 + i});
    }
    std::sort(expected.begin(), expected.end());
    std::vector<int> bounds;
    CHECK(playPass(layer, 128, AnswerLength::Full, plans, &bounds) == expected);
    CHECK(plans == 2 && bounds == std::vector<int>{64});

    // The high two thirds of the pool participate in planning and overlap
    // detection just like the first 64 entries.
    layer.reset();
    layer.beginUnit();
    expected.clear();
    for (int i = 0; i < 192; ++i) {
        const int onset = i % 128;
        const uint8_t pitch = static_cast<uint8_t>(40 + i / 128);
        CHECK(layer.noteOn(i, 0, pitch, 100, onset, 128) == Write::Stored);
        CHECK(layer.noteOff(i, onset + 1.0, 128));
        expected.push_back(PassNote{onset, onset + 1, pitch, 100});
    }
    std::sort(expected.begin(), expected.end());
    CHECK(playPass(layer, 128, AnswerLength::Full, plans) == expected);
    CHECK(plans == 6);

    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 130; ++i) {
        CHECK(layer.noteOn(i, 0, static_cast<uint8_t>(40 + i / 128), 90, i % 128, 128) == Write::Stored);
        CHECK(layer.noteOff(i, i % 128 + 1.0, 128));
    }
    CHECK(layer.noteOn(3000, 0, 100, 90, 2.0, 128) == Write::Stored);
    CHECK(layer.noteOff(3000, 10.0, 128));
    CHECK(layer.noteOn(3001, 1, 100, 90, 5.0, 128) == Write::Stored);
    CHECK(layer.noteOff(3001, 8.0, 128));
    const auto overlap = playPass(layer, 128, AnswerLength::Full, plans);
    CHECK(std::count(overlap.begin(), overlap.end(), PassNote{2, 5, 100, 90}) == 1);
    CHECK(std::count(overlap.begin(), overlap.end(), PassNote{5, 8, 100, 90}) == 1);

    // Legato notes (each to the next onset): the boundary still falls on a
    // step no note crosses (an onset step, the previous note ends there).
    layer.reset();
    layer.beginUnit();
    expected.clear();
    for (int i = 0; i < 40; ++i) {
        const uint8_t pitch = static_cast<uint8_t>(60 + i % 7);
        CHECK(layer.noteOn(static_cast<uint16_t>(i), 0, pitch, 90, i * 3.0, 128) == Write::Stored);
        CHECK(layer.noteOff(static_cast<uint16_t>(i), i * 3 + 3.0, 128));
        expected.push_back(PassNote{i * 3, i * 3 + 3, pitch, 90});
    }
    std::sort(expected.begin(), expected.end());
    bounds.clear();
    CHECK(playPass(layer, 128, AnswerLength::Full, plans, &bounds) == expected);
    CHECK(plans == 2 && bounds.size() == 1 && bounds[0] == 96);  // 32 onsets 0..93, next at 96

    // A drone over the whole pass + 63 short notes: no clean step, the drone
    // is cut at the boundary and the next plan carries it on (same pitch,
    // NoteOff at the boundary, NoteOn again).
    layer.reset();
    layer.beginUnit();
    expected.clear();
    CHECK(layer.noteOn(900, 0, 36, 110, 0.0, 128) == Write::Stored);
    for (int i = 0; i < 63; ++i) {
        const uint8_t pitch = static_cast<uint8_t>(60 + i % 12);
        CHECK(layer.noteOn(static_cast<uint16_t>(i), 0, pitch, 80, 1.0 + i * 2.0, 128) == Write::Stored);
        CHECK(layer.noteOff(static_cast<uint16_t>(i), 2.0 + i * 2.0, 128));
        expected.push_back(PassNote{1 + i * 2, 2 + i * 2, pitch, 80});
    }
    CHECK(layer.noteOff(900, 128.0, 128));
    expected.push_back(PassNote{0, 128, 36, 110});
    std::sort(expected.begin(), expected.end());
    bounds.clear();
    const auto pieces = playPass(layer, 128, AnswerLength::Full, plans, &bounds);
    // 0..62: drone + 31 notes; 63..124: drone (carried) + 31; 125: drone + 1.
    CHECK(plans == 3 && bounds == (std::vector<int>{63, 125}));
    CHECK(pieces.size() == 66);  // the drone in three pieces
    CHECK(joined(pieces) == expected);
    // LENGTH 25 %: the drone (32 steps) no longer crosses the boundary.
    bounds.clear();
    const auto quarter = playPass(layer, 128, AnswerLength::Quarter, plans, &bounds);
    CHECK(quarter.size() == 64 && plans == 2);
    for (const PassNote& n : quarter) if (n.pitch == 36) CHECK(n.start == 0 && n.end == 32);

    // Random dense passes: the plans together play exactly the single
    // (unlimited) pass plan, pieces joined; never more than 32 per plan.
    std::srand(777);
    for (int session = 0; session < 3000; ++session) {
        const uint8_t W = static_cast<uint8_t>(16 << (std::rand() % 4));
        layer.reset();
        layer.beginUnit();
        const int notes = 1 + std::rand() % 64;
        std::vector<std::tuple<int, int, int, int>> raw;  // onset, pitch, len, vel
        for (int k = 0; k < notes; ++k) {
            const int onset = std::rand() % W;
            const int pitch = 60 + std::rand() % 6;
            const int len = 1 + std::rand() % (W - onset);
            const int vel = 1 + std::rand() % 127;
            const Write w = layer.noteOn(static_cast<uint16_t>(k + 1), 0, static_cast<uint8_t>(pitch),
                                         static_cast<uint8_t>(vel), onset, W);
            CHECK(w == Write::Stored || w == Write::Replaced || w == Write::Full);
            if (w != Write::Full) CHECK(layer.noteOff(static_cast<uint16_t>(k + 1), onset + len, W));
        }
        const AnswerLength len = static_cast<AnswerLength>(std::rand() % 4);
        // Reference: every visible note, cut at the next onset of its pitch,
        // to the pass end at most, then LENGTH.
        std::vector<PassNote> ref;
        for (uint8_t i = 0; i < layer.slotCount(); ++i) {
            const LoopNote& n = layer.note(i);
            if (n.shadow()) continue;
            int l = n.length() == LoopNote::kLengthToEnd ? W - n.onset() : std::min<int>(n.length(), W - n.onset());
            for (uint8_t j = 0; j < layer.slotCount(); ++j) {
                const LoopNote& o = layer.note(j);
                if (o.shadow() || o.note() != n.note()) continue;
                if (o.onset() > n.onset() && o.onset() < n.onset() + l) l = o.onset() - n.onset();
            }
            l = GroovePuterCallResponse::scaledAnswerNoteLength(static_cast<uint8_t>(l),
                    GroovePuterCallResponse::answerLengthPercent(len));
            ref.push_back(PassNote{n.onset(), n.onset() + l, n.note(), n.velocity()});
        }
        std::sort(ref.begin(), ref.end());
        const auto got = joined(playPass(layer, W, len, plans));
        // Both sides joined: two real notes of one pitch and velocity that
        // touch (the cut) read as one, on both sides alike.
        CHECK_MSG_PASS(got == joined(ref), session);
        if (ref.size() <= 32) CHECK(plans == 1);
    }
}

}  // namespace

// Visible notes of one layer.
std::vector<Tuple> visibleOf(const LoopLayer& layer, uint8_t l) {
    std::vector<Tuple> out;
    for (uint8_t i = 0; i < layer.slotCount(); ++i) {
        const LoopNote& n = layer.note(i);
        if (n.shadow() || n.layer() != l) continue;
        out.emplace_back(n.onset(), n.note(), n.length(), n.velocity());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void recordNote(LoopLayer& layer, uint16_t tag, uint8_t l, uint8_t pitch, double on, double off, uint8_t steps,
                uint8_t vel = 100) {
    CHECK(layer.noteOn(tag, l, pitch, vel, on, steps) != Write::Full);
    CHECK(layer.noteOff(tag, off, steps));
}

void testTwoLayers() {
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    recordNote(layer, 1, 0, 60, 0.0, 2.0, 16);
    recordNote(layer, 2, 0, 64, 4.0, 5.0, 16);
    layer.closePass(16, false);
    const auto layer0 = visibleOf(layer, 0);
    // DUB into layer 1 on the same step and pitch as layer 0: stored, never a
    // SHADOW of layer 0 (isolation), not even inside one unit.
    layer.beginUnit();
    CHECK(layer.noteOn(3, 1, 60, 90, 0.1, 16) == Write::Stored);
    CHECK(layer.noteOff(3, 3.0, 16));
    CHECK(layer.noteOn(4, 1, 64, 90, 3.9, 16) == Write::Stored);
    CHECK(layer.noteOff(4, 6.0, 16));
    CHECK(visibleOf(layer, 0) == layer0);
    CHECK(layer.visibleCount(0) == 2 && layer.visibleCount(1) == 2 && layer.visibleTotal() == 4);
    CHECK(layer.undoLayerMask() == 0x02u);
    // Inside layer 1 the rule stays: same step + pitch in the same unit
    // replaces in place; in the next unit hides the older note.
    CHECK(layer.noteOn(5, 1, 60, 30, 0.2, 16) == Write::Replaced);
    CHECK(layer.noteOff(5, 1.0, 16));
    layer.closePass(16, true);
    CHECK(layer.noteOn(6, 1, 64, 33, 4.0, 16) == Write::Stored);
    CHECK(layer.noteOff(6, 5.0, 16));
    CHECK(visibleOf(layer, 0) == layer0);
    CHECK(visibleOf(layer, 1) == (std::vector<Tuple>{{0, 60, 1, 30}, {4, 64, 1, 33}}));
    CHECK(layer.undoLayerMask() == 0x02u);
    // Undo: layer 1 only.
    CHECK(layer.undo());
    CHECK(visibleOf(layer, 0) == layer0);
    CHECK(visibleOf(layer, 1) == (std::vector<Tuple>{{0, 60, 1, 30}, {4, 64, 2, 90}}));
    // One step + pitch in both layers sounds once (one MIDI note).
    CHECK(planOf(layer, 16).size() == 2);
    // MUTE in the plan: layer 1 muted -> layer 0 as recorded; layer 0 muted
    // -> layer 1 only.
    ResponsePlan plan;
    layer.buildPlan(plan, 16, AnswerLength::Full, 0, 0x02u);
    CHECK(planTuples(plan) == layer0);
    layer.buildPlan(plan, 16, AnswerLength::Full, 0, 0x01u);
    CHECK(planTuples(plan) == (std::vector<Tuple>{{0, 60, 1, 30}, {4, 64, 2, 90}}));
    layer.buildPlan(plan, 16, AnswerLength::Full, 0, 0x03u);
    CHECK(plan.count == 0);
    // CLEAR of layer 1: layer 0 untouched; undo mask = layer 1; undo brings it back.
    const auto layer1 = visibleOf(layer, 1);
    CHECK(layer.clear(1));
    CHECK(visibleOf(layer, 0) == layer0 && layer.visibleCount(1) == 0);
    CHECK(layer.undoLayerMask() == 0x02u);
    CHECK(layer.undo() && visibleOf(layer, 1) == layer1 && visibleOf(layer, 0) == layer0);
    // CLEAR of layer 0 then a write in layer 1 commits the CLEAR: layer 1 intact.
    CHECK(layer.clear(0));
    layer.beginUnit();
    recordNote(layer, 7, 1, 67, 8.0, 9.0, 16);
    CHECK(layer.visibleCount(0) == 0 && layer.visibleCount(1) == 3 && layer.slotCount() == 3);
    std::printf("two layers: isolation, undo mask, MUTE, CLEAR per layer\n");
}

void testResize() {
    // 1 bar -> 2 -> 4 -> 8: the loop repeats; lengths and velocities kept.
    LoopLayer layer;
    layer.reset();
    layer.beginUnit();
    recordNote(layer, 1, 0, 60, 0.0, 2.0, 16, 90);
    recordNote(layer, 2, 0, 64, 6.0, 7.0, 16, 80);
    recordNote(layer, 3, 1, 67, 12.0, 16.0, 16, 70);  // layer 1, to the pass end
    layer.closePass(16, false);
    const auto bar = planOf(layer, 16);
    CHECK(bar.size() == 3);
    auto repeated = [&](uint8_t steps) {
        std::vector<Tuple> out;
        for (int k = 0; k < steps / 16; ++k)
            for (const Tuple& t : bar)
                out.emplace_back(std::get<0>(t) + 16 * k, std::get<1>(t), std::get<2>(t), std::get<3>(t));
        std::sort(out.begin(), out.end());
        return out;
    };
    CHECK(layer.undoBars() == 0);
    CHECK(layer.resizeFits(16, 32) && layer.resize(16, 32, 1));
    CHECK(planOf(layer, 32) == repeated(32));
    CHECK(layer.visibleCount(0) == 4 && layer.visibleCount(1) == 2);  // each layer repeats itself
    CHECK(layer.canUndo() && layer.undoBars() == 1 && layer.undoLayerMask() == 0x0Fu);
    CHECK(layer.resize(32, 64, 2));
    CHECK(planOf(layer, 64) == repeated(64) && layer.undoBars() == 2);
    CHECK(layer.resize(64, 128, 4));
    CHECK(planOf(layer, 128) == repeated(128) && layer.visibleTotal() == 24 && layer.undoBars() == 4);
    // Undo: back to 4 bars of notes (the copies go), bars 4 reported first.
    CHECK(layer.undo());
    CHECK(planOf(layer, 64) == repeated(64) && layer.visibleTotal() == 12 && layer.undoBars() == 0);
    CHECK(!layer.canUndo());
    // Shorter: 4 -> 2 hides bars 3-4; undo shows them again.
    CHECK(layer.resize(64, 32, 4));
    CHECK(layer.visibleTotal() == 6 && planOf(layer, 32) == repeated(32) && layer.undoBars() == 4);
    CHECK(layer.undo() && layer.visibleTotal() == 12 && planOf(layer, 64) == repeated(64));
    // 4 -> 1 then 1 -> 2: the 1-bar loop repeated (the hidden notes are gone
    // at the commit, not doubled).
    CHECK(layer.resize(64, 16, 4) && planOf(layer, 16) == bar);
    CHECK(layer.resize(16, 32, 1) && planOf(layer, 32) == repeated(32) && layer.slotCount() == 6);
    // A note across the new end: cut by the pass end while short, its repeat
    // starts on the next bar (not the old tail).
    layer.reset();
    layer.beginUnit();
    recordNote(layer, 1, 0, 50, 8.0, 24.0, 32);  // 8..24 in a 2-bar pass
    layer.closePass(32, false);
    CHECK(layer.resize(32, 16, 2));
    CHECK(planOf(layer, 16) == (std::vector<Tuple>{{8, 50, 8, 100}}));
    CHECK(layer.resize(16, 32, 1));
    CHECK(planOf(layer, 32) == (std::vector<Tuple>{{8, 50, 8, 100}, {24, 50, 8, 100}}));
    // No room: 100 notes in 4 bars cannot repeat to 8 bars; nothing changes.
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 100; ++i) recordNote(layer, static_cast<uint16_t>(i), 0, static_cast<uint8_t>(40 + i / 64), i % 64, i % 64 + 1.0, 64);
    layer.closePass(64, false);
    const auto before = planOf(layer, 64);
    CHECK(!layer.resizeFits(64, 128) && !layer.resize(64, 128, 4));
    CHECK(planOf(layer, 64) == before && layer.undoBars() == 0);
    CHECK(layer.resizeFits(64, 32) && !layer.resizeFits(64, 48) && !layer.resize(64, 64, 4));
    // 96 notes fit exactly at x2 (192).
    layer.reset();
    layer.beginUnit();
    for (int i = 0; i < 96; ++i) recordNote(layer, static_cast<uint16_t>(i), 0, static_cast<uint8_t>(40 + i / 64), i % 64, i % 64 + 1.0, 64);
    layer.closePass(64, false);
    CHECK(layer.resize(64, 128, 4) && layer.full() && layer.visibleTotal() == 192);
    // A write after BARS commits it: no BARS undo any more.
    layer.beginUnit();
    layer.reset();
    layer.beginUnit();
    recordNote(layer, 1, 0, 60, 0.0, 1.0, 16);
    layer.closePass(16, false);
    CHECK(layer.resize(16, 32, 1) && layer.undoBars() == 1);
    layer.beginUnit();
    recordNote(layer, 2, 1, 62, 3.0, 4.0, 32);
    CHECK(layer.undoBars() == 0 && layer.undoLayerMask() == 0x02u && layer.visibleCount(0) == 2);
    std::printf("resize: repeat x2/x4/x8, cut + undo, no room\n");
}

int main() {
    testBitPacking();
    testQuantizeAndPickup();
    testNoteOffByTag();
    testPassCloseAndLateNoteOff();
    testSameOnsetSamePitch();
    testOverflow();
    testUndoBasics();
    testRandomSessionsAgainstModel();
    testPlan();
    testSegments();
    testTwoLayers();
    testResize();
    std::printf("looper layer tests passed (%d checks)\n", gChecks);
    return 0;
}
