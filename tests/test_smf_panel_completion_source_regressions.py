#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_block(source: str, start_token: str, end_token: str) -> str:
    start = source.index(start_token)
    end = source.index(end_token, start)
    return source[start:end]


def main() -> None:
    page_h = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")
    page_cpp = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")
    manager_h = (ROOT / "src/ui/midi_file_manager.h").read_text(encoding="utf-8")
    manager_cpp = (ROOT / "src/ui/midi_file_manager.cpp").read_text(encoding="utf-8")
    player_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")
    dispatcher = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    smf_queue = (ROOT / "src/midi/scheduled_smf_midi_event_queue.h").read_text(encoding="utf-8")
    mute_state = (ROOT / "src/midi/smf_track_mute.h").read_text(encoding="utf-8")

    require("midi_file_manager.h" in page_h,
            "SMF page must use the shared bounded MIDI browser")
    require("midiFileManager().open()" in page_cpp,
            "SMF browser must initialize through the shared manager lifecycle")
    require("midiFileManager().handleEvent" in page_cpp and
            "midiFileManager().draw" in page_cpp,
            "SMF browser input and rendering must share one manager")
    require("static_assert(sizeof(MidiFileManager) <= 1024" in manager_h and
            "union Workspace" in manager_h and
            "EntryWindow entries" in manager_h and
            "loadWindow" in manager_h,
            "shared MIDI browser must retain a bounded DRAM contract")
    require("void MidiFileManager::open()" in manager_cpp and
            "bool MidiFileManager::refresh()" in manager_cpp,
            "shared MIDI browser must own its refresh lifecycle")
    require("BrowserRow" not in page_h and "refreshFiles" not in page_h,
            "SMF page must not retain a second browser cache")

    draw_block = function_block(
        page_cpp,
        "void SmfPlayerPage::drawHeader",
        "void SmfPlayerPage::drawBrowser",
    )
    for forbidden in (
        "SD.open",
        "openNextFile",
        "ensureCardputerSdMounted",
        "SD.mkdir",
        "refreshFiles()",
        "resolveEntry(",
    ):
        require(forbidden not in draw_block,
                f"SMF draw path must not traverse storage: {forbidden}")
    require("::Serial" not in page_cpp,
            "SMF page must not write routine browser diagnostics directly")

    guard_block = page_h[page_h.index("template <typename F>"):]
    require("gfx" not in guard_block and "draw" not in guard_block,
            "AudioGuard helper must not contain UI drawing")

    require("drawMuteMixer" in page_h and
            "void SmfPlayerPage::drawMuteMixer" in page_cpp and
            '"MIDI MUTES"' in page_cpp,
            "SMF player must expose a dedicated track mute table")
    require("muteMixerVisible_" in page_h and
            "event.key == 'u'" in page_cpp,
            "U must open and close the dedicated mute table")
    require("GROOVEPUTER_UP" in page_cpp and
            "GROOVEPUTER_DOWN" in page_cpp and
            "GROOVEPUTER_LEFT" in page_cpp and
            "GROOVEPUTER_RIGHT" in page_cpp,
            "mute table must be operable from Cardputer arrow keys")
    require("event.key == 'a'" in page_cpp and
            '"ALL MIDI TRACKS ON"' in page_cpp,
            "A must replace unavailable Shift+K for all-tracks-on")
    require("Shift+K" not in page_cpp and "J/L Track" not in page_cpp,
            "mute mixer help must not advertise unavailable Shift or J/L navigation")
    require("bool selectTrack(uint16_t trackIndex)" in mute_state and
            "std::atomic<uint32_t> mutedMaskLow_" in mute_state and
            "std::atomic<uint32_t> mutedMaskHigh_" in mute_state,
            "mute table must keep bounded physical-track selection and atomic masks")

    require("queueSongPositionPointerAtCurrentAnchor(targetTick)" in player_service and
            "tryPushSongPositionPointer" in player_service,
            "active PROJECT seek must publish a bounded SPP intent")
    require("ScheduledSmfMidiEventType::SongPositionPointer" in dispatcher and
            "handleSmfSongPositionPointer" in dispatcher,
            "MidiDispatchTask must own the physical active-seek SPP write")
    require("pendingSppEpoch_" in smf_queue and
            "takePendingSongPositionPointer" in smf_queue,
            "active-seek SPP must use a latest-wins bounded mailbox")
    require("g_smfQueue->recordDispatched(pendingSmf)" in dispatcher,
            "track ownership must commit only after a successful USB write")

    print("SMF panel completion source regressions: OK")


if __name__ == "__main__":
    main()
