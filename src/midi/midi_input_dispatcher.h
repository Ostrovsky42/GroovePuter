#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_DISPATCHER_H
#define GROOVEPUTER_MIDI_INPUT_DISPATCHER_H

#include <cstddef>
#include <cstdint>

#include "src/input/musical_event_router.h"
#include "midi_input_queue.h"
#include "midi_io_state.h"

namespace GroovePuterMidi {

enum class MidiInputTarget : uint8_t {
    SynthA = 0,
    SynthB = 1,
    Drums = 2,
    Perform = 3,  // external keyboard played through the PERFORM keyboard (chords / arp / latch)
};

// Phases of the external Mod button (a short press = tap, a long hold = a different action).
enum class ModPhase : uint8_t {
    Press = 1,    // the ramp started
    Release = 2,  // the ramp started back down
    Cancel = 3,   // the session ended while held: forget the press without acting on it
};

// Sink for the PERFORM target. Called from the MIDI dispatch task; the implementation must only
// enqueue (the PERFORM keyboard lives on the UI/loop task).
class MidiExternalNoteSink {
public:
    virtual ~MidiExternalNoteSink() = default;
    virtual void externalNoteOn(uint8_t note, uint8_t velocity) = 0;
    virtual void externalNoteOff(uint8_t note) = 0;
    // Sustain button / pedal (CC64) of the external keyboard: true while held down.
    virtual void externalSustain(bool down) = 0;
    // Pitch buttons of the external keyboard: -1 = left/down pressed, +1 = right/up pressed, 0 = the
    // button was released (or the session ended while it was held).
    virtual void externalNudge(int direction) = 0;
    // Mod button of the external keyboard: press, release (tap or hold is decided by the consumer).
    virtual void externalMod(ModPhase phase) = 0;
};

enum class MidiInputChannelMode : uint8_t {
    Omni = 0,
    Single = 1,
};

struct MidiInputRoutingConfig {
    bool enabled{false};
    MidiInputChannelMode channelMode{MidiInputChannelMode::Omni};
    uint8_t channel{0};
    MidiInputTarget target{MidiInputTarget::SynthA};
};

class MidiInputDispatcher {
public:
    static constexpr std::size_t kMaxActiveNotes = 24;

    MidiInputDispatcher() = default;
    MidiInputDispatcher(MusicalEventRouter& router, MidiIoState& io) {
        bind(router, io);
    }

    void bind(MusicalEventRouter& router, MidiIoState& io) {
        router_ = &router;
        io_ = &io;
        observedUsbGeneration_ = io.usbInputGeneration();
        observedUartGeneration_ = io.uartInputGeneration();
        releaseAllOwnedNotes();
    }

    static bool isValidConfig(const MidiInputRoutingConfig& config) {
        if (config.channel >= 16u) return false;
        switch (config.target) {
            case MidiInputTarget::SynthA:
            case MidiInputTarget::SynthB:
            case MidiInputTarget::Drums:
            case MidiInputTarget::Perform:
                return true;
        }
        return false;
    }

    void setPerformSink(MidiExternalNoteSink* sink) { performSink_ = sink; }

    const MidiInputRoutingConfig& config() const { return config_; }

    bool setConfig(const MidiInputRoutingConfig& config) {
        if (!isValidConfig(config)) return false;
        if (!sameConfig(config_, config)) releaseAllOwnedNotes();
        config_ = config;
        return true;
    }

    std::size_t service(MidiInputQueue& queue,
                        std::size_t budget = MidiInputQueue::kCapacity) {
        if (router_ == nullptr || io_ == nullptr) return 0;
        syncGenerations();

        if (queue.takeRecoveryPending()) {
            MidiInputEvent discarded{};
            while (queue.tryPop(discarded)) {}
            releaseAllOwnedNotes();
            return 0;
        }

        std::size_t drained = 0;
        MidiInputEvent event{};
        while (drained < budget && queue.tryPop(event)) {
            ++drained;
            if (!io_->acceptsInput(event)) continue;
            dispatch(event);
        }
        return drained;
    }

private:
    struct ActiveOwner {
        bool active{false};
        InputSource source{InputSource::Usb};
        uint32_t generation{0};
        uint8_t inputChannel{0};
        uint8_t sourceNote{0};
        MusicalEventTarget target{MusicalEventTarget::SynthA};
        uint8_t routedChannel{0};
        uint8_t routedNote{0};
        bool perform{false};  // owned by the PERFORM keyboard bridge, not the router
    };

    static bool sameConfig(const MidiInputRoutingConfig& lhs,
                           const MidiInputRoutingConfig& rhs) {
        return lhs.enabled == rhs.enabled &&
               lhs.channelMode == rhs.channelMode &&
               lhs.channel == rhs.channel &&
               lhs.target == rhs.target;
    }

    static MusicalEventTarget musicalTarget(MidiInputTarget target) {
        switch (target) {
            case MidiInputTarget::SynthA: return MusicalEventTarget::SynthA;
            case MidiInputTarget::SynthB: return MusicalEventTarget::SynthB;
            case MidiInputTarget::Drums: return MusicalEventTarget::Drums;
            case MidiInputTarget::Perform: return MusicalEventTarget::SynthA;  // unused: see perform
        }
        return MusicalEventTarget::SynthA;
    }

    static bool mapDrum(uint8_t note, uint8_t& lane) {
        switch (note) {
            case 36: lane = 0; return true;  // kick
            case 38: lane = 1; return true;  // snare
            case 42: lane = 2; return true;  // closed hat
            case 46: lane = 3; return true;  // open hat
            case 43: lane = 4; return true;  // mid tom
            case 47: lane = 5; return true;  // high tom
            case 37: lane = 6; return true;  // rim
            case 39: lane = 7; return true;  // clap
            default: return false;
        }
    }

    static uint8_t clampSynthNote(uint8_t note) {
        if (note < 24u) return 24u;
        if (note > 71u) return 71u;
        return note;
    }

    bool acceptsConfig(const MidiInputEvent& event) const {
        if (!config_.enabled) return false;
        return config_.channelMode == MidiInputChannelMode::Omni ||
               event.id.channel == config_.channel;
    }

    int findSourceOwner(const MidiInputEvent& event) const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const ActiveOwner& owner = owners_[i];
            if (!owner.active) continue;
            if (owner.source == event.id.source &&
                owner.generation == event.id.generation &&
                owner.inputChannel == event.id.channel &&
                owner.sourceNote == event.id.key) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int findFreeOwner() const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            if (!owners_[i].active) return static_cast<int>(i);
        }
        return -1;
    }

    int findResolvedOwner(MusicalEventTarget target, uint8_t routedChannel) const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const ActiveOwner& owner = owners_[i];
            if (!owner.active || owner.perform || owner.target != target) continue;
            if (target != MusicalEventTarget::Drums ||
                owner.routedChannel == routedChannel) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void publish(MusicalEventType type, const ActiveOwner& owner, uint8_t velocity) {
        if (owner.perform) {
            if (performSink_ == nullptr) return;
            if (type == MusicalEventType::NoteOn) performSink_->externalNoteOn(owner.routedNote, velocity);
            else performSink_->externalNoteOff(owner.routedNote);
            return;
        }
        if (router_ == nullptr) return;
        router_->route(MusicalEvent{type,
                                   MusicalEventSource::MidiInput,
                                   owner.target,
                                   owner.routedChannel,
                                   owner.routedNote,
                                   velocity});
    }

    void releaseOwner(std::size_t index, uint8_t velocity = 0) {
        if (index >= kMaxActiveNotes || !owners_[index].active) return;
        const ActiveOwner owner = owners_[index];
        owners_[index] = ActiveOwner{};
        publish(MusicalEventType::NoteOff, owner, velocity);
    }

    void releaseSustain() {
        if (bendZone_ != 0 && performSink_ != nullptr) performSink_->externalNudge(0);
        bendZone_ = 0;
        if (modState_ == ModState::Held && performSink_ != nullptr) {
            performSink_->externalMod(ModPhase::Cancel);
        }
        modState_ = ModState::Idle;
        modLast_ = 0;
        if (!sustainDown_) return;
        sustainDown_ = false;
        if (performSink_ != nullptr) performSink_->externalSustain(false);
    }

    void releaseAllOwnedNotes() {
        releaseSustain();
        if (router_ == nullptr) {
            for (auto& owner : owners_) owner = ActiveOwner{};
            return;
        }
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) releaseOwner(i);
    }

    void releaseSource(InputSource source) {
        if (source == InputSource::Usb) releaseSustain();
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            if (owners_[i].active && owners_[i].source == source) releaseOwner(i);
        }
    }

    void releaseChannel(const MidiInputEvent& event) {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const ActiveOwner& owner = owners_[i];
            if (owner.active && owner.source == event.id.source &&
                owner.generation == event.id.generation &&
                owner.inputChannel == event.id.channel) {
                releaseOwner(i);
            }
        }
    }

    void syncGenerations() {
        const uint32_t usb = io_->usbInputGeneration();
        if (usb != observedUsbGeneration_) {
            releaseSource(InputSource::Usb);
            observedUsbGeneration_ = usb;
        }
        const uint32_t uart = io_->uartInputGeneration();
        if (uart != observedUartGeneration_) {
            releaseSource(InputSource::Uart);
            observedUartGeneration_ = uart;
        }
    }

    void handleNoteOn(const MidiInputEvent& event) {
        if (!acceptsConfig(event)) return;

        ActiveOwner candidate{};
        candidate.active = true;
        candidate.source = event.id.source;
        candidate.generation = event.id.generation;
        candidate.inputChannel = event.id.channel;
        candidate.sourceNote = event.id.key;
        candidate.target = musicalTarget(config_.target);
        candidate.perform = config_.target == MidiInputTarget::Perform;
        if (candidate.perform && performSink_ == nullptr) return;
        candidate.routedNote = candidate.perform ? event.id.key : clampSynthNote(event.id.key);
        candidate.routedChannel = 0;
        if (candidate.target == MusicalEventTarget::Drums) {
            if (!mapDrum(event.id.key, candidate.routedChannel)) return;
            candidate.routedNote = event.id.key;
        }

        const int existingSource = findSourceOwner(event);
        if (existingSource >= 0) releaseOwner(static_cast<std::size_t>(existingSource));

        // The PERFORM keyboard is polyphonic and does its own arbitration; the router targets are mono.
        const int resolved = candidate.perform
            ? -1 : findResolvedOwner(candidate.target, candidate.routedChannel);
        if (resolved >= 0) releaseOwner(static_cast<std::size_t>(resolved));

        const int freeIndex = findFreeOwner();
        if (freeIndex < 0) {
            releaseAllOwnedNotes();
            return;
        }
        owners_[static_cast<std::size_t>(freeIndex)] = candidate;
        publish(MusicalEventType::NoteOn,
                owners_[static_cast<std::size_t>(freeIndex)],
                event.velocity == 0u ? 1u : event.velocity);
    }

    void dispatch(const MidiInputEvent& event) {
        switch (event.kind) {
            case InputKind::NoteOn:
                handleNoteOn(event);
                break;
            case InputKind::NoteOff: {
                const int index = findSourceOwner(event);
                if (index >= 0) releaseOwner(static_cast<std::size_t>(index), event.velocity);
                break;
            }
            case InputKind::AllNotesOff:
            case InputKind::AllSoundOff:
                releaseChannel(event);
                break;
            case InputKind::Mod: {
                // The button sends a short ramp (the owner's nanoKEY2: 0..15 up on press, back to 0 on
                // release), not a level. The first non-zero value fires once and disarms; only a zero
                // (the end of the ramp back down) re-arms it, so one press is exactly one one-shot.
                if (config_.target != MidiInputTarget::Perform || !acceptsConfig(event) ||
                    performSink_ == nullptr) {
                    break;
                }
                const uint8_t value = event.velocity;
                if (value == 0u) {
                    // The end of the ramp down. A press that never showed a decrease still ends here.
                    if (modState_ == ModState::Held) performSink_->externalMod(ModPhase::Release);
                    modState_ = ModState::Idle;
                } else if (modState_ == ModState::Idle) {
                    modState_ = ModState::Held;
                    performSink_->externalMod(ModPhase::Press);
                } else if (modState_ == ModState::Held && value < modLast_) {
                    modState_ = ModState::Released;   // the ramp turned downwards: the button is up
                    performSink_->externalMod(ModPhase::Release);
                }
                modLast_ = value;
                break;
            }
            case InputKind::PitchBend: {
                // The pitch buttons jump to an extreme and spring back to the centre: only the
                // press edge is a nudge. Direct targets ignore the message (no bend in the engine).
                if (config_.target != MidiInputTarget::Perform || !acceptsConfig(event) ||
                    performSink_ == nullptr) {
                    break;
                }
                // The pitch buttons send a slow ramp away from the centre (64) and back (the owner's
                // nanoKEY2: a short touch only reaches 54, a hold goes on to 0). Arm at a small
                // deviation so a touch counts, re-arm only near the centre (hysteresis).
                const int deviation = static_cast<int>(event.velocity) - 64;
                int zone = bendZone_;
                if (deviation <= -kBendPress) zone = -1;
                else if (deviation >= kBendPress) zone = 1;
                else if (deviation >= -kBendRelease && deviation <= kBendRelease) zone = 0;
                if (zone != bendZone_) {
                    bendZone_ = zone;
                    performSink_->externalNudge(zone);   // 0 = back at the centre = released
                }
                break;
            }
            case InputKind::Sustain:
                // Direct synth/drum targets keep the historical R6 policy (parsed and bounded, not
                // applied). The PERFORM target maps it to LATCH while the button is held.
                if (config_.target == MidiInputTarget::Perform && acceptsConfig(event) &&
                    performSink_ != nullptr) {
                    const bool down = event.velocity >= 64u;
                    if (down != sustainDown_) {
                        sustainDown_ = down;
                        performSink_->externalSustain(down);
                    }
                }
                break;
        }
    }

    MidiExternalNoteSink* performSink_{nullptr};
    MusicalEventRouter* router_{nullptr};
    MidiIoState* io_{nullptr};
    MidiInputRoutingConfig config_{};
    ActiveOwner owners_[kMaxActiveNotes]{};
    bool sustainDown_{false};
    int bendZone_{0};
    enum class ModState : uint8_t { Idle, Held, Released };
    ModState modState_{ModState::Idle};
    uint8_t modLast_{0};
    static constexpr int kBendPress = 6;    // deviation from the centre that counts as a press
    static constexpr int kBendRelease = 3;  // deviation under which the button is released again
    uint32_t observedUsbGeneration_{0};
    uint32_t observedUartGeneration_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_DISPATCHER_H
