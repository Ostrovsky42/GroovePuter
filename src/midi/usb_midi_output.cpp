#include "usb_midi_output.h"

UsbMidiOutput::UsbMidiOutput(IUsbMidiTransport& transport,
                             UsbMidiRouteConfig config)
    : transport_(transport),
      config_(config),
      abandonedSmfChannels_(0),
      enabled_(true),
      begun_(false),
      mounted_(false) {
    // Keep the global constructor deliberately trivial. On Cardputer-Adv the
    // USB MIDI service is a static object, and Launcher hands control to the app
    // before Arduino setup(). Expanded DX/drum lane construction belongs in
    // begin(), which runs inside MidiDispatchTask after setup has started.
}

uint8_t UsbMidiOutput::patternDrumChannel(uint8_t logicalVoice) {
    // Internal GroovePuter drum voices -> native SEQTRAK tracks.
    // KICK, SNARE, CLOSED HAT, OPEN HAT, MID TOM, HIGH TOM, RIM, CLAP
    // map to CH1, CH2, CH4, CH5, CH6, CH7, CH6, CH3 respectively.
    static constexpr uint8_t kChannels[kPatternDrumVoiceCount] = {
        0, 1, 3, 4, 5, 6, 5, 2,
    };
    return logicalVoice < kPatternDrumVoiceCount
        ? kChannels[logicalVoice]
        : 0;
}

void UsbMidiOutput::configureLanes() {
    std::size_t lane = 0;
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        clampChannel(config_.performanceSynthAChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled,
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthB,
        0,
        clampChannel(config_.performanceSynthBChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled,
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::Dx,
        0,
        clampChannel(config_.performanceDxChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled,
        false,
    };
    for (uint8_t drumChannel = 0;
         drumChannel < kSeqtrakDrumLaneCount;
         ++drumChannel) {
        lanes_[lane++] = MidiVoiceLane{
            MusicalEventSource::PerformanceKeyboard,
            MusicalEventTarget::Drums,
            drumChannel,
            drumChannel,
            -1,
            0,
            config_.performanceKeyboardEnabled,
            false,
        };
    }
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        0,
        clampChannel(config_.patternSynthAChannel),
        -1,
        0,
        config_.patternPlayerEnabled,
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthB,
        0,
        clampChannel(config_.patternSynthBChannel),
        -1,
        0,
        config_.patternPlayerEnabled,
        false,
    };
    for (uint8_t voice = 0; voice < kPatternDrumVoiceCount; ++voice) {
        lanes_[lane++] = MidiVoiceLane{
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::Drums,
            voice,
            patternDrumChannel(voice),
            -1,
            0,
            config_.patternPlayerEnabled,
            false,
        };
    }
}

uint8_t UsbMidiOutput::clampChannel(uint8_t channel) {
    return channel > 15 ? 15 : channel;
}

uint8_t UsbMidiOutput::clampDataByte(uint8_t value) {
    return value > 127 ? 127 : value;
}

bool UsbMidiOutput::begin() {
    configureLanes();
    abandonedSmfChannels_ = 0;
    clearActiveState();
    begun_ = transport_.begin();
    mounted_ = false;
    return begun_;
}

void UsbMidiOutput::pollConnection() {
    const bool nextMounted = begun_ && transport_.mounted();
    if (nextMounted == mounted_) return;

    if (mounted_ && !nextMounted) {
        // The remote device may retain sounding notes across a transient USB
        // disconnect. Preserve only channels that have no non-SMF owner.
        abandonAllSmfNotes();
    }
    clearActiveState();
    mounted_ = nextMounted;
    if (mounted_) releaseAbandonedSmfChannels();
}

void UsbMidiOutput::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    if (!enabled) {
        pollConnection();
        releaseAllActiveNotes();
        releaseAllSmfNotes();
    }
    enabled_ = enabled;
}

UsbMidiStatus UsbMidiOutput::status() const {
    if (!enabled_ || !begun_) return UsbMidiStatus::Off;
    return mounted_ ? UsbMidiStatus::Ready : UsbMidiStatus::Wait;
}

UsbMidiOutput::MidiVoiceLane* UsbMidiOutput::laneFor(
    MusicalEventSource source,
    MusicalEventTarget target,
    uint8_t logicalChannel) {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source != source || lanes_[i].target != target) continue;
        if (target == MusicalEventTarget::Drums &&
            lanes_[i].logicalChannel != logicalChannel) {
            continue;
        }
        return &lanes_[i];
    }
    return nullptr;
}

const UsbMidiOutput::MidiVoiceLane* UsbMidiOutput::laneFor(
    MusicalEventSource source,
    MusicalEventTarget target,
    uint8_t logicalChannel) const {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source != source || lanes_[i].target != target) continue;
        if (target == MusicalEventTarget::Drums &&
            lanes_[i].logicalChannel != logicalChannel) {
            continue;
        }
        return &lanes_[i];
    }
    return nullptr;
}

int UsbMidiOutput::activeNote(MusicalEventSource source,
                              MusicalEventTarget target) const {
    if (!begun_) return -1;
    if (target == MusicalEventTarget::Drums) {
        const uint8_t count = source == MusicalEventSource::PatternPlayer
            ? kPatternDrumVoiceCount
            : kSeqtrakDrumLaneCount;
        for (uint8_t channel = 0; channel < count; ++channel) {
            const int note = activeNote(source, target, channel);
            if (note >= 0) return note;
        }
        return -1;
    }
    return activeNote(source, target, 0);
}

int UsbMidiOutput::activeNote(MusicalEventSource source,
                              MusicalEventTarget target,
                              uint8_t logicalChannel) const {
    if (!begun_) return -1;
    const MidiVoiceLane* lane = laneFor(source, target, logicalChannel);
    return lane ? lane->activeNote : -1;
}

int UsbMidiOutput::activeNote(MusicalEventTarget target) const {
    if (!begun_) return -1;
    const int live = activeNote(MusicalEventSource::PerformanceKeyboard, target);
    if (live >= 0) return live;
    return activeNote(MusicalEventSource::PatternPlayer, target);
}

uint8_t UsbMidiOutput::activeGateCount(MusicalEventSource source,
                                       MusicalEventTarget target,
                                       uint8_t logicalChannel) const {
    if (!begun_) return 0;
    const MidiVoiceLane* lane = laneFor(source, target, logicalChannel);
    return lane ? lane->activeCount : 0;
}

uint8_t UsbMidiOutput::channelFor(MusicalEventSource source,
                                  MusicalEventTarget target) const {
    return channelFor(source, target, 0);
}

uint8_t UsbMidiOutput::channelFor(MusicalEventSource source,
                                  MusicalEventTarget target,
                                  uint8_t logicalChannel) const {
    if (!begun_) return 0;
    const MidiVoiceLane* lane = laneFor(source, target, logicalChannel);
    return lane ? lane->channel : 0;
}

uint8_t UsbMidiOutput::wireOwnerCount(uint8_t zeroBasedChannel,
                                      uint8_t note) const {
    if (!begun_) return 0;
    return wireOwners_[clampChannel(zeroBasedChannel)][clampDataByte(note)];
}

uint8_t UsbMidiOutput::smfOwnerCount(uint8_t zeroBasedChannel,
                                     uint8_t note) const {
    if (!begun_) return 0;
    return smfOwners_[clampChannel(zeroBasedChannel)][clampDataByte(note)];
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
    lane.activeCount = 1;
    lane.pendingRelease = false;
    return true;
}

bool UsbMidiOutput::releaseActiveNote(MidiVoiceLane& lane, uint8_t velocity) {
    if (lane.activeNote < 0) {
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    const uint8_t note = clampDataByte(static_cast<uint8_t>(lane.activeNote));
    uint8_t& owners = wireOwners_[lane.channel][note];
    if (owners > 1) {
        --owners;
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }
    if (owners == 0) {
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    velocity = clampDataByte(velocity);
    if (!mounted_ || !transport_.sendNoteOff(lane.channel, note, velocity)) {
        lane.pendingRelease = true;
        return false;
    }

    owners = 0;
    lane.activeNote = -1;
    lane.activeCount = 0;
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

bool UsbMidiOutput::acquirePercussiveNote(MidiVoiceLane& lane,
                                          uint8_t note,
                                          uint8_t velocity) {
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);
    if (velocity < 1) velocity = 1;

    if (lane.pendingRelease && !releasePercussiveLane(lane)) return false;
    if (lane.activeNote >= 0 && lane.activeNote != static_cast<int16_t>(note) &&
        !releasePercussiveLane(lane)) {
        return false;
    }

    uint8_t& owners = wireOwners_[lane.channel][note];
    if (lane.activeCount == 255 || owners == 255) return false;

    // Percussive retriggers must remain audible even when an older gate for the
    // same channel+note is still open. Ownership counting only controls when a
    // physical NoteOff is finally safe to emit.
    if (!transport_.sendNoteOn(lane.channel, note, velocity)) return false;
    transport_.flush();

    ++lane.activeCount;
    ++owners;
    lane.activeNote = static_cast<int16_t>(note);
    lane.pendingRelease = false;
    return true;
}

bool UsbMidiOutput::releasePercussiveNote(MidiVoiceLane& lane,
                                          uint8_t velocity) {
    if (lane.activeNote < 0 || lane.activeCount == 0) {
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    const uint8_t note = clampDataByte(static_cast<uint8_t>(lane.activeNote));
    uint8_t& owners = wireOwners_[lane.channel][note];
    if (owners == 0) {
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    if (owners > 1) {
        --owners;
        --lane.activeCount;
        if (lane.activeCount == 0) lane.activeNote = -1;
        lane.pendingRelease = false;
        return true;
    }

    velocity = clampDataByte(velocity);
    if (!mounted_ || !transport_.sendNoteOff(lane.channel, note, velocity)) {
        lane.pendingRelease = true;
        return false;
    }

    owners = 0;
    lane.activeCount = 0;
    lane.activeNote = -1;
    lane.pendingRelease = false;
    transport_.flush();
    return true;
}

bool UsbMidiOutput::releasePercussiveLane(MidiVoiceLane& lane,
                                          uint8_t velocity) {
    if (lane.activeNote < 0 || lane.activeCount == 0) {
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    const uint8_t note = clampDataByte(static_cast<uint8_t>(lane.activeNote));
    uint8_t& owners = wireOwners_[lane.channel][note];
    const uint8_t laneOwners = lane.activeCount;
    const uint8_t otherOwners = owners > laneOwners
        ? static_cast<uint8_t>(owners - laneOwners)
        : 0;

    if (otherOwners > 0) {
        owners = otherOwners;
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    velocity = clampDataByte(velocity);
    if (owners > 0 &&
        (!mounted_ || !transport_.sendNoteOff(lane.channel, note, velocity))) {
        lane.pendingRelease = true;
        return false;
    }
    if (owners > 0) transport_.flush();

    owners = 0;
    lane.activeNote = -1;
    lane.activeCount = 0;
    lane.pendingRelease = false;
    return true;
}

void UsbMidiOutput::releaseTargetAllNotes(MusicalEventSource source,
                                          MusicalEventTarget target) {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source != source || lanes_[i].target != target) continue;
        if (target == MusicalEventTarget::Drums) {
            releasePercussiveLane(lanes_[i]);
        } else {
            releaseActiveNote(lanes_[i]);
        }
    }
}

void UsbMidiOutput::releaseAllActiveNotes() {
    if (!begun_) return;
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].target == MusicalEventTarget::Drums) {
            releasePercussiveLane(lanes_[i]);
        } else {
            releaseActiveNote(lanes_[i]);
        }
    }
}

bool UsbMidiOutput::handleSmfNoteOn(uint8_t zeroBasedChannel,
                                    uint8_t note,
                                    uint8_t velocity) {
    pollConnection();
    if (!enabled_ || !begun_ || !mounted_) return false;

    const uint8_t channel = clampChannel(zeroBasedChannel);
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);
    if (velocity < 1) velocity = 1;

    if (!transport_.sendNoteOn(channel, note, velocity)) return false;
    transport_.flush();

    uint8_t& smfOwners = smfOwners_[channel][note];
    uint8_t& wireOwners = wireOwners_[channel][note];
    if (smfOwners < 255) ++smfOwners;
    if (wireOwners < 255) ++wireOwners;
    return true;
}

bool UsbMidiOutput::handleSmfNoteOff(uint8_t zeroBasedChannel,
                                     uint8_t note,
                                     uint8_t velocity) {
    pollConnection();
    const uint8_t channel = clampChannel(zeroBasedChannel);
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);

    uint8_t& smfOwners = smfOwners_[channel][note];
    uint8_t& wireOwners = wireOwners_[channel][note];
    if (smfOwners == 0) return true;

    if (wireOwners > 1) {
        --smfOwners;
        --wireOwners;
        return true;
    }

    if (wireOwners == 0) {
        smfOwners = 0;
        return true;
    }

    if (!mounted_ || !transport_.sendNoteOff(channel, note, velocity)) {
        return false;
    }
    transport_.flush();
    smfOwners = 0;
    wireOwners = 0;
    return true;
}

bool UsbMidiOutput::releaseAllSmfNotes() {
    pollConnection();
    bool allReleased = true;
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
            uint8_t& smfOwners = smfOwners_[channel][note];
            if (smfOwners == 0) continue;

            uint8_t& wireOwners = wireOwners_[channel][note];
            const uint8_t otherOwners = wireOwners > smfOwners
                ? static_cast<uint8_t>(wireOwners - smfOwners)
                : 0;

            if (otherOwners > 0) {
                wireOwners = otherOwners;
                smfOwners = 0;
                continue;
            }

            if (!mounted_ || !transport_.sendNoteOff(
                    static_cast<uint8_t>(channel),
                    static_cast<uint8_t>(note),
                    0)) {
                allReleased = false;
                continue;
            }
            transport_.flush();
            wireOwners = 0;
            smfOwners = 0;
        }
    }
    return releaseAbandonedSmfChannels() && allReleased;
}

void UsbMidiOutput::abandonAllSmfNotes() {
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
            uint8_t& smfOwners = smfOwners_[channel][note];
            if (smfOwners == 0) continue;

            uint8_t& wireOwners = wireOwners_[channel][note];
            if (wireOwners <= smfOwners) {
                abandonedSmfChannels_ |= static_cast<uint16_t>(1u << channel);
            }
            wireOwners = wireOwners > smfOwners
                ? static_cast<uint8_t>(wireOwners - smfOwners)
                : 0;
            smfOwners = 0;
        }
    }
}

bool UsbMidiOutput::releaseAbandonedSmfChannels() {
    if (abandonedSmfChannels_ == 0) return true;
    if (!mounted_) return false;

    bool allReleased = true;
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        const uint16_t mask = static_cast<uint16_t>(1u << channel);
        if ((abandonedSmfChannels_ & mask) == 0) continue;

        bool hasKnownOwner = false;
        for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
            if (wireOwners_[channel][note] != 0) {
                hasKnownOwner = true;
                break;
            }
        }
        if (hasKnownOwner) {
            allReleased = false;
            continue;
        }
        if (!transport_.sendControlChange(static_cast<uint8_t>(channel),
                                          123,
                                          0)) {
            allReleased = false;
            continue;
        }
        transport_.flush();
        abandonedSmfChannels_ &= static_cast<uint16_t>(~mask);
    }
    return allReleased;
}

void UsbMidiOutput::clearActiveState() {
    for (std::size_t i = 0; i < kLaneCount; ++i) {
        lanes_[i].activeNote = -1;
        lanes_[i].activeCount = 0;
        lanes_[i].pendingRelease = false;
    }
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
            wireOwners_[channel][note] = 0;
            smfOwners_[channel][note] = 0;
        }
    }
}

void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    if (!begun_) return;
    pollConnection();

    if (event.type == MusicalEventType::AllNotesOff) {
        releaseTargetAllNotes(event.source, event.target);
        return;
    }

    MidiVoiceLane* lane = laneFor(event.source, event.target, event.channel);
    if (!accepts(lane)) return;
    if (lane->pendingRelease) {
        const bool released = event.target == MusicalEventTarget::Drums
            ? releasePercussiveLane(*lane)
            : releaseActiveNote(*lane);
        if (!released) return;
    }

    if (event.target == MusicalEventTarget::Drums) {
        switch (event.type) {
            case MusicalEventType::NoteOn:
                acquirePercussiveNote(*lane, event.note, event.velocity);
                break;
            case MusicalEventType::NoteOff:
                if (lane->activeNote ==
                    static_cast<int16_t>(clampDataByte(event.note))) {
                    releasePercussiveNote(*lane, event.velocity);
                }
                break;
            case MusicalEventType::AllNotesOff:
                break;
        }
        return;
    }

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
            break;
    }
}
