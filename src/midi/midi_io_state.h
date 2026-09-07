#pragma once
#ifndef GROOVEPUTER_MIDI_IO_STATE_H
#define GROOVEPUTER_MIDI_IO_STATE_H

#include <cstdint>

#include "midi_input_event.h"

namespace GroovePuterMidi {

enum class UsbRole : uint8_t {
    Off,
    Device,
    Host,
};

enum class UsbPhase : uint8_t {
    Off,
    Waiting,
    Enumerating,
    Ready,
    Fault,
};

struct MidiRoutes {
    bool usbInputEnabled{false};
    bool uartInputEnabled{false};
    bool usbOutputEnabled{false};
    bool uartOutputEnabled{false};

    constexpr bool operator==(const MidiRoutes& other) const {
        return usbInputEnabled == other.usbInputEnabled &&
               uartInputEnabled == other.uartInputEnabled &&
               usbOutputEnabled == other.usbOutputEnabled &&
               uartOutputEnabled == other.uartOutputEnabled;
    }
};

class MidiIoState {
public:
    void setRoutes(MidiRoutes routes) { routes_ = routes; }
    MidiRoutes routes() const { return routes_; }

    void requestUsbRole(UsbRole role) { pendingUsbRole_ = role; }
    UsbRole activeUsbRole() const { return activeUsbRole_; }
    UsbRole pendingUsbRole() const { return pendingUsbRole_; }

    void boot() {
        activeUsbRole_ = pendingUsbRole_;
        usbPhase_ = activeUsbRole_ == UsbRole::Off ? UsbPhase::Off : UsbPhase::Waiting;
        usbCanReceive_ = false;
        usbCanSend_ = false;
        ++usbSessionGeneration_;
    }

    void usbAttached() {
        if (activeUsbRole_ == UsbRole::Off) return;
        usbPhase_ = UsbPhase::Enumerating;
        usbCanReceive_ = false;
        usbCanSend_ = false;
    }

    void usbReady(bool canReceive, bool canSend) {
        if (usbPhase_ != UsbPhase::Enumerating) return;
        usbPhase_ = UsbPhase::Ready;
        usbCanReceive_ = canReceive;
        usbCanSend_ = canSend;
        ++usbSessionGeneration_;
    }

    void usbDetached() {
        if (activeUsbRole_ == UsbRole::Off) return;
        usbPhase_ = UsbPhase::Waiting;
        usbCanReceive_ = false;
        usbCanSend_ = false;
        ++usbSessionGeneration_;
    }

    void usbFault() {
        usbPhase_ = UsbPhase::Fault;
        usbCanReceive_ = false;
        usbCanSend_ = false;
        ++usbSessionGeneration_;
    }

    UsbPhase usbPhase() const { return usbPhase_; }
    bool usbCanReceive() const { return usbCanReceive_; }
    bool usbCanSend() const { return usbCanSend_; }
    uint32_t usbSessionGeneration() const { return usbSessionGeneration_; }

    bool acceptsInput(const MidiInputEvent& event) const {
        if (event.id.channel >= 16 || event.id.key >= 128) return false;
        if (event.id.source == InputSource::Qwerty) return true;
        if (event.id.source == InputSource::Uart) return routes_.uartInputEnabled;
        return routes_.usbInputEnabled && usbPhase_ == UsbPhase::Ready &&
               usbCanReceive_ && event.id.generation == usbSessionGeneration_;
    }

private:
    MidiRoutes routes_{};
    UsbRole activeUsbRole_{UsbRole::Off};
    UsbRole pendingUsbRole_{UsbRole::Off};
    UsbPhase usbPhase_{UsbPhase::Off};
    uint32_t usbSessionGeneration_{0};
    bool usbCanReceive_{false};
    bool usbCanSend_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_IO_STATE_H
