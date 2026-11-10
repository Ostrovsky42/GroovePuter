#pragma once
#ifndef GROOVEPUTER_LOOPER_H
#define GROOVEPUTER_LOOPER_H

#include <cmath>
#include <cstdint>

#include "call_response_listener.h"
#include "call_response_responder.h"
#include "looper_clock.h"
#include "looper_layer.h"
#include "performance_input_observer.h"

// LOOPER phase 1 (docs/design/LOOPER_DESIGN.md, LOOPER_PLAN.md): one layer,
// overdub, one undo level, HOLD, MIDI output only.
//
// A separate object, not a PARTNER mode. It borrows the PARTNER's
// ResponsePlayer (callResponseListener().responder()) for playback; the
// PARTNER is hidden and Off, the two never run together (Z/C refuse while the
// PARTNER is enabled). Nothing in call_response_*.h changes.
//
// States: IDLE, ARMED, RECORD, PLAY, OVERDUB, STOPPED; HOLD is a flag over
// PLAY/STOPPED (armed during RECORD). The layer is only ever erased by CLEAR
// (and CLEAR is undone by V); clocks never touch it.
//
// Clock: project PLAY when it runs (a pass starts on a project bar, the bar
// phase is kept across Space stop/start), otherwise the PERFORM free clock,
// started by the first note in ARMED (that note is step 0) and stopped when
// the loop stops. Space stop silences the loop at once (NoteOff now); the next
// Space start resumes it on the project bar grid.
//
// Two layers (phase 3) on one clock and one pass: M picks the active layer;
// REC/DUB, CLEAR and HOLD act on it, undo on the layer of the last unit. A
// write never touches the other layer. HOLD is per layer (a locked layer 1
// leaves layer 2 free to record and clear); MUTE is per layer and heard at
// once. BARS changes keep the notes: longer repeats the loop, shorter cuts
// it, V takes it back.
//
// Main loop only: the observer callback comes from PerformanceKeyboard on the
// main loop, service() once per loop pass. No allocation, no locks, no FS.
namespace GroovePuterLooper {

using GroovePuterCallResponse::ResponsePlayer;

class Looper final : public PerformanceInputObserver {
public:
    enum class State : uint8_t { Idle = 0, Armed, Record, Play, Overdub, Stopped };
    enum class Result : uint8_t {
        Done = 0,
        Ignored,        // nothing to do in this state
        HoldLocked,     // HOLD is on: B to unlock
        PartnerActive,  // the PARTNER owns the player
        Empty,          // no loop
        NothingToUndo,
        NothingToHold,
        Recording,      // BARS / layer: not while the first pass records
        NoRoom,         // BARS: the repeated loop would not fit in the pool
    };
    using NowFn = uint32_t (*)();
    static constexpr uint8_t kChannelFollowDx = ResponsePlayer::kFollowDx;

    void setNowFunction(NowFn fn) { nowFn_ = fn; }
    void setPlayer(ResponsePlayer* player) { player_ = player; }

    bool recordSound() const { return (layerFlags_ & 0x20u) != 0; }
    bool setRecordSound(bool sound) {
        if (state_ == State::Armed || state_ == State::Record || state_ == State::Overdub)
            return false;
        layerFlags_ = static_cast<uint8_t>(sound ? layerFlags_ | 0x20u : layerFlags_ & ~0x20u);
        soundMono_ = 0xff;
        return true;
    }

    // ---- settings (session only, never saved)
    uint8_t bars() const { return bars_; }
    uint8_t passSteps() const { return static_cast<uint8_t>(bars_ * kStepsPerBar); }
    // 1, 2, 4 or 8 (anything else = 2). Not while a layer is locked or the
    // first pass records. With notes: longer repeats the loop to the new
    // length, shorter cuts what lies past the new end; one undo unit (V), the
    // pass goes on from where it is.
    Result setBars(uint8_t bars) {
        if (bars != 1 && bars != 4 && bars != 8) bars = 2;
        if (holdMask() != 0) return Result::HoldLocked;
        if (state_ == State::Record) return Result::Recording;
        if (bars == bars_) return Result::Ignored;
        if (loopEmpty()) {
            layer_.dropUndo();  // a hidden (CLEARed) loop cannot come back on another grid
            bars_ = bars;
            return Result::Done;
        }
        const uint8_t steps = static_cast<uint8_t>(bars * kStepsPerBar);
        if (!layer_.resizeFits(passSteps(), steps)) return Result::NoRoom;
        const uint32_t now = nowMicros();
        if (state_ == State::Overdub) layer_.closeOpenAt(relAt(now), passSteps());
        (void)layer_.resize(passSteps(), steps, bars_);
        applyBars(bars, now);
        return Result::Done;
    }
    Result cycleBars(int direction) {
        // 1 -> 2 -> 4 -> 8 -> 1
        if (direction >= 0) return setBars(bars_ == 1 ? 2 : bars_ == 2 ? 4 : bars_ == 4 ? 8 : 1);
        return setBars(bars_ == 8 ? 4 : bars_ == 4 ? 2 : bars_ == 2 ? 1 : 8);
    }
    // 0 = PERFORM DX route, 1..16 = answer lane channel. Latched per pass by
    // the player: a sounding pass keeps its channel for every NoteOff.
    uint8_t channel() const { return channel_; }
    void setChannel(uint8_t channel1Based) { channel_ = channel1Based > 16 ? 16 : channel1Based; }
    void cycleChannel(int direction) {
        setChannel(static_cast<uint8_t>((channel_ + (direction >= 0 ? 1 : 16)) % 17));
    }
    MusicalEventTarget target() const { return target_; }
    void setTarget(MusicalEventTarget target) {
        if (target == MusicalEventTarget::Drums || target == target_) return;
        target_ = target;
        if (loopRunning() && channel_ == kChannelFollowDx) restartPlanNow(nowMicros());
    }
    AnswerLength length() const { return length_; }
    uint8_t lengthPercent() const { return GroovePuterCallResponse::answerLengthPercent(length_); }
    void setLength(AnswerLength length) {
        length_ = static_cast<uint8_t>(length) < GroovePuterCallResponse::kAnswerLengthCount
            ? length : AnswerLength::Full;
    }
    void cycleLength(int direction) {
        const uint8_t count = GroovePuterCallResponse::kAnswerLengthCount;
        setLength(static_cast<AnswerLength>(
            (static_cast<uint8_t>(length_) + (direction >= 0 ? 1u : count - 1u)) % count));
    }

    // ---- layers (key M, PERFORM > INPUT LAYER / MUTE)
    uint8_t activeLayer() const { return (layerFlags_ & kLayerActive) != 0 ? 1u : 0u; }
    bool layerHold(uint8_t layer) const { return (layerFlags_ & (kLayerHold << (layer & 1u))) != 0; }
    bool layerMuted(uint8_t layer) const { return (layerFlags_ & (kLayerMute << (layer & 1u))) != 0; }
    uint8_t holdMask() const { return static_cast<uint8_t>(layerFlags_ & 0x03u); }
    uint8_t muteMask() const { return static_cast<uint8_t>((layerFlags_ >> 2) & 0x03u); }
    uint8_t layerNoteCount(uint8_t layer) const { return layer_.visibleCount(layer & 1u); }

    // M: the other layer becomes active. Not while the first pass records;
    // OVERDUB ends first (Z then dubs into the new layer).
    Result nextLayer() {
        if (state_ == State::Record) return Result::Recording;
        if (state_ == State::Overdub) punchOut(nowMicros());
        layerFlags_ = static_cast<uint8_t>(layerFlags_ ^ kLayerActive);
        return Result::Done;
    }
    // MUTE of the active layer, heard at once: the running pass goes on
    // without (or with) it from the current step.
    Result toggleMute() {
        layerFlags_ = static_cast<uint8_t>(layerFlags_ ^ (kLayerMute << activeLayer()));
        restartPlanNow(nowMicros());
        return Result::Done;
    }

    // ---- actions (keys Z / C / V / B / Del / X, page exit)

    // Z: IDLE->ARMED, ARMED->IDLE, RECORD: "then DUB" on/off, PLAY<->OVERDUB
    // at once, STOPPED->PLAY+OVERDUB at once.
    Result recDub() {
        if (hold()) return Result::HoldLocked;
        if (partnerActive()) return Result::PartnerActive;
        const uint32_t now = nowMicros();
        switch (state_) {
            case State::Idle:
                state_ = State::Armed;
                setFlag(kDubAfterRec | kHoldAfterRec, false);
                if (clock_.source() == LoopClock::Source::Transport)
                    passStart_ = nextBarStep(clock_.positionAt(now));
                return Result::Done;
            case State::Armed:
                state_ = State::Idle;
                return Result::Done;
            case State::Record:
                setFlag(kDubAfterRec, !flag(kDubAfterRec));
                return Result::Done;
            case State::Play:
                state_ = State::Overdub;
                layer_.beginUnit();
                return Result::Done;
            case State::Overdub:
                punchOut(now);
                return Result::Done;
            case State::Stopped:
                resumeNow(now);
                state_ = State::Overdub;
                layer_.beginUnit();
                return Result::Done;
        }
        return Result::Ignored;
    }

    // C: loop sound on/off. Stopping sends the NoteOffs now.
    Result playStop() {
        const uint32_t now = nowMicros();
        switch (state_) {
            case State::Idle: return Result::Empty;
            case State::Armed:
                state_ = State::Idle;
                return Result::Done;
            case State::Record:
                stopLoop(now, !loopEmpty() ? State::Stopped : State::Idle);
                return Result::Done;
            case State::Play:
            case State::Overdub:
                stopLoop(now, State::Stopped);
                return Result::Done;
            case State::Stopped:
                if (partnerActive()) return Result::PartnerActive;
                resumeNow(now);
                return Result::Done;
        }
        return Result::Ignored;
    }

    // V (and Ctrl+Z while canUndo()): the last unit. In RECORD: cancel the
    // recording. Undo of CLEAR brings the loop back as it was (playing or not).
    // Refused when the last unit touched a locked layer (a BARS change
    // touches every layer); the active layer's lock alone does not refuse it.
    Result undo() {
        const uint32_t now = nowMicros();
        if (state_ == State::Record) {
            if (hold()) return Result::HoldLocked;
            layer_.closeOpenAt(relAt(now), passSteps());
            if (!layer_.unitPending()) (void)layer_.undo();
            stopLoop(now, !loopEmpty() ? State::Stopped : State::Idle);
            return Result::Done;
        }
        if (!layer_.canUndo()) return hold() ? Result::HoldLocked : Result::NothingToUndo;
        if ((layer_.undoLayerMask() & holdMask()) != 0) return Result::HoldLocked;
        if (state_ == State::Overdub) layer_.closeOpenAt(relAt(now), passSteps());
        const bool wasEmpty = loopEmpty();
        const uint8_t barsBefore = layer_.undoBars();
        (void)layer_.undo();
        if (barsBefore != 0) applyBars(barsBefore, now);
        if (loopEmpty()) {
            stopLoop(now, state_ == State::Armed ? State::Armed : State::Idle);
        } else if (wasEmpty) {  // CLEAR undone
            if (flag(kClearedPlaying) && !partnerActive()) {
                resumeNow(now);
            } else {
                stopLoop(now, State::Stopped);
            }
        } else if (state_ == State::Overdub) {
            layer_.beginUnit();
        }
        // PLAY/OVERDUB: the change is heard from the next pass.
        return Result::Done;
    }

    // Del: every note of the active layer hidden (undo with V). The layer
    // goes silent at once; when it was the whole loop, the loop stops.
    Result clear() {
        if (hold()) return Result::HoldLocked;
        if (state_ == State::Record) return undo();  // cancel the recording
        const uint8_t active = activeLayer();
        const uint8_t notes = layer_.visibleCount(active);
        if (notes == 0) return Result::Empty;
        const uint32_t now = nowMicros();
        if (notes != layer_.visibleTotal()) {  // the other layer plays on
            if (state_ == State::Overdub) punchOut(now);
            (void)layer_.clear(active);
            restartPlanNow(now);
            return Result::Done;
        }
        setFlag(kClearedPlaying, state_ == State::Play || state_ == State::Overdub);
        stopLoop(now, State::Idle);
        (void)layer_.clear(active);
        setFlag(kResumeOnPlay, false);
        return Result::Done;
    }

    // B: HOLD of the active layer on/off. OVERDUB: punch-out now + HOLD.
    // RECORD: HOLD after the pass. An empty layer has nothing to hold.
    Result toggleHold() {
        if (hold()) {
            setHold(false);
            return Result::Done;
        }
        switch (state_) {
            case State::Idle:
            case State::Armed:
                return Result::NothingToHold;
            case State::Record:
                setFlag(kHoldAfterRec, !flag(kHoldAfterRec));
                return Result::Done;
            case State::Overdub:
                if (layer_.visibleCount(activeLayer()) == 0) return Result::NothingToHold;
                punchOut(nowMicros());
                setHold(true);
                return Result::Done;
            case State::Play:
            case State::Stopped:
                if (layer_.visibleCount(activeLayer()) == 0) return Result::NothingToHold;
                setHold(true);
                return Result::Done;
        }
        return Result::Ignored;
    }

    // X: loop stops now (NoteOffs sent), the layer is kept.
    Result panicStop() {
        const uint32_t now = nowMicros();
        if (player_) (void)player_->cancel();
        switch (state_) {
            case State::Armed: state_ = State::Idle; break;
            case State::Record:
                stopLoop(now, !loopEmpty() ? State::Stopped : State::Idle);
                break;
            case State::Play:
            case State::Overdub:
                stopLoop(now, State::Stopped);
                break;
            default: break;
        }
        setFlag(kResumeOnPlay, false);
        return Result::Done;
    }

    // Leaving PERFORM: recording ends (RECORD keeps what it has), ARMED is
    // dropped. Under PLAY the loop keeps sounding on any page; on the free
    // clock it stops (no invisible sound outside PERFORM).
    void onPageExit() {
        const uint32_t now = nowMicros();
        if (state_ == State::Armed) state_ = State::Idle;
        if (state_ == State::Record) {
            layer_.closeOpenAt(relAt(now), passSteps());
            if (loopEmpty()) {
                stopLoop(now, State::Idle);
            } else {
                if (flag(kHoldAfterRec)) setHold(true);
                setFlag(kDubAfterRec | kHoldAfterRec, false);
                state_ = State::Play;  // the next pass plays what was recorded
            }
        }
        if (state_ == State::Overdub) punchOut(now);
        if (state_ == State::Play && clock_.source() != LoopClock::Source::Transport) {
            stopLoop(now, State::Stopped);
            setFlag(kResumeOnPlay, false);
        }
    }

    // ---- input (PerformanceKeyboard observer; input pitch before Smart Play)
    void onPerformanceInputNote(bool on, uint16_t tag, uint8_t note, uint8_t velocity) override {
        if (recordSound()) return;
        recordInput(on, tag, note, velocity);
    }

    void onPerformanceOutput(const MusicalEvent& event) override {
        if (!recordSound() || event.target == MusicalEventTarget::Drums) return;
        if (event.type == MusicalEventType::AllNotesOff) {
            if (clock_.running()) layer_.closeOpenAt(relAt(nowMicros()), passSteps());
            soundMono_ = 0xff;
            return;
        }
        if (event.type != MusicalEventType::NoteOn && event.type != MusicalEventType::NoteOff) return;
        const bool on = event.type == MusicalEventType::NoteOn && event.velocity != 0;
        if (event.source == MusicalEventSource::PerformanceKeyboard) {
            if (on && soundMono_ != 0xff && clock_.running())
                (void)layer_.noteOff(static_cast<uint16_t>(0x4000u | soundMono_), relAt(nowMicros()), passSteps());
            if (on) soundMono_ = event.note;
            else if (soundMono_ == event.note) soundMono_ = 0xff;
        } else if (on && soundMono_ != 0xff) {
            if (clock_.running())
                (void)layer_.noteOff(static_cast<uint16_t>(0x4000u | soundMono_), relAt(nowMicros()), passSteps());
            soundMono_ = 0xff;
        }
        if (on && (state_ == State::Idle || state_ == State::Armed)) {
            target_ = event.target;
        }
        // PerformanceKeyboard collapses overlapping owners of the same sounding
        // pitch before emitting output; pitch therefore identifies its voice.
        recordInput(on,
                    static_cast<uint16_t>(0x4000u | event.note), event.note, event.velocity);
    }

    void recordInput(bool on, uint16_t tag, uint8_t note, uint8_t velocity) {
        const uint32_t now = nowMicros();
        if (!on) {
            if (clock_.running()) (void)layer_.noteOff(tag, relAt(now), passSteps());
            return;
        }
        if (state_ == State::Armed) {
            if (!clock_.running()) {
                // Free clock: the first note is bar 1, step 0.
                clock_.startFree(now, 0.0);
                beginRecord(0, now);
            } else {
                const double rel = clock_.positionAt(now) - passStart_;
                if (rel < -PhraseCapture::kPickupFraction) return;  // before the bar: live only
                beginRecord(passStart_, clock_.microsAtStep(passStart_));
            }
        }
        if (state_ != State::Record && state_ != State::Overdub) return;
        const LoopLayer::Write result =
            layer_.noteOn(tag, activeLayer(), note, velocity, relAt(now), passSteps());
        if (result == LoopLayer::Write::Full && !flag(kFullShown)) setFlag(kFullNotice | kFullShown, true);
    }

    // ---- main loop, once per pass. Returns true when a new pass began.
    bool service(uint32_t now, const TransportView* transport, float performBpm) {
        if (player_) player_->service(now);
        const double before = clock_.running() ? positionNow(now) : 0.0;
        const LoopClock::Event event = clock_.update(now, transport, performBpm);
        if (event == LoopClock::Event::TransportStopped) onTransportStop(now, before);
        else if (event == LoopClock::Event::TransportStarted) onTransportStart(now, before);
        if (!clock_.running()) return false;
        const double position = positionNow(now);
        if (state_ == State::Armed) {
            if (clock_.source() == LoopClock::Source::Transport && position >= passStart_)
                beginRecord(passStart_, clock_.microsAtStep(passStart_));
            return false;
        }
        if (!loopRunning()) return false;
        if (position < static_cast<double>(passStart_ + passSteps())) {
            // A pass of more than 32 notes: its next plan starts on its step.
            if (nextSegment_ != 0 && state_ != State::Record &&
                position >= static_cast<double>(passStart_ + nextSegment_))
                startSegment(now, position);
            return false;
        }
        advancePass(now, position);
        return true;
    }

    // ---- read-only view (UI, log)
    State state() const { return state_; }
    const char* stateName() const {
        switch (state_) {
            case State::Idle: return "IDLE";
            case State::Armed: return "ARM";
            case State::Record: return "REC";
            case State::Play: return "PLAY";
            case State::Overdub: return "DUB";
            case State::Stopped: return "STOP";
        }
        return "IDLE";
    }
    bool hold() const { return layerHold(activeLayer()); }  // the active layer is locked
    bool holdArmed() const { return flag(kHoldAfterRec); }
    bool dubArmed() const { return flag(kDubAfterRec); }
    bool canUndo() const { return layer_.canUndo(); }
    bool empty() const { return loopEmpty(); }  // no note in any layer
    uint8_t noteCount() const { return layer_.visibleTotal(); }
    bool full() const { return layer_.full(); }
    const LoopLayer& layer() const { return layer_; }
    LoopClock::Source clockSource() const { return clock_.source(); }
    uint8_t passCount() const { return passCount_; }
    uint32_t passDownbeatMicros() const { return gridMicros_; }
    // Steps since the current pass downbeat (0..passSteps), -1 when no pass runs.
    double passPosition(uint32_t now) const {
        if (!loopRunning() || !clock_.running()) return -1.0;
        double rel = positionNow(now) - passStart_;
        if (rel < 0.0) rel = 0.0;
        if (rel > passSteps()) rel = passSteps();
        return rel;
    }
    double passPositionNow() const { return passPosition(nowMicros()); }
    // ARMED under PLAY: steps until the recording starts (-1 otherwise).
    double stepsToRecord() const {
        if (state_ != State::Armed || clock_.source() != LoopClock::Source::Transport) return -1.0;
        const double left = passStart_ - clock_.positionAt(nowMicros());
        return left > 0.0 ? left : 0.0;
    }
    // Once per pass: notes were dropped because the layer is full.
    bool takeFullNotice() {
        if (!flag(kFullNotice)) return false;
        setFlag(kFullNotice, false);
        return true;
    }
    static bool partnerActive() {
        return GroovePuterCallResponse::callResponseListener().enabled();
    }

private:
    // layerFlags_: bit l HOLD of layer l, bit 2+l MUTE, bit 4 active layer, bit 5 SOUND capture.
    static constexpr uint8_t kLayerHold = 0x01u;
    static constexpr uint8_t kLayerMute = 0x04u;
    static constexpr uint8_t kLayerActive = 0x10u;
    static constexpr uint8_t kHoldAfterRec = 0x02u;
    static constexpr uint8_t kDubAfterRec = 0x04u;
    static constexpr uint8_t kResumeOnPlay = 0x08u;   // stopped by Space: Space resumes
    static constexpr uint8_t kClearedPlaying = 0x10u; // CLEAR hit a sounding loop
    static constexpr uint8_t kGridValid = 0x20u;      // gridMicros_ = downbeat of passStart_
    static constexpr uint8_t kFullNotice = 0x40u;
    static constexpr uint8_t kFullShown = 0x80u;      // notice given in this pass

    bool flag(uint8_t mask) const { return (flags_ & mask) != 0; }
    void setFlag(uint8_t mask, bool on) {
        flags_ = static_cast<uint8_t>(on ? (flags_ | mask) : (flags_ & ~mask));
    }
    uint32_t nowMicros() const { return nowFn_ ? nowFn_() : gridMicros_; }
    bool loopEmpty() const { return layer_.visibleTotal() == 0; }
    void setHold(bool on) {
        const uint8_t bit = static_cast<uint8_t>(kLayerHold << activeLayer());
        layerFlags_ = static_cast<uint8_t>(on ? (layerFlags_ | bit) : (layerFlags_ & ~bit));
    }
    bool loopRunning() const {
        return state_ == State::Record || state_ == State::Play || state_ == State::Overdub;
    }

    // Under PLAY a running pass follows its own grid (downbeat + elapsed) while
    // it agrees with the block-anchored estimate within half a step: the
    // estimate jitters by an audio block (PARTNER phase 7, closePosition).
    double positionNow(uint32_t now) const {
        const double position = clock_.positionAt(now);
        if (!loopRunning() || !flag(kGridValid) ||
            clock_.source() != LoopClock::Source::Transport || clock_.sixteenthMicros() == 0)
            return position;
        const double since = static_cast<double>(static_cast<int32_t>(now - gridMicros_));
        const double chained = static_cast<double>(passStart_) + since / clock_.sixteenthMicros();
        const double diff = chained - position;
        return diff <= 0.5 && diff >= -0.5 ? chained : position;
    }
    double relAt(uint32_t now) const {
        return clock_.running() ? positionNow(now) - passStart_ : 0.0;
    }

    static int32_t nextBarStep(double position) {
        return static_cast<int32_t>(std::ceil(position / kStepsPerBar - 1e-9)) * kStepsPerBar;
    }
    // Pass start containing position on the project grid, keeping the bar
    // phase the loop was recorded on.
    int32_t alignedPassStart(double position) const {
        const int32_t steps = passSteps();
        const int32_t offset = (static_cast<int32_t>(phaseBars_) * kStepsPerBar) % steps;
        return static_cast<int32_t>(std::floor((position - offset) / steps)) * steps + offset;
    }

    void beginRecord(int32_t passStart, uint32_t downbeatMicros) {
        state_ = State::Record;
        passStart_ = passStart;
        gridMicros_ = downbeatMicros;
        setFlag(kGridValid, true);
        setFlag(kFullShown, false);
        const int32_t bar = static_cast<int32_t>(std::floor(static_cast<double>(passStart) / kStepsPerBar));
        phaseBars_ = clock_.source() == LoopClock::Source::Transport
            ? static_cast<uint8_t>(((bar % bars_) + bars_) % bars_) : 0u;
        nextSegment_ = 0;
        layer_.beginUnit();
        ++passCount_;
    }

    void advancePass(uint32_t now, double position) {
        const int32_t steps = passSteps();
        int32_t next = passStart_ + steps;
        bool skipped = false;
        if (position >= static_cast<double>(next + steps)) {  // stalled loop: skip silent passes
            next += steps * static_cast<int32_t>(std::floor((position - next) / steps));
            skipped = true;
        }
        uint32_t grid = clock_.microsAtStep(static_cast<double>(next));
        if (flag(kGridValid) && !skipped) {
            const uint32_t chained = gridMicros_ + static_cast<uint32_t>(steps) * clock_.sixteenthMicros();
            const int32_t diff = static_cast<int32_t>(chained - grid);
            const int32_t limit = static_cast<int32_t>(clock_.sixteenthMicros() / 2u);
            if (diff <= limit && diff >= -limit) grid = chained;
        }
        State nextState = state_;
        if (state_ == State::Record && !loopEmpty()) {
            if (flag(kHoldAfterRec)) {
                setHold(true);
                nextState = State::Play;
            } else {
                nextState = flag(kDubAfterRec) ? State::Overdub : State::Play;
            }
            setFlag(kDubAfterRec | kHoldAfterRec, false);
        }
        layer_.closePass(passSteps(), nextState == State::Record || nextState == State::Overdub);
        state_ = nextState;
        passStart_ = next;
        gridMicros_ = grid;
        setFlag(kGridValid, true);
        setFlag(kFullShown, false);
        ++passCount_;
        if (state_ == State::Play || state_ == State::Overdub) startPlan(0, grid, now);
    }

    // carry: the next segment of a running pass (notes cut at the segment
    // boundary go on); otherwise onsets >= fromStep only.
    bool startPlan(uint8_t fromStep, uint32_t originMicros, uint32_t now, bool carry = false) {
        nextSegment_ = 0;
        if (!player_) return false;
        ResponsePlan plan;  // 131 bytes, main-loop stack
        uint8_t next = 0;
        if (layer_.buildPlan(plan, passSteps(), length_, fromStep, muteMask(), carry, &next) == 0) {
            (void)player_->cancel();
            return false;
        }
        nextSegment_ = next;
        // The loop channel is latched for this pass only; the PARTNER setting
        // stored in the shared player is put back right away.
        const uint8_t partnerChannel = player_->wireChannel();
        const MusicalEventTarget partnerTarget = player_->target();
        player_->setWireChannel(channel_);
        player_->setTarget(channel_ == kChannelFollowDx ? target_ : MusicalEventTarget::Dx);
        const bool started = player_->start(plan, originMicros, clock_.sixteenthMicros(), true);
        player_->setWireChannel(partnerChannel);
        player_->setTarget(partnerTarget);
        player_->service(now);
        return started;
    }

    // The next plan of the running pass, on the grid the pass trigger used
    // (position): its step 0 is nextSegment_. Any note of the previous plan
    // still sounding was cut at this step (NoteOff due now).
    void startSegment(uint32_t now, double position) {
        const uint8_t from = nextSegment_;
        const double late = position - static_cast<double>(passStart_ + from);
        const uint32_t lateMicros = static_cast<uint32_t>(late * clock_.sixteenthMicros() + 0.5);
        (void)startPlan(from, now - lateMicros, now, true);
    }

    // Loop sounds again now: on the project grid (from the current step) or,
    // without PLAY, from step 0 of a new free clock.
    void resumeNow(uint32_t now) {
        state_ = State::Play;
        setFlag(kResumeOnPlay, false);
        if (!clock_.running()) {
            clock_.startFree(now, 0.0);
            passStart_ = 0;
            gridMicros_ = now;
            setFlag(kGridValid, true);
            ++passCount_;
            (void)startPlan(0, now, now);
            return;
        }
        const double position = clock_.positionAt(now);
        passStart_ = alignedPassStart(position);
        gridMicros_ = clock_.microsAtStep(static_cast<double>(passStart_));
        setFlag(kGridValid, true);
        // A step less than half a step ago still plays (a little late).
        int32_t from = static_cast<int32_t>(std::ceil(position - passStart_ - 0.5));
        if (from < 0) from = 0;
        if (from >= passSteps()) from = passSteps() - 1;
        ++passCount_;
        (void)startPlan(static_cast<uint8_t>(from),
                        gridMicros_ + static_cast<uint32_t>(from) * clock_.sixteenthMicros(), now);
    }

    // The running pass is played again from the current step with the layer
    // as it is now (MUTE, CLEAR of one layer, BARS): the player's start()
    // sends NoteOff to every sounding note first; a note still sounding over
    // the step starts again there (carry).
    void restartPlanNow(uint32_t now) {
        if ((state_ != State::Play && state_ != State::Overdub) || !clock_.running() ||
            !flag(kGridValid))
            return;
        const double rel = positionNow(now) - passStart_;
        int32_t from = static_cast<int32_t>(std::ceil(rel - 0.5));
        if (from < 0) from = 0;
        if (from >= passSteps()) from = passSteps() - 1;
        (void)startPlan(static_cast<uint8_t>(from),
                        gridMicros_ + static_cast<uint32_t>(from) * clock_.sixteenthMicros(), now, true);
    }

    // New BARS (setBars, or undo of a BARS change). A running pass keeps its
    // downbeat: longer, it goes on into the new bars; shorter, when already
    // past the new end, the pass starts where the new length puts it. The bar
    // phase follows the pass start on the project grid.
    void applyBars(uint8_t bars, uint32_t now) {
        const bool running = (state_ == State::Play || state_ == State::Overdub) &&
                             clock_.running() && flag(kGridValid);
        const double position = running ? positionNow(now) : 0.0;
        bars_ = bars;
        const int32_t steps = passSteps();
        if (running) {
            const double rel = position - passStart_;
            if (rel >= steps) {
                const int32_t k = static_cast<int32_t>(std::floor(rel / steps));
                passStart_ += k * steps;
                gridMicros_ += static_cast<uint32_t>(k * steps) * clock_.sixteenthMicros();
            }
        }
        if (running && clock_.source() == LoopClock::Source::Transport) {
            const int32_t bar = static_cast<int32_t>(std::floor(static_cast<double>(passStart_) / kStepsPerBar));
            phaseBars_ = static_cast<uint8_t>(((bar % bars_) + bars_) % bars_);
        } else {
            phaseBars_ = static_cast<uint8_t>(phaseBars_ % bars_);
        }
        restartPlanNow(now);
    }

    void punchOut(uint32_t now) {
        layer_.closeOpenAt(relAt(now), passSteps());
        state_ = State::Play;
    }

    // Recording ends, the loop goes silent now (NoteOff of every sounding
    // note), the free clock stops.
    void stopLoop(uint32_t now, State next) {
        if (state_ == State::Record || state_ == State::Overdub)
            layer_.closeOpenAt(relAt(now), passSteps());
        if (player_) (void)player_->cancel();
        nextSegment_ = 0;
        if (next != State::Record) setFlag(kDubAfterRec | kHoldAfterRec, false);
        state_ = next;
        setFlag(kGridValid, false);
        if (clock_.source() == LoopClock::Source::Free) clock_.stop();
    }

    void onTransportStop(uint32_t now, double before) {
        (void)now;
        const double rel = before - passStart_;
        switch (state_) {
            case State::Record:
                layer_.closeOpenAt(rel, passSteps());
                if (player_) (void)player_->cancel();
                if (loopEmpty()) {
                    state_ = State::Armed;  // waits for the first note (free clock)
                } else {
                    if (flag(kHoldAfterRec)) setHold(true);
                    state_ = State::Stopped;
                    setFlag(kResumeOnPlay, true);
                }
                setFlag(kDubAfterRec | kHoldAfterRec, false);
                break;
            case State::Play:
            case State::Overdub:
                layer_.closeOpenAt(rel, passSteps());
                if (player_) (void)player_->cancel();
                state_ = State::Stopped;
                setFlag(kResumeOnPlay, true);
                break;
            default:
                break;
        }
        setFlag(kGridValid, false);
    }

    // before: the position on the previous clock (free clock), for the
    // notes still held when PLAY takes over.
    void onTransportStart(uint32_t now, double before) {
        const double position = clock_.positionAt(now);
        const double rel = before - passStart_;
        switch (state_) {
            case State::Armed:
                passStart_ = nextBarStep(position);
                return;
            case State::Record:
                // The pass on the free clock is dropped; record again on the bar.
                layer_.closeOpenAt(rel, passSteps());
                if (!layer_.unitPending()) (void)layer_.undo();
                if (player_) (void)player_->cancel();
                if (loopEmpty()) {
                    state_ = State::Armed;
                    passStart_ = nextBarStep(position);
                    setFlag(kGridValid, false);
                    return;
                }
                state_ = State::Play;
                break;
            case State::Overdub:
                layer_.closeOpenAt(rel, passSteps());
                state_ = State::Play;
                break;
            case State::Play:
                break;
            case State::Stopped:
                if (!flag(kResumeOnPlay) || partnerActive()) return;
                break;
            default:
                return;
        }
        resumeNow(now);
    }

    // The layer holds 192 four-byte events and six open recording notes.
    // ESP32 build: Looper is 856 bytes; the 32-bit budget below is 864 bytes.
    LoopLayer layer_{};
    uint8_t passCount_{0};     // log only (wraps)
    State state_{State::Idle};
    uint8_t bars_{2};
    uint8_t channel_{kChannelFollowDx};
    AnswerLength length_{AnswerLength::Full};
    uint8_t phaseBars_{0};     // bar of the project grid the loop starts on, mod BARS
    uint8_t flags_{0};
    uint8_t nextSegment_{0};   // step of the next plan of this pass, 0 = none
    uint8_t layerFlags_{0};    // HOLD / MUTE per layer, the active layer (phase 3)
    uint8_t soundMono_{0xff};  // current MONO performance pitch, or none
    MusicalEventTarget target_{MusicalEventTarget::SynthA};
    LoopClock clock_{};
    ResponsePlayer* player_{nullptr};
    NowFn nowFn_{nullptr};
    uint32_t gridMicros_{0};   // downbeat (micros) of the pass at passStart_
    int32_t passStart_{0};     // clock step of the current pass (ARMED: of the first pass)
};

// §3.3: <= 864 bytes on the 32-bit target (the host build has 8-byte pointers).
static_assert(sizeof(void*) != 4 || sizeof(Looper) <= 864, "Looper DRAM budget");

inline Looper& looper() {
    static Looper instance;
    return instance;
}

}  // namespace GroovePuterLooper

#endif  // GROOVEPUTER_LOOPER_H
