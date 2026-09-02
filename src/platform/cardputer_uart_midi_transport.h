#pragma once
#ifndef GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H
#define GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_transport.h"
#include "src/midi/uart_midi_transport_core.h"

namespace GroovePuterMidi {

// DIN MIDI output over the Cardputer ADV Grove UART, as proven by the SAM2695
// H0 smoke test: GPIO1 RX, GPIO2 TX, 31250 baud, 8N1.
//
// The link is write-only in the sense that matters here. There is no
// acknowledgement, so `mounted()` reports only that the port was opened and
// `linkKind()` is Unverifiable; nothing downstream may render this endpoint as
// connected.
//
// The M5Stack Unit MIDI in SEPARATE mode drives the onboard SAM2695 and the
// DIN OUT jack from the same TX line. They are one endpoint with two
// consumers, not two endpoints: no routing can address them separately, and
// the local synth will audibly play whatever is sent to DIN OUT.
constexpr int kCardputerUartMidiRxPin = 1;
constexpr int kCardputerUartMidiTxPin = 2;

// Bounded work per drain call. At 320 us per byte this is about 10 ms of wire
// time, so a saturated link cannot starve the dispatcher.
constexpr std::size_t kCardputerUartMidiDrainBudget = 32;

class CardputerUartMidiTransport final : public IMidiTransport {
public:
    bool begin() override;
    bool mounted() const override { return begun_; }

    MidiTransportLink linkKind() const override {
        return MidiTransportLink::Unverifiable;
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
    bool sendContinue() override;
    bool sendStop() override;
    bool sendSongPositionPointer(uint16_t midiBeats) override;

    // Queue only. The wire is drained by service(), never by the musical path.
    void flush() override {}

    // Moves queued bytes to the UART as far as the driver accepts them. Call
    // from the dispatcher loop, not from the audio callback.
    void service();

    const UartMidiDiagnostics& diagnostics() const {
        return core_.diagnostics();
    }
    std::size_t pendingBytes() const { return core_.pendingBytes(); }

private:
    bool queueChannelMessage(uint8_t status,
                             uint8_t zeroBasedChannel,
                             uint8_t data1,
                             uint8_t data2,
                             UartMidiPriority priority);

    UartMidiTransportCore core_;
    bool begun_{false};
};

// Process-wide DIN endpoint transport. Declared here so the dispatcher and the
// diagnostics surface share one instance.
CardputerUartMidiTransport& cardputerUartMidiTransport();

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H
