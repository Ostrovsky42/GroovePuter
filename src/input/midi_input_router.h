#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_ROUTER_H
#define GROOVEPUTER_MIDI_INPUT_ROUTER_H

#include <cstddef>
#include <cstdint>

#include "midi_input_queue.h"
#include "musical_event_router.h"

// MIDI-input routing is deliberately separate from output ownership and from
// external device profiles. The target is an incoming-controller destination,
// not an outbound route/profile selection.
enum class MidiInputTarget : uint8_t {
    SynthA = 0,
    SynthB = 1,
    Drums = 2,
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
    uint32_t unmappedDrumNotes{0};
    uint32_t sessionCleanups{0};
    uint32_t sessionNotesReleased{0};
};

class MidiInputRouter {
public:
    static constexpr uint8_t kSynthNoteMin = 24u;  // C1
    static constexpr uint8_t kSynthNoteMax = 71u;  // B4
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

    void panic() { releaseAllOwnedNotes(); }

    std::size_t releaseSession(MidiInputTransportId transportId,
                               MidiInputSessionId sessionId) {
        if (transportId == kInvalidMidiInputTransportId ||
            sessionId == kInvalidMidiInputSessionId) {
            return 0u;
        }
        std::size_t released = 0;
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const auto& owner = owners_[i];
            if (!owner.active || owner.transportId != transportId ||
                owner.sessionId != sessionId) {
                continue;
            }
            releaseOwner(i);
            ++released;
        }
        ++diagnostics_.sessionCleanups;
        diagnostics_.sessionNotesReleased += static_cast<uint32_t>(released);
        return released;
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
            case MidiInputTarget::Drums:
                return true;
        }
        return false;
    }

    static uint8_t normalizeSynthNote(uint8_t note) {
        if (note < kSynthNoteMin) return kSynthNoteMin;
        if (note > kSynthNoteMax) return kSynthNoteMax;
        return note;
    }

    // Fixed GM-style v1 map into GroovePuter's eight logical drum lanes.
    // This is incoming-controller policy and intentionally does not depend on
    // the selected outbound DeviceProfile.
    static bool mapDrumNote(uint8_t note, uint8_t& logicalLane) {
        switch (note) {
            case 36u: logicalLane = 0u; return true;  // Kick
            case 38u: logicalLane = 1u; return true;  // Snare
            case 42u: logicalLane = 2u; return true;  // Closed hat
            case 46u: logicalLane = 3u; return true;  // Open hat
            case 43u: logicalLane = 4u; return true;  // Mid tom
            case 47u: logicalLane = 5u; return true;  // High tom
            case 37u: logicalLane = 6u; return true;  // Rim
            case 39u: logicalLane = 7u; return true;  // Clap
            default: return false;
        }
    }

private:
    struct ActiveNoteOwner {
        MidiInputSessionId sessionId{kInvalidMidiInputSessionId};
        MidiInputTransportId transportId{kInvalidMidiInputTransportId};
        uint8_t inputChannel{0};
        uint8_t sourceNote{0};
        uint8_t routedChannel{0};
        uint8_t routedNote{0};
        MusicalEventTarget target{MusicalEventTarget::SynthA};
        bool active{false};
    };

    static MusicalEventTarget musicalTarget(MidiInputTarget target) {
        switch (target) {
            case MidiInputTarget::SynthB: return MusicalEventTarget::SynthB;
            case MidiInputTarget::Drums: return MusicalEventTarget::Drums;
            case MidiInputTarget::SynthA: return MusicalEventTarget::SynthA;
        }
        return MusicalEventTarget::SynthA;
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
                owner.inputChannel == message.channel &&
                owner.sourceNote == message.note()) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int findResolvedOwner(MusicalEventTarget target,
                          uint8_t routedChannel,
                          uint8_t routedNote) const {
        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {
            const auto& owner = owners_[i];
            if (!owner.active || owner.target != target) continue;
            const bool sameLogicalVoice = target == MusicalEventTarget::Drums
                ? owner.routedChannel == routedChannel
                : owner.routedNote == routedNote;
            if (sameLogicalVoice) return static_cast<int>(i);
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
                     uint8_t routedChannel,
                     uint8_t note,
                     uint8_t velocity) {
        router_.route(MusicalEvent{
            type,
            MusicalEventSource::MidiInput,
            target,
            routedChannel,
            note,
            velocity,
        });
    }

    void releaseOwner(std::size_t index, uint8_t velocity = 0u) {
        ActiveNoteOwner& owner = owners_[index];
        if (!owner.active) return;
        const MusicalEventTarget target = owner.target;
        const uint8_t routedChannel = owner.routedChannel;
        const uint8_t routedNote = owner.routedNote;
        owner = ActiveNoteOwner{};
        publishNote(MusicalEventType::NoteOff,
                    target,
                    routedChannel,
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
        uint8_t routedChannel = message.channel;
        uint8_t routedNote = normalizeSynthNote(message.note());
        if (target == MusicalEventTarget::Drums) {
            if (!mapDrumNote(message.note(), routedChannel)) {
                ++diagnostics_.unmappedDrumNotes;
                return false;
            }
            routedNote = message.note();
        }

        int ownerIndex = findSourceOwner(message);
        if (ownerIndex >= 0) {
            releaseOwner(static_cast<std::size_t>(ownerIndex));
            ++diagnostics_.repeatedNoteRetriggers;
        }

        const int resolvedOwnerIndex =
            findResolvedOwner(target, routedChannel, routedNote);
        if (resolvedOwnerIndex >= 0) {
            releaseOwner(static_cast<std::size_t>(resolvedOwnerIndex));
            ++diagnostics_.routedPitchReplacements;
            if (ownerIndex < 0) ownerIndex = resolvedOwnerIndex;
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
        owner.inputChannel = message.channel;
        owner.sourceNote = message.note();
        owner.routedChannel = routedChannel;
        owner.routedNote = routedNote;
        owner.target = target;
        owner.active = true;

        publishNote(MusicalEventType::NoteOn,
                    owner.target,
                    owner.routedChannel,
                    owner.routedNote,
                    message.velocity());
        ++diagnostics_.noteOnsRouted;
        return true;
    }

    bool handleNoteOff(const NormalizedMidiInputMessage& message) {
        // NoteOff uses the retained physical owner and deliberately ignores the
        // current enabled/channel/target configuration.
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
              "MIDI input router/owner must remain below the 320-byte budget");

#endif  // GROOVEPUTER_MIDI_INPUT_ROUTER_H
