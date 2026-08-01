#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "host-tests"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def compile_and_run(source_name: str) -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests" / source_name
    output = BUILD / source.stem
    subprocess.run(
        [
            os.environ.get("CXX", "g++"),
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{ROOT}",
            str(source),
            "-o",
            str(output),
        ],
        check=True,
    )
    subprocess.run([str(output)], check=True)


def main() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(
        encoding="utf-8"
    )
    transport_h = (ROOT / "src/platform/cardputer_usb_midi_transport.h").read_text(
        encoding="utf-8"
    )
    player = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
    smf_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8"
    )
    parser = (ROOT / "src/midi/usb_midi_realtime_parser.h").read_text(
        encoding="utf-8"
    )
    follower = (ROOT / "src/midi/external_midi_clock_follower.h").read_text(
        encoding="utf-8"
    )
    tracker = (ROOT / "src/midi/external_midi_clock_tracker.h").read_text(
        encoding="utf-8"
    )
    timeline = (ROOT / "src/midi/project_transport_timeline.h").read_text(
        encoding="utf-8"
    )

    require("midi_.readPacket(&packet)" in transport,
            "the sole platform transport must own TinyUSB RX")
    require("readPacket(midiEventPacket_t& packet)" in transport_h,
            "the Cardputer transport must expose bounded packet polling")
    require("kMidiRxDrainBudget = 32" in transport and
            "drainIncomingMidiPackets()" in transport,
            "MidiDispatchTask must bound every RX drain pass")
    require(all(status in parser for status in ("0xf8", "0xfa", "0xfb", "0xfc")),
            "the pure parser must recognize Clock/Start/Continue/Stop")
    require("g_externalMidiTransportQueue" in sketch and
            "g_externalClockFollower.processBlock" in sketch,
            "AudioTask must consume the external transport queue")
    require(sketch.index("g_externalClockFollower.processBlock") <
            sketch.index("beginMidiRenderBlock"),
            "external transport must apply at the audio block boundary")
    require("transportClockSourcePublishesOutboundClock" in sketch and
            "outboundTransportSuppressed" in transport,
            "SEQ MASTER needs producer and physical-write echo suppression")
    require("event.key == 'c' || event.key == 'C'" in player and
            "transportClockRuntime().toggleSource()" in player,
            "MIDI Player must expose an explicit master-source control")
    require("USBMIDI" not in sketch and "tud_midi" not in sketch and
            "USBMIDI" not in player and "tud_midi" not in player,
            "USB access must stay out of AudioTask and UI")

    require("kMaximumTempoTrim" in follower and
            "kPhaseTrimEnterSteps" in follower and
            "kSourceBpmHysteresis" in follower and
            "phaseTrimDirection_" in follower and
            "projectTransportTimeline().snapshot()" in follower,
            "SEQ MASTER needs a stable hysteretic phase PLL")
    require("pendingClock" in follower and "flushPendingClock" in follower and
            "kClockCoalesceWindowUs" in follower,
            "only compressed buffered F8 packets may be coalesced")
    require("sourceBpmQ16" in tracker and
            "lastTimingPulseOrdinal_" in tracker and
            "lastPhasePulseOrdinal_" in tracker and
            "timestamp is not allowed to poison" in tracker,
            "source tempo, musical phase and timing anchors must remain separate")
    require("restartedFromBeginning" in timeline,
            "project timeline must distinguish Start from Continue epochs")
    require("projectResumeOnExternalContinue_" in smf_service and
            "kContinuePrefillBlocks" in smf_service and
            "transport.restartedFromBeginning" in smf_service and
            '"ARMED / CONTINUE"' in smf_service,
            "only an active PROJECT SMF may use bounded Continue resume")
    require("kContinuePrefillBlocks" not in timeline,
            "generic project bar quantization must not own SMF resume policy")

    compile_and_run("test_project_transport_continue.cpp")
    compile_and_run("test_external_midi_clock_startup.cpp")
    print("seqtrak master source regressions: OK")


if __name__ == "__main__":
    main()
