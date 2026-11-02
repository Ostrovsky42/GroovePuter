#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_LISTENER_H
#define GROOVEPUTER_CALL_RESPONSE_LISTENER_H

#include <cmath>
#include <cstdint>

#include "call_response_motif.h"
#include "call_response_development.h"
#include "call_response_performance_plan.h"
#include "call_response_phrase_capture.h"
#include "call_response_planner.h"
#include "call_response_responder.h"
#include "performance_input_observer.h"

// CALL/RESPONSE phase 0 listener: owns the CALL clock and the phrase capture.
// No sound output. Main loop only: fed by PerformanceKeyboard (observer) and
// serviced once per loop. Off by default and not persisted.
//
// Clock:
//   - PLAY running: project transport steps (bars of the project, two-bar
//     windows aligned to even project bars);
//   - PLAY stopped and freeClock (default on): the PERFORM free clock at the
//     PERFORM tempo, anchored so the first note after silence is bar 1 beat 1;
//     it stops after kIdleWindowsToStop silent windows with no held note.
//   Any change of clock source, a new transport epoch or toggling the mode is
//   a new epoch: pending notes are discarded, nothing is logged for them.
//
// Phase 1 (mode ANSWER): turn-taking on the same clock. A closed non-empty
// window is a CALL; the next window is the ANSWER window. If the person has not
// already started that window (no pickup / early note pending), the answer is
// planned (ECHO_VARIANT) and played by ResponsePlayer. Any person NoteOn while
// the answer is pending or sounding cancels it at once (sounding notes get their
// NoteOff immediately) and is captured as the start of a new CALL. A new epoch
// (PLAY start/stop, transport epoch, free clock stop) or leaving ANSWER mode
// cancels the answer as well.
namespace GroovePuterCallResponse {

struct TransportView {
    bool playing{false};
    uint32_t epoch{0};
    uint32_t blockSequence{0};
    double steps{0.0};
    double bpm{0.0};  // <= 0 means "unknown": 120
};

class CallResponseListener final : public PerformanceInputObserver {
public:
    enum class ClockSource : uint8_t { None = 0, Transport, Free };
    enum class Mode : uint8_t { Off = 0, Listen, Answer, Duet, Loop };
    static constexpr uint8_t kIdleWindowsToStop = 2;
    using NowFn = uint32_t (*)();

    void setNowFunction(NowFn fn) { nowFn_ = fn; }

    void setEnabled(bool enabled) {
        if (enabled == this->enabled()) return;
        setMode(enabled ? Mode::Listen : Mode::Off);
    }
    bool enabled() const { return mode_ != Mode::Off; }
    void toggleEnabled() { setEnabled(!enabled()); }

    // OFF <-> LISTEN/ANSWER is a new epoch; LISTEN <-> ANSWER keeps the capture.
    void setMode(Mode mode) {
        if (mode == mode_) return;
        const bool wasEnabled = enabled();
        if (mode_ == Mode::Loop && mode != Mode::Loop) { loopValid_ = false; loopHold_ = false; captureResumeStep_ = 0; }
        mode_ = mode;
        if (wasEnabled != enabled()) stopClock();
        else if (mode_ != Mode::Answer && mode_ != Mode::Duet && mode_ != Mode::Loop) responder_.cancel();
    }
    Mode mode() const { return mode_; }
    void cycleMode(int direction) {
        const Mode modes[] = {Mode::Off, Mode::Answer, Mode::Duet, Mode::Loop};
        int index = 0;
        for (int i = 0; i < 4; ++i) if (modes[i] == mode_) index = i;
        setMode(modes[(index + (direction >= 0 ? 1 : 3)) % 4]);
    }
    const char* modeName() const {
        return mode_ == Mode::Answer ? "ANSWER" : mode_ == Mode::Duet ? "DUET" : mode_ == Mode::Loop ? "LOOP" : mode_ == Mode::Listen ? "LISTEN" : "OFF";
    }

    void setOutputRouter(MusicalEventRouter* router) { responder_.setRouter(router); }
    ResponsePlayer& responder() { return responder_; }
    const ResponsePlayer& responder() const { return responder_; }
    bool answering() const { return responder_.active(); }
    const ReplyMemory& replyMemory() const { return replyMemory_; }
    void rememberStartedReply(uint32_t signature, ReplyStrategy strategy) {
        rememberReply(replyMemory_, signature, strategy);
    }
    void cancelAnswer() { (void)responder_.cancel(); }
    bool hasLoopPhrase() const { return loopValid_; }
    bool loopHold() const { return loopHold_; }
    void setLoopHold(bool hold) {
        if (mode_ != Mode::Loop || !loopValid_ || hold == loopHold_) return;
        loopHold_ = hold;
        capture_.discardPending();
        captureResumeStep_ = hold ? 0 : capture_.windowStartStep() + capture_.windowSteps();
    }
    uint8_t loopEvolve() const { return loopEvolve_; }
    void setLoopEvolve(uint8_t level) { loopEvolve_ = level > 3 ? 3 : level; }
    void cycleLoopEvolve(int direction) { setLoopEvolve((loopEvolve_ + (direction >= 0 ? 1 : 3)) % 4); }

    void setFreeClockEnabled(bool enabled) {
        if (enabled != freeClockDisabled_) return;
        freeClockDisabled_ = !enabled;
        if (source_ == ClockSource::Free) stopClock();
    }
    bool freeClockEnabled() const { return !freeClockDisabled_; }

    ClockSource clockSource() const { return source_; }
    const char* clockName() const {
        return source_ == ClockSource::Transport ? "PLAY"
             : source_ == ClockSource::Free ? "FREE" : "NONE";
    }

    void onPerformanceInputNote(bool on, uint16_t tag, uint8_t note,
                                uint8_t velocity) override {
        if (!enabled()) return;
        const uint32_t now = nowFn_ ? nowFn_() : lastServiceMicros_;
        if (mode_ == Mode::Loop && (loopHold_ || positionAt(now) < captureResumeStep_)) return;
        if (on) {
            // Playing over the answer is a new CALL: the answer yields at once.
            if (mode_ == Mode::Answer && responder_.active()) {
                (void)responder_.cancel();
                ++humanCancels_;
            }
            if (source_ == ClockSource::None) {
                if (lastTransport_.playing || freeClockDisabled_) return;
                startFreeClock(now);
            }
            idleWindows_ = 0;
            (void)capture_.noteOn(tag, note, velocity, positionAt(now));
        } else if (source_ != ClockSource::None) {
            capture_.noteOff(tag, positionAt(now));
        }
    }

    // Once per main-loop pass, after PerformanceKeyboard::service().
    // Returns Phrase when a non-empty two-bar CALL window has just closed.
    PhraseCapture::CloseResult service(uint32_t nowMicros,
                                       const TransportView& transport,
                                       float performBpm,
                                       PhraseCapture::Phrase& out) {
        lastServiceMicros_ = nowMicros;
        lastTransport_ = transport;
        if (!enabled()) return PhraseCapture::CloseResult::None;

        if (transport.playing) {
            if (source_ != ClockSource::Transport || transport.epoch != transportEpoch_) {
                stopClock();
                source_ = ClockSource::Transport;
                transportEpoch_ = transport.epoch;
                lastBlockSequence_ = transport.blockSequence;
                anchorSteps_ = transport.steps;
                anchorMicros_ = nowMicros;
            } else if (transport.blockSequence != lastBlockSequence_) {
                lastBlockSequence_ = transport.blockSequence;
                anchorSteps_ = transport.steps;
                anchorMicros_ = nowMicros;
            }
            sixteenthMicros_ = sixteenthMicrosFor(transport.bpm);
        } else if (source_ == ClockSource::Transport) {
            stopClock();  // PLAY stopped: a new epoch; the free clock starts at the next note
        } else if (source_ == ClockSource::Free) {
            if (freeClockDisabled_) {
                stopClock();
            } else {
                const uint32_t sixteenth = sixteenthMicrosFor(performBpm);
                if (sixteenth != sixteenthMicros_) {
                    // Tempo change keeps the musical position continuous.
                    anchorSteps_ = positionAt(nowMicros);
                    anchorMicros_ = nowMicros;
                    sixteenthMicros_ = sixteenth;
                }
            }
        }
        if (source_ == ClockSource::None) {
            freeBpmForStart_ = performBpm;
            return PhraseCapture::CloseResult::None;
        }
        responder_.service(nowMicros);

        PhraseCapture::CloseResult result =
            capture_.closeDueWindow(positionAt(nowMicros), out);
        if (mode_ == Mode::Loop && result == PhraseCapture::CloseResult::Phrase) {
            loopPhrase_ = out;
            loopValid_ = true;
        } else if (mode_ == Mode::Loop && result == PhraseCapture::CloseResult::Empty && loopValid_) {
            const int32_t windowStart = out.windowStartStep;
            out = loopPhrase_;
            out.windowStartStep = windowStart;
            result = PhraseCapture::CloseResult::Phrase;
        }
        if (result == PhraseCapture::CloseResult::Phrase) {
            idleWindows_ = 0;
        } else if (result == PhraseCapture::CloseResult::Empty &&
                   source_ == ClockSource::Free) {
            if (idleWindows_ < 255u) ++idleWindows_;
            if (idleWindows_ >= kIdleWindowsToStop && capture_.pendingCount() == 0) {
                stopClock();
            }
        }
        return result;
    }

    double positionAt(uint32_t nowMicros) {
        if (source_ == ClockSource::None || sixteenthMicros_ == 0) return 0.0;
        const int32_t elapsed = static_cast<int32_t>(nowMicros - anchorMicros_);
        double position = anchorSteps_ +
            (elapsed > 0 ? static_cast<double>(elapsed) / sixteenthMicros_ : 0.0);
        if (position < lastPosition_) position = lastPosition_;  // never step back in an epoch
        lastPosition_ = position;
        return position;
    }

    const PhraseCapture& capture() const { return capture_; }

    // Clock time of an epoch step (extrapolated from the current anchor).
    uint32_t microsAtStep(double step) const {
        const double offset = (step - anchorSteps_) * static_cast<double>(sixteenthMicros_);
        const int64_t rounded = static_cast<int64_t>(offset >= 0.0 ? offset + 0.5 : offset - 0.5);
        return anchorMicros_ + static_cast<uint32_t>(rounded);
    }
    uint32_t sixteenthMicros() const { return sixteenthMicros_; }

    // After service() returned Phrase: true when, in ANSWER mode, the window
    // right after phrase is free for an answer (the person has not already
    // started it and the clock did not skip windows).
    bool canAnswer(const PhraseCapture::Phrase& phrase) const {
        if ((mode_ != Mode::Answer && mode_ != Mode::Duet && mode_ != Mode::Loop) || source_ == ClockSource::None) return false;
        if (phrase.windowSteps != capture_.windowSteps()) return false;  // BARS changed
        const int32_t answerWindow = phrase.windowStartStep + phrase.windowSteps;
        if (capture_.windowStartStep() != answerWindow) return false;
        return mode_ != Mode::Answer || capture_.pendingCount() == 0;
    }

    // Starts an already planned answer in the window right after phrase.
    bool startAnswer(const PhraseCapture::Phrase& phrase, const ResponsePlan& plan,
                     uint32_t nowMicros) {
        if (!canAnswer(phrase) || plan.count == 0) return false;
        const int32_t answerWindow = phrase.windowStartStep + phrase.windowSteps;
        const uint32_t origin = microsAtStep(static_cast<double>(answerWindow + plan.delaySteps));
        if (!responder_.start(plan, origin, sixteenthMicros_)) return false;
        responder_.service(nowMicros);
        return true;
    }

    // Phase 1: plans (ECHO_VARIANT, key only) and starts the answer. Returns
    // false (nothing sounds) when canAnswer() is false.
    bool answerPhrase(const PhraseCapture::Phrase& phrase, const MotifDescriptor& motif,
                      const ResponseKey& key, uint32_t nowMicros, ResponsePlan& plan) {
        plan = ResponsePlan{};
        if (!canAnswer(phrase)) return false;
        if (mode_ == Mode::Loop || mode_ == Mode::Duet) {
            ReplyStrategy actual{};
            ResponseHarmonyReport report{};
            if (!planPerformanceReply(phrase, motif, key, ResponseHarmony{}, ResponseStyle{},
                replyMemory_, mode_ == Mode::Loop, mode_ == Mode::Duet, loopEvolve_,
                answerDelaySteps_, plan, actual, report)) return false;
        } else if (!planEchoVariant(phrase, motif, key, plan, answerDelaySteps_)) return false;
        return startAnswer(phrase, plan, nowMicros);
    }

    // ---- PARTNER settings (phase 3). Session only, never persisted; the
    // defaults are the phase-2 behaviour: 2 bars, follow DX, project genre,
    // 1/16 delay.

    // Answer MIDI channel, 1..16; 0 = follow the PERFORM DX route (default).
    // An explicit channel plays on UsbMidiOutput's answer lane (USB + DIN).
    // A running answer keeps its channel to the end (NoteOffs stay balanced).
    static constexpr uint8_t kAnswerChannelFollowDx = ResponsePlayer::kFollowDx;
    void setAnswerChannel(uint8_t channel1Based) { responder_.setWireChannel(channel1Based); }
    uint8_t answerChannel() const { return responder_.wireChannel(); }
    void cycleAnswerChannel(int direction) {
        const int next = (static_cast<int>(answerChannel()) + (direction >= 0 ? 1 : 16)) % 17;
        setAnswerChannel(static_cast<uint8_t>(next));
    }

    // CALL (and ANSWER) length in bars: 1 or 2. A change is a new epoch (the
    // window grid changes): capture and a running answer are dropped.
    void setAnswerBars(uint8_t bars) {
        bars = bars == 1 ? 1 : 2;
        if (bars == answerBars()) return;
        stopClock();
        capture_.setWindowBars(bars);
    }
    uint8_t answerBars() const {
        return static_cast<uint8_t>(capture_.windowSteps() / PhraseCapture::kStepsPerBar);
    }
    void cycleAnswerBars(int) { setAnswerBars(answerBars() == 1 ? 2 : 1); }

    // Answer delay after the ANSWER window boundary, in sixteenths: 0 or 1.
    void setAnswerDelaySteps(uint8_t steps) { answerDelaySteps_ = clampAnswerDelay(steps); }
    uint8_t answerDelaySteps() const { return answerDelaySteps_; }
    void cycleAnswerDelay(int) { setAnswerDelaySteps(answerDelaySteps_ == 0 ? 1 : 0); }

    // Answer genre: AUTO = the project genre (phase 2), NONE = no genre ending
    // (phase-1 answer), otherwise one GenreIdiom genre (0..kGenreCount-1).
    static constexpr uint8_t kGenreAuto = 0;
    static constexpr uint8_t kGenreNone = 1;
    static constexpr uint8_t kGenreFirst = 2;
    static constexpr uint8_t kGenreSettingCount = kGenreFirst + 16;
    void setAnswerGenreSetting(uint8_t setting) {
        answerGenre_ = setting < kGenreSettingCount ? setting : kGenreAuto;
    }
    uint8_t answerGenreSetting() const { return answerGenre_; }
    void cycleAnswerGenre(int direction) {
        const int next = (static_cast<int>(answerGenre_) +
                          (direction >= 0 ? 1 : kGenreSettingCount - 1)) % kGenreSettingCount;
        answerGenre_ = static_cast<uint8_t>(next);
    }
    // Genre index for pickResponseStyle(); 0xFF (no genre) for NONE.
    uint8_t answerGenre(uint8_t projectGenre) const {
        if (answerGenre_ == kGenreAuto) return projectGenre;
        if (answerGenre_ == kGenreNone) return 0xFF;
        return static_cast<uint8_t>(answerGenre_ - kGenreFirst);
    }

    // Last transport view given to service(); the main loop reuses it when the
    // timeline snapshot is momentarily unavailable (a writer was mid-update),
    // instead of reading that as "PLAY stopped" and resetting the epoch.
    const TransportView& lastTransport() const { return lastTransport_; }
    uint32_t humanCancels() const { return humanCancels_; }

    static uint32_t sixteenthMicrosFor(double bpm) {
        if (!std::isfinite(bpm) || bpm <= 0.0) bpm = 120.0;
        if (bpm < 30.0) bpm = 30.0;
        if (bpm > 300.0) bpm = 300.0;
        return static_cast<uint32_t>(15000000.0 / bpm + 0.5);
    }

private:
    void stopClock() {
        replyMemory_ = ReplyMemory{};
        loopValid_ = false;
        loopHold_ = false;
        captureResumeStep_ = 0;
        (void)responder_.cancel();
        source_ = ClockSource::None;
        capture_.reset();
        idleWindows_ = 0;
        anchorSteps_ = 0.0;
        lastPosition_ = 0.0;
    }

    void startFreeClock(uint32_t nowMicros) {
        stopClock();
        source_ = ClockSource::Free;
        anchorSteps_ = 0.0;
        anchorMicros_ = nowMicros;
        sixteenthMicros_ = sixteenthMicrosFor(freeBpmForStart_);
    }

    PhraseCapture::Phrase loopPhrase_{};
    bool loopValid_{false};
    bool loopHold_{false};
    uint8_t loopEvolve_{1};
    int32_t captureResumeStep_{0};
    ReplyMemory replyMemory_{};
    PhraseCapture capture_{};
    ResponsePlayer responder_{};
    NowFn nowFn_{nullptr};
    TransportView lastTransport_{};
    double anchorSteps_{0.0};
    double lastPosition_{0.0};
    float freeBpmForStart_{0.0f};
    uint32_t anchorMicros_{0};
    uint32_t sixteenthMicros_{0};
    uint32_t lastServiceMicros_{0};
    uint32_t transportEpoch_{0};
    uint32_t lastBlockSequence_{0};
    uint32_t humanCancels_{0};
    ClockSource source_{ClockSource::None};
    uint8_t idleWindows_{0};
    Mode mode_{Mode::Off};
    bool freeClockDisabled_{false};  // PERFORM free clock without PLAY: on by default
    uint8_t answerDelaySteps_{kAnswerDelaySteps};
    uint8_t answerGenre_{kGenreAuto};
};

inline CallResponseListener& callResponseListener() {
    static CallResponseListener listener;
    return listener;
}

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_LISTENER_H
