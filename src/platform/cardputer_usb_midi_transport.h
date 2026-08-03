#pragma once

#include <cstdint>

#include "src/midi/usb_midi_transport.h"
#include "src/midi/usb_midi_packet_pacer.h"

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
// The platform owns one global instance so USBMIDI can register its descriptor
// before Arduino's app_main() starts the TinyUSB CDC composite. begin() and
// router registration are deliberately deferred until setup().
class CardputerUsbMidiTransport final : public IUsbMidiTransport {
public:
    CardputerUsbMidiTransport() = default;

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

    USBMIDI midi_;
    GroovePuterMidi::UsbMidiPacketPacer txPacer_{kPacketSpacingMicros};
    bool begun_{false};
    mutable CardputerUsbMidiTransportDiagnostics diagnostics_{};
    mutable bool mountStateKnown_{false};
    mutable bool lastMounted_{false};
    mutable bool suspendStateKnown_{false};
    mutable bool lastSuspended_{false};
};
