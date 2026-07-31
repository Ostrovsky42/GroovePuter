#pragma once
#ifndef GROOVEPUTER_USB_MIDI_OUTPUT_H
#define GROOVEPUTER_USB_MIDI_OUTPUT_H

#include <cstddef>
#include <cstdint>

#include "src/input/musical_event_router.h"
#include "usb_midi_transport.h"

struct UsbMidiRouteConfig {
    // Channels are zero-based internally and displayed as 1..16 externally.
    // Keep the original five fields first so existing aggregate initializers
    // preserve their meaning. Later live-target fields extend the contract.
    uint8_t performanceSynthAChannel{7};
    uint8_t patternSynthAChannel{7};
    uint8_t patternSynthBChannel{8};
    bool performanceKeyboardEnabled{true};
    bool patternPlayerEnabled{true};
    uint8_t performanceSynthBChannel{8};
    // Kept for source compatibility with the previous single-lane Drums stage.
    // Native SEQTRAK performance drums now use fixed channels 0..6 (CH1..7).
    uint8_t performanceDrumsChannel{9};
    uint8_t performanceDxChannel{9};
};

enum class UsbMidiStatus : uint8_t {
    Off,
    Wait,
    Ready,
};

// Translates normalized GroovePuter events into fixed logical lanes.
// Synth/DX lanes are monophonic. Live Drums owns seven independent native
// SEQTRAK lanes (logical 0..6 -> MIDI CH1..7), allowing simultaneous pads.
// Wire-level channel+note ownership remains reference counted so two logical
// owners sharing one physical MIDI note cannot accidentally silence each other.
class UsbMidiOutput final : public IMusicalEventSink {
public:
    static constexpr uint8_t kSeqtrakDrumLaneCount = 7;

    explicit UsbMidiOutput(IUsbMidiTransport& transport,
                           UsbMidiRouteConfig config = {});

    bool begin();
    void pollConnection();

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    UsbMidiStatus status() const;

    int activeNote(MusicalEventSource source, MusicalEventTarget target) const;
    int activeNote(MusicalEventSource source,
                   MusicalEventTarget target,
                   uint8_t logicalChannel) const;
    int activeNote(MusicalEventTarget target) const;
    uint8_t channelFor(MusicalEventSource source,
                       MusicalEventTarget target) const;
    uint8_t channelFor(MusicalEventSource source,
                       MusicalEventTarget target,
                       uint8_t logicalChannel) const;
    uint8_t wireOwnerCount(uint8_t zeroBasedChannel, uint8_t note) const;
    uint8_t synthAChannel() const {
        return channelFor(MusicalEventSource::PerformanceKeyboard,
                          MusicalEventTarget::SynthA);
    }

    void handleMusicalEvent(const MusicalEvent& event) override;

private:
    struct MidiVoiceLane {
        MusicalEventSource source{MusicalEventSource::PerformanceKeyboard};
        MusicalEventTarget target{MusicalEventTarget::SynthA};
        uint8_t logicalChannel{0};
        uint8_t channel{7};
        int16_t activeNote{-1};
        bool enabled{false};
        bool pendingRelease{false};
    };

    static constexpr std::size_t kLaneCount = 12;
    static constexpr std::size_t kMidiChannelCount = 16;
    static constexpr std::size_t kMidiNoteCount = 128;

    static uint8_t clampChannel(uint8_t channel);
    static uint8_t clampDataByte(uint8_t value);

    MidiVoiceLane* laneFor(MusicalEventSource source,
                           MusicalEventTarget target,
                           uint8_t logicalChannel = 0);
    const MidiVoiceLane* laneFor(MusicalEventSource source,
                                 MusicalEventTarget target,
                                 uint8_t logicalChannel = 0) const;
    bool accepts(const MidiVoiceLane* lane) const;
    bool acquireActiveNote(MidiVoiceLane& lane,
                           uint8_t note,
                           uint8_t velocity);
    bool replaceActiveNote(MidiVoiceLane& lane,
                           uint8_t note,
                           uint8_t velocity);
    bool releaseActiveNote(MidiVoiceLane& lane, uint8_t velocity = 0);
    void releaseTargetAllNotes(MusicalEventSource source,
                               MusicalEventTarget target);
    void releaseAllActiveNotes();
    void clearActiveState();

    IUsbMidiTransport& transport_;
    MidiVoiceLane lanes_[kLaneCount]{};
    uint8_t wireOwners_[kMidiChannelCount][kMidiNoteCount]{};
    bool enabled_{true};
    bool begun_{false};
    bool mounted_{false};
};

#endif  // GROOVEPUTER_USB_MIDI_OUTPUT_H
