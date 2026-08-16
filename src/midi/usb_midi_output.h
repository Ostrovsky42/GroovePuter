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
// Device-profile routes are frozen while begin() configures the lanes. No
// control-side profile state is re-read from the dispatcher path, so NoteOn,
// NoteOff and scoped cleanup retain one physical wire identity until restart.
class UsbMidiOutput final : public IMusicalEventSink {
public:
    static constexpr uint8_t kSeqtrakDrumLaneCount = 7;
    static constexpr uint8_t kPatternDrumVoiceCount = 8;
    static constexpr uint8_t kSeqtrakDrumNote = 60;
    static constexpr uint8_t kSeqtrakMonoPolyController = 26;
    static constexpr uint8_t kSeqtrakMonoValue = 0;
    static constexpr uint8_t kSeqtrakPolyValue = 1;

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
    uint8_t activeGateCount(MusicalEventSource source,
                            MusicalEventTarget target,
                            uint8_t logicalChannel) const;
    uint8_t channelFor(MusicalEventSource source,
                       MusicalEventTarget target) const;
    uint8_t channelFor(MusicalEventSource source,
                       MusicalEventTarget target,
                       uint8_t logicalChannel) const;
    uint8_t wireOwnerCount(uint8_t zeroBasedChannel, uint8_t note) const;
    uint8_t smfOwnerCount(uint8_t zeroBasedChannel, uint8_t note) const;
    uint8_t synthAChannel() const {
        return channelFor(MusicalEventSource::PerformanceKeyboard,
                          MusicalEventTarget::SynthA);
    }

    void handleMusicalEvent(const MusicalEvent& event) override;

    bool handleSmfNoteOn(uint8_t zeroBasedChannel,
                         uint8_t note,
                         uint8_t velocity);
    bool handleSmfNoteOff(uint8_t zeroBasedChannel,
                          uint8_t note,
                          uint8_t velocity = 0);
    bool handleSmfSongPositionPointer(uint16_t midiBeats) {
        return enabled_ && begun_ && mounted_ &&
               transport_.sendSongPositionPointer(midiBeats);
    }
    bool releaseAllSmfNotes();
    void abandonAllSmfNotes();

private:
    struct MidiVoiceLane {
        MusicalEventSource source;
        MusicalEventTarget target;
        uint8_t logicalChannel;
        uint8_t channel;
        int16_t activeNote;
        uint8_t activeCount;
        bool enabled;
        bool pendingRelease;
    };

    enum class PerformanceReceiverMode : uint8_t {
        Unknown = 0,
        Mono,
        Poly,
    };

    static constexpr std::size_t kLaneCount = 20;
    static constexpr std::size_t kMidiChannelCount = 16;
    static constexpr std::size_t kMidiNoteCount = 128;
    static constexpr std::size_t kGeneratedTargetCount = 3;
    static constexpr std::size_t kGeneratedBitsetBytes = kMidiNoteCount / 8;

    static uint8_t clampChannel(uint8_t channel);
    static uint8_t clampDataByte(uint8_t value);
    static uint8_t patternDrumChannel(uint8_t logicalVoice);
    static int generatedTargetIndex(MusicalEventTarget target);
    static bool isSynthPerformanceSource(MusicalEventSource source);
    static bool sourceRequestsPolyReceiver(MusicalEventSource source);

    void configureLanes();
    uint8_t wireNoteFor(const MidiVoiceLane& lane, uint8_t eventNote) const;
    uint8_t generatedChannel(MusicalEventTarget target) const;
    void ensurePerformanceReceiverMode(MusicalEventTarget target,
                                       bool polyphonic);
    bool generatedNoteActive(int targetIndex, uint8_t note) const;
    bool generatedNotePendingRelease(int targetIndex, uint8_t note) const;
    void setGeneratedNoteActive(int targetIndex, uint8_t note, bool active);
    void setGeneratedNotePendingRelease(int targetIndex,
                                        uint8_t note,
                                        bool pending);
    int generatedActiveNote(MusicalEventTarget target) const;
    uint8_t generatedActiveCount(MusicalEventTarget target) const;
    bool retryGeneratedPendingReleases(MusicalEventTarget target);
    bool acquireGeneratedNote(MusicalEventTarget target,
                              uint8_t note,
                              uint8_t velocity);
    bool releaseGeneratedNote(MusicalEventTarget target,
                              uint8_t note,
                              uint8_t velocity = 0);
    bool releaseGeneratedTarget(MusicalEventTarget target);
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
    bool acquirePercussiveNote(MidiVoiceLane& lane,
                               uint8_t note,
                               uint8_t velocity);
    bool releasePercussiveNote(MidiVoiceLane& lane,
                               uint8_t velocity = 0);
    bool releasePercussiveLane(MidiVoiceLane& lane,
                               uint8_t velocity = 0);
    void releaseTargetAllNotes(MusicalEventSource source,
                               MusicalEventTarget target);
    void releaseAllActiveNotes();
    bool releaseAbandonedSmfChannels();
    void clearActiveState();

    IUsbMidiTransport& transport_;
    UsbMidiRouteConfig config_;
    MidiVoiceLane lanes_[kLaneCount];
    uint8_t generatedActive_[kGeneratedTargetCount][kGeneratedBitsetBytes];
    uint8_t generatedPendingRelease_[kGeneratedTargetCount][kGeneratedBitsetBytes];
    PerformanceReceiverMode performanceReceiverMode_[kGeneratedTargetCount];
    uint8_t wireOwners_[kMidiChannelCount][kMidiNoteCount];
    uint8_t smfOwners_[kMidiChannelCount][kMidiNoteCount];
    uint8_t patternDrumNotes_[kPatternDrumVoiceCount];
    uint8_t performanceDrumNotes_[kSeqtrakDrumLaneCount];
    uint16_t abandonedSmfChannels_;
    bool patternStartupRoutesBound_;
    bool performanceStartupRoutesComplete_;
    bool seqtrakReceiverModeControl_;
    bool enabled_;
    bool begun_;
    bool mounted_;
};

#endif  // GROOVEPUTER_USB_MIDI_OUTPUT_H
