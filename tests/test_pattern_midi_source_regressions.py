#!/usr/bin/env python3
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
    require("MusicalEventTarget::Drums" in event_header,
            "live keyboard routing must expose the Drums target")
    require("MusicalEventSource::PerformanceKeyboard" in sink and
            "MusicalEventSource::PatternPlayer" in sink,
            "USB sink must expose separate live and PatternPlayer lanes")
    require("MusicalEventTarget::SynthA" in sink and
            "MusicalEventTarget::SynthB" in sink,
            "Pattern MIDI must route both synth targets")
    require("kLaneCount = 5" in sink_h,
            "USB output must keep five fixed live/pattern ownership lanes")
    constructor_end = sink.index("uint8_t UsbMidiOutput::clampChannel")
    constructor = sink[:constructor_end]
    require("MusicalEventSource::PerformanceKeyboard,\n        MusicalEventTarget::Drums" in constructor,
            "Drums must be a live keyboard lane")
    require("MusicalEventSource::PatternPlayer,\n        MusicalEventTarget::Drums" not in constructor,
            "PatternPlayer must remain limited to Synth A/B")
    require("wireOwners_[kMidiChannelCount][kMidiNoteCount]" in sink_h,
            "logical lanes sharing a MIDI channel need wire-level note ownership")
    require("pendingRelease" in sink_h and "pendingRelease" in sink,
            "failed replacement NoteOff must remain retryable")
    require("patternSynthAChannel{7}" in sink_h and
            "patternSynthBChannel{8}" in sink_h,
            "fixed PatternPlayer routes must remain MIDI channels 8 and 9")
    require("performanceSynthBChannel{8}" in sink_h and
            "performanceDrumsChannel{9}" in sink_h,
            "live B/Drums routes must be MIDI channels 9 and 10")

    require("publishPatternNoteOn_" in engine and
            "publishPatternNoteOff_" in engine and
            "publishPatternAllNotesOff_" in engine,
            "PatternPlayer lifecycle must publish normalized events")
    scene_apply = engine.index("void MiniAcid::applySceneStateFromManager()")
    scene_body = engine[scene_apply:scene_apply + 240]
    require("if (playing) publishPatternAllNotesOff_();" in scene_body,
            "scene application must release stale PatternPlayer MIDI ownership")
    require("MusicalEventSource::PatternPlayer" in engine,
            "engine events must identify PatternPlayer as their source")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP engine must not depend on hardware USB APIs")
    require("event.source == MusicalEventSource::PatternPlayer" in internal,
            "internal sink must ignore already-rendered PatternPlayer fan-out")
    require("event.target == MusicalEventTarget::Drums" in internal,
            "external Drums target must not alias to an internal synth voice")

    require("kStorageSize = 64" in queue and
            "kCapacity = kStorageSize - 1" in queue,
            "audio-to-control handoff must expose its 64-slot/63-event bounds")
    require('asm volatile("memw"' in queue and
            "alignas(4) volatile uint32_t" in queue,
            "ESP32-S3 realtime publication must use aligned native words and barriers")
    require("__atomic_always_lock_free" not in queue and
            "is_always_lock_free" not in queue,
            "queue must not rely on unsupported pinned-toolchain atomic traits")
    for token in ("std::vector", "std::deque", "new ", "malloc("):
        require(token not in queue, f"realtime queue must not allocate: {token}")
    require("takePendingAllNotesOffMask" in queue,
            "critical queue overflow must degrade to a target-scoped panic")
    require("pcTaskGetName(nullptr)" in queue and '"AudioTask"' in queue,
            "Cardputer queue must accept realtime pattern events only from AudioTask")
    require("suppressedNonRealtime_" in queue,
            "offline render suppression must remain observable")
    render_start = engine.index("bool MiniAcid::renderProjectToWav")
    render_body = engine[render_start:render_start + 5200]
    require("stop();" in render_body and "generateAudioBuffer" in render_body,
            "offline render must remain a stopped, synchronous non-AudioTask lifecycle")

    setup_start = sketch.index("void setup()")
    loop_start = sketch.index("void loop()")
    require("setPatternEventQueue(&g_patternMusicalEventQueue)" in sketch[setup_start:loop_start],
            "setup must connect PatternPlayer to the fixed event queue")
    require("drainPatternMusicalEvents();" in sketch[loop_start:],
            "control loop must drain PatternPlayer events into the shared router")
    require("g_musicalEventRouter.route(event)" in sketch,
            "queued PatternPlayer events must use MusicalEventRouter")
    require('xTaskCreatePinnedToCore(audioTask, "AudioTask"' in sketch,
            "offline-render suppression depends on the pinned AudioTask identity")

    require("router.addSink(g_output)" in transport,
            "USB MIDI must remain an independent router sink")
    require("UsbMidiRouteConfig" in transport and "7," in transport and "8," in transport,
            "platform route config must retain PatternPlayer channels 8/9")
    require("9,     // live Drums" in transport,
            "platform route config must map live Drums to channel 10")
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
