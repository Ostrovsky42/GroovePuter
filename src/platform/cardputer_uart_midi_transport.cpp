#include "cardputer_uart_midi_transport.h"

#if defined(ARDUINO_M5STACK_CARDPUTER)
#include <Arduino.h>
#include <HardwareSerial.h>
#endif

namespace GroovePuterMidi {
namespace {

constexpr uint8_t kStatusNoteOff = 0x80;
constexpr uint8_t kStatusNoteOn = 0x90;
constexpr uint8_t kStatusControlChange = 0xB0;
constexpr uint8_t kStatusSongPositionPointer = 0xF2;
constexpr uint8_t kStatusTimingClock = 0xF8;
constexpr uint8_t kStatusStart = 0xFA;
constexpr uint8_t kStatusContinue = 0xFB;
constexpr uint8_t kStatusStop = 0xFC;

uint8_t clampChannel(uint8_t channel) { return channel & 0x0Fu; }
uint8_t clampDataByte(uint8_t value) { return value & 0x7Fu; }

#if defined(ARDUINO_M5STACK_CARDPUTER)
// UART1 is free on Cardputer ADV: UART0 is the USB CDC console.
HardwareSerial& uart() {
    static HardwareSerial port(1);
    return port;
}
#endif

}  // namespace

bool CardputerUartMidiTransport::begin() {
    if (begun_) return true;
    core_.reset();
#if defined(ARDUINO_M5STACK_CARDPUTER)
    uart().begin(static_cast<unsigned long>(kUartMidiBaud),
                 SERIAL_8N1,
                 kCardputerUartMidiRxPin,
                 kCardputerUartMidiTxPin);
    begun_ = true;
#else
    // Host builds have no UART. The transport stays queue-only so the
    // ownership and routing layers above remain testable off target.
    begun_ = true;
#endif
    return begun_;
}

bool CardputerUartMidiTransport::queueChannelMessage(
    uint8_t status,
    uint8_t zeroBasedChannel,
    uint8_t data1,
    uint8_t data2,
    UartMidiPriority priority) {
    if (!begun_) return false;
    const uint8_t bytes[3] = {
        static_cast<uint8_t>(status | clampChannel(zeroBasedChannel)),
        clampDataByte(data1),
        clampDataByte(data2),
    };
    return core_.enqueue(bytes, sizeof(bytes), priority);
}

bool CardputerUartMidiTransport::sendNoteOn(uint8_t zeroBasedChannel,
                                            uint8_t note,
                                            uint8_t velocity) {
    // Musical priority: a refused NoteOn never reached the wire, so it carries
    // no cleanup obligation.
    return queueChannelMessage(kStatusNoteOn, zeroBasedChannel, note, velocity,
                               UartMidiPriority::Musical);
}

bool CardputerUartMidiTransport::sendNoteOff(uint8_t zeroBasedChannel,
                                             uint8_t note,
                                             uint8_t velocity) {
    // Critical: dropping this is what strands a sounding note.
    return queueChannelMessage(kStatusNoteOff, zeroBasedChannel, note, velocity,
                               UartMidiPriority::Critical);
}

bool CardputerUartMidiTransport::sendControlChange(uint8_t zeroBasedChannel,
                                                   uint8_t controller,
                                                   uint8_t value) {
    // The only CC the output path uses is the all-notes-off recovery, so it
    // shares NoteOff priority.
    return queueChannelMessage(kStatusControlChange, zeroBasedChannel,
                               controller, value,
                               UartMidiPriority::Critical);
}

bool CardputerUartMidiTransport::sendTimingClock() {
    return begun_ && core_.enqueueRealtime(kStatusTimingClock);
}

bool CardputerUartMidiTransport::sendStart() {
    return begun_ && core_.enqueueRealtime(kStatusStart);
}

bool CardputerUartMidiTransport::sendContinue() {
    return begun_ && core_.enqueueRealtime(kStatusContinue);
}

bool CardputerUartMidiTransport::sendStop() {
    return begun_ && core_.enqueueRealtime(kStatusStop);
}

bool CardputerUartMidiTransport::sendSongPositionPointer(uint16_t midiBeats) {
    if (!begun_) return false;
    const uint16_t beats = midiBeats & 0x3FFFu;
    const uint8_t bytes[3] = {
        kStatusSongPositionPointer,
        static_cast<uint8_t>(beats & 0x7Fu),
        static_cast<uint8_t>((beats >> 7) & 0x7Fu),
    };
    return core_.enqueue(bytes, sizeof(bytes), UartMidiPriority::Critical);
}

void CardputerUartMidiTransport::service() {
    if (!begun_ || core_.empty()) return;
#if defined(ARDUINO_M5STACK_CARDPUTER)
    core_.drain(
        [](const uint8_t* data, std::size_t length) -> std::size_t {
            // Never offer more than the driver can take without blocking:
            // write() blocks once its own buffer is full, and blocking here
            // would stall the dispatcher on a 320 us-per-byte wire.
            const int room = uart().availableForWrite();
            if (room <= 0) return 0;
            std::size_t writable = static_cast<std::size_t>(room);
            if (writable > length) writable = length;
            return uart().write(data, writable);
        },
        kCardputerUartMidiDrainBudget);
#else
    core_.drain(
        [](const uint8_t*, std::size_t length) -> std::size_t { return length; },
        kCardputerUartMidiDrainBudget);
#endif
}

CardputerUartMidiTransport& cardputerUartMidiTransport() {
    static CardputerUartMidiTransport transport;
    return transport;
}

}  // namespace GroovePuterMidi
