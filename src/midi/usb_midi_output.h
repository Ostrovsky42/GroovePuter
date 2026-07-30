#pragma once

#include <cstdint>

#include "src/input/musical_event_router.h"
#include "usb_midi_transport.h"

struct UsbMidiRouteConfig {
    // Zero-based MIDI channel. Channel 7 is displayed as MIDI channel 8 and
    // matches SEQTRAK SYNTH 1 in the first hardware spike.
    uint8_t synthAChannel{7};
    bool performanceKeyboardEnabled{true};
};

enum class UsbMidiStatus : uint8_t {
    Off,
    Wait,
    Ready,
};

// Translates normalized GroovePuter musical events into a monophonic USB-MIDI
// stream. The sink owns only external MIDI state; it never mutates DSP, scenes,
// transport, or UI state.
class UsbMidiOutput final : public IMusicalEventSink {
public:
    explicit UsbMidiOutput(IUsbMidiTransport& transport,
                           UsbMidiRouteConfig config = {});

    bool begin();
    void pollConnection();

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    UsbMidiStatus status() const;

    int activeNote(MusicalEventTarget target) const;
    uint8_t synthAChannel() const { return channel_; }

    void handleMusicalEvent(const MusicalEvent& event) override;

private:
    static uint8_t clampChannel(uint8_t channel);
    bool accepts(const MusicalEvent& event) const;
    void replaceActiveNote(uint8_t note, uint8_t velocity);
    void releaseActiveNote(uint8_t velocity = 0);
    void clearActiveState();

    IUsbMidiTransport& transport_;
    uint8_t channel_{7};
    int16_t activeNote_{-1};
    bool enabled_{true};
    bool begun_{false};
    bool mounted_{false};
};
