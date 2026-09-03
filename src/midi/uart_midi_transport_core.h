#pragma once
#ifndef GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H
#define GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H

#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

constexpr uint32_t kUartMidiBaud = 31250;
constexpr uint32_t kUartMidiByteMicros = 320;
constexpr std::size_t kUartMidiRingBytes = 256;
constexpr std::size_t kUartMidiCriticalReserveBytes = 48;
constexpr std::size_t kUartMidiRealtimeLaneBytes = 8;

enum class UartMidiPriority : uint8_t { Musical = 0, Critical = 1 };

struct UartMidiDiagnostics {
    uint32_t messagesQueued{0};
    uint32_t bytesSent{0};
    uint32_t droppedMusical{0};
    uint32_t droppedCritical{0};
    uint32_t droppedRealtime{0};
    uint16_t maxFillBytes{0};
};

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

    bool enqueue(const uint8_t* bytes,
                 std::size_t length,
                 UartMidiPriority priority) {
        if (bytes == nullptr || length == 0 || length > kUartMidiRingBytes) return false;
        const std::size_t usable = priority == UartMidiPriority::Critical
            ? kUartMidiRingBytes
            : kUartMidiRingBytes - kUartMidiCriticalReserveBytes;
        if (count_ + length > usable) {
            if (priority == UartMidiPriority::Critical) ++diagnostics_.droppedCritical;
            else ++diagnostics_.droppedMusical;
            return false;
        }
        for (std::size_t i = 0; i < length; ++i) {
            ring_[head_] = bytes[i];
            head_ = advance(head_, kUartMidiRingBytes);
        }
        count_ += length;
        ++diagnostics_.messagesQueued;
        if (count_ > diagnostics_.maxFillBytes) {
            diagnostics_.maxFillBytes = static_cast<uint16_t>(count_);
        }
        return true;
    }

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
              "DIN transport queue exceeds the protected DRAM budget");

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_UART_MIDI_TRANSPORT_CORE_H
