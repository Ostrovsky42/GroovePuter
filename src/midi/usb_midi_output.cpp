#include "usb_midi_output.h"

#include "midi_pattern_startup_routes.h"

UsbMidiOutput::UsbMidiOutput(IUsbMidiTransport& transport,
                             UsbMidiRouteConfig config)
    : transport_(transport),
      config_(config),
      abandonedSmfChannels_(0),
      patternStartupRoutesBound_(false),
      performanceStartupRoutesComplete_(false),
      seqtrakReceiverModeControl_(true),
      enabled_(true),
      begun_(false),
      mounted_(false) {
    // Keep the global constructor deliberately trivial. On Cardputer-Adv the
    // USB MIDI service is a static object, and Launcher hands control to the app
    // before Arduino setup(). Expanded state is initialized in begin().
}

bool UsbMidiOutput::isSynthPerformanceSource(MusicalEventSource source) {
    return source == MusicalEventSource::PerformanceKeyboard ||
           source == MusicalEventSource::PerformanceKeyboardPoly ||
           source == MusicalEventSource::Arpeggiator;
}

bool UsbMidiOutput::sourceRequestsPolyReceiver(MusicalEventSource source) {
    return source == MusicalEventSource::PerformanceKeyboardPoly ||
           source == MusicalEventSource::Arpeggiator;
}

uint8_t UsbMidiOutput::patternDrumChannel(uint8_t logicalVoice) {
    static constexpr uint8_t kChannels[kPatternDrumVoiceCount] = {
        0, 1, 3, 4, 5, 6, 5, 2,
    };
    return logicalVoice < kPatternDrumVoiceCount
        ? kChannels[logicalVoice]
        : 0;
}

void UsbMidiOutput::configureLanes() {
    GroovePuterMidi::MidiPatternStartupRoutes startup{};
    patternStartupRoutesBound_ =
        GroovePuterMidi::midiPatternStartupRouteRuntime().snapshot(startup);
    performanceStartupRoutesComplete_ =
        patternStartupRoutesBound_ && startup.performanceRoutesComplete;
    seqtrakReceiverModeControl_ = !patternStartupRoutesBound_ ||
        startup.receiverModeControl ==
            GroovePuterMidi::MidiReceiverModeControl::SeqtrakCc26;

    for (uint8_t voice = 0; voice < kPatternDrumVoiceCount; ++voice) {
        patternDrumNotes_[voice] = kSeqtrakDrumNote;
    }
    for (uint8_t lane = 0; lane < kSeqtrakDrumLaneCount; ++lane) {
        performanceDrumNotes_[lane] = kSeqtrakDrumNote;
    }

    if (performanceStartupRoutesComplete_) {
        config_.performanceSynthAChannel =
            clampChannel(startup.performanceSynthA.channel);
        config_.performanceSynthBChannel =
            clampChannel(startup.performanceSynthB.channel);
        config_.performanceDxChannel =
            clampChannel(startup.performanceDx.channel);
        config_.performanceKeyboardEnabled = startup.performanceSynthA.enabled;
    }

    std::size_t lane = 0;
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA,
        0,
        clampChannel(config_.performanceSynthAChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled &&
            (!performanceStartupRoutesComplete_ ||
             startup.performanceSynthA.enabled),
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthB,
        0,
        clampChannel(config_.performanceSynthBChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled &&
            (!performanceStartupRoutesComplete_ ||
             startup.performanceSynthB.enabled),
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::Dx,
        0,
        clampChannel(config_.performanceDxChannel),
        -1,
        0,
        config_.performanceKeyboardEnabled &&
            (!performanceStartupRoutesComplete_ ||
             startup.performanceDx.enabled),
        false,
    };
    for (uint8_t drumChannel = 0;
         drumChannel < kSeqtrakDrumLaneCount;
         ++drumChannel) {
        const GroovePuterMidi::DrumMidiRoute& route =
            startup.performanceDrums[drumChannel];
        const uint8_t channel = performanceStartupRoutesComplete_
            ? clampChannel(route.channel)
            : drumChannel;
        performanceDrumNotes_[drumChannel] = performanceStartupRoutesComplete_
            ? clampDataByte(route.note)
            : kSeqtrakDrumNote;
        lanes_[lane++] = MidiVoiceLane{
            MusicalEventSource::PerformanceKeyboard,
            MusicalEventTarget::Drums,
            drumChannel,
            channel,
            -1,
            0,
            config_.performanceKeyboardEnabled &&
                (!performanceStartupRoutesComplete_ || route.enabled),
            false,
        };
    }
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        0,
        clampChannel(patternStartupRoutesBound_
                         ? startup.synthAChannel
                         : config_.patternSynthAChannel),
        -1,
        0,
        config_.patternPlayerEnabled &&
            (!patternStartupRoutesBound_ || startup.synthAEnabled),
        false,
    };
    lanes_[lane++] = MidiVoiceLane{
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthB,
        0,
        clampChannel(patternStartupRoutesBound_
                         ? startup.synthBChannel
                         : config_.patternSynthBChannel),
        -1,
        0,
        config_.patternPlayerEnabled &&
            (!patternStartupRoutesBound_ || startup.synthBEnabled),
        false,
    };
    for (uint8_t voice = 0; voice < kPatternDrumVoiceCount; ++voice) {
        const GroovePuterMidi::DrumMidiRoute& route = startup.drums[voice];
        const uint8_t channel = patternStartupRoutesBound_
            ? clampChannel(route.channel)
            : patternDrumChannel(voice);
        patternDrumNotes_[voice] = patternStartupRoutesBound_
            ? clampDataByte(route.note)
            : kSeqtrakDrumNote;
        lanes_[lane++] = MidiVoiceLane{
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::Drums,
            voice,
            channel,
            -1,
            0,
            config_.patternPlayerEnabled &&
                (!patternStartupRoutesBound_ || route.enabled),
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

uint8_t UsbMidiOutput::wireNoteFor(const MidiVoiceLane& lane,
                                   uint8_t eventNote) const {
    if (performanceStartupRoutesComplete_ &&
        lane.source == MusicalEventSource::PerformanceKeyboard &&
        lane.target == MusicalEventTarget::Drums &&
        lane.logicalChannel < kSeqtrakDrumLaneCount) {
        return performanceDrumNotes_[lane.logicalChannel];
    }
    if (patternStartupRoutesBound_ &&
        lane.source == MusicalEventSource::PatternPlayer &&
        lane.target == MusicalEventTarget::Drums &&
        lane.logicalChannel < kPatternDrumVoiceCount) {
        return patternDrumNotes_[lane.logicalChannel];
    }
    return clampDataByte(eventNote);
}

int UsbMidiOutput::generatedTargetIndex(MusicalEventTarget target) {
    switch (target) {
        case MusicalEventTarget::SynthA: return 0;
        case MusicalEventTarget::SynthB: return 1;
        case MusicalEventTarget::Dx: return 2;
        case MusicalEventTarget::Drums: break;
    }
    return -1;
}

uint8_t UsbMidiOutput::generatedChannel(MusicalEventTarget target) const {
    switch (target) {
        case MusicalEventTarget::SynthA:
            return clampChannel(config_.performanceSynthAChannel);
        case MusicalEventTarget::SynthB:
            return clampChannel(config_.performanceSynthBChannel);
        case MusicalEventTarget::Dx:
            return clampChannel(config_.performanceDxChannel);
        case MusicalEventTarget::Drums:
            break;
    }
    return 0;
}

void UsbMidiOutput::ensurePerformanceReceiverMode(
    MusicalEventTarget target,
    bool polyphonic) {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0 || !enabled_ || !begun_ || !mounted_ ||
        !config_.performanceKeyboardEnabled) {
        return;
    }

    const PerformanceReceiverMode desired = polyphonic
        ? PerformanceReceiverMode::Poly
        : PerformanceReceiverMode::Mono;
    if (performanceReceiverMode_[targetIndex] == desired) return;

    // Only a profile that explicitly advertises SEQTRAK CC26 may emit this
    // vendor parameter. GM, Generic and Custom must never inherit it merely
    // because the historical USB output was SEQTRAK-first.
    if (!seqtrakReceiverModeControl_) {
        performanceReceiverMode_[targetIndex] = desired;
        return;
    }

    if (transport_.sendControlChange(
            generatedChannel(target),
            kSeqtrakMonoPolyController,
            polyphonic ? kSeqtrakPolyValue : kSeqtrakMonoValue)) {
        transport_.flush();
    }
    performanceReceiverMode_[targetIndex] = desired;
}

bool UsbMidiOutput::generatedNoteActive(int targetIndex, uint8_t note) const {
    if (targetIndex < 0 ||
        targetIndex >= static_cast<int>(kGeneratedTargetCount)) {
        return false;
    }
    note = clampDataByte(note);
    const std::size_t byteIndex = static_cast<std::size_t>(note >> 3u);
    const uint8_t mask = static_cast<uint8_t>(1u << (note & 7u));
    return (generatedActive_[targetIndex][byteIndex] & mask) != 0u;
}

bool UsbMidiOutput::generatedNotePendingRelease(int targetIndex,
                                                uint8_t note) const {
    if (targetIndex < 0 ||
        targetIndex >= static_cast<int>(kGeneratedTargetCount)) {
        return false;
    }
    note = clampDataByte(note);
    const std::size_t byteIndex = static_cast<std::size_t>(note >> 3u);
    const uint8_t mask = static_cast<uint8_t>(1u << (note & 7u));
    return (generatedPendingRelease_[targetIndex][byteIndex] & mask) != 0u;
}

void UsbMidiOutput::setGeneratedNoteActive(int targetIndex,
                                           uint8_t note,
                                           bool active) {
    if (targetIndex < 0 ||
        targetIndex >= static_cast<int>(kGeneratedTargetCount)) {
        return;
    }
    note = clampDataByte(note);
    const std::size_t byteIndex = static_cast<std::size_t>(note >> 3u);
    const uint8_t mask = static_cast<uint8_t>(1u << (note & 7u));
    if (active) generatedActive_[targetIndex][byteIndex] |= mask;
    else generatedActive_[targetIndex][byteIndex] &= static_cast<uint8_t>(~mask);
}

void UsbMidiOutput::setGeneratedNotePendingRelease(int targetIndex,
                                                   uint8_t note,
                                                   bool pending) {
    if (targetIndex < 0 ||
        targetIndex >= static_cast<int>(kGeneratedTargetCount)) {
        return;
    }
    note = clampDataByte(note);
    const std::size_t byteIndex = static_cast<std::size_t>(note >> 3u);
    const uint8_t mask = static_cast<uint8_t>(1u << (note & 7u));
    if (pending) generatedPendingRelease_[targetIndex][byteIndex] |= mask;
    else generatedPendingRelease_[targetIndex][byteIndex] &= static_cast<uint8_t>(~mask);
}

int UsbMidiOutput::generatedActiveNote(MusicalEventTarget target) const {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0) return -1;
    for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
        if (generatedNoteActive(targetIndex, static_cast<uint8_t>(note))) {
            return static_cast<int>(note);
        }
    }
    return -1;
}

uint8_t UsbMidiOutput::generatedActiveCount(MusicalEventTarget target) const {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0) return 0;
    uint8_t count = 0;
    for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
        if (generatedNoteActive(targetIndex, static_cast<uint8_t>(note)) &&
            count < 255u) {
            ++count;
        }
    }
    return count;
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
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        if (logicalChannel != 0) return -1;
        return generatedActiveNote(target);
    }
    const MidiVoiceLane* lane = laneFor(source, target, logicalChannel);
    return lane ? lane->activeNote : -1;
}

int UsbMidiOutput::activeNote(MusicalEventTarget target) const {
    if (!begun_) return -1;
    const int live = activeNote(MusicalEventSource::PerformanceKeyboard, target);
    if (live >= 0) return live;
    const int generated = activeNote(MusicalEventSource::Arpeggiator, target);
    if (generated >= 0) return generated;
    return activeNote(MusicalEventSource::PatternPlayer, target);
}

uint8_t UsbMidiOutput::activeGateCount(MusicalEventSource source,
                                       MusicalEventTarget target,
                                       uint8_t logicalChannel) const {
    if (!begun_) return 0;
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        if (logicalChannel != 0) return 0;
        return generatedActiveCount(target);
    }
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
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        if (logicalChannel != 0) return 0;
        return generatedChannel(target);
    }
    const MidiVoiceLane* lane = laneFor(source, target, logicalChannel);
    return lane ? lane->channel : 0;
}

uint8_t UsbMidiOutput::wireOwnerCount(uint8_t zeroBasedChannel,
                                      uint8_t note) const {
    if (!begun_) return 0;
    return owners_.wireOwnerCount(clampChannel(zeroBasedChannel),
                                  clampDataByte(note));
}

uint8_t UsbMidiOutput::smfOwnerCount(uint8_t zeroBasedChannel,
                                     uint8_t note) const {
    if (!begun_) return 0;
    return owners_.smfOwnerCount(clampChannel(zeroBasedChannel),
                                 clampDataByte(note));
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

    // A full ownership table means this NoteOn cannot be tracked. NoteOn has
    // no cleanup obligation, so refusing it leaves nothing on the wire.
    auto* cell = owners_.open(lane.channel, note);
    if (cell == nullptr) return false;
    if (cell->wire == 0) {
        if (!transport_.sendNoteOn(lane.channel, note, velocity)) {
            owners_.prune();
            return false;
        }
        transport_.flush();
    }

    if (cell->wire < 255) ++cell->wire;
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
    // Release never inserts: a missing cell means no owner, which is exactly
    // the owners == 0 case below.
    auto* cell = owners_.peek(lane.channel, note);
    const uint8_t owners = cell ? cell->wire : 0;
    if (owners > 1) {
        --cell->wire;
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

    if (cell != nullptr) cell->wire = 0;
    owners_.prune();
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

    auto* cell = owners_.open(lane.channel, note);
    if (cell == nullptr) return false;
    if (lane.activeCount == 255 || cell->wire == 255) {
        owners_.prune();
        return false;
    }
    if (!transport_.sendNoteOn(lane.channel, note, velocity)) {
        owners_.prune();
        return false;
    }
    transport_.flush();

    ++lane.activeCount;
    ++cell->wire;
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
    auto* cell = owners_.peek(lane.channel, note);
    const uint8_t owners = cell ? cell->wire : 0;
    if (owners == 0) {
        lane.activeNote = -1;
        lane.activeCount = 0;
        lane.pendingRelease = false;
        return true;
    }

    if (owners > 1) {
        --cell->wire;
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

    cell->wire = 0;
    owners_.prune();
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
    auto* cell = owners_.peek(lane.channel, note);
    const uint8_t owners = cell ? cell->wire : 0;
    const uint8_t laneOwners = lane.activeCount;
    const uint8_t otherOwners = owners > laneOwners
        ? static_cast<uint8_t>(owners - laneOwners)
        : 0;

    if (otherOwners > 0) {
        cell->wire = otherOwners;
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

    if (cell != nullptr) cell->wire = 0;
    owners_.prune();
    lane.activeNote = -1;
    lane.activeCount = 0;
    lane.pendingRelease = false;
    return true;
}

bool UsbMidiOutput::retryGeneratedPendingReleases(
    MusicalEventTarget target) {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0) return false;
    bool allReleased = true;
    for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
        const uint8_t midiNote = static_cast<uint8_t>(note);
        if (!generatedNotePendingRelease(targetIndex, midiNote)) continue;
        if (!releaseGeneratedNote(target, midiNote)) allReleased = false;
    }
    return allReleased;
}

bool UsbMidiOutput::acquireGeneratedNote(MusicalEventTarget target,
                                         uint8_t note,
                                         uint8_t velocity) {
    const int targetIndex = generatedTargetIndex(target);
    if (!enabled_ || !begun_ || !mounted_ ||
        !config_.performanceKeyboardEnabled || targetIndex < 0) {
        return false;
    }

    note = clampDataByte(note);
    velocity = clampDataByte(velocity);
    if (velocity < 1) velocity = 1;
    const uint8_t channel = generatedChannel(target);

    if (generatedNoteActive(targetIndex, note)) {
        // Generated tools may intentionally retrigger the same tone (ratchet).
        // Direct physical keys normally have unique pitches; the shared bounded
        // domain preserves existing no-allocation ownership behavior.
        if (!transport_.sendNoteOn(channel, note, velocity)) return false;
        transport_.flush();
        setGeneratedNotePendingRelease(targetIndex, note, false);
        return true;
    }

    // Reserved only once the retrigger path above is ruled out, so a
    // bitset/table disagreement cannot leave an empty cell behind.
    auto* cell = owners_.open(channel, note);
    if (cell == nullptr) return false;
    if (cell->wire == 255u) {
        owners_.prune();
        return false;
    }
    if (cell->wire == 0u) {
        if (!transport_.sendNoteOn(channel, note, velocity)) {
            owners_.prune();
            return false;
        }
        transport_.flush();
    }
    ++cell->wire;
    setGeneratedNoteActive(targetIndex, note, true);
    setGeneratedNotePendingRelease(targetIndex, note, false);
    return true;
}

bool UsbMidiOutput::releaseGeneratedNote(MusicalEventTarget target,
                                         uint8_t note,
                                         uint8_t velocity) {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0) return true;
    note = clampDataByte(note);
    if (!generatedNoteActive(targetIndex, note)) {
        setGeneratedNotePendingRelease(targetIndex, note, false);
        return true;
    }

    const uint8_t channel = generatedChannel(target);
    auto* cell = owners_.peek(channel, note);
    const uint8_t owners = cell ? cell->wire : 0;
    if (owners > 1u) {
        --cell->wire;
        setGeneratedNoteActive(targetIndex, note, false);
        setGeneratedNotePendingRelease(targetIndex, note, false);
        return true;
    }
    if (owners == 0u) {
        setGeneratedNoteActive(targetIndex, note, false);
        setGeneratedNotePendingRelease(targetIndex, note, false);
        return true;
    }

    velocity = clampDataByte(velocity);
    if (!mounted_ || !transport_.sendNoteOff(channel, note, velocity)) {
        setGeneratedNotePendingRelease(targetIndex, note, true);
        return false;
    }
    transport_.flush();
    if (cell != nullptr) cell->wire = 0;
    owners_.prune();
    setGeneratedNoteActive(targetIndex, note, false);
    setGeneratedNotePendingRelease(targetIndex, note, false);
    return true;
}

bool UsbMidiOutput::releaseGeneratedTarget(MusicalEventTarget target) {
    const int targetIndex = generatedTargetIndex(target);
    if (targetIndex < 0) return true;
    bool allReleased = true;
    for (std::size_t note = 0; note < kMidiNoteCount; ++note) {
        const uint8_t midiNote = static_cast<uint8_t>(note);
        if (!generatedNoteActive(targetIndex, midiNote)) continue;
        if (!releaseGeneratedNote(target, midiNote)) allReleased = false;
    }
    return allReleased;
}

void UsbMidiOutput::releaseTargetAllNotes(MusicalEventSource source,
                                          MusicalEventTarget target) {
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        releaseGeneratedTarget(target);
        return;
    }

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
    releaseGeneratedTarget(MusicalEventTarget::SynthA);
    releaseGeneratedTarget(MusicalEventTarget::SynthB);
    releaseGeneratedTarget(MusicalEventTarget::Dx);
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

    // Reserve the ownership cell before the wire write. If the table is full
    // the NoteOn is refused outright rather than sounding a note nothing owns.
    auto* cell = owners_.open(channel, note);
    if (cell == nullptr) return false;

    if (!transport_.sendNoteOn(channel, note, velocity)) {
        owners_.prune();
        return false;
    }
    transport_.flush();

    if (cell->smf < 255) ++cell->smf;
    if (cell->wire < 255) ++cell->wire;
    return true;
}

bool UsbMidiOutput::handleSmfNoteOff(uint8_t zeroBasedChannel,
                                     uint8_t note,
                                     uint8_t velocity) {
    pollConnection();
    const uint8_t channel = clampChannel(zeroBasedChannel);
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);

    auto* cell = owners_.peek(channel, note);
    if (cell == nullptr || cell->smf == 0) return true;

    if (cell->wire > 1) {
        --cell->smf;
        --cell->wire;
        owners_.prune();
        return true;
    }

    if (cell->wire == 0) {
        cell->smf = 0;
        owners_.prune();
        return true;
    }

    if (!mounted_ || !transport_.sendNoteOff(channel, note, velocity)) {
        return false;
    }
    transport_.flush();
    cell->smf = 0;
    cell->wire = 0;
    owners_.prune();
    return true;
}

bool UsbMidiOutput::releaseAllSmfNotes() {
    pollConnection();
    bool allReleased = true;
    // Only cells with a live owner exist, so this walks the notes that are
    // actually sounding instead of the 2048-cell address space. Cells are
    // mutated in place; nothing is inserted or pruned until the walk ends.
    for (auto& cell : owners_) {
        if (cell.smf == 0) continue;

        const uint8_t otherOwners = cell.wire > cell.smf
            ? static_cast<uint8_t>(cell.wire - cell.smf)
            : 0;

        if (otherOwners > 0) {
            cell.wire = otherOwners;
            cell.smf = 0;
            continue;
        }

        if (!mounted_ || !transport_.sendNoteOff(cell.channel, cell.note, 0)) {
            allReleased = false;
            continue;
        }
        transport_.flush();
        cell.wire = 0;
        cell.smf = 0;
    }
    owners_.prune();
    return releaseAbandonedSmfChannels() && allReleased;
}

void UsbMidiOutput::abandonAllSmfNotes() {
    for (auto& cell : owners_) {
        if (cell.smf == 0) continue;

        if (cell.wire <= cell.smf) {
            abandonedSmfChannels_ |=
                static_cast<uint16_t>(1u << cell.channel);
        }
        cell.wire = cell.wire > cell.smf
            ? static_cast<uint8_t>(cell.wire - cell.smf)
            : 0;
        cell.smf = 0;
    }
    owners_.prune();
}

bool UsbMidiOutput::releaseAbandonedSmfChannels() {
    if (abandonedSmfChannels_ == 0) return true;
    if (!mounted_) return false;

    bool allReleased = true;
    for (std::size_t channel = 0; channel < kMidiChannelCount; ++channel) {
        const uint16_t mask = static_cast<uint16_t>(1u << channel);
        if ((abandonedSmfChannels_ & mask) == 0) continue;

        if (owners_.channelHasWireOwner(static_cast<uint8_t>(channel))) {
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
    for (std::size_t target = 0; target < kGeneratedTargetCount; ++target) {
        performanceReceiverMode_[target] = PerformanceReceiverMode::Unknown;
        for (std::size_t byte = 0; byte < kGeneratedBitsetBytes; ++byte) {
            generatedActive_[target][byte] = 0;
            generatedPendingRelease_[target][byte] = 0;
        }
    }
    owners_.clear();
}

void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    if (!begun_) return;
    pollConnection();

    if (event.type == MusicalEventType::AllNotesOff) {
        releaseTargetAllNotes(event.source, event.target);
        return;
    }

    if (event.target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(event.source)) {
        if (!retryGeneratedPendingReleases(event.target)) return;
        switch (event.type) {
            case MusicalEventType::NoteOn:
                ensurePerformanceReceiverMode(
                    event.target, sourceRequestsPolyReceiver(event.source));
                acquireGeneratedNote(event.target, event.note, event.velocity);
                break;
            case MusicalEventType::NoteOff:
                releaseGeneratedNote(event.target, event.note, event.velocity);
                break;
            case MusicalEventType::AllNotesOff:
                break;
        }
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
        const uint8_t wireNote = wireNoteFor(*lane, event.note);
        switch (event.type) {
            case MusicalEventType::NoteOn:
                acquirePercussiveNote(*lane, wireNote, event.velocity);
                break;
            case MusicalEventType::NoteOff:
                if (lane->activeNote == static_cast<int16_t>(wireNote)) {
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
