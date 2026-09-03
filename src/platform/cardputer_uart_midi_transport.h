#pragma once
#ifndef GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H
#define GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_transport.h"
#include "src/midi/uart_midi_transport_core.h"

namespace GroovePuterMidi {

constexpr int kCardputerUartMidiRxPin = 1;
constexpr int kCardputerUartMidiTxPin = 2;
constexpr std::size_t kCardputerUartMidiDrainBudget = 32;
constexpr std::size_t kCardputerUartMidiTxBufferBytes = 256;

class CardputerUartMidiTransport final : public IMidiTransport {
public:
    bool begin() override;
    bool mounted() const override { return begun_; }
    MidiTransportLink linkKind() const override { return MidiTransportLink::Unverifiable; }

    bool sendNoteOn(uint8_t zeroBasedChannel, uint8_t note, uint8_t velocity) override;
    bool sendNoteOff(uint8_t zeroBasedChannel, uint8_t note, uint8_t velocity) override;
    bool sendControlChange(uint8_t zeroBasedChannel, uint8_t controller, uint8_t value) override;
    bool sendTimingClock() override;
    bool sendStart() override;
    bool sendContinue() override;
    bool sendStop() override;
    bool sendSongPositionPointer(uint16_t midiBeats) override;
    void flush() override {}

    void service();
    const UartMidiDiagnostics& diagnostics() const { return core_.diagnostics(); }
    std::size_t pendingBytes() const { return core_.pendingBytes(); }
    std::size_t deferredNoteOffs() const { return deferredCount_; }
    uint16_t channelsAwaitingPanic() const { return panicChannels_; }

private:
    bool queueChannelMessage(uint8_t status,
                             uint8_t zeroBasedChannel,
                             uint8_t data1,
                             uint8_t data2,
                             UartMidiPriority priority);
    bool deferNoteOff(uint8_t channel, uint8_t note);
    void retryDeferredNoteOffs();
    void retryChannelPanics();

    static constexpr std::size_t kDeferredNoteOffCapacity = 64;
    struct DeferredNoteOff { uint8_t channel; uint8_t note; };

    UartMidiTransportCore core_;
    DeferredNoteOff deferred_[kDeferredNoteOffCapacity]{};
    std::size_t deferredCount_{0};
    uint16_t panicChannels_{0};
    bool begun_{false};
};

CardputerUartMidiTransport& cardputerUartMidiTransport();

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_CARDPUTER_UART_MIDI_TRANSPORT_H
