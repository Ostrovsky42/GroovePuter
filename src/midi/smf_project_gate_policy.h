#pragma once

#include <cstdint>

#include "scheduled_smf_midi_event.h"

// Pure policy for the PROJECT-transport gate of a scheduled SMF event.
//
// ProjectTransportTimeline::trySnapshot() is a seqlock reader. A `false`
// result means the audio task was mid-update on every attempt: the transport
// state is UNKNOWN, not stopped. The gate must therefore tell a miss apart
// from a definite "not playing / other epoch" answer:
//   - NoteOff and SongPositionPointer never wait for, or get dropped by, a miss.
//   - The early phase (event may be far in the future) never sleeps or drops
//     on a miss; the late phase (right before the USB write) decides.
//   - A NoteOn that cannot be verified is re-read a bounded number of times
//     without sleeping; if still unknown and never verified it is dropped
//     under its own reason (a miss), not as a stale epoch.

enum class SmfProjectRead : uint8_t {
    Unavailable = 0,  // trySnapshot() returned false (seqlock writer mid-update)
    NotPlaying,       // read ok, !valid || !playing
    PlayingOtherEpoch,
    PlayingCurrentEpoch,
};

enum class SmfProjectGateAction : uint8_t {
    Dispatch = 0,   // send now / carry on to the normal ordering
    DropStale,      // transport stopped or restarted, or an unverifiable NoteOn
    ReadAgainNow,   // immediate re-read, no sleep, no continue; bounded
};

// Early = the pre-selection check; Late = the check right before the write.
enum class SmfGatePhase : uint8_t { Early = 0, Late };

inline constexpr uint8_t kSmfProjectGateMaxImmediateRereads = 2;

inline constexpr SmfProjectRead smfProjectGateClassify(
        bool snapshotAvailable,
        bool valid,
        bool playing,
        bool epochCurrent) {
    if (!snapshotAvailable) return SmfProjectRead::Unavailable;
    if (!valid || !playing) return SmfProjectRead::NotPlaying;
    return epochCurrent ? SmfProjectRead::PlayingCurrentEpoch
                        : SmfProjectRead::PlayingOtherEpoch;
}

// verifiedCurrent: an earlier read of this same event already saw its epoch
// current.
inline constexpr SmfProjectGateAction smfProjectGateAction(
        ScheduledSmfMidiEventType type,
        SmfGatePhase phase,
        SmfProjectRead read,
        bool verifiedCurrent,
        uint8_t rereadsDone) {
    if (read == SmfProjectRead::PlayingCurrentEpoch) {
        return SmfProjectGateAction::Dispatch;
    }
    if (read != SmfProjectRead::Unavailable) return SmfProjectGateAction::DropStale;
    // Unavailable: never a stop, never a reason to wait for anyone else.
    if (type != ScheduledSmfMidiEventType::NoteOn) {
        return SmfProjectGateAction::Dispatch;
    }
    if (rereadsDone < kSmfProjectGateMaxImmediateRereads) {
        return SmfProjectGateAction::ReadAgainNow;
    }
    if (phase == SmfGatePhase::Early) return SmfProjectGateAction::Dispatch;
    return verifiedCurrent ? SmfProjectGateAction::Dispatch
                           : SmfProjectGateAction::DropStale;
}

struct SmfProjectGateResult {
    SmfProjectGateAction action{SmfProjectGateAction::Dispatch};
    bool verifiedCurrent{false};  // carry into the next call for this event
    bool missDrop{false};         // DropStale caused by an unavailable read
};

// Runs the policy, performing the bounded immediate re-reads. `readOnce` is a
// callable returning SmfProjectRead; it must not block or allocate.
template <typename ReadOnce>
inline SmfProjectGateResult resolveSmfProjectGate(
        ScheduledSmfMidiEventType type,
        SmfGatePhase phase,
        bool verifiedCurrent,
        ReadOnce&& readOnce) {
    SmfProjectGateResult result;
    result.verifiedCurrent = verifiedCurrent;
    uint8_t rereads = 0;
    while (true) {
        const SmfProjectRead read = readOnce();
        if (read == SmfProjectRead::PlayingCurrentEpoch) {
            result.verifiedCurrent = true;
        }
        result.action = smfProjectGateAction(
            type, phase, read, result.verifiedCurrent, rereads);
        if (result.action == SmfProjectGateAction::ReadAgainNow) {
            ++rereads;
            continue;
        }
        result.missDrop = result.action == SmfProjectGateAction::DropStale &&
                          read == SmfProjectRead::Unavailable;
        return result;
    }
}
