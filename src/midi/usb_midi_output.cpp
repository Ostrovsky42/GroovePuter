#include "usb_midi_output.h"

UsbMidiOutput::UsbMidiOutput(IUsbMidiTransport& transport,
                             UsbMidiRouteConfig config)
    : transport_(transport) {
    lanes_[0] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        clampChannel(config.performanceSynthAChannel),
        -1,
        config.performanceKeyboardEnabled,
    };
    lanes_[1] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        clampChannel(config.patternSynthAChannel),
        -1,
        config.patternPlayerEnabled,
    };
    lanes_[2] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthB,
        clampChannel(config.patternSynthBChannel),
        -1,
        config.patternPlayerEnabled,
    };
}

uint8_t UsbMidiOutput::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

bool UsbMidiOutput::begin() {
    clearActiveState();
    begun_ = transport_.begin();
    mounted_ = false;
    return begun_;
}

void UsbMidiOutput::pollConnection() {
    const bool nextMounted = begun_ && transport_.mounted();
    if (nextMounted == mounted_) return;

    // Never replay state across a USB disconnect. A new physical or sequencer
    // event after reconnect starts a fresh ownership lane.
    clearActiveState();
    mounted_ = nextMounted;
}

void UsbMidiOutput::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    if (!enabled) {
        pollConnection();
        releaseAllActiveNotes();
    }
    enabled_ = enabled;
}

UsbMidiStatus UsbMidiOutput::status() const {
    if (!enabled_ || !begun_) return UsbMidiStatus::Off;
    return mounted_ ? UsbMidiStatus::Ready : UsbMidiStatus::Wait;
}

UsbMidiOutput::MidiVoiceLane* UsbMidiOutput::laneFor(
    MusicalEventSource source,
    MusicalEventTarget target) {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source == source && lanes_[i].target == target) {
            return &lanes_[i];
        }
    }
    return nullptr;
}

const UsbMidiOutput::MidiVoiceLane* UsbMidiOutput::laneFor(
    MusicalEventSource source,
    MusicalEventTarget target) const {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source == source && lanes_[i].target == target) {
            return &lanes_[i];
        }
    }
    return nullptr;
}

int UsbMidiOutput::activeNote(MusicalEventSource source,
                              MusicalEventTarget target) const {
    const MidiVoiceLane* lane = laneFor(source, target);
    return lane ? lane->activeNote : -1;
}

int UsbMidiOutput::activeNote(MusicalEventTarget target) const {
    if (target == MusicalEventTarget::SynthA) {
        const MidiVoiceLane* live = laneFor(MusicalEventSource::PerformanceKeyboard,
                                            MusicalEventTarget::SynthA);
        if (live && live->activeNote >= 0) return live->activeNote;
        const MidiVoiceLane* pattern = laneFor(MusicalEventSource::PatternPlayer,
                                               MusicalEventTarget::SynthA);
        return pattern ? pattern->activeNote : -1;
    }
    const MidiVoiceLane* pattern = laneFor(MusicalEventSource::PatternPlayer,
                                           MusicalEventTarget::SynthB);
    return pattern ? pattern->activeNote : -1;
}

uint8_t UsbMidiOutput::channelFor(MusicalEventSource source,
                                  MusicalEventTarget target) const {
    const MidiVoiceLane* lane = laneFor(source, target);
    return lane ? lane->channel : 0;
}

bool UsbMidiOutput::accepts(const MidiVoiceLane* lane) const {
    return enabled_ && begun_ && mounted_ && lane && lane->enabled;
}

void UsbMidiOutput::replaceActiveNote(MidiVoiceLane& lane,
                                      uint8_t note,
                                      uint8_t velocity) {
    bool wrotePacket = false;

    if (lane.activeNote >= 0) {
        const uint8_t oldNote = static_cast<uint8_t>(lane.activeNote);
        // Fail closed: do not layer a replacement when the required NoteOff
        // cannot enter the TinyUSB queue. Keep ownership for a later retry.
        if (!transport_.sendNoteOff(lane.channel, oldNote, 0)) return;
        lane.activeNote = -1;
        wrotePacket = true;
    }

    if (velocity < 1) velocity = 1;
    if (transport_.sendNoteOn(lane.channel, note, velocity)) {
        lane.activeNote = static_cast<int16_t>(note);
        wrotePacket = true;
    }

    if (wrotePacket) transport_.flush();
}

void UsbMidiOutput::releaseActiveNote(MidiVoiceLane& lane, uint8_t velocity) {
    if (lane.activeNote < 0 || !mounted_) return;

    const uint8_t note = static_cast<uint8_t>(lane.activeNote);
    if (transport_.sendNoteOff(lane.channel, note, velocity)) {
        lane.activeNote = -1;
        transport_.flush();
    }
}

void UsbMidiOutput::releaseAllActiveNotes() {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        releaseActiveNote(lanes_[i]);
    }
}

void UsbMidiOutput::clearActiveState() {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        lanes_[i].activeNote = -1;
    }
}

void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    pollConnection();
    MidiVoiceLane* lane = laneFor(event.source, event.target);
    if (!accepts(lane)) return;

    switch (event.type) {
        case MusicalEventType::NoteOn:
            replaceActiveNote(*lane, event.note, event.velocity);
            break;
        case MusicalEventType::NoteOff:
            if (lane->activeNote == static_cast<int16_t>(event.note)) {
                releaseActiveNote(*lane, event.velocity);
            }
            break;
        case MusicalEventType::AllNotesOff:
            // Panic is scoped to source + target. Live panic never cuts a
            // PatternPlayer lane, and Synth A never cuts Synth B.
            releaseActiveNote(*lane);
            break;
    }
}
