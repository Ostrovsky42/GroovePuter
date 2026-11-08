#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_RESPONDER_H
#define GROOVEPUTER_CALL_RESPONSE_RESPONDER_H

#include <cstdint>

#include "call_response_planner.h"
#include "musical_event_router.h"

// CALL/RESPONSE phase 1: plays one ResponsePlan through the existing router.
//
// Ownership: the responder keeps its own fixed 16-note table (not
// PerformanceKeyboard::scheduled_), so cancelling the answer never touches the
// arpeggiator or any other pending PERFORM output, and the keyboard's own
// clearScheduled()/stopGeneratedOutput() can never drop an answer NoteOff.
//
// Output: source Arpeggiator (an existing generated-performance source, no new
// output owner), target Dx by default. Dx is external-only: InternalSynthOutput
// ignores it, UsbMidiOutput sends it on the PERFORM DX MIDI route (USB + DIN via
// the tee transport). Main loop only; no allocation, no locks.
//
// Phase 3: an explicit answer channel (1..16, PARTNER CHAN) sends the notes with
// kMusicalEventWireChannel and channel = wire channel, so UsbMidiOutput plays
// them on its own answer lane on that channel instead of the DX route. The
// channel is latched per answer: every NoteOff (also on cancel) goes to the
// channel of its NoteOn even if the setting changes meanwhile.
namespace GroovePuterCallResponse {

inline uint32_t answerStepDueMicros(uint32_t originMicros, uint32_t sixteenthMicros,
                                    uint32_t step) {
    return originMicros + step * sixteenthMicros;
}

class ResponsePlayer {
public:
    void setRouter(MusicalEventRouter* router) { router_ = router; }
    void setTarget(MusicalEventTarget target) {
        if (target == target_) return;
        (void)cancel();
        target_ = target;
    }
    MusicalEventTarget target() const { return target_; }

    // 0 = follow the PERFORM DX route (phase 1/2 output, default), 1..16 = MIDI
    // channel of the answer lane. Takes effect at the next answer.
    static constexpr uint8_t kFollowDx = 0;
    void setWireChannel(uint8_t channel1Based) {
        wireChannel_ = channel1Based > 16 ? 16 : channel1Based;
    }
    uint8_t wireChannel() const { return wireChannel_; }
    uint8_t activeWireChannel() const { return activeWireChannel_; }

    // originMicros = time of answer step 0. Replaces (cancels) a running answer.
    bool start(const ResponsePlan& plan, uint32_t originMicros, uint32_t sixteenthMicros) {
        (void)cancel();
        if (plan.count == 0 || sixteenthMicros == 0) return false;
        for (uint8_t i = 0; i < plan.count && i < ResponsePlan::kCapacity; ++i) {
            const ResponseNote& note = plan.notes[i];
            Slot& slot = slots_[i];
            slot.onMicros = answerStepDueMicros(originMicros, sixteenthMicros, note.step);
            slot.offMicros = answerStepDueMicros(originMicros, sixteenthMicros,
                                                 static_cast<uint32_t>(note.step) + note.length);
            slot.note = note.note;
            slot.velocity = note.velocity < 1 ? 1 : note.velocity;
            slot.state = State::Pending;
        }
        count_ = plan.count;
        activeWireChannel_ = wireChannel_;
        playedThisAnswer_ = 0;
        ++answersStarted_;
        return true;
    }

    // Main loop, once per pass. Sends due NoteOn/NoteOff in time order per note.
    void service(uint32_t nowMicros) {
        if (count_ == 0) return;
        bool anyLeft = false;
        for (uint8_t i = 0; i < count_; ++i) {
            Slot& slot = slots_[i];
            if (slot.state == State::Pending && due(nowMicros, slot.onMicros)) {
                if (due(nowMicros, slot.offMicros)) {
                    slot.state = State::Done;  // too late to have any length: skip
                    ++staleDrops_;
                } else {
                    emit(MusicalEventType::NoteOn, slot.note, slot.velocity);
                    slot.state = State::Sounding;
                    ++playedThisAnswer_;
                }
            }
            if (slot.state == State::Sounding && due(nowMicros, slot.offMicros)) {
                emit(MusicalEventType::NoteOff, slot.note, 0);
                slot.state = State::Done;
            }
            if (slot.state != State::Done) anyLeft = true;
        }
        if (!anyLeft) count_ = 0;
    }

    // Drops pending NoteOns and releases sounding notes now. Returns the number
    // of NoteOffs sent. Idempotent.
    uint8_t cancel() {
        if (count_ == 0) return 0;
        uint8_t released = 0;
        bool hadPending = false;
        for (uint8_t i = 0; i < count_; ++i) {
            Slot& slot = slots_[i];
            if (slot.state == State::Sounding) {
                emit(MusicalEventType::NoteOff, slot.note, 0);
                ++released;
            } else if (slot.state == State::Pending) {
                hadPending = true;
            }
            slot.state = State::Done;
        }
        count_ = 0;
        if (hadPending || released > 0) {
            ++answersCancelled_;
            lastCancelPlayed_ = playedThisAnswer_;
        }
        return released;
    }

    bool active() const { return count_ != 0; }
    uint8_t soundingCount() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < count_; ++i) if (slots_[i].state == State::Sounding) ++n;
        return n;
    }
    uint32_t answersStarted() const { return answersStarted_; }
    uint32_t answersCancelled() const { return answersCancelled_; }
    uint8_t lastCancelPlayed() const { return lastCancelPlayed_; }
    uint32_t staleDrops() const { return staleDrops_; }

private:
    enum class State : uint8_t { Done = 0, Pending, Sounding };
    struct Slot {
        uint32_t onMicros{0};
        uint32_t offMicros{0};
        uint8_t note{0};
        uint8_t velocity{0};
        State state{State::Done};
    };

    static bool due(uint32_t now, uint32_t at) {
        return static_cast<int32_t>(now - at) >= 0;
    }

    void emit(MusicalEventType type, uint8_t note, uint8_t velocity) {
        if (!router_) return;
        const bool lane = activeWireChannel_ != kFollowDx;
        router_->route(MusicalEvent{type, MusicalEventSource::Arpeggiator, target_,
                                    static_cast<uint8_t>(lane ? activeWireChannel_ - 1u : 0u),
                                    note, velocity,
                                    lane ? kMusicalEventWireChannel : kMusicalEventAnswerFollowDx});
    }

    MusicalEventRouter* router_{nullptr};
    Slot slots_[ResponsePlan::kCapacity]{};
    uint32_t answersStarted_{0};
    uint32_t answersCancelled_{0};
    uint32_t staleDrops_{0};
    uint8_t count_{0};
    uint8_t playedThisAnswer_{0};
    uint8_t lastCancelPlayed_{0};
    uint8_t wireChannel_{kFollowDx};
    uint8_t activeWireChannel_{kFollowDx};
    // Non-zero default: the owning singleton is in .dram0.data instead of .bss
    // (same DRAM, a few hundred bytes of flash for the initial image).
    MusicalEventTarget target_{MusicalEventTarget::Dx};
};

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_RESPONDER_H
