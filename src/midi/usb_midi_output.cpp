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
        false,
    };
    lanes_[1] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        clampChannel(config.patternSynthAChannel),
        -1,
        config.patternPlayerEnabled,
        false,
    };
    lanes_[2] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthB,
        clampChannel(config.patternSynthBChannel),
        -1,
        config.patternPlayerEnabled,
        false,
    };
}

uint8_t UsbMidiOutput::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

uint8_t UsbMidiOutput::clampDataByte(uint8_t value) {
    return value > 127 ? 127 : value;
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
    // event after reconnect starts fresh logical and wire-level ownership.
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

uint8_t UsbMidiOutput::wireOwnerCount(uint8_t zeroBasedChannel,
                                      uint8_t note) const {
    return wireOwners_[clampChannel(zeroBasedChannel)][clampDataByte(note)];
}

bool UsbMidiOutput::accepts(const MidiVoiceLane* lane) const {
    return enabled_ && begun_ && mounted_ && lane && lane->enabled;
}

bool UsbMidiOutput::acquireActiveNote(MidiVoiceLane& lane,
                                      uint8_t note,
                                      uint8_t velocity) {
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);
    if (velocity < 1) velocity = 1;

    uint8_t& owners = wireOwners_[lane.channel][note];
    if (owners == 0) {
        if (!transport_.sendNoteOn(lane.channel, note, velocity)) return false;
        transport_.flush();
    }

    if (owners < 255) ++owners;
    lane.activeNote = static_cast<int16_t>(note);
    lane.pendingRelease = false;
    return true;
}

bool UsbMidiOutput::releaseActiveNote(MidiVoiceLane& lane, uint8_t velocity) {
    if (lane.activeNote < 0) {
        lane.pendingRelease = false;
        return true;
    }

    const uint8_t note = clampDataByte(static_cast<uint8_t>(lane.activeNote));
    uint8_t& owners = wireOwners_[lane.channel][note];

    // Another logical lane still owns the same physical channel+note. Release
    // only this lane; a wire NoteOff would incorrectly silence the other owner.
    if (owners > 1) {
        --owners;
        lane.activeNote = -1;
        lane.pendingRelease = false;
        return true;
    }

    // Recover conservatively from any local bookkeeping mismatch.
    if (owners == 0) {
        lane.activeNote = -1;
        lane.pendingRelease = false;
        return true;
    }

    velocity = clampDataByte(velocity);
    if (!mounted_ || !transport_.sendNoteOff(lane.channel, note, velocity)) {
        // Keep the actual old note as lane ownership. The next event for this
        // lane must retry this release even when that event names a newer note.
        lane.pendingRelease = true;
        return false;
    }

    owners = 0;
    lane.activeNote = -1;
    lane.pendingRelease = false;
    transport_.flush();
    return true;
}

bool UsbMidiOutput::replaceActiveNote(MidiVoiceLane& lane,
                                      uint8_t note,
                                      uint8_t velocity) {
    if (lane.pendingRelease && !releaseActiveNote(lane)) return false;
    if (lane.activeNote >= 0 && !releaseActiveNote(lane)) return false;
    return acquireActiveNote(lane, note, velocity);
}

void UsbMidiOutput::releaseAllActiveNotes() {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        releaseActiveNote(lanes_[i]);
    }
}

void UsbMidiOutput::clearActiveState() {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        lanes_[i].activeNote = -1;
        lanes_[i].pendingRelease = false;
    }
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
            wireOwners_[channel][note] = 0;
        }
    }
}

void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    pollConnection();
    MidiVoiceLane* lane = laneFor(event.source, event.target);
    if (!accepts(lane)) return;

    // A failed replacement NoteOff leaves the old physical note as the only
    // truth. Retry it before interpreting any later event from the sequencer.
    if (lane->pendingRelease && !releaseActiveNote(*lane)) return;

    switch (event.type) {
        case MusicalEventType::NoteOn:
            replaceActiveNote(*lane, event.note, event.velocity);
            break;
        case MusicalEventType::NoteOff:
            if (lane->activeNote == static_cast<int16_t>(clampDataByte(event.note))) {
                releaseActiveNote(*lane, event.velocity);
            }
            break;
        case MusicalEventType::AllNotesOff:
            // Panic is source+target scoped logically. Wire ownership prevents a
            // shared channel+note from being silenced while another lane owns it.
            releaseActiveNote(*lane);
            break;
    }
}
