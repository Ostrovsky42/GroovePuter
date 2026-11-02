#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_PHRASE_CAPTURE_H
#define GROOVEPUTER_CALL_RESPONSE_PHRASE_CAPTURE_H

#include <cmath>
#include <cstddef>
#include <cstdint>

// CALL/RESPONSE phase 0: capture of the notes a person plays into fixed
// two-bar windows. Fixed storage, no allocation, main-loop only.
//
// Positions are in sixteenth steps from the start of the clock epoch (the
// caller owns the clock: project transport while PLAY runs, otherwise the
// PERFORM free clock). Windows are aligned to multiples of kWindowSteps.
//
// Quantization contract:
//   - an onset rounds to the nearest sixteenth;
//   - pickup: a note in the last kPickupFraction of the step before a window
//     boundary belongs to the next window (onset 0); a note earlier than that
//     stays in the current window (clamped to its last step);
//   - several notes on the same quantized onset are a chord: the top note wins;
//   - length = quantized NoteOff - onset (min 1), clamped to the window end;
//     a note still held when the window closes is clamped the same way, its
//     later NoteOff is ignored (no state is left behind);
//   - more than kCapacity pending notes: the new note is dropped and counted.
namespace GroovePuterCallResponse {

struct CapturedNote {
    uint8_t onset{0};     // 0..kWindowSteps-1, sixteenths from the window start
    uint8_t length{0};    // 1..kWindowSteps, sixteenths, clamped to the window
    uint8_t note{0};      // input MIDI pitch (what the person pressed)
    uint8_t velocity{0};  // input velocity
};

class PhraseCapture {
public:
    static constexpr uint8_t kCapacity = 16;
    static constexpr int32_t kStepsPerBar = 16;
    // Default and maximum window (2 bars). The PARTNER BARS setting may make
    // the window 1 bar (setWindowBars); every buffer stays sized for 2 bars.
    static constexpr int32_t kBarsPerWindow = 2;
    static constexpr int32_t kWindowSteps = kStepsPerBar * kBarsPerWindow;
    static constexpr double kPickupFraction = 0.3;

    enum class CloseResult : uint8_t { None = 0, Empty, Phrase };

    struct Phrase {
        int32_t windowStartStep{0};  // epoch step of onset 0
        uint8_t count{0};
        uint8_t dropped{0};          // notes dropped (overflow) since the previous close
        uint8_t windowSteps{static_cast<uint8_t>(kWindowSteps)};  // length of this window
        CapturedNote notes[kCapacity]{};

        int32_t firstBar() const { return windowStartStep / kStepsPerBar; }
    };

    // 1 or 2 bars (anything else = 2). A new length is a new grid: pending
    // notes are discarded (the caller treats it as a new epoch).
    void setWindowBars(uint8_t bars) {
        const uint8_t steps = static_cast<uint8_t>((bars == 1 ? 1 : 2) * kStepsPerBar);
        if (steps == windowSteps_) return;
        windowSteps_ = steps;
        reset();
    }
    int32_t windowSteps() const { return windowSteps_; }

    void reset() {
        count_ = 0;
        windowValid_ = false;
        windowStart_ = 0;
        droppedSinceClose_ = 0;
    }

    // Returns false when the note was not stored (overflow, or a lower chord note).
    bool noteOn(uint16_t tag, uint8_t note, uint8_t velocity, double posSteps) {
        if (!std::isfinite(posSteps) || posSteps < 0.0) return false;
        ensureWindow(posSteps);
        int32_t onset = quantizeOnset(posSteps, windowSteps_);
        if (onset < windowStart_) onset = windowStart_;

        // The same source attacking again closes its previous note first.
        const int open = findOpen(tag);
        if (open >= 0) closeAt(static_cast<std::size_t>(open), onset);

        for (uint8_t i = 0; i < count_; ++i) {
            Event& existing = events_[i];
            if (existing.onset != onset) continue;
            if (note <= existing.note) return false;  // chord: keep the top note
            existing.note = note;
            existing.velocity = velocity;
            existing.tag = tag;
            existing.open = true;
            existing.offStep = onset;
            return true;
        }
        if (count_ >= kCapacity) {
            ++overflowCount_;
            if (droppedSinceClose_ < 255u) ++droppedSinceClose_;
            return false;
        }
        Event& slot = events_[count_++];
        slot.onset = onset;
        slot.offStep = onset;
        slot.tag = tag;
        slot.note = note;
        slot.velocity = velocity;
        slot.open = true;
        return true;
    }

    void noteOff(uint16_t tag, double posSteps) {
        if (!std::isfinite(posSteps)) return;
        const int open = findOpen(tag);
        if (open < 0) return;
        closeAt(static_cast<std::size_t>(open),
                static_cast<int32_t>(std::floor(posSteps + 0.5)));
    }

    // Closes at most one window per call once posSteps reached its end.
    CloseResult closeDueWindow(double posSteps, Phrase& out) {
        if (!std::isfinite(posSteps) || posSteps < 0.0) return CloseResult::None;
        if (!windowValid_) {
            ensureWindow(posSteps);
            return CloseResult::None;
        }
        const int32_t windowEnd = windowStart_ + windowSteps_;
        if (posSteps < static_cast<double>(windowEnd)) return CloseResult::None;

        out = Phrase{};
        out.windowStartStep = windowStart_;
        out.windowSteps = windowSteps_;
        out.dropped = droppedSinceClose_;
        droppedSinceClose_ = 0;

        uint8_t keep = 0;
        for (uint8_t i = 0; i < count_; ++i) {
            const Event& event = events_[i];
            if (event.onset >= windowEnd) {
                events_[keep++] = event;  // pickup for a later window
                continue;
            }
            if (event.onset < windowStart_) continue;  // stale (cannot happen; defensive)
            int32_t end = event.open ? windowEnd : event.offStep;
            if (end > windowEnd) end = windowEnd;
            int32_t length = end - event.onset;
            if (length < 1) length = 1;
            insertSorted(out, CapturedNote{
                static_cast<uint8_t>(event.onset - windowStart_),
                static_cast<uint8_t>(length), event.note, event.velocity});
        }
        count_ = keep;

        // A long gap (stalled loop) skips silent windows instead of closing them one by one.
        int32_t nextStart = windowEnd;
        const int32_t current = alignedWindowStart(posSteps, windowSteps_);
        if (current > nextStart) nextStart = current;
        windowStart_ = nextStart;
        keep = 0;
        for (uint8_t i = 0; i < count_; ++i) {
            if (events_[i].onset >= windowStart_) events_[keep++] = events_[i];
        }
        count_ = keep;

        return out.count > 0 ? CloseResult::Phrase : CloseResult::Empty;
    }

    void discardPending() { count_ = 0; droppedSinceClose_ = 0; }
    uint8_t pendingCount() const { return count_; }
    bool hasOpenNotes() const {
        for (uint8_t i = 0; i < count_; ++i) if (events_[i].open) return true;
        return false;
    }
    uint32_t overflowCount() const { return overflowCount_; }
    bool windowValid() const { return windowValid_; }
    int32_t windowStartStep() const { return windowStart_; }

    static int32_t alignedWindowStart(double posSteps, int32_t windowSteps = kWindowSteps) {
        return static_cast<int32_t>(std::floor(posSteps / windowSteps)) * windowSteps;
    }

    static int32_t quantizeOnset(double posSteps, int32_t windowSteps = kWindowSteps) {
        int32_t onset = static_cast<int32_t>(std::floor(posSteps + 0.5));
        const int32_t boundary = alignedWindowStart(posSteps, windowSteps) + windowSteps;
        if (onset >= boundary &&
            posSteps < static_cast<double>(boundary) - kPickupFraction) {
            onset = boundary - 1;  // early, but not a pickup: stays in this window
        }
        return onset;
    }

private:
    struct Event {
        int32_t onset{0};
        int32_t offStep{0};
        uint16_t tag{0};
        uint8_t note{0};
        uint8_t velocity{0};
        bool open{false};
    };

    void ensureWindow(double posSteps) {
        if (windowValid_) return;
        windowValid_ = true;
        windowStart_ = alignedWindowStart(posSteps, windowSteps_);
    }

    int findOpen(uint16_t tag) const {
        for (uint8_t i = 0; i < count_; ++i) {
            if (events_[i].open && events_[i].tag == tag) return i;
        }
        return -1;
    }

    void closeAt(std::size_t index, int32_t offStep) {
        Event& event = events_[index];
        event.open = false;
        event.offStep = offStep > event.onset ? offStep : event.onset + 1;
    }

    static void insertSorted(Phrase& out, const CapturedNote& note) {
        if (out.count >= kCapacity) return;
        uint8_t at = out.count;
        while (at > 0 && out.notes[at - 1].onset > note.onset) {
            out.notes[at] = out.notes[at - 1];
            --at;
        }
        out.notes[at] = note;
        ++out.count;
    }

    Event events_[kCapacity]{};
    uint8_t count_{0};
    bool windowValid_{false};
    uint8_t droppedSinceClose_{0};
    uint8_t windowSteps_{static_cast<uint8_t>(kWindowSteps)};
    int32_t windowStart_{0};
    uint32_t overflowCount_{0};
};

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_PHRASE_CAPTURE_H
