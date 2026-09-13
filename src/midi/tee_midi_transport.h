#pragma once
#ifndef GROOVEPUTER_TEE_MIDI_TRANSPORT_H
#define GROOVEPUTER_TEE_MIDI_TRANSPORT_H

#include <cstdint>

#include "midi_transport.h"

namespace GroovePuterMidi {

// Sends one MIDI stream to two wires while keeping a single musical owner.
// The primary (USB) owns retry semantics while healthy. A stalled primary may
// be demoted so a live DIN secondary can continue, but any cleanup accepted
// only by DIN creates primary cleanup debt. USB cannot regain authority until
// that debt has been closed on USB itself.
class TeeMidiTransport final : public IMidiTransport {
public:
    static constexpr uint32_t kPrimaryStallRejects = 64;
    static constexpr uint8_t kRecoveryAllNotesOffController = 123;

    struct Diagnostics {
        uint32_t secondaryRejected{0};
        uint32_t secondaryOnlyDelivered{0};
        uint32_t secondarySkipped{0};
        uint32_t primaryConsecutiveRejects{0};
        uint32_t primaryStallDemotions{0};
        uint32_t primaryRecoveryCleanups{0};
        uint32_t primaryRecoveryCleanupRejects{0};
    };

    TeeMidiTransport(IMidiTransport& primary, IMidiTransport& secondary)
        : primary_(primary), secondary_(secondary) {}

    void setSecondaryEnabled(bool enabled) { secondaryEnabled_ = enabled; }
    bool secondaryEnabled() const { return secondaryEnabled_; }
    const Diagnostics& diagnostics() const { return diagnostics_; }

    bool begin() override {
        const bool primaryBegun = primary_.begin();
        const bool secondaryBegun = secondary_.begin();
        return primaryBegun || secondaryBegun;
    }

    bool mounted() const override {
        return primary_.mounted() || (secondaryEnabled_ && secondary_.mounted());
    }

    bool primaryStalled() const {
        return diagnostics_.primaryConsecutiveRejects >= kPrimaryStallRejects;
    }

    MidiTransportLink linkKind() const override {
        if (secondaryEnabled_ && secondary_.mounted() &&
            secondary_.linkKind() == MidiTransportLink::Unverifiable) {
            return MidiTransportLink::Unverifiable;
        }
        return primary_.linkKind();
    }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch(
            [&](IMidiTransport& t) { return t.sendNoteOn(channel, note, velocity); },
            channel,
            false);
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch(
            [&](IMidiTransport& t) { return t.sendNoteOff(channel, note, velocity); },
            channel,
            true);
    }

    bool sendControlChange(uint8_t channel,
                           uint8_t controller,
                           uint8_t value) override {
        const bool cleanupCritical = controller == 120u || controller == 123u;
        return dispatch([&](IMidiTransport& t) {
            return t.sendControlChange(channel, controller, value);
        }, channel, cleanupCritical);
    }

    bool sendTimingClock() override {
        return dispatch([](IMidiTransport& t) { return t.sendTimingClock(); }, 0, false);
    }
    bool sendStart() override {
        return dispatch([](IMidiTransport& t) { return t.sendStart(); }, 0, false);
    }
    bool sendContinue() override {
        return dispatch([](IMidiTransport& t) { return t.sendContinue(); }, 0, false);
    }
    bool sendStop() override {
        return dispatch([](IMidiTransport& t) { return t.sendStop(); }, 0, false);
    }
    bool sendSongPositionPointer(uint16_t midiBeats) override {
        return dispatch([&](IMidiTransport& t) {
            return t.sendSongPositionPointer(midiBeats);
        }, 0, false);
    }

    void flush() override {
        primary_.flush();
        if (secondaryEnabled_) secondary_.flush();
    }

private:
    static uint16_t channelMask(uint8_t channel) {
        return static_cast<uint16_t>(1u << (channel & 0x0fu));
    }

    bool recoverPrimaryCleanupDebt() {
        if (primaryCleanupDebtMask_ == 0u) return true;
        if (!primary_.mounted()) return false;

        for (uint8_t channel = 0; channel < 16u; ++channel) {
            const uint16_t mask = channelMask(channel);
            if ((primaryCleanupDebtMask_ & mask) == 0u) continue;
            if (!primary_.sendControlChange(channel,
                                            kRecoveryAllNotesOffController,
                                            0u)) {
                ++diagnostics_.primaryRecoveryCleanupRejects;
                return false;
            }
            primary_.flush();
            primaryCleanupDebtMask_ &= static_cast<uint16_t>(~mask);
            ++diagnostics_.primaryRecoveryCleanups;
        }
        diagnostics_.primaryConsecutiveRejects = 0;
        return true;
    }

    template <typename SendFn>
    bool dispatch(SendFn&& send, uint8_t channel, bool cleanupCritical) {
        const bool primaryMounted = primary_.mounted();
        if (!primaryMounted) {
            diagnostics_.primaryConsecutiveRejects = 0;
            primaryCleanupDebtMask_ = 0;
        }

        bool wasStalled = primaryStalled();
        if (primaryMounted && wasStalled && primaryCleanupDebtMask_ != 0u) {
            // Do not use a normal NoteOn/clock as the recovery probe while the
            // recovered USB endpoint may still hold notes that DIN already
            // released. Reconcile USB first, and only then permit application
            // traffic to make it authoritative again.
            if (!recoverPrimaryCleanupDebt()) {
                bool secondaryResult = false;
                if (secondaryEnabled_ && secondary_.mounted()) {
                    secondaryResult = send(secondary_);
                    if (!secondaryResult) ++diagnostics_.secondaryRejected;
                }
                if (secondaryResult) {
                    ++diagnostics_.secondaryOnlyDelivered;
                    if (cleanupCritical) {
                        primaryCleanupDebtMask_ |= channelMask(channel);
                    }
                }
                return secondaryResult;
            }
            wasStalled = false;
        }

        const bool primaryResult = primaryMounted ? send(primary_) : false;
        if (primaryMounted) {
            if (primaryResult) {
                diagnostics_.primaryConsecutiveRejects = 0;
            } else if (diagnostics_.primaryConsecutiveRejects <
                       kPrimaryStallRejects) {
                ++diagnostics_.primaryConsecutiveRejects;
                if (primaryStalled()) ++diagnostics_.primaryStallDemotions;
            }
        }

        const bool primaryAuthoritative =
            primaryMounted && !(wasStalled || primaryStalled());
        const bool skipSecondary = primaryAuthoritative && !primaryResult;

        bool secondaryResult = false;
        if (skipSecondary) {
            ++diagnostics_.secondarySkipped;
        } else if (secondaryEnabled_ && secondary_.mounted()) {
            secondaryResult = send(secondary_);
            if (!secondaryResult) ++diagnostics_.secondaryRejected;
        }

        if (!primaryResult && secondaryResult && cleanupCritical && primaryMounted) {
            primaryCleanupDebtMask_ |= channelMask(channel);
        }

        if (primaryAuthoritative) return primaryResult;
        if (secondaryResult) ++diagnostics_.secondaryOnlyDelivered;
        return primaryResult || secondaryResult;
    }

    IMidiTransport& primary_;
    IMidiTransport& secondary_;
    Diagnostics diagnostics_{};
    uint16_t primaryCleanupDebtMask_{0};
    bool secondaryEnabled_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_TEE_MIDI_TRANSPORT_H
