#pragma once
#ifndef GROOVEPUTER_DIN_ENABLE_REQUEST_H
#define GROOVEPUTER_DIN_ENABLE_REQUEST_H

#include <cstdint>

#include "midi_realtime_word.h"

namespace GroovePuterMidi {

// DIN enable toggle without a second writer. UI or control code calls request().
// Only the MIDI dispatcher calls takeChange() and markApplied(), so the DIN ring
// and the tee's secondary flag keep a single owner. The two words are the only
// state shared between tasks; each holds one of kNone / kOff / kOn.
class DinEnableRequest {
public:
    static constexpr uint32_t kNone = 0u;
    static constexpr uint32_t kOff = 1u;
    static constexpr uint32_t kOn = 2u;

    // Any task. The last request wins.
    void request(bool enabled) { request_.storeRelease(enabled ? kOn : kOff); }

    // Dispatcher only. True when a request differs from the state last applied,
    // and `enabled` then holds the requested state. Equal states are not a change,
    // so repeating a request never sends the all-notes-off again.
    bool takeChange(bool& enabled) {
        const uint32_t requested = request_.loadAcquire();
        if (requested == kNone || requested == applied_.loadAcquire()) return false;
        enabled = requested == kOn;
        return true;
    }

    // Dispatcher only, after the change has taken effect on the wire.
    void markApplied(bool enabled) { applied_.storeRelease(enabled ? kOn : kOff); }

    // Any task: the state the dispatcher last applied. Before the first apply the
    // caller's fallback is returned, so readers never see a state that is not on
    // the wire.
    bool appliedOr(bool fallback) const {
        const uint32_t applied = applied_.loadAcquire();
        if (applied == kNone) return fallback;
        return applied == kOn;
    }

private:
    MidiRealtimeWord request_;
    MidiRealtimeWord applied_;
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_DIN_ENABLE_REQUEST_H
