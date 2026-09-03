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
HardwareSerial& uart() {
    static HardwareSerial port(1);
    return port;
}
#endif
}  // namespace

bool CardputerUartMidiTransport::begin() {
    if (begun_) return true;
    core_.reset();
    deferredCount_ = 0;
    panicChannels_ = 0;
#if defined(ARDUINO_M5STACK_CARDPUTER)
    uart().setTxBufferSize(kCardputerUartMidiTxBufferBytes);
    uart().begin(static_cast<unsigned long>(kUartMidiBaud),
                 SERIAL_8N1,
                 kCardputerUartMidiRxPin,
                 kCardputerUartMidiTxPin);
#endif
    begun_ = true;
    return true;
}

bool CardputerUartMidiTransport::queueChannelMessage(
    uint8_t status,
    uint8_t zeroBasedChannel,
    uint8_t data1,
    uint8_t data2,
    UartMidiPriority priority) {
    if (!begun_) return false;
    // Deliberately no running status: WIRE capacity calculations assume every
    // channel voice message is exactly three bytes.
    const uint8_t bytes[3] = {
        static_cast<uint8_t>(status | clampChannel(zeroBasedChannel)),
        clampDataByte(data1),
        clampDataByte(data2),
    };
    return core_.enqueue(bytes, sizeof(bytes), priority);
}

bool CardputerUartMidiTransport::sendNoteOn(uint8_t channel,
                                            uint8_t note,
                                            uint8_t velocity) {
    return queueChannelMessage(kStatusNoteOn, channel, note, velocity,
                               UartMidiPriority::Musical);
}

bool CardputerUartMidiTransport::sendNoteOff(uint8_t channel,
                                             uint8_t note,
                                             uint8_t velocity) {
    if (queueChannelMessage(kStatusNoteOff, channel, note, velocity,
                            UartMidiPriority::Critical)) {
        return true;
    }
    return deferNoteOff(clampChannel(channel), clampDataByte(note));
}

bool CardputerUartMidiTransport::deferNoteOff(uint8_t channel, uint8_t note) {
    if (!begun_) return false;
    for (std::size_t i = 0; i < deferredCount_; ++i) {
        if (deferred_[i].channel == channel && deferred_[i].note == note) return true;
    }
    if (deferredCount_ >= kDeferredNoteOffCapacity) {
        panicChannels_ |= static_cast<uint16_t>(1u << channel);
        return false;
    }
    deferred_[deferredCount_++] = DeferredNoteOff{channel, note};
    return true;
}

void CardputerUartMidiTransport::retryDeferredNoteOffs() {
    std::size_t i = 0;
    while (i < deferredCount_) {
        const uint8_t bytes[3] = {
            static_cast<uint8_t>(kStatusNoteOff | deferred_[i].channel),
            deferred_[i].note,
            0,
        };
        if (!core_.enqueue(bytes, sizeof(bytes), UartMidiPriority::Critical)) {
            ++i;
            continue;
        }
        deferred_[i] = deferred_[deferredCount_ - 1];
        --deferredCount_;
    }
}

void CardputerUartMidiTransport::retryChannelPanics() {
    if (panicChannels_ == 0) return;
    for (uint8_t channel = 0; channel < 16; ++channel) {
        const uint16_t mask = static_cast<uint16_t>(1u << channel);
        if ((panicChannels_ & mask) == 0) continue;
        const uint8_t bytes[3] = {
            static_cast<uint8_t>(kStatusControlChange | channel), 123, 0,
        };
        if (!core_.enqueue(bytes, sizeof(bytes), UartMidiPriority::Critical)) return;
        panicChannels_ &= static_cast<uint16_t>(~mask);
    }
}

bool CardputerUartMidiTransport::sendControlChange(uint8_t channel,
                                                   uint8_t controller,
                                                   uint8_t value) {
    return queueChannelMessage(kStatusControlChange, channel, controller, value,
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
    if (!begun_) return;
    retryDeferredNoteOffs();
    retryChannelPanics();
    if (core_.empty()) return;
#if defined(ARDUINO_M5STACK_CARDPUTER)
    core_.drain(
        [](const uint8_t* data, std::size_t length) -> std::size_t {
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
