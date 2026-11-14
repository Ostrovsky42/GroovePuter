#pragma once
#ifndef GROOVEPUTER_LOOPER_LAYER_H
#define GROOVEPUTER_LOOPER_LAYER_H

#include <cmath>
#include <cstdint>

#include "call_response_phrase_capture.h"
#include "call_response_planner.h"

// LOOPER phase 0: the layer core. Pure logic, no clock, no sound, no UI.
// Fixed storage, no allocation, main loop only (docs/design/LOOPER_DESIGN.md §3).
//
// One pool of kLoopCapacity events for every layer; each event carries its
// layer index (2 bits). Phase 1 uses layer 0 only; the field is there for the
// second layer (phase 3) at no data cost.
//
// Time grid: ticks, kTicksPerStep (6) to a sixteenth step, so a triplet
// sixteenth (2/3 step = 4 ticks), a 1/32 (3) and a triplet eighth (4/3 step =
// 8) are exact, and so is a 50 % gate of a sixteenth (3). BARS 8 = 128 steps
// = 768 ticks.
//
// The event is 5 bytes: onset 10 bits (0..767 ticks), length 10 bits (0 = still
// held, never played; 1023 = "to the pass end", every real length fits), pitch
// 7, velocity 7, layer 2, NEW, SHADOW = 38 bits. 192 events = 960 bytes.
// A pass plan (LoopPlan, ticks) holds LoopPlan::kCapacity (32) notes, the
// shared player as many: a pass with more notes is played as consecutive plans
// (segments, buildPlan nextStep). Segment boundaries stay on whole steps, cut
// on a step where no note sounds across when there is one
// (docs/design/LOOPER_DESIGN.md §3.4).
//
// The callers speak steps: positions are given relative to the start of the
// current pass ("rel", in sixteenth steps as a double, may be slightly negative
// for a note played just before the first downbeat, or >= passSteps for a note
// that belongs to the next pass) and are converted to ticks here.
//
// Quantization (quantizeOnsetTicks): the nearest tick; a note that rounds to
// the pass boundary is a pickup = tick 0 of the next pass. A NoteOff rounds to the nearest tick, the
// stored length is at least one tick. NoteOff is matched by tag only: the
// keyboard's NoteOff carries the interpreted pitch, not the pressed one.
//
// Undo without a snapshot: the NEW flag (written in the current undo unit)
// and the SHADOW flag (hidden by the current unit, brought back by undo). A unit is a RECORD pass, an OVERDUB
// pass with at least one note, or CLEAR. The unit is committed lazily: the
// first write of the next unit drops NEW flags and deletes SHADOW notes.
//
// Two layers (phase 3): a write goes to one layer and only ever replaces or
// hides a note of that layer; CLEAR hides one layer. The undo level is shared:
// it touches the layer(s) of the last unit (undoLayerMask).
//
// BARS on a loop with notes (resize) is a unit of its own: longer = the loop
// repeated to the new length (copies are NEW), shorter = the notes past the
// new end hidden (SHADOW). Undo takes it back; the bars before are kept in
// the flags (undoBars) for the caller to restore.
namespace GroovePuterLooper {

using GroovePuterCallResponse::AnswerLength;
using GroovePuterCallResponse::PhraseCapture;
using GroovePuterCallResponse::ResponsePlan;

constexpr uint8_t kLoopCapacity = 192;   // events of every layer, one pool
constexpr uint8_t kMaxLayers = 2;       // phase 3; the event field allows 4
constexpr uint8_t kOpenCapacity = 6;    // notes held at once while recording (more: the oldest ends)
constexpr uint8_t kStepsPerBar = 16;
constexpr uint8_t kMaxLoopSteps = 128;  // 8 bars
static_assert(kLoopCapacity <= 255, "pool indices fit in a byte");
constexpr uint8_t kTicksPerStep = 6;     // 1/16T = 4 ticks, 1/32 = 3, 1/8T = 8
constexpr uint16_t kMaxLoopTicks = 768;  // 8 bars
static_assert(kMaxLoopSteps * kTicksPerStep == kMaxLoopTicks, "128 steps of 6 ticks");
static_assert(kMaxLoopTicks <= 1023, "the onset is 10 bits");

struct LoopNote {
    static constexpr uint16_t kTickMask = 0x3FFu;
    static constexpr uint8_t kValueMask = 0x7Fu;
    static constexpr uint16_t kLengthToEnd = 0x3FFu;  // saturated length: to the pass end
    static constexpr uint8_t kNewBit = 0x40u;
    static constexpr uint8_t kShadowBit = 0x80u;

    uint8_t onsetLo{0};   // onset ticks, bits 0..7
    uint8_t lengthLo{0};  // length ticks, bits 0..7
    uint8_t high{0};      // bits 0..1 onset 8..9, bits 2..3 length 8..9,
                          // bits 4..5 layer, bit 6 NEW, bit 7 SHADOW
    uint8_t pitch{0};     // bits 0..6 pitch, bit 7 pending pickup
    uint8_t vel{0};       // velocity 1..127

    static LoopNote make(uint16_t onsetTicks, uint8_t layer, uint16_t lengthTicks, uint8_t note,
                         uint8_t velocity, bool isNew, bool shadow) {
        LoopNote n;
        n.high = static_cast<uint8_t>(((layer & 0x03u) << 4) | (isNew ? kNewBit : 0u) |
                                      (shadow ? kShadowBit : 0u));
        n.setOnsetTicks(onsetTicks);
        n.setLength(lengthTicks);
        n.pitch = static_cast<uint8_t>(note & kValueMask);
        n.vel = static_cast<uint8_t>(velocity & kValueMask);
        return n;
    }
    uint16_t onsetTicks() const { return static_cast<uint16_t>(onsetLo | ((high & 0x03u) << 8)); }
    // 0 = held; kLengthToEnd = to the pass end.
    uint16_t lengthTicks() const { return static_cast<uint16_t>(lengthLo | ((high & 0x0Cu) << 6)); }
    // Whole steps for readers on the 1/16 grid (UI, TO MELODY, tests): the
    // nearest step (may equal the pass length for a note in its last half
    // step), and the nearest number of steps, at least 1.
    uint8_t onsetStep() const {
        return static_cast<uint8_t>((onsetTicks() + kTicksPerStep / 2u) / kTicksPerStep);
    }
    uint8_t lengthSteps() const {
        const uint16_t steps = static_cast<uint16_t>((lengthTicks() + kTicksPerStep / 2u) / kTicksPerStep);
        return static_cast<uint8_t>(steps < 1 ? 1 : steps);
    }
    uint8_t layer() const { return static_cast<uint8_t>((high >> 4) & 0x03u); }
    uint8_t note() const { return pitch & kValueMask; }
    bool pickup() const { return (pitch & 0x80u) != 0; }
    void setPickup(bool on) {
        pitch = static_cast<uint8_t>(on ? (pitch | 0x80u) : (pitch & kValueMask));
    }
    uint8_t velocity() const { return vel; }
    bool isNew() const { return (high & kNewBit) != 0; }
    bool shadow() const { return (high & kShadowBit) != 0; }
    bool held() const { return lengthTicks() == 0; }
    void setOnsetTicks(uint16_t ticks) {
        ticks &= kTickMask;
        onsetLo = static_cast<uint8_t>(ticks & 0xFFu);
        high = static_cast<uint8_t>((high & ~0x03u) | (ticks >> 8));
    }
    // 0 = held; longer than 1022 ticks saturates to kLengthToEnd.
    void setLength(uint16_t ticks) {
        if (ticks > kLengthToEnd) ticks = kLengthToEnd;
        lengthLo = static_cast<uint8_t>(ticks & 0xFFu);
        high = static_cast<uint8_t>((high & ~0x0Cu) | ((ticks >> 8) << 2));
    }
    void setNew(bool on) { high = static_cast<uint8_t>(on ? (high | kNewBit) : (high & ~kNewBit)); }
    void setShadow(bool on) { high = static_cast<uint8_t>(on ? (high | kShadowBit) : (high & ~kShadowBit)); }
    void setVelocity(uint8_t v) { vel = static_cast<uint8_t>(v & kValueMask); }
};
static_assert(sizeof(LoopNote) == 5, "LoopNote is 5 bytes");

// One plan of a pass for the player, in ticks from the plan start.
struct LoopPlanNote {
    uint16_t tick{0};    // from the plan start
    uint16_t length{0};  // ticks, tick + length <= windowTicks
    uint8_t note{0};
    uint8_t velocity{0};
};

struct LoopPlan {
    static constexpr uint8_t kCapacity = ResponsePlan::kCapacity;  // 32: the player's slots
    uint8_t count{0};
    bool overflow{false};  // more than 32 simultaneous events at a tick
    uint16_t windowTicks{0};  // the plan covers this many ticks from its start
    LoopPlanNote notes[kCapacity]{};
};

// ceil(ticks * percent / 100) (percent <= 100): NOTE LENGTH on the tick grid.
constexpr uint16_t scaledLoopNoteLength(uint16_t ticks, uint8_t percent) {
    return static_cast<uint16_t>((static_cast<uint32_t>(ticks) * percent + 99u) / 100u);
}
// A played note is never shorter than this (a third of a step; the stored
// minimum is one tick) unless the same
// pitch starts again or the pass ends sooner: the player drops a note whose
// NoteOff is already due when its NoteOn comes up.
constexpr uint16_t kMinPlayedTicks = 2;

class LoopLayer {
public:
    enum class Write : uint8_t { Stored = 0, Replaced, Duplicate, Full, Invalid };

    void reset() {
        count_ = 0;
        dropped_ = 0;
        flags_ = 0;
        for (uint8_t i = 0; i < kOpenCapacity; ++i) open_[i].index = kOpenFree;
    }

    // The next write starts a new undo unit (and commits the previous one).
    void beginUnit() { flags_ |= kUnitPending; }
    bool unitPending() const { return (flags_ & kUnitPending) != 0; }

    // rel (steps from the pass start) as an onset in ticks: the nearest tick.
    // pickup: the note belongs to the next pass (rel in the last
    // half a tick before the pass end = tick 0 of the next pass;
    // rel past the pass end = the close was not serviced yet). False: outside
    // this pass and the next one.
    static bool quantizeOnsetTicks(double rel, uint8_t passSteps, uint16_t& ticks, bool& pickup) {
        const double pass = static_cast<double>(passSteps);
        constexpr double halfTick = 0.5 / kTicksPerStep;
        pickup = rel >= pass - halfTick;
        if (pickup) {
            rel -= pass;
            if (rel >= pass - halfTick) return false;
        }
        // rel < pass - halfTick, so the tick is before the next pass.
        ticks = rel <= 0.0 ? uint16_t{0}
                           : static_cast<uint16_t>(std::floor(rel * kTicksPerStep + 0.5));
        return true;
    }

    Write noteOn(uint16_t tag, uint8_t layer, uint8_t note, uint8_t velocity,
                 double rel, uint8_t passSteps) {
        if (!std::isfinite(rel) || passSteps == 0 || passSteps > kMaxLoopSteps) return Write::Invalid;
        uint16_t onset = 0;
        bool pickup = false;
        if (!quantizeOnsetTicks(rel, passSteps, onset, pickup)) return Write::Invalid;
        note &= LoopNote::kValueMask;
        velocity &= LoopNote::kValueMask;
        if (velocity == 0) velocity = 1;
        layer &= 0x03u;

        // A pickup over a note the loop already plays on tick 0 adds nothing.
        if (pickup) {
            for (uint8_t i = 0; i < count_; ++i) {
                const LoopNote& n = notes_[i];
                if (!n.shadow() && n.layer() == layer && n.onsetTicks() == onset && n.note() == note &&
                    !isPickupIndex(i)) return Write::Duplicate;
            }
        }

        // The same source attacking again closes its previous note first.
        const int prev = findOpen(tag);
        if (prev >= 0) closeEntry(static_cast<uint8_t>(prev), rel, passSteps);

        if (flags_ & kUnitPending) {
            commit(false);
            flags_ = static_cast<uint8_t>((flags_ & ~kUnitPending) | kUndoValid);
        }

        if (!pickup) {
            for (uint8_t i = 0; i < count_; ++i) {
                LoopNote& n = notes_[i];
                if (n.shadow() || n.layer() != layer || n.onsetTicks() != onset || n.note() != note ||
                    isPickupIndex(i)) continue;
                if (n.isNew()) {  // same unit: replace in place
                    n.setVelocity(velocity);
                    n.setLength(0);
                    dropOpenFor(i);
                    addOpen(tag, i, false, rel, passSteps);
                    return Write::Replaced;
                }
                if (count_ >= kLoopCapacity) return overflow();
                n.setShadow(true);  // older unit: hidden, undo brings it back
                break;
            }
        }
        if (count_ >= kLoopCapacity) return overflow();
        const uint8_t index = count_++;
        notes_[index] = LoopNote::make(onset, layer, 0, note, velocity, true, false);
        notes_[index].setPickup(pickup);
        addOpen(tag, index, pickup, rel, passSteps);
        return Write::Stored;
    }

    // Returns true when an open note was closed.
    bool noteOff(uint16_t tag, double rel, uint8_t passSteps) {
        if (!std::isfinite(rel)) return false;
        const int entry = findOpen(tag);
        if (entry < 0) return false;
        closeEntry(static_cast<uint8_t>(entry), rel, passSteps);
        return true;
    }

    // End of a pass. Held notes end at the pass end (their late NoteOff is
    // ignored). Pickups become notes of the new pass when it records, and are
    // dropped otherwise. A recording pass starts a new undo unit.
    void closePass(uint8_t passSteps, bool nextRecording) {
        bool pickups = false;
        for (uint8_t e = 0; e < kOpenCapacity; ++e) {
            OpenNote& o = open_[e];
            if (o.index == kOpenFree) continue;
            const uint8_t index = o.index & kOpenIndexMask;
            if (o.index & kOpenPickup) {
                if (nextRecording) {
                    o.index = index;  // now a note of the current pass
                    pickups = true;
                } else {
                    o.index = kOpenFree;
                    removeAt(index);
                }
                continue;
            }
            LoopNote& n = notes_[index];
            const int32_t length = passTicksOf(passSteps) - n.onsetTicks();
            n.setLength(static_cast<uint16_t>(length < 1 ? 1 : length));
            o.index = kOpenFree;
        }
        // A pickup can already have been released. Its marker lives on the
        // note as well as the open entry so it survives that early NoteOff.
        for (int i = static_cast<int>(count_) - 1; i >= 0; --i) {
            if (!notes_[i].pickup()) continue;
            if (nextRecording) pickups = true;
            else removeAt(static_cast<uint8_t>(i));
        }
        if (!nextRecording) return;
        if (pickups) {
            // The pickups are the first notes of the new unit.
            commit(true);
            for (uint8_t i = 0; i < count_; ++i) notes_[i].setPickup(false);
            flags_ = static_cast<uint8_t>((flags_ & ~kUnitPending) | kUndoValid);
        } else {
            beginUnit();
        }
    }

    // Recording stops now (punch-out, stop, panic): held notes end here,
    // pickups for a pass that will not record are dropped.
    void closeOpenAt(double rel, uint8_t passSteps) {
        discardPickups();
        if (!std::isfinite(rel)) rel = static_cast<double>(passSteps);
        for (uint8_t e = 0; e < kOpenCapacity; ++e) closeEntry(e, rel, passSteps);
    }

    void discardPickups() {
        for (int i = static_cast<int>(count_) - 1; i >= 0; --i)
            if (notes_[i].pickup()) removeAt(static_cast<uint8_t>(i));
    }

    bool canUndo() const {
        if ((flags_ & kUndoValid) == 0) return false;
        if (flags_ & kUndoBars) return true;
        for (uint8_t i = 0; i < count_; ++i)
            if (notes_[i].isNew() || notes_[i].shadow()) return true;
        return false;
    }

    // One level: removes the notes of the last unit and shows what it hid.
    bool undo() {
        if (!canUndo()) return false;
        for (int i = static_cast<int>(count_) - 1; i >= 0; --i) {
            LoopNote& n = notes_[i];
            if (n.isNew()) {
                removeAt(static_cast<uint8_t>(i));
            } else if (n.shadow()) {
                n.setShadow(false);
            }
        }
        flags_ = static_cast<uint8_t>((flags_ & ~(kUndoValid | kUndoBars | kBarsCodeMask)) | kUnitPending);
        return true;
    }

    // Bit l: layer l has notes in the last unit (what undo would change).
    // A BARS unit touches every layer.
    uint8_t undoLayerMask() const {
        if (!canUndo()) return 0;
        if (flags_ & kUndoBars) return 0x0Fu;
        uint8_t mask = 0;
        for (uint8_t i = 0; i < count_; ++i)
            if (notes_[i].isNew() || notes_[i].shadow()) mask = static_cast<uint8_t>(mask | (1u << notes_[i].layer()));
        return mask;
    }
    // The last unit is a BARS change: the bars before it (1/2/4/8), else 0.
    uint8_t undoBars() const {
        if ((flags_ & (kUndoValid | kUndoBars)) != (kUndoValid | kUndoBars)) return 0;
        return static_cast<uint8_t>(1u << ((flags_ & kBarsCodeMask) >> kBarsCodeShift));
    }

    // Room for resize(): longer, every visible note is repeated.
    bool resizeFits(uint8_t oldSteps, uint8_t newSteps) const {
        if (!validResize(oldSteps, newSteps)) return false;
        if (newSteps <= oldSteps) return true;
        uint16_t visible = 0;
        for (uint8_t i = 0; i < count_; ++i) if (!notes_[i].shadow()) ++visible;
        return visible * (newSteps / oldSteps) <= kLoopCapacity;
    }

    // BARS with notes: one undo unit (oldBars = the bars to restore on undo).
    // Longer: the loop repeats to the new length; a note longer than the old
    // pass end is cut there first (that is how it sounded). Shorter: the notes
    // from the new end on are hidden. Recording must be closed by the caller
    // (held notes end, pickups dropped).
    bool resize(uint8_t oldSteps, uint8_t newSteps, uint8_t oldBars) {
        if (!resizeFits(oldSteps, newSteps) || newSteps == oldSteps) return false;
        discardPickups();
        commit(false);
        for (uint8_t e = 0; e < kOpenCapacity; ++e) open_[e].index = kOpenFree;
        const uint16_t oldTicks = passTicksOf(oldSteps);
        const uint16_t newTicks = passTicksOf(newSteps);
        if (newSteps > oldSteps) {
            const uint8_t base = count_;
            for (uint8_t i = 0; i < base; ++i) {
                LoopNote& n = notes_[i];
                const uint16_t room = n.onsetTicks() < oldTicks
                    ? static_cast<uint16_t>(oldTicks - n.onsetTicks()) : uint16_t{1};
                if (n.held()) n.setLength(1);
                if (n.lengthTicks() == LoopNote::kLengthToEnd || n.lengthTicks() > room) n.setLength(room);
            }
            for (uint8_t k = 1; k < newSteps / oldSteps; ++k) {
                for (uint8_t i = 0; i < base; ++i) {
                    LoopNote copy = notes_[i];
                    copy.setOnsetTicks(static_cast<uint16_t>(copy.onsetTicks() + k * oldTicks));
                    copy.setNew(true);
                    notes_[count_++] = copy;
                }
            }
        } else {
            for (uint8_t i = 0; i < count_; ++i)
                if (notes_[i].onsetTicks() >= newTicks) notes_[i].setShadow(true);
        }
        uint8_t code = 0;
        while (code < 3 && (1u << code) < oldBars) ++code;
        flags_ = static_cast<uint8_t>(flags_ | kUndoValid | kUnitPending | kUndoBars |
                                      (code << kBarsCodeShift));
        return true;
    }

    // CLEAR is a unit of its own: every visible note of the layer is hidden,
    // undo shows it again. The slots are freed by the next commit.
    bool clear(uint8_t layer) {
        if (visibleCount(layer) == 0) return false;
        commit(false);
        for (uint8_t e = 0; e < kOpenCapacity; ++e) open_[e].index = kOpenFree;
        for (uint8_t i = 0; i < count_; ++i) {
            LoopNote& n = notes_[i];
            if (n.layer() != (layer & 0x03u)) continue;
            if (n.held()) n.setLength(1);
            n.setShadow(true);
        }
        flags_ = static_cast<uint8_t>(flags_ | kUndoValid | kUnitPending);
        return true;
    }

    // Final: no undo, hidden notes gone.
    void dropUndo() {
        commit(false);
        flags_ = static_cast<uint8_t>(flags_ & ~kUndoValid);
    }

    // Visible notes of the layers not in muteMask, onset >= fromStep, as a
    // plan in ticks whose tick 0 is fromStep. Held notes are not played (the
    // person is holding them live). Two notes of one pitch never overlap (the
    // later onset cuts the earlier note). LENGTH scales the plan only.
    //
    // A plan holds LoopPlan::kCapacity notes. When the rest of the pass has
    // more, the plan ends early and *nextStep (0 = the plan reaches the pass
    // end) is where the next one starts, always a whole step: the latest step
    // that keeps the plan within capacity and that no note sounds across;
    // without such a step, the latest one that fits, the notes across it end
    // there and the next plan (carry = true) starts them again at its tick 0
    // with the rest of their length. Without carry (resume mid-pass) only
    // onsets >= fromStep play, and with them the onsets less than half a step
    // before fromStep, moved onto it (kResumeLeadTicks): a note played a hair
    // before the beat still starts the resumed loop, as it did on the 1/16
    // grid.
    //
    // Main-loop stack: 384 bytes (played lengths) + 128 (onsets per step) +
    // 24 (play mask) + 16 here; the caller's LoopPlan is 196 bytes.
    uint8_t buildPlan(LoopPlan& plan, uint8_t passSteps, AnswerLength length,
                      uint8_t fromStep = 0, uint8_t muteMask = 0, bool carry = false,
                      uint8_t* nextStep = nullptr) const {
        plan = LoopPlan{};
        if (nextStep) *nextStep = 0;
        if (passSteps == 0 || passSteps > kMaxLoopSteps || fromStep >= passSteps) return 0;
        const uint8_t percent = GroovePuterCallResponse::answerLengthPercent(length);
        constexpr uint8_t cap = LoopPlan::kCapacity;
        const uint16_t passTicks = passTicksOf(passSteps);
        const uint16_t fromTick = passTicksOf(fromStep);

        PlayMask play{};
        playableMask(passSteps, muteMask, play);
        // playedLength of each note, computed at most once per plan (0 = not yet;
        // a played length is never 0). 384 bytes of main-loop stack.
        uint16_t played[kLoopCapacity] = {};
        uint8_t carried = 0, total = 0;
        uint16_t firstOnset = passTicks;
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const uint16_t onset = planOnset(notes_[i].onsetTicks(), fromTick, carry);
            if (onset >= fromTick) {
                ++total;
                if (onset < firstOnset) firstOnset = onset;
            } else if (carry && onset + playedOnce(played, i, passSteps, muteMask, percent) > fromTick) {
                ++carried;
            }
        }
        uint8_t end = passSteps;
        if (carried + total > cap)
            end = segmentEnd(play, played, passSteps, fromStep, muteMask, percent, carry, carried, firstOnset);
        const uint16_t endTick = passTicksOf(end);

        plan.windowTicks = static_cast<uint16_t>(endTick - fromTick);
        for (uint8_t i = 0; i < count_ && plan.count < cap; ++i) {
            if (!inMask(play, i)) continue;
            const LoopNote& n = notes_[i];
            const uint16_t onset = planOnset(n.onsetTicks(), fromTick, carry);
            if (onset >= endTick) continue;
            if (onset < fromTick && !carry) continue;
            const int32_t stop = static_cast<int32_t>(onset) +
                                 playedOnce(played, i, passSteps, muteMask, percent);
            const int32_t start = onset >= fromTick ? onset : fromTick;
            if (onset < fromTick && stop <= fromTick) continue;
            const int32_t len = (stop < endTick ? stop : endTick) - start;
            plan.notes[plan.count++] = LoopPlanNote{static_cast<uint16_t>(start - fromTick),
                                                    static_cast<uint16_t>(len), n.note(),
                                                    n.velocity()};
        }
        if (end < passSteps && nextStep) *nextStep = end;
        return plan.count;
    }

    // Tick-granular variant for the runtime player. A boundary never divides
    // a group of simultaneous onsets. Dense chords beyond the slot budget are
    // explicitly reported via overflow.
    uint8_t buildPlanTicks(LoopPlan& plan, uint8_t passSteps, AnswerLength length,
                           uint16_t fromTick, uint8_t muteMask, bool carry,
                           uint16_t* nextTick) const {
        plan = LoopPlan{};
        if (nextTick) *nextTick = 0;
        const uint16_t passTicks = passTicksOf(passSteps);
        if (!passSteps || passSteps > kMaxLoopSteps || fromTick >= passTicks) return 0;
        const uint8_t percent = GroovePuterCallResponse::answerLengthPercent(length);
        PlayMask play{};
        playableMask(passSteps, muteMask, play);
        uint16_t played[kLoopCapacity] = {};
        // Keep only the earliest 33 starts: no large per-plan stack buffer.
        uint16_t starts[LoopPlan::kCapacity + 1]{};
        uint8_t eligible = 0;
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const uint16_t onset = planOnset(notes_[i].onsetTicks(), fromTick, carry);
            if (onset < fromTick && (!carry || onset + playedOnce(played, i, passSteps, muteMask, percent) <= fromTick)) continue;
            const uint16_t start = onset < fromTick ? fromTick : onset;
            if (eligible == LoopPlan::kCapacity + 1 && start >= starts[LoopPlan::kCapacity]) continue;
            uint8_t at = eligible < LoopPlan::kCapacity + 1 ? eligible++ : LoopPlan::kCapacity;
            while (at && starts[at - 1] > start) { starts[at] = starts[at - 1]; --at; }
            starts[at] = start;
        }
        uint16_t endTick = passTicks;
        if (eligible > LoopPlan::kCapacity) {
            endTick = starts[LoopPlan::kCapacity];
            if (endTick <= fromTick) {
                plan.overflow = true;
                return 0;  // caller stops playback rather than losing notes
            }
            // Prefer a boundary where no voice needs to be cut and restarted.
            for (int candidateIndex = LoopPlan::kCapacity; candidateIndex >= 0; --candidateIndex) {
                const uint16_t candidate = starts[candidateIndex];
                if (candidate <= fromTick) break;
                bool soundingAcross = false;
                for (uint8_t i = 0; i < count_ && !soundingAcross; ++i) {
                    if (!inMask(play, i)) continue;
                            const uint16_t onset = planOnset(notes_[i].onsetTicks(), fromTick, carry);
                    if (onset >= candidate || (onset < fromTick && !carry)) continue;
                    const uint16_t stop = static_cast<uint16_t>(onset + playedOnce(played, i, passSteps, muteMask, percent));
                    if (stop > candidate) soundingAcross = true;
                }
                if (!soundingAcross) { endTick = candidate; break; }
            }
        }
        plan.windowTicks = static_cast<uint16_t>(endTick - fromTick);
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const LoopNote& n = notes_[i];
            const uint16_t onset = planOnset(n.onsetTicks(), fromTick, carry);
            if (onset >= endTick || (onset < fromTick && !carry)) continue;
            const int32_t stop = static_cast<int32_t>(onset) + playedOnce(played, i, passSteps, muteMask, percent);
            const int32_t start = onset >= fromTick ? onset : fromTick;
            if (stop <= start) continue;
            if (plan.count == LoopPlan::kCapacity) { plan.overflow = true; continue; }
            const int32_t len = (stop < endTick ? stop : endTick) - start;
            plan.notes[plan.count++] = LoopPlanNote{static_cast<uint16_t>(start - fromTick),
                                                    static_cast<uint16_t>(len), n.note(), n.velocity()};
        }
        if (endTick < passTicks && nextTick) *nextTick = endTick;
        return plan.count;
    }

    uint8_t slotCount() const { return count_; }
    bool full() const { return count_ >= kLoopCapacity; }
    uint8_t dropped() const { return dropped_; }
    uint8_t visibleCount(uint8_t layer = 0) const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < count_; ++i)
            if (!notes_[i].shadow() && notes_[i].layer() == (layer & 0x03u)) ++n;
        return n;
    }
    uint8_t visibleTotal() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < count_; ++i) if (!notes_[i].shadow()) ++n;
        return n;
    }
    uint8_t newCount() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < count_; ++i) if (notes_[i].isNew() && !notes_[i].shadow()) ++n;
        return n;
    }
    uint8_t openCount() const {
        uint8_t n = 0;
        for (uint8_t e = 0; e < kOpenCapacity; ++e) if (open_[e].index != kOpenFree) ++n;
        return n;
    }
    const LoopNote& note(uint8_t index) const { return notes_[index < kLoopCapacity ? index : 0]; }

private:
    static constexpr uint8_t kUnitPending = 0x01u;
    static constexpr uint8_t kUndoValid = 0x02u;
    static constexpr uint8_t kUndoBars = 0x04u;      // the last unit is a BARS change
    static constexpr uint8_t kBarsCodeShift = 3;     // bars before it: 1 << code
    static constexpr uint8_t kBarsCodeMask = 0x18u;
    static constexpr uint16_t kOpenFree = 0xFFFFu;
    static constexpr uint16_t kOpenPickup = 0x8000u;
    static constexpr uint16_t kOpenIndexMask = 0x00FFu;

    // 4 bytes: tag and a 16-bit pool index with a separate pickup flag.
    struct OpenNote {
        uint8_t tagLo{0};
        uint8_t tagHi{0};
        uint16_t index{kOpenFree};
        uint16_t tag() const { return static_cast<uint16_t>(tagLo | (tagHi << 8)); }
    };

    static constexpr uint16_t passTicksOf(uint8_t steps) {
        return static_cast<uint16_t>(steps * kTicksPerStep);
    }
    // Resume (no carry): an onset less than half a step before the plan start
    // plays on the plan start.
    static constexpr uint16_t kResumeLeadTicks = kTicksPerStep / 2u;
    static constexpr uint16_t planOnset(uint16_t onset, uint16_t fromTick, bool carry) {
        return !carry && onset < fromTick && onset + kResumeLeadTicks >= fromTick ? fromTick : onset;
    }
    // rel (steps) as the nearest tick, bounded so the cast is always defined.
    static int32_t roundTicks(double rel) {
        const double ticks = std::floor(rel * kTicksPerStep + 0.5);
        constexpr double limit = 4.0 * kMaxLoopTicks;
        return static_cast<int32_t>(ticks > limit ? limit : ticks < -limit ? -limit : ticks);
    }

    // A note that sounds in a plan: visible, closed, layer not muted, inside
    // the pass; a second note of one tick and pitch (never stored in one
    // layer, possible across layers) does not play.
    bool sounds(uint8_t i, uint8_t passSteps, uint8_t muteMask) const {
        const LoopNote& n = notes_[i];
        return !n.shadow() && !n.pickup() && !n.held() && (muteMask & (1u << n.layer())) == 0 &&
               n.onsetTicks() < passTicksOf(passSteps);
    }
    using PlayMask = uint64_t[(kLoopCapacity + 63u) / 64u];
    static bool inMask(const PlayMask& mask, uint8_t i) { return ((mask[i / 64u] >> (i % 64u)) & 1u) != 0; }
    // Bit i: pool note i plays (sounds, and is the first of its tick + pitch).
    void playableMask(uint8_t passSteps, uint8_t muteMask, PlayMask& mask) const {
        for (uint8_t i = 0; i < count_; ++i) {
            if (!sounds(i, passSteps, muteMask)) continue;
            bool first = true;
            for (uint8_t j = 0; j < i && first; ++j)
                if (inMask(mask, j) && notes_[j].onsetTicks() == notes_[i].onsetTicks() &&
                    notes_[j].note() == notes_[i].note()) first = false;
            if (first) mask[i / 64u] |= uint64_t{1} << (i % 64u);
        }
    }

    // Played length of a playable note in ticks: to the pass end at most, cut
    // at the next onset of its pitch, then LENGTH; at least kMinPlayedTicks
    // when the pass end and the next onset of the pitch leave that much.
    uint16_t playedLength(uint8_t i, uint8_t passSteps, uint8_t muteMask, uint8_t percent) const {
        const LoopNote& n = notes_[i];
        const uint16_t onset = n.onsetTicks();
        uint16_t limit = static_cast<uint16_t>(passTicksOf(passSteps) - onset);
        for (uint8_t j = 0; j < count_; ++j) {
            if (j == i || notes_[j].note() != n.note() || !sounds(j, passSteps, muteMask)) continue;
            const uint16_t other = notes_[j].onsetTicks();
            if (other > onset && other - onset < limit) limit = static_cast<uint16_t>(other - onset);
        }
        const uint16_t len = n.lengthTicks() > limit ? limit : n.lengthTicks();  // kLengthToEnd too
        const uint16_t scaled = scaledLoopNoteLength(len, percent);
        if (scaled >= kMinPlayedTicks) return scaled;
        return limit < kMinPlayedTicks ? limit : kMinPlayedTicks;
    }

    uint16_t playedOnce(uint16_t* played, uint8_t i, uint8_t passSteps, uint8_t muteMask, uint8_t percent) const {
        if (played[i] == 0) played[i] = playedLength(i, passSteps, muteMask, percent);
        return played[i];
    }

    // End (a whole step) of a plan from fromStep that cannot hold the rest of
    // the pass.
    uint8_t segmentEnd(const PlayMask& play, uint16_t* played, uint8_t passSteps, uint8_t fromStep, uint8_t muteMask, uint8_t percent,
                       bool carry, uint8_t carried, uint16_t firstOnset) const {
        constexpr uint8_t cap = LoopPlan::kCapacity;
        const uint16_t fromTick = passTicksOf(fromStep);
        // The latest end that fits: the step of the first note past capacity.
        // Onsets per step, 128 bytes of main-loop stack.
        uint8_t perStep[kMaxLoopSteps] = {};
        for (uint8_t i = 0; i < count_; ++i) {
            const uint16_t onset = planOnset(notes_[i].onsetTicks(), fromTick, carry);
            if (!inMask(play, i) || onset < fromTick) continue;
            ++perStep[onset / kTicksPerStep];
        }
        uint8_t maxEnd = fromStep;
        for (uint16_t sum = carried; maxEnd < passSteps; ++maxEnd) {
            sum = static_cast<uint16_t>(sum + perStep[maxEnd]);
            if (sum > cap) break;
        }
        if (maxEnd <= fromStep) return static_cast<uint8_t>(fromStep + 1);  // defensive: truncated
        // Step boundaries some note sounds across (strictly inside the note).
        uint32_t across[4]{};
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const uint16_t onset = planOnset(notes_[i].onsetTicks(), fromTick, carry);
            const uint16_t stop = static_cast<uint16_t>(onset + playedOnce(played, i, passSteps, muteMask, percent));
            uint16_t t = onset;
            if (onset < fromTick) {
                if (!carry || stop <= fromTick) continue;
                t = fromTick;
            }
            for (uint16_t s = static_cast<uint16_t>(t / kTicksPerStep + 1u);
                 s < kMaxLoopSteps && passTicksOf(static_cast<uint8_t>(s)) < stop; ++s)
                across[s >> 5] |= uint32_t{1} << (s & 31u);
        }
        for (uint8_t t = maxEnd; t > fromStep; --t) {
            if (carried == 0 && passTicksOf(t) <= firstOnset) break;  // the plan would be empty
            if ((across[t >> 5] & (uint32_t{1} << (t & 31u))) == 0) return t;
        }
        return maxEnd;
    }

    static bool validResize(uint8_t oldSteps, uint8_t newSteps) {
        if (oldSteps == 0 || newSteps == 0 || oldSteps > kMaxLoopSteps || newSteps > kMaxLoopSteps) return false;
        return newSteps >= oldSteps ? newSteps % oldSteps == 0 : oldSteps % newSteps == 0;
    }

    Write overflow() {
        if (dropped_ < 255u) ++dropped_;
        return Write::Full;
    }

    int findOpen(uint16_t tag) const {
        for (uint8_t e = 0; e < kOpenCapacity; ++e)
            if (open_[e].index != kOpenFree && open_[e].tag() == tag) return e;
        return -1;
    }

    bool isPickupIndex(uint8_t index) const {
        return notes_[index].pickup();
    }

    void dropOpenFor(uint8_t index) {
        for (uint8_t e = 0; e < kOpenCapacity; ++e)
            if (open_[e].index != kOpenFree && (open_[e].index & kOpenIndexMask) == index)
                open_[e].index = kOpenFree;
    }

    void addOpen(uint16_t tag, uint8_t index, bool pickup, double rel, uint8_t passSteps) {
        int free = -1;
        for (uint8_t e = 0; e < kOpenCapacity; ++e) if (open_[e].index == kOpenFree) { free = e; break; }
        if (free < 0) {  // more than kOpenCapacity held: the oldest entry ends now
            closeEntry(0, rel, passSteps);
            free = 0;
        }
        OpenNote& o = open_[free];
        o.tagLo = static_cast<uint8_t>(tag & 0xFFu);
        o.tagHi = static_cast<uint8_t>(tag >> 8);
        o.index = static_cast<uint16_t>(index | (pickup ? kOpenPickup : 0u));
    }

    // The note ends on the tick nearest to rel, at least one tick long.
    void closeEntry(uint8_t e, double rel, uint8_t passSteps) {
        OpenNote& o = open_[e];
        if (o.index == kOpenFree) return;
        const bool pickup = (o.index & kOpenPickup) != 0;
        LoopNote& n = notes_[o.index & kOpenIndexMask];
        o.index = kOpenFree;
        const int32_t passTicks = passTicksOf(passSteps);
        const int32_t off = roundTicks(rel) - (pickup ? passTicks : 0);
        int32_t length = off - static_cast<int32_t>(n.onsetTicks());
        const int32_t maxLength = passTicks - n.onsetTicks();
        if (length > maxLength) length = maxLength;
        if (length < 1) length = 1;
        n.setLength(static_cast<uint16_t>(length));
    }

    void removeAt(uint8_t index) {
        if (index >= count_) return;
        for (uint8_t i = index; i + 1u < count_; ++i) notes_[i] = notes_[i + 1u];
        --count_;
        for (uint8_t e = 0; e < kOpenCapacity; ++e) {
            OpenNote& o = open_[e];
            if (o.index == kOpenFree) continue;
            const uint8_t at = o.index & kOpenIndexMask;
            if (at == index) o.index = kOpenFree;
            else if (at > index) o.index = static_cast<uint16_t>(o.index - 1u);
        }
    }

    // Drops NEW flags and deletes SHADOW notes. keepPickups: notes held as
    // pickups of the new unit (closePass) stay NEW.
    void commit(bool keepOpen) {
        flags_ = static_cast<uint8_t>(flags_ & ~(kUndoBars | kBarsCodeMask));
        for (int i = static_cast<int>(count_) - 1; i >= 0; --i) {
            LoopNote& n = notes_[i];
            if (n.shadow()) {
                removeAt(static_cast<uint8_t>(i));
                continue;
            }
            if (keepOpen && (n.pickup() || isOpenIndex(static_cast<uint8_t>(i)))) continue;
            n.setNew(false);
        }
    }

    bool isOpenIndex(uint8_t index) const {
        for (uint8_t e = 0; e < kOpenCapacity; ++e)
            if (open_[e].index != kOpenFree && (open_[e].index & kOpenIndexMask) == index) return true;
        return false;
    }

    LoopNote notes_[kLoopCapacity]{};
    OpenNote open_[kOpenCapacity]{};
    uint8_t count_{0};
    uint8_t dropped_{0};
    uint8_t flags_{0};
};

}  // namespace GroovePuterLooper

#endif  // GROOVEPUTER_LOOPER_LAYER_H
