#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    content = read(path)
    count = content.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one match, found {count}: {old[:120]!r}")
    write(path, content.replace(old, new, 1))


QUEUE_HEADER = r'''#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_MUSICAL_EVENT_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "musical_event.h"

// Fixed-capacity event handoff from the audio-side PatternPlayer to the control
// loop. The realtime producer never allocates or blocks. Control-plane engine
// mutations are already serialized against the audio task by AudioMutationGate,
// so producer calls never run concurrently with the audio producer.
class MusicalEventQueue {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;

    bool tryPush(const MusicalEvent& event) {
        const uint16_t head = head_.load(std::memory_order_relaxed);
        const uint16_t next = static_cast<uint16_t>((head + 1u) % kCapacity);
        if (next == tail_.load(std::memory_order_acquire)) {
            dropped_.fetch_add(1u, std::memory_order_relaxed);
            if (event.type != MusicalEventType::NoteOn) {
                pendingAllNotesOffMask_.fetch_or(targetMask(event.target),
                                                 std::memory_order_release);
            }
            return false;
        }

        events_[head] = event;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool tryPop(MusicalEvent& event) {
        const uint16_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;

        event = events_[tail];
        tail_.store(static_cast<uint16_t>((tail + 1u) % kCapacity),
                    std::memory_order_release);
        return true;
    }

    uint8_t takePendingAllNotesOffMask() {
        return pendingAllNotesOffMask_.exchange(0u, std::memory_order_acq_rel);
    }

    uint32_t droppedCount() const {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint8_t targetMask(MusicalEventTarget target) {
        return target == MusicalEventTarget::SynthB ? kSynthBMask : kSynthAMask;
    }

    MusicalEvent events_[kCapacity]{};
    std::atomic<uint16_t> head_{0};
    std::atomic<uint16_t> tail_{0};
    std::atomic<uint8_t> pendingAllNotesOffMask_{0};
    std::atomic<uint32_t> dropped_{0};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
'''

USB_OUTPUT_HEADER = r'''#pragma once
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
'''

USB_OUTPUT_CPP = r'''#include "usb_midi_output.h"

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

bool UsbMidiOutput::accepts(const MusicalEvent& event,
                            const MidiVoiceLane* lane) const {
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
    if (!accepts(event, lane)) return;

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
'''

USB_OUTPUT_TEST = r'''#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"

namespace {
enum class PacketType : uint8_t { NoteOn, NoteOff };

struct Packet {
    PacketType type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override {
        ++beginCalls;
        return beginResult;
    }
    bool mounted() const override { return mountedState; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }
    void flush() override { ++flushCalls; }
    void clear() { packets.clear(); flushCalls = 0; }

    bool beginResult{true};
    bool mountedState{false};
    bool sendResult{true};
    int beginCalls{0};
    int flushCalls{0};
    std::vector<Packet> packets;
};

class CountingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent&) override { ++count; }
    int count{0};
};

MusicalEvent event(MusicalEventType type,
                   uint8_t note,
                   uint8_t velocity = 0,
                   MusicalEventSource source = MusicalEventSource::PerformanceKeyboard,
                   MusicalEventTarget target = MusicalEventTarget::SynthA) {
    return MusicalEvent{type, source, target, 0, note, velocity};
}

void expectPacket(const Packet& packet,
                  PacketType type,
                  uint8_t channel,
                  uint8_t note,
                  uint8_t velocity) {
    assert(packet.type == type);
    assert(packet.channel == channel);
    assert(packet.note == note);
    assert(packet.velocity == velocity);
}
}  // namespace

int main() {
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    assert(transport.packets.empty());
    assert(output.status() == UsbMidiStatus::Off);

    assert(output.begin());
    assert(output.status() == UsbMidiStatus::Wait);
    transport.mountedState = true;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Ready);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 8);

    // Three independent ownership lanes: live A, pattern A and pattern B.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 48, 90,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 52, 80,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.size() == 3);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 36, 100);
    expectPacket(transport.packets[1], PacketType::NoteOn, 7, 48, 90);
    expectPacket(transport.packets[2], PacketType::NoteOn, 8, 52, 80);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 48);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    // Pattern A replacement does not cancel live A or pattern B.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 50, 91,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 5);
    expectPacket(transport.packets[3], PacketType::NoteOff, 7, 48, 0);
    expectPacket(transport.packets[4], PacketType::NoteOn, 7, 50, 91);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 48, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 5);
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 6);
    expectPacket(transport.packets[5], PacketType::NoteOff, 7, 50, 0);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    assert(transport.packets.size() == 7);
    expectPacket(transport.packets[6], PacketType::NoteOff, 7, 36, 0);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.size() == 8);
    expectPacket(transport.packets[7], PacketType::NoteOff, 8, 52, 0);

    // Unsupported sources and live Synth B remain outside this stage.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                    MusicalEventSource::Arpeggiator));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 61, 100,
                                    MusicalEventSource::MidiInput));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 100,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.empty());

    // Router fan-out remains independent.
    MusicalEventRouter router;
    CountingSink internalLikeSink;
    assert(router.addSink(internalLikeSink));
    assert(router.addSink(output));
    router.route(event(MusicalEventType::NoteOn, 55, 88,
                       MusicalEventSource::PatternPlayer,
                       MusicalEventTarget::SynthA));
    assert(internalLikeSink.count == 1);
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 55, 88);

    // Queue-full NoteOff failure retains ownership and prevents layering.
    transport.sendResult = false;
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 57, 77,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 1);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 55);
    transport.sendResult = true;
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 55, 0);

    // Disconnect clears every lane and does not replay stale notes.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64, 100,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    transport.mountedState = false;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Wait);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == -1);
    transport.mountedState = true;
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 65, 70,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    expectPacket(transport.packets.back(), PacketType::NoteOn, 8, 65, 70);

    // Global disable releases all currently owned lanes.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 66, 71,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    const std::size_t beforeDisable = transport.packets.size();
    output.setEnabled(false);
    assert(output.status() == UsbMidiStatus::Off);
    assert(transport.packets.size() == beforeDisable + 2);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == -1);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == -1);

    // Invalid route values are bounded to channel 16 (zero-based 15).
    FakeUsbMidiTransport clampedTransport;
    clampedTransport.mountedState = true;
    UsbMidiOutput clampedOutput(clampedTransport,
        UsbMidiRouteConfig{99, 98, 97, true, true});
    assert(clampedOutput.begin());
    clampedOutput.pollConnection();
    clampedOutput.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 64,
                                            MusicalEventSource::PatternPlayer,
                                            MusicalEventTarget::SynthB));
    assert(clampedTransport.packets.size() == 1);
    expectPacket(clampedTransport.packets[0], PacketType::NoteOn, 15, 62, 64);

    // Pattern routes can be disabled independently at construction.
    FakeUsbMidiTransport liveOnlyTransport;
    liveOnlyTransport.mountedState = true;
    UsbMidiOutput liveOnly(liveOnlyTransport,
        UsbMidiRouteConfig{7, 7, 8, true, false});
    assert(liveOnly.begin());
    liveOnly.pollConnection();
    liveOnly.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                      MusicalEventSource::PatternPlayer,
                                      MusicalEventTarget::SynthA));
    assert(liveOnlyTransport.packets.empty());

    return 0;
}
'''

QUEUE_TEST = r'''#include <cassert>
#include <cstdint>

#include "src/input/musical_event_queue.h"

static MusicalEvent event(MusicalEventType type,
                          MusicalEventTarget target,
                          uint8_t note) {
    return MusicalEvent{
        type,
        MusicalEventSource::PatternPlayer,
        target,
        0,
        note,
        100,
    };
}

int main() {
    MusicalEventQueue queue;

    assert(queue.tryPush(event(MusicalEventType::NoteOn,
                               MusicalEventTarget::SynthA, 36)));
    assert(queue.tryPush(event(MusicalEventType::NoteOff,
                               MusicalEventTarget::SynthA, 36)));

    MusicalEvent out{};
    assert(queue.tryPop(out));
    assert(out.type == MusicalEventType::NoteOn);
    assert(out.note == 36);
    assert(queue.tryPop(out));
    assert(out.type == MusicalEventType::NoteOff);
    assert(!queue.tryPop(out));

    // A ring with one sentinel slot holds capacity - 1 events.
    for (std::size_t i = 0; i < MusicalEventQueue::kCapacity - 1; ++i) {
        assert(queue.tryPush(event(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA,
                                   static_cast<uint8_t>(24 + (i % 40)))));
    }
    assert(!queue.tryPush(event(MusicalEventType::NoteOff,
                                MusicalEventTarget::SynthB, 48)));
    assert(queue.droppedCount() == 1);
    assert(queue.takePendingAllNotesOffMask() == MusicalEventQueue::kSynthBMask);
    assert(queue.takePendingAllNotesOffMask() == 0);

    std::size_t popped = 0;
    while (queue.tryPop(out)) ++popped;
    assert(popped == MusicalEventQueue::kCapacity - 1);

    // Dropped NoteOn is observable but does not request a destructive panic.
    for (std::size_t i = 0; i < MusicalEventQueue::kCapacity - 1; ++i) {
        assert(queue.tryPush(event(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 36)));
    }
    assert(!queue.tryPush(event(MusicalEventType::NoteOn,
                                MusicalEventTarget::SynthA, 37)));
    assert(queue.takePendingAllNotesOffMask() == 0);
    assert(queue.droppedCount() == 2);

    return 0;
}
'''

SOURCE_REGRESSION = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    event_header = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    sink = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
    sink_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    queue = (ROOT / "src/input/musical_event_queue.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("MusicalEventSource::PerformanceKeyboard" in sink and
            "MusicalEventSource::PatternPlayer" in sink,
            "USB sink must expose separate live and PatternPlayer lanes")
    require("MusicalEventTarget::SynthA" in sink and
            "MusicalEventTarget::SynthB" in sink,
            "Stage 1 must route both synth targets")
    require("kLaneCount = 3" in sink_h,
            "Stage 1 must keep exactly three fixed ownership lanes")
    require("patternSynthAChannel{7}" in sink_h and
            "patternSynthBChannel{8}" in sink_h,
            "fixed Stage 1 routes must be MIDI channels 8 and 9")

    require("publishPatternNoteOn_" in engine and
            "publishPatternNoteOff_" in engine and
            "publishPatternAllNotesOff_" in engine,
            "PatternPlayer lifecycle must publish normalized events")
    require("MusicalEventSource::PatternPlayer" in engine,
            "engine events must identify PatternPlayer as their source")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP engine must not depend on hardware USB APIs")
    require("event.source != MusicalEventSource::PerformanceKeyboard" in internal,
            "internal live sink must ignore PatternPlayer fan-out without locking audio")

    require("std::atomic" in queue and "kCapacity = 64" in queue,
            "audio-to-control handoff must be fixed and lock-free")
    for token in ("std::vector", "std::deque", "new ", "malloc("):
        require(token not in queue, f"realtime queue must not allocate: {token}")
    require("takePendingAllNotesOffMask" in queue,
            "critical queue overflow must degrade to a target-scoped panic")

    setup_start = sketch.index("void setup()")
    loop_start = sketch.index("void loop()")
    require("setPatternEventQueue(&g_patternMusicalEventQueue)" in sketch[setup_start:loop_start],
            "setup must connect PatternPlayer to the fixed event queue")
    require("drainPatternMusicalEvents();" in sketch[loop_start:],
            "control loop must drain PatternPlayer events into the shared router")
    require("g_musicalEventRouter.route(event)" in sketch,
            "queued PatternPlayer events must use MusicalEventRouter")

    require("router.addSink(g_output)" in transport,
            "USB MIDI must remain an independent router sink")
    require("UsbMidiRouteConfig" in transport and "7," in transport and "8," in transport,
            "platform route config must retain channels 8/9")
    require("USBMIDI.h" not in sketch and "USB.h" not in sketch,
            "TinyUSB headers must stay isolated from the main UI translation unit")

    tinyusb_options = "USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    require(tinyusb_options in build and tinyusb_options in upload,
            "build and upload must use the same pinned TinyUSB FQBN")
    require("#if ARDUINO_USB_MODE" in transport,
            "hardware transport must fail closed outside TinyUSB OTG mode")

    forbidden_tokens = (
        "controlChange(", "programChange(", "pitchBend(", "SysEx",
        "MIDI_CLOCK", "MIDI_START", "MIDI_STOP",
    )
    for token in forbidden_tokens:
        require(token not in transport and token not in sink and token not in engine,
                f"out-of-scope MIDI feature entered Stage 1: {token}")

    require("UsbMidi" not in scenes_h and "UsbMidi" not in scenes_cpp,
            "USB MIDI settings must remain runtime-only in Stage 1")
    require("usbMidi" not in scenes_h and "usbMidi" not in scenes_cpp,
            "scene schema must not gain USB MIDI fields")

    print("Pattern MIDI source regressions: OK")


if __name__ == "__main__":
    main()
'''

HARDWARE_DOC = r'''# PatternPlayer USB-MIDI — Cardputer-Adv acceptance

## Purpose

Validate Stage 1 pattern output without changing GroovePuter's autonomous audio behavior:

```text
PatternPlayer Synth A -> USB MIDI channel 8
PatternPlayer Synth B -> USB MIDI channel 9
```

Live NOTE mode remains the Stage 0 route. Drums, MIDI clock and transport messages are not part of this test.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, no PSRAM)
- data-capable USB-C cable; a charge-only cable will not work
- Linux computer with ALSA utilities
- Yamaha SEQTRAK for the second test stage
- headphones or speaker path for confirming internal GroovePuter audio

## Wiring

Computer test:

```text
Cardputer-Adv USB-C -> USB-C data cable -> Linux computer
```

SEQTRAK test:

```text
Cardputer-Adv USB-C -> USB-C data cable -> SEQTRAK USB-C
```

No GPIO, PORT.A or external power wiring is used.

## Build and flash

Use the repository-pinned M5Stack ESP32 core 3.2.2:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The required FQBN options are:

```text
USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
```

## Linux MIDI monitor

```bash
aconnect -l
aseqdump -l
aseqdump -p <client:port>
```

Prepare different audible patterns for Synth A and Synth B, then start GroovePuter transport.

## Expected behavior

- Synth A pattern sends NoteOn/NoteOff on MIDI channel 8.
- Synth B pattern sends NoteOn/NoteOff on MIDI channel 9.
- Velocity follows each pattern step.
- A replacement pitch sends old NoteOff before new NoteOn.
- TIE extends the existing note and does not create a duplicate NoteOn.
- Retrig produces explicit retriggered NoteOff/NoteOn pairs.
- Stop releases both pattern lanes.
- Muting Synth A releases only PatternPlayer Synth A.
- Muting Synth B releases only PatternPlayer Synth B.
- Pattern or Song-row changes do not leave stale notes.
- Internal Synth A and Synth B continue to sound at the same time.
- Live NOTE panic does not cancel PatternPlayer lanes.

## Serial output

Normal boot should pass the existing USB stages:

```text
[BOOT-STAGE] 52 before USB MIDI sink
[BOOT-STAGE] 53 after USB MIDI sink
```

There is no production per-note Serial logging. Periodic performance output must not show continual underrun growth.

## SEQTRAK procedure

1. Complete the Linux MIDI-monitor test first.
2. Connect Cardputer-Adv directly to SEQTRAK with the same data cable.
3. Configure/observe SYNTH 1 on MIDI channel 8 and SYNTH 2 on MIDI channel 9.
4. Start a GroovePuter pattern containing distinct Synth A and Synth B notes.
5. Stop, mute each synth independently, and change patterns while listening for stale notes.

## Troubleshooting

- No USB device: verify the cable carries data and rebuild with pinned core 3.2.2.
- CDC works but MIDI is absent: verify `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Only one synth responds: confirm channel 8/9 receive configuration on the monitor or SEQTRAK.
- Stuck note after Stop: capture the final `aseqdump` events and the periodic underrun line; do not merge.
- Internal audio disappears: treat as a regression; USB MIDI is an additive sink only.

## Acceptance checklist

- [ ] host-tests pass
- [ ] SDL build passes
- [ ] Cardputer-Adv build passes with M5Stack core 3.2.2
- [ ] Cardputer enumerates as CDC + class-compliant MIDI
- [ ] Synth A pattern appears on MIDI channel 8
- [ ] Synth B pattern appears on MIDI channel 9
- [ ] step velocity is preserved
- [ ] Stop releases both external synth notes
- [ ] Synth A mute does not cancel Synth B
- [ ] Synth B mute does not cancel Synth A
- [ ] pattern change leaves no stuck note
- [ ] direct SEQTRAK test succeeds
- [ ] internal Synth A and Synth B remain audible
- [ ] no reboot, watchdog, heap corruption or continual underrun growth

## Known limitations

- routes are fixed; channel settings UI is a later stage
- 303 slide is translated as old-note NoteOff followed by new-note NoteOn
- drums, MIDI clock, Start/Stop messages, Song-specific rendering and BLE-MIDI are out of scope
'''


def patch_engine_header() -> None:
    path = "src/dsp/miniacid_engine.h"
    replace_once(path,
        "// ===================== Parameters =====================\n",
        "class MusicalEventQueue;\n\n// ===================== Parameters =====================\n")
    replace_once(path,
        "  void allLiveNotesOff();\n  int liveNote(int synthIndex) const;\n",
        "  void allLiveNotesOff();\n  void setPatternEventQueue(MusicalEventQueue* queue);\n  int liveNote(int synthIndex) const;\n")
    replace_once(path,
        "  void triggerSynthStep_(int synthIdx, int stepIdx);\n  void triggerDrumVoice_(int voiceIdx, int stepIdx);\n",
        "  void triggerSynthStep_(int synthIdx, int stepIdx);\n  void publishPatternNoteOn_(int synthIdx, uint8_t note, uint8_t velocity);\n  void publishPatternNoteOff_(int synthIdx, uint8_t velocity = 0);\n  void publishPatternAllNotesOff_();\n  void triggerDrumVoice_(int voiceIdx, int stepIdx);\n")
    replace_once(path,
        "  int16_t liveNotes_[NUM_303_VOICES] = {-1, -1};\n  uint32_t liveInputEpoch_ = 0;\n",
        "  int16_t liveNotes_[NUM_303_VOICES] = {-1, -1};\n  MusicalEventQueue* patternEventQueue_ = nullptr;\n  int16_t patternMidiNotes_[NUM_303_VOICES] = {-1, -1};\n  uint32_t liveInputEpoch_ = 0;\n")


def patch_engine_cpp() -> None:
    path = "src/dsp/miniacid_engine.cpp"
    replace_once(path,
        "#include \"../audio/audio_diagnostics.h\"\n\n#include \"../sampler/sample_index.h\"\n",
        "#include \"../audio/audio_diagnostics.h\"\n#include \"../input/musical_event_queue.h\"\n\n#include \"../sampler/sample_index.h\"\n")
    replace_once(path,
        "  // PatternPlayer takes exclusive ownership of the monophonic voices.\n  allLiveNotesOff();\n  playing = true;\n",
        "  // PatternPlayer takes exclusive ownership of the monophonic voices.\n  allLiveNotesOff();\n  publishPatternAllNotesOff_();\n  playing = true;\n")
    replace_once(path,
        "void MiniAcid::stop() {\n  LOG_PRINTLN(\"[DSP] STOP command received\");\n  playing = false;\n",
        "void MiniAcid::stop() {\n  LOG_PRINTLN(\"[DSP] STOP command received\");\n  publishPatternAllNotesOff_();\n  playing = false;\n")
    replace_once(path,
        "  liveNotes_[0] = -1;\n  liveNotes_[1] = -1;\n  ++liveInputEpoch_;\n",
        "  liveNotes_[0] = -1;\n  liveNotes_[1] = -1;\n  publishPatternAllNotesOff_();\n  ++liveInputEpoch_;\n")

    helper = r'''void MiniAcid::setPatternEventQueue(MusicalEventQueue* queue) {
  patternEventQueue_ = queue;
  patternMidiNotes_[0] = -1;
  patternMidiNotes_[1] = -1;
}

void MiniAcid::publishPatternNoteOn_(int synthIdx,
                                     uint8_t note,
                                     uint8_t velocity) {
  const int idx = clamp303Voice(synthIdx);
  if (!patternEventQueue_) return;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;
  const MusicalEventTarget target = idx == 0
      ? MusicalEventTarget::SynthA
      : MusicalEventTarget::SynthB;
  const MusicalEvent event{
      MusicalEventType::NoteOn,
      MusicalEventSource::PatternPlayer,
      target,
      0,
      note,
      velocity,
  };
  if (patternEventQueue_->tryPush(event)) {
    patternMidiNotes_[idx] = static_cast<int16_t>(note);
  }
}

void MiniAcid::publishPatternNoteOff_(int synthIdx, uint8_t velocity) {
  const int idx = clamp303Voice(synthIdx);
  const int16_t note = patternMidiNotes_[idx];
  if (note < 0) return;
  if (patternEventQueue_) {
    const MusicalEventTarget target = idx == 0
        ? MusicalEventTarget::SynthA
        : MusicalEventTarget::SynthB;
    patternEventQueue_->tryPush(MusicalEvent{
        MusicalEventType::NoteOff,
        MusicalEventSource::PatternPlayer,
        target,
        0,
        static_cast<uint8_t>(note),
        velocity,
    });
  }
  // A failed critical enqueue records a target-scoped panic in the queue.
  patternMidiNotes_[idx] = -1;
}

void MiniAcid::publishPatternAllNotesOff_() {
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    if (patternEventQueue_) {
      patternEventQueue_->tryPush(MusicalEvent{
          MusicalEventType::AllNotesOff,
          MusicalEventSource::PatternPlayer,
          idx == 0 ? MusicalEventTarget::SynthA : MusicalEventTarget::SynthB,
          0,
          0,
          0,
      });
    }
    patternMidiNotes_[idx] = -1;
  }
}

'''
    replace_once(path,
        "int MiniAcid::liveNote(int synthIndex) const {\n",
        helper + "int MiniAcid::liveNote(int synthIndex) const {\n")

    replace_once(path,
        "      if (gateCountdownA_ > 0 && --gateCountdownA_ <= 0) if (synthVoices_[0]) synthVoices_[0]->release();\n      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) if (synthVoices_[1]) synthVoices_[1]->release();\n",
        "      if (gateCountdownA_ > 0 && --gateCountdownA_ <= 0) {\n        if (synthVoices_[0]) synthVoices_[0]->release();\n        publishPatternNoteOff_(0);\n      }\n      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) {\n        if (synthVoices_[1]) synthVoices_[1]->release();\n        publishPatternNoteOff_(1);\n      }\n")
    replace_once(path,
        "            if (synthVoices_[0]) {\n                synthVoices_[0]->startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);\n            }\n            LedManager::instance().onVoiceTriggered(VoiceId::SynthA, sceneManager_.currentScene().led);\n",
        "            if (synthVoices_[0]) {\n                synthVoices_[0]->startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);\n            }\n            publishPatternNoteOn_(0, static_cast<uint8_t>(step.note), step.velocity);\n            LedManager::instance().onVoiceTriggered(VoiceId::SynthA, sceneManager_.currentScene().led);\n")
    replace_once(path,
        "            if (synthVoices_[1]) {\n                synthVoices_[1]->startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);\n            }\n            LedManager::instance().onVoiceTriggered(VoiceId::SynthB, sceneManager_.currentScene().led);\n",
        "            if (synthVoices_[1]) {\n                synthVoices_[1]->startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);\n            }\n            publishPatternNoteOn_(1, static_cast<uint8_t>(step.note), step.velocity);\n            LedManager::instance().onVoiceTriggered(VoiceId::SynthB, sceneManager_.currentScene().led);\n")
    replace_once(path,
        "        if (synthVoices_[synthIdx]) synthVoices_[synthIdx]->startNote(noteToFreq(step.note), step.accent, step.slide, (uint8_t)step.velocity);\n        long dur = (long)(samplesPerStep_ * effectiveGateMult);\n",
        "        if (synthVoices_[synthIdx]) synthVoices_[synthIdx]->startNote(noteToFreq(step.note), step.accent, step.slide, (uint8_t)step.velocity);\n        publishPatternNoteOn_(synthIdx, static_cast<uint8_t>(step.note), static_cast<uint8_t>(step.velocity));\n        long dur = (long)(samplesPerStep_ * effectiveGateMult);\n")

    replace_once(path,
        "void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {\n  int idx = clamp303Voice(voiceIndex);\n  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);\n}\n",
        "void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {\n  int idx = clamp303Voice(voiceIndex);\n  if (playing) publishPatternNoteOff_(idx);\n  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);\n}\n")
    replace_once(path,
        "void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {\n  int idx = clamp303Voice(voiceIndex);\n",
        "void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {\n  int idx = clamp303Voice(voiceIndex);\n  if (playing) publishPatternNoteOff_(idx);\n")
    replace_once(path,
        "void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {\n  int idx = clamp303Voice(voiceIndex);\n  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);\n}\n",
        "void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {\n  int idx = clamp303Voice(voiceIndex);\n  if (playing) publishPatternNoteOff_(idx);\n  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);\n}\n")
    replace_once(path,
        "void MiniAcid::randomize303Pattern(int voiceIndex) {\n  int idx = clamp303Voice(voiceIndex);\n",
        "void MiniAcid::randomize303Pattern(int voiceIndex) {\n  int idx = clamp303Voice(voiceIndex);\n  if (playing) publishPatternNoteOff_(idx);\n")
    replace_once(path,
        "void MiniAcid::setSongMode(bool enabled) {\n  if (enabled == songMode_) return;\n",
        "void MiniAcid::setSongMode(bool enabled) {\n  if (enabled == songMode_) return;\n  if (playing) publishPatternAllNotesOff_();\n")
    replace_once(path,
        "void MiniAcid::applySongPositionSelection() {\n  if (!songMode_) return;\n",
        "void MiniAcid::applySongPositionSelection() {\n  if (!songMode_) return;\n  if (playing) publishPatternAllNotesOff_();\n")
    replace_once(path,
        "  if (synthVoices_[idx]) synthVoices_[idx]->release();\n  liveNotes_[idx] = -1;\n",
        "  if (playing) publishPatternNoteOff_(idx);\n  if (synthVoices_[idx]) synthVoices_[idx]->release();\n  liveNotes_[idx] = -1;\n")

    old_toggle = r'''void MiniAcid::toggleMute303(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  bool muted;
  if (idx == 0) {
    mute303 = !mute303;
    muted = mute303;
  } else {
    mute303_2 = !mute303_2;
    muted = mute303_2;
  }
  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);
}
'''
    new_toggle = old_toggle.replace(
        "  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);\n",
        "  if (muted) publishPatternNoteOff_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);\n")
    replace_once(path, old_toggle, new_toggle)

    old_set = r'''void MiniAcid::setMute303(int voiceIndex, bool muted) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) mute303 = muted;
  else mute303_2 = muted;
  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);
}
'''
    new_set = old_set.replace(
        "  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);\n",
        "  if (muted) publishPatternNoteOff_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);\n")
    replace_once(path, old_set, new_set)


def patch_sketch() -> None:
    path = "GroovePuter.ino"
    replace_once(path,
        "#include \"src/input/internal_synth_output.h\"\n#include \"src/ui/workflow_mode.h\"\n",
        "#include \"src/input/internal_synth_output.h\"\n#include \"src/input/musical_event_queue.h\"\n#include \"src/ui/workflow_mode.h\"\n")
    replace_once(path,
        "static MusicalEventRouter g_musicalEventRouter;\nstatic PerformanceKeyboard g_performanceKeyboard(g_musicalEventRouter);\n",
        "static MusicalEventRouter g_musicalEventRouter;\nstatic MusicalEventQueue g_patternMusicalEventQueue;\nstatic PerformanceKeyboard g_performanceKeyboard(g_musicalEventRouter);\n")
    replace_once(path,
        "void drawUI() {\n",
        r'''static void drainPatternMusicalEvents() {
  MusicalEvent event{};
  std::size_t drained = 0;
  while (drained < MusicalEventQueue::kCapacity &&
         g_patternMusicalEventQueue.tryPop(event)) {
    g_musicalEventRouter.route(event);
    ++drained;
  }

  // Critical events that could not enter a full queue degrade to a final
  // target-scoped PatternPlayer panic after all queued events are drained.
  const uint8_t panicMask = g_patternMusicalEventQueue.takePendingAllNotesOffMask();
  if (panicMask & MusicalEventQueue::kSynthAMask) {
    g_musicalEventRouter.route(MusicalEvent{
        MusicalEventType::AllNotesOff,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthA,
        0, 0, 0,
    });
  }
  if (panicMask & MusicalEventQueue::kSynthBMask) {
    g_musicalEventRouter.route(MusicalEvent{
        MusicalEventType::AllNotesOff,
        MusicalEventSource::PatternPlayer,
        MusicalEventTarget::SynthB,
        0, 0, 0,
    });
  }
}

void drawUI() {
''')
    replace_once(path,
        "  g_miniAcid->init();\n  g_musicalEventRouter.addSink(g_internalSynthOutput);\n",
        "  g_miniAcid->init();\n  g_miniAcid->setPatternEventQueue(&g_patternMusicalEventQueue);\n  g_musicalEventRouter.addSink(g_internalSynthOutput);\n")
    replace_once(path,
        "void loop() {\n  M5Cardputer.update();\n  LedManager::instance().update();\n",
        "void loop() {\n  M5Cardputer.update();\n  LedManager::instance().update();\n  drainPatternMusicalEvents();\n")


def patch_internal_sink() -> None:
    path = "src/input/internal_synth_output.cpp"
    replace_once(path,
        "void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {\n    // The logical channel is intentionally ignored by the internal engine.\n    AudioMutationScope mutationScope(mutationGate_);\n",
        "void InternalSynthOutput::handleMusicalEvent(const MusicalEvent& event) {\n    // PatternPlayer already owns and renders the internal voices inside the\n    // audio task. Its router fan-out is for additive outputs; taking the control\n    // mutation gate here would deadlock the audio producer and double-trigger.\n    if (event.source != MusicalEventSource::PerformanceKeyboard) return;\n\n    // The logical channel is intentionally ignored by the internal engine.\n    AudioMutationScope mutationScope(mutationGate_);\n")


def patch_platform_config() -> None:
    path = "src/platform/cardputer_usb_midi_transport.cpp"
    replace_once(path,
        "    UsbMidiRouteConfig{\n        7,     // zero-based channel 7 == MIDI channel 8 / SEQTRAK SYNTH 1\n        true,\n    });\n",
        "    UsbMidiRouteConfig{\n        7,     // live Synth A -> MIDI channel 8\n        7,     // PatternPlayer Synth A -> MIDI channel 8\n        8,     // PatternPlayer Synth B -> MIDI channel 9\n        true,\n        true,\n    });\n")


def patch_tests() -> None:
    write("src/input/musical_event_queue.h", QUEUE_HEADER)
    write("src/midi/usb_midi_output.h", USB_OUTPUT_HEADER)
    write("src/midi/usb_midi_output.cpp", USB_OUTPUT_CPP)
    write("tests/test_usb_midi_output.cpp", USB_OUTPUT_TEST)
    write("tests/test_musical_event_queue.cpp", QUEUE_TEST)
    write("tests/test_pattern_midi_source_regressions.py", SOURCE_REGRESSION)
    write("docs/tests/PATTERN_MIDI_OUTPUT_CARDPUTER_ADV.md", HARDWARE_DOC)

    path = "tests/run_host_tests.sh"
    replace_once(path,
        "python3 \"${ROOT_DIR}/tests/test_usb_midi_source_regressions.py\"\n",
        "python3 \"${ROOT_DIR}/tests/test_usb_midi_source_regressions.py\"\npython3 \"${ROOT_DIR}/tests/test_pattern_midi_source_regressions.py\"\n")
    replace_once(path,
        "\"${BUILD_DIR}/test_usb_midi_output\"\n\n\"${CXX}\" \\\n",
        "\"${BUILD_DIR}/test_usb_midi_output\"\n\n\"${CXX}\" \\\n  -std=c++17 \\\n  -Wall \\\n  -Wextra \\\n  -Werror \\\n  -I\"${ROOT_DIR}\" \\\n  \"${ROOT_DIR}/tests/test_musical_event_queue.cpp\" \\\n  -o \"${BUILD_DIR}/test_musical_event_queue\"\n\n\"${BUILD_DIR}/test_musical_event_queue\"\n\n\"${CXX}\" \\\n")

    stage_path = "docs/stages/PATTERN_MIDI_OUTPUT_STAGE.md"
    stage = read(stage_path)
    marker = "## Implementation status\n"
    if marker not in stage:
        stage += r'''

## Implementation status

The Stage 1 implementation uses a fixed 64-entry audio-to-control event queue.
PatternPlayer publishes normalized NoteOn/NoteOff/AllNotesOff events from the
same gate and retrig lifecycle that drives the internal voices. The control loop
drains those events through `MusicalEventRouter`; TinyUSB is never called from
the DSP engine or audio callback.

USB ownership lanes are fixed for this stage:

```text
PerformanceKeyboard / Synth A -> channel 8
PatternPlayer / Synth A        -> channel 8
PatternPlayer / Synth B        -> channel 9
```

Hardware acceptance is documented in:

```text
docs/tests/PATTERN_MIDI_OUTPUT_CARDPUTER_ADV.md
```
'''
        write(stage_path, stage)


def main() -> None:
    patch_engine_header()
    patch_engine_cpp()
    patch_sketch()
    patch_internal_sink()
    patch_platform_config()
    patch_tests()
    print("Pattern MIDI Stage 1 patch applied")


if __name__ == "__main__":
    main()
