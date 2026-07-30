#include "usb_midi_output.h"

UsbMidiOutput::UsbMidiOutput(IUsbMidiTransport& transport,
                             UsbMidiRouteConfig config)
    : transport_(transport),
      channel_(clampChannel(config.synthAChannel)),
      enabled_(config.performanceKeyboardEnabled) {}

uint8_t UsbMidiOutput::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

bool UsbMidiOutput::begin() {
    clearActiveState();
    begun_ = transport_.begin();
    mounted_ = begun_ && transport_.mounted();
    return begun_;
}

void UsbMidiOutput::pollConnection() {
    const bool nextMounted = begun_ && transport_.mounted();
    if (nextMounted == mounted_) return;

    // A disconnect makes the host-side note state unknowable. Do not queue or
    // replay stale events; the next physical key press after reconnect starts a
    // fresh monophonic stream.
    clearActiveState();
    mounted_ = nextMounted;
}

void UsbMidiOutput::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    if (!enabled) {
        pollConnection();
        releaseActiveNote();
    }
    enabled_ = enabled;
}

UsbMidiStatus UsbMidiOutput::status() const {
    if (!enabled_ || !begun_) return UsbMidiStatus::Off;
    return mounted_ ? UsbMidiStatus::Ready : UsbMidiStatus::Wait;
}

int UsbMidiOutput::activeNote(MusicalEventTarget target) const {
    return target == MusicalEventTarget::SynthA ? activeNote_ : -1;
}

bool UsbMidiOutput::accepts(const MusicalEvent& event) const {
    return enabled_ && begun_ && mounted_ &&
           event.source == MusicalEventSource::PerformanceKeyboard &&
           event.target == MusicalEventTarget::SynthA;
}

void UsbMidiOutput::replaceActiveNote(uint8_t note, uint8_t velocity) {
    bool wrotePacket = false;

    if (activeNote_ >= 0) {
        wrotePacket = transport_.sendNoteOff(
            channel_, static_cast<uint8_t>(activeNote_), 0) || wrotePacket;
        activeNote_ = -1;
    }

    if (transport_.sendNoteOn(channel_, note, velocity)) {
        activeNote_ = static_cast<int16_t>(note);
        wrotePacket = true;
    }

    if (wrotePacket) transport_.flush();
}

void UsbMidiOutput::releaseActiveNote(uint8_t velocity) {
    if (activeNote_ < 0) return;

    const uint8_t note = static_cast<uint8_t>(activeNote_);
    activeNote_ = -1;
    if (mounted_ && transport_.sendNoteOff(channel_, note, velocity)) {
        transport_.flush();
    }
}

void UsbMidiOutput::clearActiveState() {
    activeNote_ = -1;
}

void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    pollConnection();
    if (!accepts(event)) return;

    switch (event.type) {
        case MusicalEventType::NoteOn:
            replaceActiveNote(event.note, event.velocity);
            break;
        case MusicalEventType::NoteOff:
            if (activeNote_ == static_cast<int16_t>(event.note)) {
                releaseActiveNote(event.velocity);
            }
            break;
        case MusicalEventType::AllNotesOff:
            releaseActiveNote();
            break;
    }
}
