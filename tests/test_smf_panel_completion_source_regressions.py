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
    page_wrapper = (ROOT / "src/ui/pages/smf_player_page_structural.cpp").read_text(
        encoding="utf-8"
    )
    hub_h = (ROOT / "src/ui/pages/sequencer_hub_page.h").read_text(encoding="utf-8")
    hub_cpp = (ROOT / "src/ui/pages/sequencer_hub_page_midi.cpp").read_text(
        encoding="utf-8"
    )
    navigation = (ROOT / "src/ui/player_hub_navigation.h").read_text(
        encoding="utf-8"
    )
    manager_h = (ROOT / "src/ui/midi_file_manager.h").read_text(encoding="utf-8")
    manager_cpp = (ROOT / "src/ui/midi_file_manager.cpp").read_text(encoding="utf-8")
    player_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")
    dispatcher = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    smf_queue = (ROOT / "src/midi/scheduled_smf_midi_event_queue.h").read_text(encoding="utf-8")

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

    guard_start = page_h.index("template <typename F>")
    guard_end = page_h.index("\n};\n\nclass SmfPlayerPage final", guard_start)
    guard_block = page_h[guard_start:guard_end]
    require("gfx" not in guard_block and "draw" not in guard_block,
            "AudioGuard helper must not contain UI drawing")

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

    require(
        "PlayerHubNavigation::kOpenMidiFromPlayerContext" in page_wrapper
        and "requestPageTransition(" in page_wrapper
        and "PlayerHubNavigation::kHubPage" in page_wrapper,
        "MIDI Player must enter HUB MIDI through the existing page transition path",
    )
    require(
        "void SmfPlayerPage::onExit()" in page_wrapper
        and "void SmfPlayerPage::onEnter(int context)" in page_wrapper
        and "PlayerHubNavigation::playerViewState()" in page_wrapper
        and "view.generation = smfSessionGeneration()" in page_wrapper
        and "view.generation != generation" in page_wrapper,
        "Player view lifetime must be bounded and tied to the current SMF session",
    )
    require(
        "struct PlayerViewState" in navigation
        and "SmfPlayerSnapshot" not in navigation
        and "SmfStructuralInspectorSnapshot" not in navigation,
        "navigation lifetime state must not retain runtime MIDI snapshots",
    )
    require(
        "void SequencerHubPage::onEnter(int context)" in hub_cpp
        and "PlayerHubNavigation::kOpenMidiFromPlayerContext" in hub_cpp
        and "requestPageTransition(PlayerHubNavigation::kPlayerPage)" in hub_cpp,
        "HUB MIDI must return to Player without load or transport commands",
    )
    require(
        "smfSnapshotGenerationsMatch(" in hub_cpp
        and "smfSessionGeneration()" in hub_cpp
        and "SYNCING" in hub_cpp,
        "HUB MIDI must show SYNCING until all snapshots match the active session",
    )
    require(
        "state.toggleTrack(track, projection.generation)" in hub_cpp
        and "smfTrackMuteState().clear(projection.generation)" in hub_cpp,
        "HUB MIDI mute commands must be generation-aware and use existing ownership",
    )
    require(
        "bool midiOverview_{false};" in hub_h
        and "uint32_t midiGeneration_{0};" in hub_h,
        "HUB MIDI navigation state must remain bounded and page-local",
    )

    for forbidden in (
        "SD.",
        "SmfFileIndexer",
        "SmfScheduler",
        "requestLoad(",
        "seekBars(",
        "restart(",
        "->pause(",
        "->stop(",
        "tud_midi",
        "TinyUSB",
        "USBMIDI",
    ):
        require(
            forbidden not in hub_cpp,
            f"HUB MIDI navigation must not acquire file, transport or USB ownership: {forbidden}",
        )

    print("SMF panel completion source regressions: OK")


if __name__ == "__main__":
    main()
