#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


visual_header = r'''#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace GroovePuterMidi {

struct SmfMidiVisualSnapshot {
    uint32_t epoch{0};
    uint32_t pulseCounter{0};
    uint32_t droppedEvents{0};
    uint8_t note{60};
    uint8_t velocity{0};
    uint8_t channel{0};
};

class SmfMidiVisualTimeline {
public:
    static constexpr std::size_t kCapacity = 64;

    void reset() {
        head_ = 0;
        size_ = 0;
        ++snapshot_.epoch;
        snapshot_.pulseCounter = 0;
        snapshot_.droppedEvents = 0;
        snapshot_.note = 60;
        snapshot_.velocity = 0;
        snapshot_.channel = 0;
    }

    void clearPending() {
        head_ = 0;
        size_ = 0;
        snapshot_.velocity = 0;
    }

    void queue(uint32_t tick, uint8_t note, uint8_t velocity, uint8_t channel) {
        if (size_ == kCapacity) {
            head_ = (head_ + 1u) % kCapacity;
            --size_;
            ++snapshot_.droppedEvents;
        }
        const std::size_t index = (head_ + size_) % kCapacity;
        events_[index] = Event{tick, note, velocity, channel};
        ++size_;
    }

    SmfMidiVisualSnapshot advanceTo(uint32_t currentTick) {
        uint32_t consumed = 0;
        uint8_t peakVelocity = 0;
        Event last{};
        while (size_ > 0 && events_[head_].tick <= currentTick) {
            const Event event = events_[head_];
            head_ = (head_ + 1u) % kCapacity;
            --size_;
            ++consumed;
            if (event.velocity >= peakVelocity) peakVelocity = event.velocity;
            last = event;
        }
        if (consumed > 0) {
            snapshot_.pulseCounter += consumed;
            snapshot_.note = last.note;
            snapshot_.velocity = peakVelocity;
            snapshot_.channel = last.channel;
        }
        return snapshot_;
    }

    const SmfMidiVisualSnapshot& snapshot() const { return snapshot_; }
    std::size_t pending() const { return size_; }

private:
    struct Event {
        uint32_t tick{0};
        uint8_t note{60};
        uint8_t velocity{0};
        uint8_t channel{0};
    };

    std::array<Event, kCapacity> events_{};
    std::size_t head_{0};
    std::size_t size_{0};
    SmfMidiVisualSnapshot snapshot_{};
};

static_assert(sizeof(SmfMidiVisualTimeline) < 1024,
              "SMF MIDI visual timeline must remain bounded");

}  // namespace GroovePuterMidi
'''
write("src/midi/smf_midi_visual.h", visual_header)

visual_test = r'''#include <cassert>
#include <cstdint>

#include "src/midi/smf_midi_visual.h"

using namespace GroovePuterMidi;

int main() {
    SmfMidiVisualTimeline timeline;
    timeline.reset();
    const uint32_t epoch = timeline.snapshot().epoch;

    timeline.queue(10, 48, 40, 0);
    timeline.queue(12, 72, 110, 8);
    assert(timeline.pending() == 2);

    SmfMidiVisualSnapshot snapshot = timeline.advanceTo(9);
    assert(snapshot.pulseCounter == 0);
    assert(timeline.pending() == 2);

    snapshot = timeline.advanceTo(12);
    assert(snapshot.pulseCounter == 2);
    assert(snapshot.note == 72);
    assert(snapshot.velocity == 110);
    assert(snapshot.channel == 8);
    assert(timeline.pending() == 0);

    for (std::size_t i = 0; i < SmfMidiVisualTimeline::kCapacity + 3; ++i) {
        timeline.queue(static_cast<uint32_t>(100 + i),
                       static_cast<uint8_t>(36 + (i % 48)),
                       static_cast<uint8_t>(60 + (i % 60)),
                       static_cast<uint8_t>(i % 16));
    }
    assert(timeline.pending() == SmfMidiVisualTimeline::kCapacity);
    assert(timeline.snapshot().droppedEvents == 3);

    timeline.clearPending();
    assert(timeline.pending() == 0);
    assert(timeline.snapshot().velocity == 0);

    timeline.reset();
    assert(timeline.snapshot().epoch == epoch + 1);
    assert(timeline.snapshot().pulseCounter == 0);
    assert(timeline.snapshot().droppedEvents == 0);
    return 0;
}
'''
write("tests/test_smf_midi_visual.cpp", visual_test)

source_regression = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    visual = (ROOT / "src/midi/smf_midi_visual.h").read_text(encoding="utf-8")
    service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")

    require("kCapacity = 64" in visual and "std::array" in visual,
            "MIDI visual events must use bounded fixed storage")
    require("std::vector" not in visual and "new " not in visual and "malloc(" not in visual,
            "MIDI visual path must remain allocation-free")
    require("midiVisualTimeline_.queue" in service and
            "snapshot_.midiVisual" in service,
            "SMF service must publish actual accepted NoteOn activity")
    require("drawMidiWaveOverlay" in page and
            page.index("MusicVisuals::drawProgressBar") < page.index("drawMidiWaveOverlay"),
            "wave overlay must be drawn over the current MIDI progress track")
    require("midiWaveEnvelope_" in header and "midiWavePhase_" in header,
            "animation state must stay local to the UI page")
    require("TinyUSB" not in page and "USBMIDI" not in page,
            "MIDI Player UI must not become a USB owner")
    print("SMF MIDI wave source regressions: OK")


if __name__ == "__main__":
    main()
'''
write("tests/test_smf_midi_wave_source_regressions.py", source_regression)

stage_doc = r'''# SMF MIDI Wave Overlay Stage

## Purpose

Draw a small MIDI-reactive waveform directly over the current SMF progress track. The animation reacts to NoteOn events that were accepted by routing, track mute and the bounded SMF output queue. It is a musical activity display, not an audio oscilloscope of SEQTRAK output.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- SD card with `.mid` files under `/midi`

## Wiring

```text
Cardputer-Adv USB-C <---- MIDI ----> Yamaha SEQTRAK USB-C
```

PORT.A is unused. If unrelated I2C hardware is attached, keep GPIO2 SDA / GPIO1 SCL on `Wire`.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

- the existing Tape PCM waveform is unchanged;
- the MIDI Player progress bar contains a thin animated wave;
- NoteOn velocity controls visual amplitude;
- note pitch changes visual frequency;
- chords and dense passages create stronger movement;
- Pause, Stop, Panic and muted tracks stop producing new impulses;
- the envelope decays to a flat center line when MIDI activity stops;
- Inspector, Track Mute, RAW/SEQTRAK routing and SEQ MASTER remain usable.

## Troubleshooting

### Progress moves but the wave stays flat

Confirm that the selected track is not muted and that the file's source channels are mapped in the selected RAW/SEQTRAK routing mode. Unmapped and muted NoteOn events intentionally do not animate the overlay.

### Wave moves before the audible note

Treat as a regression. Scheduling may look ahead, but the visual timeline releases an impulse only when `currentTick` reaches the queued event tick.

### Existing Tape waveform changed

Treat as a regression. This stage does not modify `MiniAcid::WaveformBuffer` or `WaveformVisualization`.

## Acceptance checklist

```text
[ ] Tape page PCM waveform is unchanged
[ ] MIDI progress bar remains readable
[ ] wave moves on accepted SMF NoteOn events
[ ] stronger velocity gives a larger impulse
[ ] different pitches visibly change wave density
[ ] muted track stops creating new impulses
[ ] unmapped SEQTRAK-safe channels do not animate
[ ] Pause/Stop/Panic decays to a flat line
[ ] Continue resumes without an artificial first impulse
[ ] Inspector and Track Mute controls remain usable
[ ] no full-screen redraw was added specifically for the wave
[ ] no new task, heap allocation or USB owner was added
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
'''
write("docs/stages/SMF_MIDI_WAVE_OVERLAY_STAGE.md", stage_doc)

# Versioned service snapshot.
path = "src/midi/smf_player_service.h"
text = read(path)
text = replace_once(
    text,
    '#include "smf_channel_inspector.h"\n',
    '#include "smf_channel_inspector.h"\n#include "smf_midi_visual.h"\n',
    "service visual include")
text = replace_once(
    text,
    '    SmfPlayerPerformanceSnapshot performance{};\n',
    '    SmfPlayerPerformanceSnapshot performance{};\n    SmfMidiVisualSnapshot midiVisual{};\n',
    "service visual snapshot")
write(path, text)

# Cardputer service declarations and storage.
path = "src/platform/cardputer_smf_player.h"
text = read(path)
text = replace_once(
    text,
    '    void updatePlaybackSnapshot();\n',
    '    void updatePlaybackSnapshot();\n'
    '    void resetMidiVisual();\n'
    '    void queueMidiVisualNote(uint32_t tick, uint8_t note, uint8_t velocity, uint8_t channel);\n',
    "player visual methods")
text = replace_once(
    text,
    '    GroovePuterMidi::SmfDocument timingDocument_;\n',
    '    GroovePuterMidi::SmfDocument timingDocument_;\n'
    '    GroovePuterMidi::SmfMidiVisualTimeline midiVisualTimeline_;\n',
    "player visual storage")
write(path, text)

# Cardputer service scheduling and due-tick publication.
path = "src/platform/cardputer_smf_player.cpp"
text = read(path)
text = replace_once(
    text,
    'bool CardputerSmfPlayerService::loadFile(const char* path) {\n    stopAndCleanup(false);\n',
    'bool CardputerSmfPlayerService::loadFile(const char* path) {\n    stopAndCleanup(false);\n    resetMidiVisual();\n',
    "load visual reset")
text = replace_once(
    text,
    '    eventQueue_.invalidateAndRequestPanic();\n    projectLaunchPlanned_ = false;\n',
    '    eventQueue_.invalidateAndRequestPanic();\n    resetMidiVisual();\n    projectLaunchPlanned_ = false;\n',
    "original start visual reset")
text = replace_once(
    text,
    '    eventQueue_.invalidateAndRequestPanic();\n    if (!prepareStreamAt(tick)) return false;\n',
    '    eventQueue_.invalidateAndRequestPanic();\n    resetMidiVisual();\n    if (!prepareStreamAt(tick)) return false;\n',
    "project arm visual reset")
text = replace_once(
    text,
    '    pausedTick_ = currentTickFromAudioClock();\n    eventQueue_.invalidateAndRequestPanic();\n',
    '    pausedTick_ = currentTickFromAudioClock();\n    eventQueue_.invalidateAndRequestPanic();\n    resetMidiVisual();\n',
    "pause visual reset")
text = replace_once(
    text,
    'void CardputerSmfPlayerService::stopAndCleanup(bool resetToMusicStart) {\n    eventQueue_.invalidateAndRequestPanic();\n',
    'void CardputerSmfPlayerService::stopAndCleanup(bool resetToMusicStart) {\n    eventQueue_.invalidateAndRequestPanic();\n    resetMidiVisual();\n',
    "stop visual reset")
old_note_block = '''        if (event.event.kind == SmfEventKind::NoteOn) {
            pushed = eventQueue_.tryPushNoteOn(
                routed.channel,
                routed.note,
                applySmfVelocityBoost(event.event.data2, velocityBoost_),
                position.blockSequence,
                position.frameOffset,
                tempoMode_ == SmfTempoMode::Project
                    ? projectTransport.transportEpoch
                    : 0u);
        } else {
'''
new_note_block = '''        uint8_t visualVelocity = 0;
        if (event.event.kind == SmfEventKind::NoteOn) {
            visualVelocity = applySmfVelocityBoost(event.event.data2, velocityBoost_);
            pushed = eventQueue_.tryPushNoteOn(
                routed.channel,
                routed.note,
                visualVelocity,
                position.blockSequence,
                position.frameOffset,
                tempoMode_ == SmfTempoMode::Project
                    ? projectTransport.transportEpoch
                    : 0u);
        } else {
'''
text = replace_once(text, old_note_block, new_note_block, "visual note velocity")
text = replace_once(
    text,
    '        lastScheduledBlock_ = position.blockSequence;\n        hasPendingEvent_ = false;\n',
    '        if (event.event.kind == SmfEventKind::NoteOn) {\n'
    '            queueMidiVisualNote(event.event.tick, routed.note, visualVelocity, routed.channel);\n'
    '        }\n'
    '        lastScheduledBlock_ = position.blockSequence;\n'
    '        hasPendingEvent_ = false;\n',
    "visual queue after accepted event")
visual_methods = r'''
void CardputerSmfPlayerService::resetMidiVisual() {
    midiVisualTimeline_.reset();
    const SmfMidiVisualSnapshot visual = midiVisualTimeline_.snapshot();
    portENTER_CRITICAL(&snapshotMux_);
    snapshot_.midiVisual = visual;
    portEXIT_CRITICAL(&snapshotMux_);
}

void CardputerSmfPlayerService::queueMidiVisualNote(
        uint32_t tick, uint8_t note, uint8_t velocity, uint8_t channel) {
    midiVisualTimeline_.queue(tick, note, velocity, channel);
}

'''
text = replace_once(
    text,
    'void CardputerSmfPlayerService::updatePlaybackSnapshot() {\n',
    visual_methods + 'void CardputerSmfPlayerService::updatePlaybackSnapshot() {\n',
    "visual method definitions")
text = replace_once(
    text,
    '    const uint16_t bpmX10 = effectiveBpmX10At(tick);\n\n    portENTER_CRITICAL(&snapshotMux_);\n',
    '    const uint16_t bpmX10 = effectiveBpmX10At(tick);\n'
    '    const SmfMidiVisualSnapshot midiVisual = midiVisualTimeline_.advanceTo(tick);\n\n'
    '    portENTER_CRITICAL(&snapshotMux_);\n',
    "advance visual at current tick")
text = replace_once(
    text,
    '    snapshot_.launchMode = launchMode_;\n    portEXIT_CRITICAL(&snapshotMux_);\n',
    '    snapshot_.launchMode = launchMode_;\n'
    '    snapshot_.midiVisual = midiVisual;\n'
    '    portEXIT_CRITICAL(&snapshotMux_);\n',
    "publish visual snapshot")
write(path, text)

# UI header: Inspector, Track Mute and local overlay animation state.
path = "src/ui/pages/smf_player_page.h"
text = read(path)
text = replace_once(
    text,
    '    void drawChannelInspector(IGfx& gfx);\n',
    '    void drawChannelInspector(IGfx& gfx);\n'
    '    void drawMidiWaveOverlay(IGfx& gfx,\n'
    '                             const GroovePuterMidi::SmfPlayerSnapshot& state,\n'
    '                             const Rect& region,\n'
    '                             IGfxColor color);\n',
    "page wave declaration")
text = replace_once(
    text,
    '    int channelInspectorScroll_{0};\n',
    '    int channelInspectorScroll_{0};\n'
    '    uint32_t lastMidiVisualEpoch_{0};\n'
    '    uint32_t lastMidiVisualPulse_{0};\n'
    '    uint16_t midiWavePhase_{0};\n'
    '    uint8_t midiWaveEnvelope_{0};\n',
    "page wave state")
write(path, text)

# UI implementation.
path = "src/ui/pages/smf_player_page.cpp"
text = read(path)
text = replace_once(
    text,
    '#include "src/midi/transport_clock_runtime.h"\n',
    '#include "src/midi/transport_clock_runtime.h"\n#include "src/midi/smf_track_mute.h"\n',
    "track mute include")
helpers = r'''
void formatMidiNote(uint8_t note, char* dst, std::size_t size) {
    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int octave = static_cast<int>(note / 12u) - 1;
    std::snprintf(dst, size, "%s%d", kNames[note % 12u], octave);
}

const char* inspectorRouteLabel(bool raw, uint8_t sourceChannel) {
    if (raw) return "RAW";
    if (sourceChannel == 0) return "S1";
    if (sourceChannel == 1) return "S2";
    if (sourceChannel == 2) return "DX";
    if (sourceChannel == 9) return "DRM";
    return "OFF";
}
'''
text = replace_once(
    text,
    '}  // namespace\n\nusing namespace GroovePuterMidi;\n',
    helpers + '}  // namespace\n\nusing namespace GroovePuterMidi;\n',
    "inspector helpers")
inspector_events = r'''    if (event.key == 'i' || event.key == 'I') {
        channelInspectorVisible_ = !channelInspectorVisible_;
        if (channelInspectorVisible_) {
            performanceVisible_ = false;
            channelInspectorScroll_ = 0;
        }
        return true;
    }
    if (channelInspectorVisible_) {
        const SmfChannelInspectorSnapshot inspector = player_->channelInspector();
        constexpr int kVisibleRows = 6;
        const int maxScroll = std::max(
            0, static_cast<int>(inspector.usedChannelCount()) - kVisibleRows);
        if (event.scancode == GROOVEPUTER_UP) {
            channelInspectorScroll_ = std::max(0, channelInspectorScroll_ - 1);
            return true;
        }
        if (event.scancode == GROOVEPUTER_DOWN) {
            channelInspectorScroll_ = std::min(maxScroll, channelInspectorScroll_ + 1);
            return true;
        }
        if (event.scancode == GROOVEPUTER_LEFT ||
            event.scancode == GROOVEPUTER_RIGHT) {
            return true;
        }
    }

'''
text = replace_once(
    text,
    '    if (!player_) return false;\n    const SmfPlayerSnapshot state = player_->snapshot();\n\n    if (event.scancode == GROOVEPUTER_LEFT) {\n',
    '    if (!player_) return false;\n'
    '    const SmfPlayerSnapshot state = player_->snapshot();\n\n' +
    inspector_events +
    '    if (event.scancode == GROOVEPUTER_LEFT) {\n',
    "inspector event routing")
track_events = r'''    if (event.key == 'j' || event.key == 'J' ||
        event.key == 'l' || event.key == 'L') {
        smfTrackMuteState().selectRelative(
            (event.key == 'j' || event.key == 'J') ? -1 : 1);
        const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
        char toast[40];
        if (tracks.trackCount == 0) {
            std::snprintf(toast, sizeof(toast), "NO MIDI TRACKS");
        } else {
            std::snprintf(toast, sizeof(toast), "TRACK %u/%u %s",
                          static_cast<unsigned>(tracks.selectedTrack + 1u),
                          static_cast<unsigned>(tracks.trackCount),
                          tracks.selectedMuted() ? "MUTED" : "ON");
        }
        UI::showToast(toast, 800);
        return true;
    }
    if (event.key == 'k' || event.key == 'K') {
        if (event.shift) {
            smfTrackMuteState().clear();
            UI::showToast("ALL MIDI TRACKS ON", 900);
        } else if (smfTrackMuteState().toggleSelected()) {
            const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
            UI::showToast(tracks.selectedMuted()
                              ? "TRACK MUTE: NEXT NOTES"
                              : "TRACK UNMUTED",
                          900);
        } else {
            UI::showToast("NO MIDI TRACKS", 900);
        }
        return true;
    }
'''
text = replace_once(
    text,
    "    if (event.key == 'v' || event.key == 'V') {\n",
    track_events + "    if (event.key == 'v' || event.key == 'V') {\n",
    "track mute controls")
text = replace_once(
    text,
    "    if (event.key == 'd' || event.key == 'D') {\n        performanceVisible_ = !performanceVisible_;\n        return true;\n    }\n",
    "    if (event.key == 'd' || event.key == 'D') {\n"
    "        performanceVisible_ = !performanceVisible_;\n"
    "        if (performanceVisible_) channelInspectorVisible_ = false;\n"
    "        return true;\n"
    "    }\n",
    "performance inspector exclusion")
text = replace_once(
    text,
    "        browserVisible_ = true;\n        refreshFiles();\n",
    "        browserVisible_ = true;\n        channelInspectorVisible_ = false;\n        refreshFiles();\n",
    "browser hides inspector")
text = replace_once(
    text,
    'void SmfPlayerPage::drawHeader(IGfx& gfx) {\n    UI::drawStandardHeader(gfx, miniAcid_, performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER");\n}\n',
    'void SmfPlayerPage::drawHeader(IGfx& gfx) {\n'
    '    UI::drawStandardHeader(gfx, miniAcid_, channelInspectorVisible_\n'
    '        ? "MIDI CHANNELS"\n'
    '        : (performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER"));\n'
    '}\n',
    "inspector header")
text = replace_once(
    text,
    '    if (browserVisible_) drawBrowser(gfx);\n    else if (performanceVisible_) drawPerformance(gfx);\n',
    '    if (browserVisible_) drawBrowser(gfx);\n'
    '    else if (channelInspectorVisible_) drawChannelInspector(gfx);\n'
    '    else if (performanceVisible_) drawPerformance(gfx);\n',
    "inspector content")
text = replace_once(
    text,
    '    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};\n\n    const bool playing = state.state == SmfPlayerState::Playing;\n',
    '    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};\n'
    '    const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();\n\n'
    '    const bool playing = state.state == SmfPlayerState::Playing;\n',
    "track snapshot in player")
progress_call = '''    MusicVisuals::drawProgressBar(gfx,
                                  Layout::COL_1,
                                  LayoutManager::lineY(3) + 1,
                                  Layout::CONTENT.w - 12,
                                  9,
                                  current,
                                  total,
                                  stateColor);
'''
text = replace_once(
    text,
    progress_call,
    progress_call +
    '    drawMidiWaveOverlay(gfx, state,\n'
    '                        Rect(Layout::COL_1 + 2, LayoutManager::lineY(3) + 3,\n'
    '                             Layout::CONTENT.w - 16, 5),\n'
    '                        MusicVisuals::secondaryForStyle());\n',
    "wave over progress")
old_progress_label = '''    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "%lu / %lu BARS    %u%%",
                  static_cast<unsigned long>(state.bar),
                  static_cast<unsigned long>(state.totalBars),
                  percent);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);
'''
new_progress_label = '''    gfx.setTextColor(tracks.selectedMuted() ? COLOR_WARN : COLOR_LABEL);
    if (tracks.trackCount > 0) {
        std::snprintf(line, sizeof(line), "%lu/%lu BAR %u%%  TRK %u/%u %s",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent,
                      static_cast<unsigned>(tracks.selectedTrack + 1u),
                      static_cast<unsigned>(tracks.trackCount),
                      tracks.selectedMuted() ? "MUTE" : "ON");
    } else {
        std::snprintf(line, sizeof(line), "%lu / %lu BARS    %u%%",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent);
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);
'''
text = replace_once(text, old_progress_label, new_progress_label, "track progress label")
text = replace_once(
    text,
    '                     ? "G FOLLOW   SPACE MIDI   R RESTART"\n'
    '                     : "G GROOVE   SPACE MIDI   R RESTART");\n',
    '                     ? "G FOLLOW SPACE MIDI J/L/K TRK"\n'
    '                     : "G GROOVE SPACE MIDI J/L/K TRK");\n',
    "track control line")
inspector_and_wave = r'''
void SmfPlayerPage::drawMidiWaveOverlay(
        IGfx& gfx,
        const SmfPlayerSnapshot& state,
        const Rect& region,
        IGfxColor color) {
    const SmfMidiVisualSnapshot& visual = state.midiVisual;
    if (visual.epoch != lastMidiVisualEpoch_) {
        lastMidiVisualEpoch_ = visual.epoch;
        lastMidiVisualPulse_ = visual.pulseCounter;
        midiWaveEnvelope_ = 0;
        midiWavePhase_ = 0;
    } else if (state.state == SmfPlayerState::Playing &&
               visual.pulseCounter != lastMidiVisualPulse_) {
        lastMidiVisualPulse_ = visual.pulseCounter;
        midiWaveEnvelope_ = std::max<uint8_t>(24, visual.velocity);
        midiWavePhase_ = static_cast<uint16_t>(
            midiWavePhase_ + visual.note * 3u + visual.channel * 11u + 7u);
    } else if (midiWaveEnvelope_ > 0) {
        midiWaveEnvelope_ = midiWaveEnvelope_ > 7u
            ? static_cast<uint8_t>(midiWaveEnvelope_ - 7u)
            : 0u;
    }

    const int midY = region.y + region.h / 2;
    gfx.drawLine(region.x, midY, region.x + region.w - 1, midY, COLOR_LABEL);
    if (midiWaveEnvelope_ == 0 || region.w < 3 || region.h < 3) return;

    const int amplitude = std::max(
        1, ((region.h / 2) * static_cast<int>(midiWaveEnvelope_)) / 127);
    const int cycles = 2 + static_cast<int>(visual.note % 7u);
    int previousX = region.x;
    int previousY = midY;
    constexpr int kPoints = 32;
    for (int point = 1; point < kPoints; ++point) {
        const int x = region.x + (point * (region.w - 1)) / (kPoints - 1);
        const int phase = static_cast<int>(
            (midiWavePhase_ + point * cycles * 4u) & 63u);
        const int triangle = phase < 32 ? phase - 16 : 48 - phase;
        const int accent = ((point + static_cast<int>(visual.pulseCounter)) & 7) == 0
            ? (visual.velocity > 96 ? 5 : 2)
            : 0;
        const int y = midY - ((triangle + accent) * amplitude) / 16;
        gfx.drawLine(previousX, previousY, x, y, color);
        previousX = x;
        previousY = y;
    }
    midiWavePhase_ = static_cast<uint16_t>(midiWavePhase_ + 3u + cycles);
}

void SmfPlayerPage::drawChannelInspector(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfChannelInspectorSnapshot inspector = player_
        ? player_->channelInspector()
        : SmfChannelInspectorSnapshot{};

    char line[64];
    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "F%u PPQN %u TRK %u USED %u",
                  static_cast<unsigned>(inspector.format),
                  static_cast<unsigned>(inspector.division),
                  static_cast<unsigned>(inspector.trackCount),
                  static_cast<unsigned>(inspector.usedChannelCount()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    uint8_t used[kSmfMidiChannelCount]{};
    int usedCount = 0;
    for (uint8_t channel = 0; channel < kSmfMidiChannelCount; ++channel) {
        if ((inspector.usedChannelMask & (1u << channel)) != 0) {
            used[usedCount++] = channel;
        }
    }

    constexpr int kVisibleRows = 6;
    const int maxScroll = std::max(0, usedCount - kVisibleRows);
    channelInspectorScroll_ = std::max(0, std::min(channelInspectorScroll_, maxScroll));

    if (usedCount == 0) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "NO NOTE OR PROGRAM CHANNELS");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "LOAD A MIDI FILE FIRST");
        return;
    }

    for (int row = 0; row < kVisibleRows; ++row) {
        const int index = channelInspectorScroll_ + row;
        if (index >= usedCount) break;
        const uint8_t channel = used[index];
        const SmfChannelInfo& info = inspector.channels[channel];

        char low[5] = "--";
        char high[5] = "--";
        if (info.hasNotes()) {
            formatMidiNote(info.minNote, low, sizeof(low));
            formatMidiNote(info.maxNote, high, sizeof(high));
        }
        char program[6] = "P---";
        if (info.hasProgramChange) {
            std::snprintf(program, sizeof(program), "P%03u",
                          static_cast<unsigned>(info.firstProgram));
        }
        const unsigned shownNotes = static_cast<unsigned>(
            info.noteCount > 9999u ? 9999u : info.noteCount);
        const unsigned shownPoly = static_cast<unsigned>(
            info.maxPolyphony > 99u ? 99u : info.maxPolyphony);
        std::snprintf(line, sizeof(line),
                      "C%02u N%04u %-3s-%-3s V%03u X%02u %s %-3s",
                      static_cast<unsigned>(channel + 1u),
                      shownNotes,
                      low,
                      high,
                      static_cast<unsigned>(info.averageVelocity()),
                      shownPoly,
                      program,
                      inspectorRouteLabel(state.rawRouting, channel));
        gfx.setTextColor(info.likelyDrums
                             ? MusicVisuals::accentForStyle()
                             : COLOR_TEXT);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(row + 1), line);
    }

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7),
                 "I PLAYER UP/DN SCROLL P=PROGRAM");
}

'''
text = replace_once(
    text,
    'void SmfPlayerPage::drawPerformance(IGfx& gfx) {\n',
    inspector_and_wave + 'void SmfPlayerPage::drawPerformance(IGfx& gfx) {\n',
    "inspector and wave methods")
old_footer = '''void SmfPlayerPage::drawFooter(IGfx& gfx) {
    const bool seqMaster = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Select  Enter Load",
                               seqMaster ? "C Master  G Follow  T Tempo"
                                         : "C Master  Space MIDI  T Tempo");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player  B Files",
                               seqMaster ? "C Master  G Follow  T Tempo"
                                         : "C Master  Space MIDI  T Tempo");
    } else {
        UI::drawStandardFooter(gfx,
                               seqMaster ? "Space MIDI  G Follow  C Master"
                                         : "Space MIDI  C Master  R Restart",
                               "B Files  T Tempo  V Vel  X Panic");
    }
}
'''
new_footer = '''void SmfPlayerPage::drawFooter(IGfx& gfx) {
    const bool seqMaster = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Select Enter Load",
                               seqMaster ? "C Master G Follow T Tempo"
                                         : "C Master Space MIDI T Tempo");
    } else if (channelInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Scroll I Player",
                               "D Perf B Files Space MIDI");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player B Files I Channels",
                               seqMaster ? "C Master G Follow T Tempo"
                                         : "C Master Space MIDI T Tempo");
    } else {
        UI::drawStandardFooter(gfx,
                               seqMaster ? "Space MIDI G Follow C Master"
                                         : "Space MIDI C Master R Restart",
                               "I Channels J/L Track K Mute");
    }
}
'''
text = replace_once(text, old_footer, new_footer, "combined footer")
write(path, text)

# Host runner additions.
path = "tests/run_host_tests.sh"
text = read(path)
python_line = 'python3 "${ROOT_DIR}/tests/test_smf_midi_wave_source_regressions.py"\n'
if python_line not in text:
    text = replace_once(
        text,
        'python3 "${ROOT_DIR}/tests/test_seqtrak_master_source_regressions.py"\n',
        'python3 "${ROOT_DIR}/tests/test_seqtrak_master_source_regressions.py"\n' + python_line,
        "wave source runner")
blocks = r'''

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_channel_inspector.cpp" \
  -o "${BUILD_DIR}/test_smf_channel_inspector"

"${BUILD_DIR}/test_smf_channel_inspector"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_midi_visual.cpp" \
  -o "${BUILD_DIR}/test_smf_midi_visual"

"${BUILD_DIR}/test_smf_midi_visual"
'''
if 'test_smf_midi_visual' not in text:
    marker = '"${BUILD_DIR}/test_smf_stream"\n'
    text = replace_once(text, marker, marker + blocks, "visual test runner")
write(path, text)

print("SMF inspector, track mute and MIDI wave integration applied")
