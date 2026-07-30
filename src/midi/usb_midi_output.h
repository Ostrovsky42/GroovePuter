#pragma once
#ifndef GROOVEPUTER_USB_MIDI_OUTPUT_H
#define GROOVEPUTER_USB_MIDI_OUTPUT_H

#include <cstddef>
#include <cstdint>

#include "src/input/musical_event_router.h"
#include "usb_midi_transport.h"

struct UsbMidiRouteConfig {
    // Channels are zero-based internally and displayed as 1..16 externally.
    uint8_t performanceSynthAChannel{7};
    uint8_t patternSynthAChannel{7};
    uint8_t patternSynthBChannel{8};
    bool performanceKeyboardEnabled{true};
    bool patternPlayerEnabled{true};
};

enum class UsbMidiStatus : uint8_t {
    Off,
    Wait,
    Ready,
};

// Translates normalized GroovePuter events into fixed monophonic ownership
// lanes. Separate source/target lanes prevent live-key panic or replacement
// from cancelling PatternPlayer Synth A/B ownership.
class UsbMidiOutput final : public IMusicalEventSink {
public:
    explicit UsbMidiOutput(IUsbMidiTransport& transport,
                           UsbMidiRouteConfig config = {});

    bool begin();
    void pollConnection();

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    UsbMidiStatus status() const;

    int activeNote(MusicalEventSource source, MusicalEventTarget target) const;
    int activeNote(MusicalEventTarget target) const;
    uint8_t channelFor(MusicalEventSource source,
                       MusicalEventTarget target) const;
    uint8_t synthAChannel() const {
        return channelFor(MusicalEventSource::PerformanceKeyboard,
                          MusicalEventTarget::SynthA);
    }

    void handleMusicalEvent(const MusicalEvent& event) override;

private:
    struct MidiVoiceLane {
        MusicalEventSource source{MusicalEventSource::PerformanceKeyboard};
        MusicalEventTarget target{MusicalEventTarget::SynthA};
        uint8_t channel{7};
        int16_t activeNote{-1};
        bool enabled{false};
    };

    static constexpr std::size_t kLaneCount = 3;
    static uint8_t clampChannel(uint8_t channel);

    MidiVoiceLane* laneFor(MusicalEventSource source,
                           MusicalEventTarget target);
    const MidiVoiceLane* laneFor(MusicalEventSource source,
                                 MusicalEventTarget target) const;
    bool accepts(const MusicalEvent& event, const MidiVoiceLane* lane) const;
    void replaceActiveNote(MidiVoiceLane& lane, uint8_t note, uint8_t velocity);
    void releaseActiveNote(MidiVoiceLane& lane, uint8_t velocity = 0);
    void releaseAllActiveNotes();
    void clearActiveState();

    IUsbMidiTransport& transport_;
    MidiVoiceLane lanes_[kLaneCount]{};
    bool enabled_{true};
    bool begun_{false};
    bool mounted_{false};
};

#endif  // GROOVEPUTER_USB_MIDI_OUTPUT_H
