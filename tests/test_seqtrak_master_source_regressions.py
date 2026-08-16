#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "host-tests"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def compile_and_run(source_name: str, extra_sources: Iterable[str] = ()) -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests" / source_name
    output = BUILD / source.stem
    command = [
        os.environ.get("CXX", "g++"),
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        f"-I{ROOT}",
        str(source),
    ]
    command.extend(str(ROOT / path) for path in extra_sources)
    command.extend(["-o", str(output)])
    subprocess.run(command, check=True)
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
    runtime = (ROOT / "src/midi/transport_clock_runtime.h").read_text(
        encoding="utf-8"
    )
    timeline = (ROOT / "src/midi/project_transport_timeline.h").read_text(
        encoding="utf-8"
    )
    settings_h = (ROOT / "src/midi/midi_companion_settings.h").read_text(
        encoding="utf-8"
    )
    codec_h = (ROOT / "src/midi/midi_companion_settings_codec.h").read_text(
        encoding="utf-8"
    )
    codec_cpp = (ROOT / "src/midi/midi_companion_settings_codec.cpp").read_text(
        encoding="utf-8"
    )
    settings_session_h = (
        ROOT / "src/platform/cardputer_midi_settings_session.h"
    ).read_text(encoding="utf-8")
    settings_session_cpp = (
        ROOT / "src/platform/cardputer_midi_settings_session.cpp"
    ).read_text(encoding="utf-8")
    display_h = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")

    require("tud_midi_packet_read" in transport,
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
    require("toggleExternalFollowEnabled()" in player and
            '"EXT FOLLOW OFF / STOP"' in player and
            '"SEQ MASTER: FOLLOW OFF"' in player,
            "G must expose an explicit SEQ MASTER follow safety gate")
    require("externalFollowDisabled_" in runtime and
            "externalFollowEnabled()" in runtime,
            "follow state must remain lock-free and default ON")
    require("transportClockRuntime().externalFollowEnabled()" in follower and
            "followEnabled_" in follower,
            "AudioTask follower must read and enforce the runtime follow gate")
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
            "observedPulseCount" in tracker and
            "timestamp is not allowed to poison" in tracker,
            "source tempo, musical phase and timing anchors must remain separate")
    require("restartedFromBeginning" in timeline,
            "project timeline must distinguish Start from Continue epochs")
    require("projectRelaunchAfterExternalStop_" in smf_service and
            "kExternalRelaunchPrefillBlocks" in smf_service and
            "transport.restartedFromBeginning" in smf_service and
            '"ARMED / RESTART"' in smf_service and
            '"ARMED / CONTINUE"' in smf_service,
            "an active PROJECT SMF needs bounded restart/Continue relaunch")
    require("kExternalRelaunchPrefillBlocks" not in timeline,
            "generic project bar quantization must not own SMF resume policy")

    require("transportClockSource" in settings_h and
            "externalFollowEnabled" in settings_h,
            "transport controls must belong to versioned MIDI settings")
    require("kLegacySchemaVersion = 1" in codec_h and
            "kSchemaVersion = 2" in codec_h and
            "kLegacyEncodedSize = 44" in codec_h and
            "kEncodedSize = 46" in codec_h,
            "codec must explicitly support schema-v1 to schema-v2 migration")
    require("recordShapeIsSupported" in codec_cpp and
            "normalizeTransportClockSource" in codec_cpp,
            "decoder must accept the legacy record and sanitize clock source")
    require("Preferences" not in settings_session_h and
            "midi_companion_settings_codec" not in settings_session_h and
            "transport_clock_runtime" not in settings_session_h,
            "public platform binding must not leak NVS or realtime headers")
    require("#ifdef ARDUINO" in settings_session_h and
            "initializeCardputerMidiSettingsSession" in settings_session_h,
            "desktop binding must be a no-op while Cardputer restores settings")
    require("Preferences" in settings_session_cpp and
            'kKey = "midi_cfg"' in settings_session_cpp and
            "applyPersistedControl" in settings_session_cpp and
            "setControlChangedCallback" in settings_session_cpp,
            "Arduino platform unit must load and save clock controls through NVS")

    settings_boot = "GroovePuterPlatform::initializeCardputerMidiSettingsSession();"
    usb_boot = "registerCardputerUsbMidiSink("
    display_boot = "new (std::nothrow) MiniAcidDisplay("
    require('#include "src/platform/cardputer_midi_settings_session.h"' in sketch and
            settings_boot in sketch,
            "root boot must explicitly restore persisted MIDI settings")
    require(sketch.index(settings_boot) < sketch.index(usb_boot) <
            sketch.index(display_boot),
            "persisted MIDI settings must be restored before USB dispatcher creation, while heavy UI allocation remains later")
    require("CardputerMidiSettingsBinding midi_settings_binding_" in display_h,
            "late UI binding may remain only as an idempotent compatibility call during the boot-ownership migration")
    require("if (initialized_) return;" in settings_session_cpp,
            "the compatibility UI call must not cause a second NVS restore")
    require("Preferences" not in sketch and "Preferences" not in transport and
            "Preferences" not in smf_service,
            "NVS access must stay out of AudioTask, MidiDispatchTask and SmfPlayerTask")

    compile_and_run("test_project_transport_continue.cpp")
    compile_and_run("test_external_midi_clock_startup.cpp")
    compile_and_run("test_external_follow_gate.cpp")
    compile_and_run(
        "test_midi_transport_settings_persistence.cpp",
        (
            "src/midi/midi_companion_settings.cpp",
            "src/midi/midi_companion_settings_codec.cpp",
        ),
    )
    print("seqtrak master source regressions: OK")


if __name__ == "__main__":
    main()
