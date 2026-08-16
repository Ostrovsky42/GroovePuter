#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_ROUTER_H
#define GROOVEPUTER_MIDI_INPUT_ROUTER_H

#include <cstddef>
#include <cstdint>

#include "midi_input_queue.h"
#include "musical_event_router.h"

// MIDI-input routing is deliberately separate from output ownership and from
// external device profiles. R3a supports only internal Synth A/B targets; Drums
// is added in the following isolated checkpoint once the note->logical-lane map
// is executable-test data.
enum class MidiInputTarget : uint8_t {
    SynthA = 0,
    SynthB = 1,
};

enum class MidiInputChannelMode : uint8_t {
    Omni = 0,
    Single = 1,
};

struct MidiInputRoutingConfig {
    // Preserve <=0.9.9 behavior until a later UI/persistence checkpoint gives
    // the user an explicit way to enable external controller input.
    bool enabled{false};
    MidiInputChannelMode channelMode{MidiInputChannelMode::Omni};
    uint8_t channel{0};
    MidiInputTarget target{MidiInputTarget::SynthA};
};

inline bool operator==(const MidiInputRoutingConfig& lhs,
                       const MidiInputRoutingConfig& rhs) {
    return lhs.enabled == rhs.enabled &&
           lhs.channelMode == rhs.channelMode &&
           lhs.channel == rhs.channel &&
           lhs.target == rhs.target;
}

inline bool operator!=(const MidiInputRoutingConfig& lhs,
                       const MidiInputRoutingConfig& rhs) {
    return !(lhs == rhs);
}

struct MidiInputRouterDiagnostics {
    uint32_t noteOnsRouted{0};
    uint32_t noteOffsRouted{0};
    uint32_t repeatedNoteRetriggers{0};
    uint32_t routedPitchReplacements{0};
    uint32_t filteredMessages{0};
    uint32_t ignoredUnsupported{0};
    uint32_t orphanNoteOffs{0};
    uint32_t ownershipCapacityDrops{0};
    uint32_t overflowRecoveries{0};
    uint32_t configPanics{0};
};

class MidiInputRouter {
public:
    // Keep the input core independent from the DSP header while matching the
    // existing MiniAcid/ClampedLiveNoteIdentity public live-note range. Source
    // regressions lock these constants together so a future engine-range change
    // cannot silently break NoteOff ownership.
    static constexpr uint8_t kSynthNoteMin = 24u;  // C1
    static constexpr uint8_t kSynthNoteMax = 71u;  // B4

    // 19 physical performance keys are already supported by Cardputer's own
    // PerformanceKeyboard. 24 owner slots cover that existing proven density
    // plus small controller headroom without allocating a 16x128 table. Sustain
    // is out of R3a and must re-measure this capacity before R6.
    static constexpr std::size_t kMaxActiveNotes = 24u;
    static constexpr std::size_t kDefaultDrainBudget = 32u;

    explicit MidiInputRouter(MusicalEventRouter& router)
        : router_(router) {}

    const MidiInputRoutingConfig& config() const { return config_; }

    bool setConfig(const MidiInputRoutingConfig& next) {
        if (!isValidConfig(next)) return false;
        if (next == config_) return true;

        releaseAllOwnedNotes();
        ++diagnostics_.configPanics;
        config_ = next;
        return true;
    }

    bool setEnabled(bool enabled) {
        MidiInputRoutingConfig next = config_;
        next.enabled = enabled;
        return setConfig(next);
    }

    bool setTarget(MidiInputTarget target) {
        MidiInputRoutingConfig next = config_;
        next.target = target;
        return setConfig(next);
    }

    bool setOmni() {
        MidiInputRoutingConfig next = config_;
        next.channelMode = MidiInputChannelMode::Omni;
        return setConfig(next);
    }

    bool setSingleChannel(uint8_t zeroBasedChannel) {
        MidiInputRoutingConfig next = config_;
        next.channelMode = MidiInputChannelMode::Single;
        next.channel = zeroBasedChannel;
        return setConfig(next);
    }

    // Consumer-side service for the R2 SPSC queue. The physical producer is
    // intentionally outside this class. Overflow is checked before and after
    // draining so a lost NoteOff can never remain silently owned forever.
    std::size_t service(MidiInputQueue& queue,
                        std::size_t budget = kDefaultDrainBudget) {
        recoverOverflowIfNeeded(queue);

        std::size_t drained = 0;
        NormalizedMidiInputMessage message{};
        while (drained < budget && queue.tryPop(message)) {
            handle(message);
            ++drained;
            if (recoverOverflowIfNeeded(queue)) break;
        }
        return drained;
    }

    bool handle(const NormalizedMidiInputMessage& message) {
        if (!message.isValid()) {
            ++diagnostics_.ignoredUnsupported;
            return false;
        }

        switch (message.type) {
            case MidiInputMessageType::NoteOn:
                return handleNoteOn(message);
            case MidiInputMessageType::NoteOff:
                return handleNoteOff(message);
            case MidiInputMessageType::PolyPressure:
            case MidiInputMessageType::ControlChange:
            case MidiInputMessageType::ProgramChange:
            case MidiInputMessageType::ChannelPressure:
            case MidiInputMessageType::PitchBend:
                ++diagnostics_.ignoredUnsupported;
                return false;
        }
        ++diagnostics_.ignoredUnsupported;
        return false;
    }

    void panic() {
        releaseAllOwnedNotes();
    }

    std::size_t activeNoteCount() const {
        std::size_t count = 0;
        for (const auto& owner : owners_) {
            if (owner.active) ++count;
        }
        return count;
    }

    const MidiInputRouterDiagnostics& diagnostics() const {
        return diagnostics_;
    }

    uint32_t observedOverflowEpoch() const { return observedOverflowEpoch_; }

    static bool isValidConfig(const MidiInputRoutingConfig& config) {
        if (config.channel > 15u) return false;
        switch (config.channelMode) {
            case MidiInputChannelMode::Omni:
            case MidiInputChannelMode::Single:
                break;
            default:
                return false;
        }
        switch (config.target) {
            case MidiInputTarget::SynthA:
            case MidiInputTarget::SynthB:
                return true;
        }
        return false;
    }

    static uint8_t normalizeSynthNote(uint8_t note) {
        if (note < kSynthNoteMin) return kSynthNoteMin;
        if (note > kSynthNoteMax) return kSynthNoteMax;
        return note;
    }

private:
    struct ActiveNoteOwner {
        MidiInputSessionId sessionId{kInvalidMidiInputSessionId};
        MidiInputTransportId transportId{kInvalidMidiInputTransportId};
        uint8_t channel{0};
        // sourceNote is the raw normalized MIDI note used to match the future
        // physical NoteOff. routedNote is the pitch actually published to the
        // internal synth after applying the same C1..B4 contract as MiniAcid.
        uint8_t sourceNote{0};
        uint8_t routedNote{0};
        MusicalEventTarget target{MusicalEventTarget::SynthA};
        bool active{false};
    };

    static MusicalEventTarget musicalTarget(MidiInputTarget target) {
        return target == MidiInputTarget::SynthB
            ? MusicalEventTarget::SynthB
            : MusicalEventTarget::SynthA;
    }

    bool channelAccepted(uint8_t channel) const {
        return config_.channelMode == MidiInputChannelMode::Omni ||
               channel == config_.channel;
    }

    int findSourceOwner(const NormalizedMidiInputMessage& message) const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const auto& owner = owners_[i];
            if (owner.active &&
                owner.transportId == message.transportId &&
                owner.sessionId == message.sessionId &&
                owner.channel == message.channel &&
                owner.sourceNote == message.note()) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int findRoutedPitchOwner(MusicalEventTarget target,
                             uint8_t routedNote) const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const auto& owner = owners_[i];
            if (owner.active &&
                owner.target == target &&
                owner.routedNote == routedNote) {
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

    void publishNote(MusicalEventType type,
                     MusicalEventTarget target,
                     uint8_t channel,
                     uint8_t note,
                     uint8_t velocity) {
        router_.route(MusicalEvent{
            type,
            MusicalEventSource::MidiInput,
            target,
            channel,
            note,
            velocity,
        });
    }

    void releaseOwner(std::size_t index, uint8_t velocity = 0u) {
        ActiveNoteOwner& owner = owners_[index];
        if (!owner.active) return;
        const MusicalEventTarget target = owner.target;
        const uint8_t channel = owner.channel;
        const uint8_t routedNote = owner.routedNote;
        owner = ActiveNoteOwner{};
        // Clear ownership before publication. If a synchronous sink indirectly
        // causes a route/config change, this NoteOff cannot be released twice.
        publishNote(MusicalEventType::NoteOff,
                    target,
                    channel,
                    routedNote,
                    velocity);
        ++diagnostics_.noteOffsRouted;
    }

    void releaseAllOwnedNotes() {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            if (owners_[i].active) releaseOwner(i);
        }
    }

    bool handleNoteOn(const NormalizedMidiInputMessage& message) {
        if (!config_.enabled || !channelAccepted(message.channel)) {
            ++diagnostics_.filteredMessages;
            return false;
        }

        const MusicalEventTarget target = musicalTarget(config_.target);
        const uint8_t routedNote = normalizeSynthNote(message.note());

        // First retire an exact physical owner. This is the deterministic
        // repeated-NoteOn Off->On policy for one transport/session/key.
        int ownerIndex = findSourceOwner(message);
        if (ownerIndex >= 0) {
            releaseOwner(static_cast<std::size_t>(ownerIndex));
            ++diagnostics_.repeatedNoteRetriggers;
        }

        // Internal Synth A/B voices use the clamped C1..B4 pitch as their live
        // identity. A different source note/session may therefore resolve to the
        // same sounding pitch. The newest NoteOn becomes the sole owner of that
        // target+pitch; a later NoteOff from the retired source becomes orphaned
        // instead of stopping the newer note.
        const int pitchOwnerIndex = findRoutedPitchOwner(target, routedNote);
        if (pitchOwnerIndex >= 0) {
            releaseOwner(static_cast<std::size_t>(pitchOwnerIndex));
            ++diagnostics_.routedPitchReplacements;
            if (ownerIndex < 0) ownerIndex = pitchOwnerIndex;
        }

        if (ownerIndex < 0) ownerIndex = findFreeOwner();
        if (ownerIndex < 0) {
            // Never publish an unowned NoteOn: otherwise its later NoteOff could
            // not be routed safely after a target/config change.
            ++diagnostics_.ownershipCapacityDrops;
            return false;
        }

        ActiveNoteOwner& owner = owners_[static_cast<std::size_t>(ownerIndex)];
        owner.sessionId = message.sessionId;
        owner.transportId = message.transportId;
        owner.channel = message.channel;
        owner.sourceNote = message.note();
        owner.routedNote = routedNote;
        owner.target = target;
        owner.active = true;

        // The resolved target and routed pitch are committed to ownership before
        // synchronous publication. NoteOff never re-reads current config or
        // repeats pitch normalization against potentially changed engine state.
        publishNote(MusicalEventType::NoteOn,
                    owner.target,
                    owner.channel,
                    owner.routedNote,
                    message.velocity());
        ++diagnostics_.noteOnsRouted;
        return true;
    }

    bool handleNoteOff(const NormalizedMidiInputMessage& message) {
        // NoteOff resolution intentionally ignores current enabled/channel/
        // target configuration and looks up the retained raw source identity.
        // The routed pitch comes from the owner created by NoteOn.
        const int ownerIndex = findSourceOwner(message);
        if (ownerIndex < 0) {
            ++diagnostics_.orphanNoteOffs;
            return false;
        }
        releaseOwner(static_cast<std::size_t>(ownerIndex), message.velocity());
        return true;
    }

    bool recoverOverflowIfNeeded(MidiInputQueue& queue) {
        const uint32_t epoch = queue.overflowEpoch();
        if (epoch == observedOverflowEpoch_) return false;

        queue.discardPendingFromConsumer();
        releaseAllOwnedNotes();
        observedOverflowEpoch_ = epoch;
        ++diagnostics_.overflowRecoveries;
        return true;
    }

    MusicalEventRouter& router_;
    MidiInputRoutingConfig config_{};
    ActiveNoteOwner owners_[kMaxActiveNotes]{};
    MidiInputRouterDiagnostics diagnostics_{};
    uint32_t observedOverflowEpoch_{0};
};

static_assert(sizeof(MidiInputRouter) <= 320u,
              "MIDI input router/owner must remain below the 320-byte R3a budget");

#endif  // GROOVEPUTER_MIDI_INPUT_ROUTER_H
