#!/usr/bin/env python3
import re
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
    engine_h = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    drum_tap = (ROOT / "src/dsp/pattern_drum_event_tap.h").read_text(encoding="utf-8")
    drum_gate = (ROOT / "src/midi/pattern_drum_gate_scheduler.h").read_text(encoding="utf-8")
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    scheduled = (ROOT / "src/midi/scheduled_musical_event.h").read_text(encoding="utf-8")
    queue = (ROOT / "src/midi/scheduled_musical_event_queue.h").read_text(encoding="utf-8")
    facade = (ROOT / "src/input/musical_event_queue.h").read_text(encoding="utf-8")
    control_queue = (ROOT / "src/midi/midi_control_event_queue.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("enum class MusicalEventTarget" in event_header and
            "    Drums," in event_header and "    Dx," in event_header,
            "live keyboard routing must expose Drums and DX targets")
    require("MusicalEventSource::PerformanceKeyboard" in sink and
            "MusicalEventSource::PatternPlayer" in sink,
            "USB sink must expose separate live and PatternPlayer lanes")
    require("MusicalEventTarget::SynthA" in sink and
            "MusicalEventTarget::SynthB" in sink and
            "MusicalEventTarget::Drums" in sink and
            "MusicalEventTarget::Dx" in sink,
            "USB output must retain Synth A/B, Drums and DX targets")
    ownership_h = (ROOT / "src/midi/midi_note_ownership_table.h").read_text(
        encoding="utf-8")
    require("kLaneCount = 20" in sink_h,
            "USB output must own live A/B/DX + seven live drums + Pattern A/B + eight Pattern drum voices")
    constructor_end = sink.index("uint8_t UsbMidiOutput::clampChannel")
    constructor = sink[:constructor_end]
    require("MusicalEventSource::PerformanceKeyboard,\n        MusicalEventTarget::Dx" in constructor,
            "DX must be a live keyboard lane")
    require("drumChannel < kSeqtrakDrumLaneCount" in constructor and
            "MusicalEventTarget::Drums" in constructor,
            "native Drums must construct seven live lanes")
    require("voice < kPatternDrumVoiceCount" in constructor and
            "MusicalEventSource::PatternPlayer,\n            MusicalEventTarget::Drums" in constructor,
            "PatternPlayer must construct all eight internal drum voice lanes")
    require("patternDrumChannel" in sink and
            "0, 1, 3, 4, 5, 6, 5, 2" in sink,
            "Pattern drums must map eight internal voices onto native SEQTRAK CH1..7")
    require("MidiEndpointOwnershipTable owners_;" in sink_h,
            "logical lanes sharing a MIDI channel need wire-level ownership")
    require("wireOwnerCount" in ownership_h and "smfOwnerCount" in ownership_h,
            "wire-level ownership must stay two-level: SMF owners are a subset "
            "of wire owners, so dropping SMF cannot silence a pattern note")
    require("activeCount" in sink_h and "acquirePercussiveNote" in sink,
            "retriggered Pattern drums must retain bounded per-lane ownership")
    require("pendingRelease" in sink_h and "pendingRelease" in sink,
            "failed replacement NoteOff must remain retryable")
    require("patternSynthAChannel{7}" in sink_h and
            "patternSynthBChannel{8}" in sink_h,
            "PatternPlayer synth routes must remain MIDI channels 8 and 9")
    require("performanceSynthBChannel{8}" in sink_h and
            "performanceDxChannel{9}" in sink_h,
            "live Synth B and DX defaults must be MIDI channels 9 and 10")
    require("kSeqtrakDrumLaneCount = 7" in sink_h and
            "kPatternDrumVoiceCount = 8" in sink_h,
            "live and Pattern drum topology must remain explicit")

    for token in ("blockSequence", "frameOffset", "generation",
                  "publicationSequence"):
        require(token in scheduled,
                f"scheduled event contract must include {token}")
    require("scheduledMusicalEventBefore" in scheduled and
            "scheduledMusicalEventGenerationIsCurrent" in scheduled,
            "scheduled event ordering and lifecycle checks must be explicit")

    require("kStorageSize = 128" in queue and
            "kCapacity = kStorageSize - 1" in queue,
            "Pattern MIDI queue must remain fixed and bounded")
    require("pcTaskGetName(nullptr)" in queue and '"AudioTask"' in queue,
            "only AudioTask may publish realtime Pattern events")
    require("kDrumsMask" in queue and "drumsGeneration_" in queue and
            "pendingDrumsEpoch_" in queue,
            "Pattern Drums must have independent generation and panic state")
    require("invalidateTarget" in queue and "generationFor" in queue,
            "lifecycle invalidation must be target scoped")
    require("event.type == MusicalEventType::AllNotesOff" in queue and
            "invalidateTarget(event.target)" in queue,
            "AllNotesOff must invalidate stale scheduled generations")
    require("takePendingAllNotesOffMask" in queue,
            "critical Pattern overflow must degrade to target-scoped cleanup")
    require("kStorageSize = 32" in control_queue,
            "live USB handoff must remain a bounded queue")
    require("kDrumsMask" in control_queue and "pendingDrumsEpoch_" in control_queue,
            "live Drums overflow must retain a scoped cleanup path")
    require("kDxMask" in control_queue and "pendingDxEpoch_" in control_queue,
            "live DX overflow must retain a scoped cleanup path")
    for path_text in (queue, facade, control_queue, drum_gate):
        for token in ("std::vector", "std::deque", "new ", "malloc("):
            require(token not in path_text,
                    f"realtime MIDI queues/gates must not allocate: {token}")

    require("class MusicalEventQueue final : public ScheduledMusicalEventQueue" in facade,
            "existing MiniAcid publication API must feed the scheduled queue")
    require("beginMidiRenderBlock" in facade and
            "endMidiRenderBlock" in facade and
            "setPhaseReader" in facade,
            "compatibility facade must own the render timing bracket")
    require("phaseReader_(phaseReaderContext_)" in facade and
            "renderBlockSequence_" in facade and
            "frame" in facade,
            "frame offsets must be calculated at the exact event publication point")
    require("if (!renderBlockActive_)" in facade and
            "suppressNonRealtimeEvent(event)" in facade,
            "offline rendering must not enqueue render-speed MIDI bursts")

    require("publishPatternNoteOn_" in engine and
            "publishPatternNoteOff_" in engine and
            "publishPatternAllNotesOff_" in engine,
            "PatternPlayer synth lifecycle must keep normalized publication")
    require("PatternEventQueueHandle patternEventQueue_" in engine_h and
            'include "pattern_drum_event_tap.h"' in engine_h,
            "MiniAcid must retain one facade while enabling the drum trigger tap")
    require("PatternPublishingDrumVoice drums" in engine_h,
            "all internal drum engines must pass through the shared Pattern trigger decorator")
    require("patternEventQueue_->tryPush(event)" in engine,
            "Pattern synth events must enter the timing facade at publication time")
    require("publishPatternDrumTrigger" in drum_tap and
            "MusicalEventSource::PatternPlayer" in drum_tap and
            "MusicalEventTarget::Drums" in drum_tap,
            "actual drum trigger calls must publish normalized PatternPlayer/Drums NoteOn events")
    for trigger in ("triggerKick", "triggerSnare", "triggerHat", "triggerOpenHat",
                    "triggerMidTom", "triggerHighTom", "triggerRim", "triggerClap"):
        require(trigger in drum_tap,
                f"drum tap must cover sequenced voice trigger {trigger}")
    require("event.target == MusicalEventTarget::SynthB" in drum_tap and
            "MusicalEventType::AllNotesOff" in drum_tap and
            "MusicalEventTarget::Drums" in drum_tap,
            "existing Pattern lifecycle barriers must also invalidate Drums")
    require("USBMIDI" not in drum_tap and "TinyUSB" not in drum_tap,
            "DSP drum tap must not know the physical USB transport")

    require("class PatternDrumGateScheduler" in drum_gate and
            "scheduleOrExtend" in drum_gate and "releaseCount" in drum_gate,
            "Pattern drum NoteOff must use a bounded retrigger-aware gate scheduler")
    require("blockSequence" in drum_gate and "frameOffset" in drum_gate and
            "sampleRate" in drum_gate and "blockFrames" in drum_gate,
            "drum gate timing must remain in the sample/block timeline")
    require("millis()" not in drum_gate and "micros()" not in drum_gate and
            "vTaskDelay" not in drum_gate and "delay(" not in drum_gate,
            "Pattern drum gate scheduler must not become a wall-clock scheduler")

    scene_apply = engine.index("void MiniAcid::applySceneStateFromManager()")
    scene_body = engine[scene_apply:scene_apply + 260]
    require("if (playing) publishPatternAllNotesOff_();" in scene_body,
            "scene application must release stale PatternPlayer ownership")
    require("MusicalEventSource::PatternPlayer" in engine,
            "engine synth events must identify PatternPlayer as their source")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP engine must not depend on hardware USB APIs")
    require("event.source == MusicalEventSource::PatternPlayer" in internal,
            "internal sink must ignore already-rendered PatternPlayer fan-out")
    require("event.target == MusicalEventTarget::Drums" in internal and
            "event.target == MusicalEventTarget::Dx" in internal,
            "external live Drums/DX must never alias to an internal synth voice")

    render_start = engine.index("bool MiniAcid::renderProjectToWav")
    render_body = engine[render_start:render_start + 5200]
    require("stop();" in render_body and "generateAudioBuffer" in render_body,
            "offline render must remain synchronous")

    setup_start = sketch.index("void setup()")
    loop_start = sketch.index("void loop()")
    require("setPhaseReader" in sketch[setup_start:loop_start] and
            "setPatternEventQueue(&g_patternMusicalEventQueue)" in sketch[setup_start:loop_start],
            "setup must connect MiniAcid phase and events to the scheduled facade")
    require("drainPatternMusicalEvents" not in sketch,
            "Arduino loop must no longer dispatch Pattern MIDI")
    require("beginMidiRenderBlock" in sketch and
            "endMidiRenderBlock" in sketch and
            "publishCardputerUsbMidiBlockAnchor" in sketch,
            "AudioTask must establish and publish sample-block timing")
    require("readPatternSequencerPhase" in sketch,
            "AudioTask timing must use MiniAcid's sequencer phase")
    require(re.search(r'xTaskCreatePinnedToCore\(\s*audioTask\s*,\s*"AudioTask"', sketch) is not None,
            "realtime publication depends on the pinned AudioTask identity")

    registration_call = "registerCardputerUsbMidiSink("
    require(registration_call in sketch[setup_start:loop_start] and
            "g_musicalEventRouter" in sketch[setup_start:loop_start] and
            "g_patternMusicalEventQueue" in sketch[setup_start:loop_start] and
            "g_externalMidiTransportQueue" in sketch[setup_start:loop_start],
            "setup must register the single USB owner with the scheduled queue")
    require("router.addSink(g_queueSink)" in transport,
            "live USB events must enter the dispatcher through a bounded sink")
    require("router.addSink(g_output)" not in transport,
            "UsbMidiOutput must not regain direct router ownership")
    require("midiDispatchTask" in transport and
            "scheduledMusicalEventGenerationIsCurrent" in transport,
            "dispatcher must reject stale generations")
    require("deadlineFor" in transport and "frameOffset" in transport,
            "dispatcher must convert sample offsets into deadlines")
    require("PatternDrumGateScheduler g_patternDrumGates" in transport and
            "g_patternDrumGates.scheduleOrExtend" in transport and
            "PatternDrumGateScheduler" in transport,
            "existing MidiDispatchTask must own the bounded Pattern drum gate state")
    require("GroovePuterMidi::kDefaultDrumGateMs" in transport,
            "Pattern drums must use the companion profile gate default")
    require("ScheduledMusicalEventQueue::kDrumsMask" in transport and
            "g_patternDrumGates.clear()" in transport,
            "Pattern drum lifecycle cleanup must clear both gates and wire ownership")
    require("g_output.handleMusicalEvent" in transport,
            "only the dispatcher translation unit may mutate UsbMidiOutput")
    require("MidiControlEventQueue::kDrumsMask" in transport and
            "MusicalEventTarget::Drums" in transport,
            "dispatcher must perform scoped recovery for dropped live Drums cleanup")
    require("MidiControlEventQueue::kDxMask" in transport and
            "MusicalEventTarget::Dx" in transport,
            "dispatcher must perform scoped recovery for dropped live DX cleanup")
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
                f"out-of-scope MIDI feature entered companion integration: {token}")

    require("UsbMidi" not in scenes_h and "UsbMidi" not in scenes_cpp,
            "USB MIDI settings must remain device-global, not scene data")
    require("usbMidi" not in scenes_h and "usbMidi" not in scenes_cpp,
            "scene schema must not gain USB MIDI fields")

    print("Pattern MIDI source regressions: OK")


if __name__ == "__main__":
    main()
