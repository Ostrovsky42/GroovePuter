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
// BARS 8 (128 steps) and 64 events: the event stays 4 bytes (onset 7 bits,
// length 7 bits, pitch 7, velocity 7, layer 2, NEW, SHADOW = 32 bits). The
// length saturates at 127 = "to the pass end" (exact for every note except
// onset 0 with length exactly 127 of an 8-bar pass, which plays 128).
// A pass plan holds ResponsePlan::kCapacity (32) notes, the shared player as
// many: a pass with more notes is played as consecutive plans (segments,
// buildPlan nextStep), cut on a step where no note sounds across when there
// is one (docs/design/LOOPER_DESIGN.md §3.4).
//
// Positions are given relative to the start of the current pass ("rel", in
// sixteenth steps, may be slightly negative for a note played just before the
// first downbeat, or >= passSteps for a note that belongs to the next pass).
//
// Quantization: PhraseCapture::quantizeOnset (nearest sixteenth; a note in the
// last 30 % of the step before the pass end is a pickup = step 0 of the next
// pass). NoteOff is matched by tag only: the keyboard's NoteOff carries the
// interpreted pitch, not the pressed one.
//
// Undo without a snapshot: bit 7 of the pitch byte is NEW (written in the
// current undo unit), bit 7 of the velocity byte is SHADOW (hidden by the
// current unit, brought back by undo). A unit is a RECORD pass, an OVERDUB
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
using GroovePuterCallResponse::ResponseNote;
using GroovePuterCallResponse::ResponsePlan;

constexpr uint8_t kLoopCapacity = 64;   // events of every layer, one pool
constexpr uint8_t kMaxLayers = 2;       // phase 3; the event field allows 4
constexpr uint8_t kOpenCapacity = 6;    // notes held at once while recording (more: the oldest ends)
constexpr uint8_t kStepsPerBar = 16;
constexpr uint8_t kMaxLoopSteps = 128;  // 8 bars
static_assert(kLoopCapacity <= 64, "the open table keeps a pool index in 6 bits");
static_assert(kMaxLoopSteps <= 128, "the onset is 7 bits");

struct LoopNote {
    static constexpr uint8_t kOnsetMask = 0x7Fu;
    static constexpr uint8_t kValueMask = 0x7Fu;
    static constexpr uint8_t kFlag = 0x80u;
    static constexpr uint8_t kLengthToEnd = 0x7Fu;  // saturated length: to the pass end

    uint8_t onsetLayer{0};   // bits 0..6 onset step 0..127, bit 7 layer bit 0
    uint8_t lengthLayer{0};  // bits 0..6 length (0 = still held, never played;
                             // 1..126 steps; 127 = to the pass end), bit 7 layer bit 1
    uint8_t noteFlag{0};     // bits 0..6 input pitch, bit 7 NEW
    uint8_t velFlag{0};      // bits 0..6 velocity 1..127, bit 7 SHADOW

    static LoopNote make(uint8_t onset, uint8_t layer, uint8_t length, uint8_t note,
                         uint8_t velocity, bool isNew, bool shadow) {
        LoopNote n;
        n.onsetLayer = static_cast<uint8_t>((onset & kOnsetMask) | ((layer & 0x01u) << 7));
        n.lengthLayer = static_cast<uint8_t>(((layer & 0x02u) << 6));
        n.setLength(length);
        n.noteFlag = static_cast<uint8_t>((note & kValueMask) | (isNew ? kFlag : 0u));
        n.velFlag = static_cast<uint8_t>((velocity & kValueMask) | (shadow ? kFlag : 0u));
        return n;
    }
    uint8_t onset() const { return onsetLayer & kOnsetMask; }
    uint8_t layer() const {
        return static_cast<uint8_t>((onsetLayer >> 7) | ((lengthLayer >> 7) << 1));
    }
    uint8_t length() const { return lengthLayer & kValueMask; }
    uint8_t note() const { return noteFlag & kValueMask; }
    uint8_t velocity() const { return velFlag & kValueMask; }
    bool isNew() const { return (noteFlag & kFlag) != 0; }
    bool shadow() const { return (velFlag & kFlag) != 0; }
    bool held() const { return length() == 0; }
    // 0 = held; longer than 126 steps saturates to kLengthToEnd.
    void setLength(uint8_t steps) {
        lengthLayer = static_cast<uint8_t>((lengthLayer & kFlag) |
                                           (steps > kLengthToEnd ? kLengthToEnd : steps));
    }
    void setNew(bool on) { noteFlag = static_cast<uint8_t>(on ? (noteFlag | kFlag) : (noteFlag & kValueMask)); }
    void setShadow(bool on) { velFlag = static_cast<uint8_t>(on ? (velFlag | kFlag) : (velFlag & kValueMask)); }
    void setVelocity(uint8_t v) { velFlag = static_cast<uint8_t>((velFlag & kFlag) | (v & kValueMask)); }
};
static_assert(sizeof(LoopNote) == 4, "LoopNote is 4 bytes");

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

    Write noteOn(uint16_t tag, uint8_t layer, uint8_t note, uint8_t velocity,
                 double rel, uint8_t passSteps) {
        if (!std::isfinite(rel) || passSteps == 0 || passSteps > kMaxLoopSteps) return Write::Invalid;
        int32_t onset = PhraseCapture::quantizeOnset(rel, passSteps);
        bool pickup = false;
        if (onset >= passSteps) {  // the next pass (pickup, or the close not serviced yet)
            pickup = true;
            onset -= passSteps;
            if (onset >= passSteps) return Write::Invalid;
        }
        if (onset < 0) onset = 0;
        note &= LoopNote::kValueMask;
        velocity &= LoopNote::kValueMask;
        if (velocity == 0) velocity = 1;
        layer &= 0x03u;

        // A pickup over a note the loop already plays on step 0 adds nothing.
        if (pickup) {
            for (uint8_t i = 0; i < count_; ++i) {
                const LoopNote& n = notes_[i];
                if (!n.shadow() && n.layer() == layer && n.onset() == onset && n.note() == note &&
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
                if (n.shadow() || n.layer() != layer || n.onset() != onset || n.note() != note ||
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
        notes_[index] = LoopNote::make(static_cast<uint8_t>(onset), layer, 0, note, velocity, true, false);
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
            const int32_t length = static_cast<int32_t>(passSteps) - n.onset();
            n.setLength(static_cast<uint8_t>(length < 1 ? 1 : length));
            o.index = kOpenFree;
        }
        if (!nextRecording) return;
        if (pickups) {
            // The pickups are the first notes of the new unit.
            commit(true);
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
        for (uint8_t e = 0; e < kOpenCapacity; ++e) {
            OpenNote& o = open_[e];
            if (o.index == kOpenFree || (o.index & kOpenPickup) == 0) continue;
            const uint8_t index = o.index & kOpenIndexMask;
            o.index = kOpenFree;
            removeAt(index);  // renumbers the other entries in place
        }
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
        if (newSteps > oldSteps) {
            const uint8_t base = count_;
            for (uint8_t i = 0; i < base; ++i) {
                LoopNote& n = notes_[i];
                const uint8_t room = static_cast<uint8_t>(oldSteps - n.onset());
                if (n.held()) n.setLength(1);
                if (n.length() == LoopNote::kLengthToEnd || n.length() > room) n.setLength(room);
            }
            for (uint8_t k = 1; k < newSteps / oldSteps; ++k) {
                for (uint8_t i = 0; i < base; ++i) {
                    LoopNote copy = notes_[i];
                    copy.onsetLayer = static_cast<uint8_t>((copy.onsetLayer & LoopNote::kFlag) |
                                                           ((copy.onset() + k * oldSteps) & LoopNote::kOnsetMask));
                    copy.setNew(true);
                    notes_[count_++] = copy;
                }
            }
        } else {
            for (uint8_t i = 0; i < count_; ++i)
                if (notes_[i].onset() >= newSteps) notes_[i].setShadow(true);
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
    // plan whose step 0 is fromStep. Held notes are not played (the person is
    // holding them live). Two notes of one pitch never overlap (the later
    // onset cuts the earlier note). LENGTH scales the plan only.
    //
    // A plan holds ResponsePlan::kCapacity notes. When the rest of the pass
    // has more, the plan ends early and *nextStep (0 = the plan reaches the
    // pass end) is where the next one starts: the latest step that keeps the
    // plan within capacity and that no note sounds across; without such a
    // step, the latest one that fits, the notes across it end there and the
    // next plan (carry = true) starts them again at its step 0 with the rest
    // of their length. Without carry (resume mid-pass) only onsets >= fromStep
    // play, as before.
    uint8_t buildPlan(ResponsePlan& plan, uint8_t passSteps, AnswerLength length,
                      uint8_t fromStep = 0, uint8_t muteMask = 0, bool carry = false,
                      uint8_t* nextStep = nullptr) const {
        plan = ResponsePlan{};
        if (nextStep) *nextStep = 0;
        if (passSteps == 0 || passSteps > kMaxLoopSteps || fromStep >= passSteps) return 0;
        const uint8_t percent = GroovePuterCallResponse::answerLengthPercent(length);
        constexpr uint8_t cap = ResponsePlan::kCapacity;

        const uint64_t play = playableMask(passSteps, muteMask);
        uint8_t carried = 0, total = 0, firstOnset = passSteps;
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const uint8_t onset = notes_[i].onset();
            if (onset >= fromStep) {
                ++total;
                if (onset < firstOnset) firstOnset = onset;
            } else if (carry && onset + playedLength(i, passSteps, muteMask, percent) > fromStep) {
                ++carried;
            }
        }
        uint8_t end = passSteps;
        if (carried + total > cap)
            end = segmentEnd(play, passSteps, fromStep, muteMask, percent, carry, carried, firstOnset);

        plan.windowSteps = static_cast<uint8_t>(end - fromStep);
        for (uint8_t i = 0; i < count_ && plan.count < cap; ++i) {
            if (!inMask(play, i)) continue;
            const LoopNote& n = notes_[i];
            const uint8_t onset = n.onset();
            if (onset >= end) continue;
            const int32_t stop = static_cast<int32_t>(onset) +
                                 playedLength(i, passSteps, muteMask, percent);
            const int32_t start = onset >= fromStep ? onset : fromStep;
            if (onset < fromStep && (!carry || stop <= fromStep)) continue;
            const int32_t len = (stop < end ? stop : end) - start;
            plan.notes[plan.count++] = ResponseNote{static_cast<uint8_t>(start - fromStep),
                                                    static_cast<uint8_t>(len), n.note(),
                                                    n.velocity()};
        }
        if (end < passSteps && nextStep) *nextStep = end;
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
    static constexpr uint8_t kOpenFree = 0xFFu;
    static constexpr uint8_t kOpenPickup = 0x80u;
    static constexpr uint8_t kOpenIndexMask = 0x3Fu;

    // 3 bytes: tag (uint16 split to avoid padding) + pool index (bit 7: pickup
    // = a note of the next pass, played before its downbeat; 0xFF = free).
    struct OpenNote {
        uint8_t tagLo{0};
        uint8_t tagHi{0};
        uint8_t index{kOpenFree};
        uint16_t tag() const { return static_cast<uint16_t>(tagLo | (tagHi << 8)); }
    };

    // A note that sounds in a plan: visible, closed, layer not muted, inside
    // the pass; a second note of one step and pitch (never stored, defensive)
    // does not play.
    bool sounds(uint8_t i, uint8_t passSteps, uint8_t muteMask) const {
        const LoopNote& n = notes_[i];
        return !n.shadow() && !n.held() && (muteMask & (1u << n.layer())) == 0 &&
               n.onset() < passSteps;
    }
    static bool inMask(uint64_t mask, uint8_t i) { return ((mask >> i) & 1u) != 0; }
    // Bit i: pool note i plays (sounds, and is the first of its step + pitch).
    uint64_t playableMask(uint8_t passSteps, uint8_t muteMask) const {
        uint64_t mask = 0;
        for (uint8_t i = 0; i < count_; ++i) {
            if (!sounds(i, passSteps, muteMask)) continue;
            bool first = true;
            for (uint8_t j = 0; j < i && first; ++j)
                if (inMask(mask, j) && notes_[j].onset() == notes_[i].onset() &&
                    notes_[j].note() == notes_[i].note()) first = false;
            if (first) mask |= uint64_t{1} << i;
        }
        return mask;
    }

    // Played length of a playable note: to the pass end at most, cut at the
    // next onset of its pitch, then LENGTH (never shorter than one step).
    uint8_t playedLength(uint8_t i, uint8_t passSteps, uint8_t muteMask, uint8_t percent) const {
        const LoopNote& n = notes_[i];
        const uint8_t onset = n.onset();
        const uint8_t room = static_cast<uint8_t>(passSteps - onset);
        uint8_t len = n.length() == LoopNote::kLengthToEnd || n.length() > room ? room : n.length();
        for (uint8_t j = 0; j < count_; ++j) {
            if (j == i || notes_[j].note() != n.note() || !sounds(j, passSteps, muteMask)) continue;
            const uint8_t other = notes_[j].onset();
            if (other > onset && other < onset + len) len = static_cast<uint8_t>(other - onset);
        }
        return GroovePuterCallResponse::scaledAnswerNoteLength(len, percent);
    }

    // End of a plan from fromStep that cannot hold the rest of the pass.
    uint8_t segmentEnd(uint64_t play, uint8_t passSteps, uint8_t fromStep, uint8_t muteMask, uint8_t percent,
                       bool carry, uint8_t carried, uint8_t firstOnset) const {
        constexpr uint8_t cap = ResponsePlan::kCapacity;
        // The latest end that fits: the onset of the first note past capacity.
        uint8_t maxEnd = 0;
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i) || notes_[i].onset() < fromStep) continue;
            const uint8_t onset = notes_[i].onset();
            uint8_t before = 0, upTo = 0;
            for (uint8_t j = 0; j < count_; ++j) {
                if (!inMask(play, j)) continue;
                const uint8_t other = notes_[j].onset();
                if (other < fromStep || other > onset) continue;
                ++upTo;
                if (other < onset) ++before;
            }
            if (carried + before <= cap && carried + upTo > cap && (maxEnd == 0 || onset < maxEnd))
                maxEnd = onset;
        }
        if (maxEnd <= fromStep) return static_cast<uint8_t>(fromStep + 1);  // defensive: truncated
        // Steps some note sounds across (strictly inside the note).
        uint32_t across[4]{};
        for (uint8_t i = 0; i < count_; ++i) {
            if (!inMask(play, i)) continue;
            const uint8_t onset = notes_[i].onset();
            const uint16_t stop = static_cast<uint16_t>(onset + playedLength(i, passSteps, muteMask, percent));
            uint16_t t = onset;
            if (onset < fromStep) {
                if (!carry || stop <= fromStep) continue;
                t = fromStep;
            }
            for (++t; t < stop && t < kMaxLoopSteps; ++t) across[t >> 5] |= uint32_t{1} << (t & 31u);
        }
        for (uint8_t t = maxEnd; t > fromStep; --t) {
            if (carried == 0 && t <= firstOnset) break;  // the plan would be empty
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
        for (uint8_t e = 0; e < kOpenCapacity; ++e)
            if (open_[e].index != kOpenFree && (open_[e].index & kOpenPickup) &&
                (open_[e].index & kOpenIndexMask) == index) return true;
        return false;
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
        o.index = static_cast<uint8_t>(index | (pickup ? kOpenPickup : 0u));
    }

    void closeEntry(uint8_t e, double rel, uint8_t passSteps) {
        OpenNote& o = open_[e];
        if (o.index == kOpenFree) return;
        const bool pickup = (o.index & kOpenPickup) != 0;
        LoopNote& n = notes_[o.index & kOpenIndexMask];
        o.index = kOpenFree;
        const int32_t base = pickup ? passSteps : 0;
        const int32_t off = static_cast<int32_t>(std::floor(rel + 0.5)) - base;
        int32_t length = off - static_cast<int32_t>(n.onset());
        const int32_t maxLength = static_cast<int32_t>(passSteps) - n.onset();
        if (length > maxLength) length = maxLength;
        if (length < 1) length = 1;
        n.setLength(static_cast<uint8_t>(length));
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
            else if (at > index) o.index = static_cast<uint8_t>(o.index - 1u);
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
            if (keepOpen && isOpenIndex(static_cast<uint8_t>(i))) continue;
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
