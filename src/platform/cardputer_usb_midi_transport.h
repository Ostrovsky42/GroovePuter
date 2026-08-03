#pragma once

#include <cstdint>

#include "src/midi/midi_transport_capabilities.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/usb_midi_packet_pacer.h"
#include "src/midi/usb_midi_transport.h"

#if !defined(ARDUINO)
#error "CardputerUsbMidiTransport is available only in the Arduino firmware build"
#endif

#include "USB.h"
#include "USBMIDI.h"

struct CardputerUsbMidiTransportDiagnostics {
    uint32_t mountUpEvents{0};
    uint32_t mountDownEvents{0};
    uint32_t txAttempts{0};
    uint32_t txAccepted{0};
    uint32_t txRejected{0};
    uint32_t txNotMounted{0};
    uint32_t txPacingWaits{0};
    uint32_t txPacingWaitMicros{0};
    uint32_t rxPackets{0};
    // A host that suspends the bus stops polling the IN endpoint entirely while
    // TinyUSB still reports the interface mounted. That looks identical to a
    // receiver that refuses data, so the two must be distinguishable in a log.
    uint32_t suspendEvents{0};
    uint32_t resumeEvents{0};
};

// Native ESP32-S3 TinyUSB MIDI transport for Cardputer-Adv.
//
// The platform owns one global instance so its MIDI descriptor is registered
// before Arduino's app_main() starts the TinyUSB CDC composite. The stock
// USBMIDI allocator can cross-pair CDC and MIDI endpoint numbers; this transport
// reserves one duplex endpoint instead, matching the working MIDI-only profile.
// begin() and router registration are deliberately deferred until setup().
class CardputerUsbMidiTransport final : public IUsbMidiTransport {
public:
    CardputerUsbMidiTransport();

    bool begin() override;
    bool started() const { return begun_; }
    bool mounted() const override;
    bool suspended() const;
    // Polled from the dispatcher: the Arduino core already owns the TinyUSB
    // suspend/resume callbacks, so edges are detected here instead.
    void pollSuspendState() const;
    bool readPacket(midiEventPacket_t& packet);
    CardputerUsbMidiTransportDiagnostics diagnostics() const {
        return diagnostics_;
    }

    bool sendNoteOn(uint8_t zeroBasedChannel,
                    uint8_t note,
                    uint8_t velocity) override;
    bool sendNoteOff(uint8_t zeroBasedChannel,
                     uint8_t note,
                     uint8_t velocity) override;
    bool sendControlChange(uint8_t zeroBasedChannel,
                           uint8_t controller,
                           uint8_t value) override;
    bool sendTimingClock() override;
    bool sendStart() override;
    bool sendStop() override;

    // Called only by MidiDispatchTask. The configured device profile decides
    // whether a real FB is valid or whether resume must use the validated Start
    // fallback. For a class-compliant/General-MIDI target, a PROJECT SMF resume
    // sends F2 immediately before FB using the current player tick and PPQN.
    bool sendContinue() override {
        const GroovePuterMidi::MidiTransportCapabilities capabilities =
            GroovePuterMidi::midiTransportCapabilityRuntime().capabilities();

        if (capabilities.songPositionTx) {
            GroovePuterMidi::ISmfPlayerService* player =
                GroovePuterMidi::smfPlayerService();
            if (player != nullptr) {
                const GroovePuterMidi::SmfPlayerSnapshot state =
                    player->snapshot();
                const bool resumableProjectState =
                    state.tempoMode == GroovePuterMidi::SmfTempoMode::Project &&
                    state.state != GroovePuterMidi::SmfPlayerState::Unloaded &&
                    state.state != GroovePuterMidi::SmfPlayerState::Loading &&
                    state.state != GroovePuterMidi::SmfPlayerState::Stopped &&
                    state.state != GroovePuterMidi::SmfPlayerState::Error;
                if (resumableProjectState) {
                    const GroovePuterMidi::SmfChannelInspectorSnapshot inspector =
                        player->channelInspector();
                    if (inspector.division > 0) {
                        const uint16_t position =
                            GroovePuterMidi::songPositionPointerFromPpqnTicks(
                                state.currentTick, inspector.division);
                        if (!sendSongPositionPointer(position)) return false;
                    }
                }
            }
        }

        if (capabilities.continueTx) {
            midiEventPacket_t packet{
                0x0F,  // USB-MIDI CIN: single-byte realtime message
                0xFB,  // MIDI Continue
                0,
                0,
            };
            return writePacket(packet);
        }

        if (capabilities.continueBehavior ==
                GroovePuterMidi::MidiContinueBehavior::RestartFromBeginning &&
            capabilities.startTx) {
            return writeRealtimePacket(0xFA);  // validated Start fallback
        }
        return false;
    }

    bool sendSongPositionPointer(uint16_t sixteenthNotes) override {
        if (!GroovePuterMidi::midiTransportCapabilityRuntime()
                 .capabilities().songPositionTx) {
            return false;
        }
        const uint16_t value =
            GroovePuterMidi::clampSongPositionPointer(sixteenthNotes);
        midiEventPacket_t packet{
            0x03,  // USB-MIDI CIN: three-byte system common message
            0xF2,  // Song Position Pointer
            GroovePuterMidi::songPositionPointerLsb(value),
            GroovePuterMidi::songPositionPointerMsb(value),
        };
        return writePacket(packet);
    }

    void flush() override;

private:
    // A three-byte DIN MIDI message occupies about 960 us on the wire. Matching
    // that established rate prevents USB-only bursts from filling the 16-packet
    // TinyUSB FIFO before compact hosts such as SEQTRAK poll the IN endpoint.
    static constexpr uint32_t kPacketSpacingMicros = 1000;

    static uint8_t clamp7Bit(uint8_t value);
    static uint8_t clampChannel(uint8_t channel);
    void observeMountState(bool mounted) const;
    bool writePacket(midiEventPacket_t& packet);
    bool writeChannelPacket(uint8_t codeIndex,
                            uint8_t statusBase,
                            uint8_t zeroBasedChannel,
                            uint8_t note,
                            uint8_t velocity);
    bool writeRealtimePacket(uint8_t status);

    GroovePuterMidi::UsbMidiPacketPacer txPacer_{kPacketSpacingMicros};
    bool descriptorRegistered_{false};
    bool begun_{false};
    mutable CardputerUsbMidiTransportDiagnostics diagnostics_{};
    mutable bool mountStateKnown_{false};
    mutable bool lastMounted_{false};
    mutable bool suspendStateKnown_{false};
    mutable bool lastSuspended_{false};
};
