#!/usr/bin/env python3
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = Path(__file__).with_name("_performance_source_regressions_base.py")

spec = importlib.util.spec_from_file_location(
    "performance_source_regressions_base", BASE
)
if spec is None or spec.loader is None:
    raise RuntimeError(f"cannot load regression base: {BASE}")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def test_transport_note_mode_keys_remain_live() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    header = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(
        encoding="utf-8"
    )

    start = keyboard.index("bool PerformanceKeyboard::keyDown")
    end = keyboard.index("bool PerformanceKeyboard::keyUp", start)
    block = keyboard[start:end]

    layout_pos = block.index("if (!isPerformanceKey(physicalKey)) return false;")
    note_mode_pos = block.index("if (!noteModeEnabled_) return false;")
    enabled_pos = block.index("if (!enabled_) return true;")
    note_pos = block.index("noteForKey", enabled_pos)

    base.require(layout_pos < note_mode_pos < enabled_pos < note_pos,
                 "layout and NOTE mode must be resolved before live NoteOn")
    base.require("transportPlaying_" not in block[note_mode_pos:note_pos],
                 "transport playback must not block Cardputer keyboard NoteOn")
    base.require("return enabled_ && noteModeEnabled_;" in header,
                 "live input ownership must no longer depend on transport state")
    base.require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
                 "lower performance row must retain K/L collision keys")
    base.require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
                 "upper performance row must retain I/O/P collision keys")

    transport_start = keyboard.index(
        "void PerformanceKeyboard::setTransportPlaying")
    transport_end = keyboard.index(
        "void PerformanceKeyboard::setTarget", transport_start)
    transport_block = keyboard[transport_start:transport_end]
    base.require("if (playing) panic();" not in transport_block,
                 "transport start must not panic held performance keys")
    base.require("stopGeneratedOutput();" in transport_block and
                 "resetStepClock();" in transport_block,
                 "clock-domain changes must clean generated notes before re-anchoring")

    service_start = keyboard.index("void PerformanceKeyboard::service(uint32_t")
    service_end = keyboard.index(
        "void PerformanceKeyboard::triggerDirectTransformed", service_start)
    service_block = keyboard[service_start:service_end]
    base.require("if (transportPlaying_)" in service_block and
                 "serviceTransportStepClock(nowMicros)" in service_block,
                 "running transport must select the project-timeline step clock")
    base.require("projectTransportTimeline().trySnapshot(snapshot)" in keyboard and
                 "snapshot.absoluteSteps()" in keyboard,
                 "step generation must read the coherent project transport phase")
    base.require("snapshotCardputerUsbMidiBlockAnchor" in keyboard and
                 "anchorPlaybackMicros" in keyboard,
                 "hardware timing must use the dispatcher playback anchor")
    base.require("const uint64_t nextOrdinal = currentOrdinal + 1u;" in keyboard and
                 "nextOrdinal % static_cast<uint64_t>(kEuclideanSteps)" in keyboard,
                 "Euclidean phase must derive from the next absolute sixteenth")
    base.require("kTransportScheduleLeadSteps = 0.5" in keyboard and
                 "transportAnchorBlockSequence_" in header and
                 "transportAnchorMicros_" in header,
                 "transport steps must be prepared ahead from a stable block anchor")
    base.require("kGeneratedNoteOnStaleMicros = 12000u" in keyboard and
                 "lateness > kGeneratedNoteOnStaleMicros" in keyboard and
                 "leadMicros < -static_cast<int32_t>(kGeneratedNoteOnStaleMicros)" in keyboard,
                 "late generated NoteOn must be shed instead of caught up")
    base.require("kMaxScheduledEvents = 112" in header,
                 "dense 8-note x4 ratchet scheduling needs overlap headroom")
    base.require("INPUT LOCK | PATTERN PLAYER ACTIVE" not in page and
                 'stepTools ? "LIVE SYNC" : "LIVE INPUT"' in page,
                 "PERFORM must show live transport input instead of the old lock")
    base.require('"STRUM: N/A / ARP IS SINGLE NOTE"' in page and
                 '"STRUM: N/A / ENABLE CHORD"' in page and
                 '"5 STRUM N/A"' in page,
                 "STRUM must expose when one-note playback makes it ineffective")
    base.require('"ROTATE: N/A / EUCLID OFF"' in page and
                 '"ROTATE: N/A / ALL 16 ACTIVE"' in page and
                 '"8 ROTATE N/A"' in page,
                 "ROTATE must expose the 0/16 and 16/16 no-op states")

    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    route_pos = display.index("performance_keyboard_.keyDown(event.key)")
    fallback_pos = display.index(
        "if (event.key == ']') { nextPage(); return true; }", route_pos
    )
    base.require(route_pos < fallback_pos,
                 "NOTE-mode routing must run before legacy global fallback")


base.test_transport_note_mode_keys_remain_live = (
    test_transport_note_mode_keys_remain_live
)


if __name__ == "__main__":
    base.main()
