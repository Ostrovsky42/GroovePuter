#pragma once
#ifndef GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H
#define GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H

#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

// Wire pacing for a 31250 baud, 8N1 DIN link.
//
// One byte costs 10 bit times, so 320 us; a three-byte NoteOn costs 960 us.
// USB writes a packet into a FIFO and returns, DIN does not: the same eight
// note chord that is free on USB occupies about 7.7 ms of wire, a third of a
// 23.2 ms audio block. The transport therefore owns a bounded queue and the
// dispatcher never waits on the wire.
constexpr uint32_t kUartMidiBaud = 31250;
constexpr uint32_t kUartMidiByteMicros = 320;

// 256 bytes is roughly 82 ms of wire time: large enough to absorb a chord
// burst, small enough that a backlog cannot age into musical nonsense.
constexpr std::size_t kUartMidiRingBytes = 256;

// Reserved tail, in bytes, usable only by traffic that must not be dropped.
// Sixteen three-byte NoteOff messages.
constexpr std::size_t kUartMidiCriticalReserveBytes = 48;

// System Real-Time messages are single bytes and are legal between any two
// bytes of another message, so they ride a separate lane that drains first.
// Queueing clock behind a full data ring would add up to the ring's whole wire
// time to every tick; at 120 BPM the 24 PPQN period is only 20.8 ms.
constexpr std::size_t kUartMidiRealtimeLaneBytes = 8;

enum class UartMidiPriority : uint8_t {
    // NoteOn and other musical traffic. Droppable: if it never reaches the
    // wire nothing sounded, so nothing needs cleanup.
    Musical = 0,
    // NoteOff, CC123 and other recovery traffic. May use the reserve.
    Critical = 1,
};

struct UartMidiDiagnostics {
    uint32_t messagesQueued{0};
    uint32_t bytesSent{0};
    uint32_t droppedMusical{0};
    uint32_t droppedCritical{0};
    uint32_t droppedRealtime{0};
    uint16_t maxFillBytes{0};
};

// Byte-oriented, fixed-capacity, no allocation, single producer / single
// consumer in the same sense as the existing scheduled queues: the dispatcher
// enqueues, the transport drain drains.
class UartMidiTransportCore {
public:
    void reset() {
        head_ = tail_ = count_ = 0;
        rtHead_ = rtTail_ = rtCount_ = 0;
        diagnostics_ = UartMidiDiagnostics{};
    }

    std::size_t pendingBytes() const { return count_ + rtCount_; }
    std::size_t freeBytes() const { return kUartMidiRingBytes - count_; }
    bool empty() const { return count_ == 0 && rtCount_ == 0; }

    const UartMidiDiagnostics& diagnostics() const { return diagnostics_; }

    // Whole messages only. A partial message must never reach the wire, so
    // enqueue is all-or-nothing.
    bool enqueue(const uint8_t* bytes,
                 std::size_t length,
                 UartMidiPriority priority) {
        if (bytes == nullptr || length == 0) return false;
        if (length > kUartMidiRingBytes) return false;

        const std::size_t usable = priority == UartMidiPriority::Critical
            ? kUartMidiRingBytes
            : kUartMidiRingBytes - kUartMidiCriticalReserveBytes;
        if (count_ + length > usable) {
            if (priority == UartMidiPriority::Critical) {
                ++diagnostics_.droppedCritical;
            } else {
                ++diagnostics_.droppedMusical;
            }
            return false;
        }

        for (std::size_t i = 0; i < length; ++i) {
            ring_[head_] = bytes[i];
            head_ = advance(head_, kUartMidiRingBytes);
        }
        count_ += length;
        ++diagnostics_.messagesQueued;
        noteFill();
        return true;
    }

    // System Real-Time. Single byte, own lane, drains ahead of data so a data
    // backlog cannot delay the clock.
    bool enqueueRealtime(uint8_t status) {
        if (status < 0xF8u) return false;
        if (rtCount_ >= kUartMidiRealtimeLaneBytes) {
            ++diagnostics_.droppedRealtime;
            return false;
        }
        realtime_[rtHead_] = status;
        rtHead_ = advance(rtHead_, kUartMidiRealtimeLaneBytes);
        ++rtCount_;
        ++diagnostics_.messagesQueued;
        return true;
    }

    // `write` receives a contiguous span and returns how many bytes the wire
    // actually accepted. Returning less than offered is normal backpressure,
    // not an error, and leaves the remainder queued. `budget` bounds the work
    // done in one call so a saturated link cannot starve the caller.
    template <typename WriteFn>
    std::size_t drain(WriteFn&& write, std::size_t budget) {
        std::size_t written = 0;

        while (rtCount_ > 0 && written < budget) {
            const uint8_t status = realtime_[rtTail_];
            if (write(&status, 1) != 1) return written;
            rtTail_ = advance(rtTail_, kUartMidiRealtimeLaneBytes);
            --rtCount_;
            ++written;
            ++diagnostics_.bytesSent;
        }

        while (count_ > 0 && written < budget) {
            // Contiguous run up to the ring wrap; the wire is a byte stream so
            // splitting at the wrap is invisible downstream.
            std::size_t run = kUartMidiRingBytes - tail_;
            if (run > count_) run = count_;
            if (run > budget - written) run = budget - written;

            const std::size_t accepted = write(&ring_[tail_], run);
            if (accepted == 0) break;

            tail_ = (tail_ + accepted) % kUartMidiRingBytes;
            count_ -= accepted;
            written += accepted;
            diagnostics_.bytesSent += static_cast<uint32_t>(accepted);
            if (accepted < run) break;
        }

        return written;
    }

private:
    static std::size_t advance(std::size_t index, std::size_t capacity) {
        return (index + 1) % capacity;
    }

    void noteFill() {
        if (count_ > diagnostics_.maxFillBytes) {
            diagnostics_.maxFillBytes = static_cast<uint16_t>(count_);
        }
    }

    uint8_t ring_[kUartMidiRingBytes]{};
    uint8_t realtime_[kUartMidiRealtimeLaneBytes]{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t count_{0};
    std::size_t rtHead_{0};
    std::size_t rtTail_{0};
    std::size_t rtCount_{0};
    UartMidiDiagnostics diagnostics_{};
};

static_assert(sizeof(UartMidiTransportCore) <= 384,
              "the DIN transport queue must stay inside the DRAM headroom "
              "freed by sparse note ownership");

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H
