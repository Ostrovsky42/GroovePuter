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
    structural = (ROOT / "src/midi/smf_structural_inspector.h").read_text(
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
            "shared MIDI browser must remain bounded and streaming")
    require("SD.begin(" not in manager_cpp,
            "shared MIDI browser must not own SD initialization")

    require("kOpenMidiFromPlayerContext" in navigation and
            "kPlayerPage" in navigation and
            "kHubPage" in navigation,
            "Player/HUB navigation constants must remain centralized")
    require("requestPageTransition(PlayerHubNavigation::kHubPage" in page_cpp and
            "PlayerHubNavigation::kOpenMidiFromPlayerContext" in page_cpp,
            "Player H shortcut must open HUB MIDI through the centralized context")
    require("context == PlayerHubNavigation::kOpenMidiFromPlayerContext" in hub_cpp and
            "requestPageTransition(PlayerHubNavigation::kPlayerPage)" in hub_cpp,
            "HUB MIDI return path must preserve the Player origin")

    require("SmfStructuralInspectorSnapshot structuralSnapshot" in page_h,
            "SMF page snapshot cache must expose structural projection")
    require("smfStructuralInspectorState().snapshot()" in page_cpp and
            "smfTrackInspectorState().snapshot()" in page_cpp and
            "smfTrackMuteState().snapshot()" in page_cpp,
            "Player panels must remain projections of the existing bounded states")
    require("smfSessionGeneration()" in page_cpp,
            "Player panels must reject cross-generation snapshots")
    require("smfSnapshotGenerationsMatch" in page_cpp,
            "Player panel projection must validate coherent generations")

    require("drawStructuralPanel" in page_wrapper,
            "structural panel wrapper must remain linked")
    require("kSmfStructuralFormSegments" in structural and
            "SmfStructuralLayerSnapshot" in structural,
            "bounded structural inspector API must remain available")

    require("ScheduledSmfMidiEventQueue" in smf_queue and
            "kCapacity" in smf_queue,
            "SMF dispatcher queue must remain fixed-capacity")
    require("scheduledSmfMidiEventQueue" in dispatcher,
            "USB MIDI transport must keep the scheduled SMF queue path")
    require("midiDispatchQueue" in dispatcher,
            "USB MIDI transport must keep live MIDI dispatch separate from scheduled SMF")
    require("smfPlayerService" in player_service,
            "Cardputer player service must remain the runtime SMF owner")

    require("midiOverview_{false};" in hub_h and
            "midiReturnToPlayer_{false};" in hub_h and
            "midiRouteEdit_{false};" in hub_h,
            "HUB MIDI UI state must remain page-local and bounded")
    require("syncMidiSessionSelection" in hub_h and
            "returnFromMidiOverview" in hub_h and
            "handleMidiOverviewEvent" in hub_h,
            "HUB MIDI page must retain its explicit local navigation helpers")

    hub_return_shortcut = hub_cpp.index("const bool hubShortcut")
    hub_modifier_guard = hub_cpp.index("if (event.alt || event.ctrl || event.meta)")
    require(
        hub_return_shortcut < hub_modifier_guard
        and "UIInput::isBack(event)" in hub_cpp[hub_return_shortcut:hub_modifier_guard]
        and "returnFromMidiOverview();" in hub_cpp[hub_return_shortcut:hub_modifier_guard],
        "HUB MIDI H/Escape return must precede the Cardputer meta guard",
    )
    require(
        "constexpr uint8_t kVisibleMidiRows = 7u;" in hub_cpp
        and "kSmfStructuralFormSegments" in hub_cpp
        and "drawArrangementRow(" in hub_cpp
        and "layer.form[segment]" in hub_cpp
        and "arrangementPlayheadX(" in hub_cpp
        and "drawOverlayBands(" in hub_cpp,
        "HUB MIDI must render a full-bleed sixteen-segment arrangement overview",
    )
    require(
        "kSmfStructuralFormSegments = 16" in structural
        and "buildNormalizedForm(" in structural
        and "barActivity[(kSmfStructuralBarLimit + 1u) / 2u]" in structural
        and "noteCount" in structural,
        "structural projection must supply bounded normalized form and footer metadata",
    )
    require(
        "event.key == 'u'" not in hub_cpp
        and "event.key == 'i'" not in hub_cpp
        and "G%s SW%u" not in hub_cpp
        and "P Player U Mixer I Chans" not in hub_cpp,
        "HUB MIDI visual redesign must not change or duplicate Player inspectors",
    )
    require(
        "smfTrackMuteState().selectTrack(" in hub_cpp
        and "formatTrackChannel(" in hub_cpp
        and "formatPitchRange(" in hub_cpp
        and '"RAW %s V%u N%u %s"' in hub_cpp
        and '"%s>%s V%u N%u %s"' in hub_cpp,
        "HUB MIDI selection, level/route and readable footer must remain projected from existing state",
    )
    require(
        "isSelected ? '>'" not in hub_cpp
        and "isMuted ? \"MUTE\" : \"ON\"" not in hub_cpp,
        "selected and muted layers must be expressed by row color rather than labels",
    )
    require(
        "bool midiOverview_{false};" in hub_h
        and "uint32_t midiGeneration_{0};" in hub_h,
        "HUB MIDI navigation state must remain bounded and page-local",
    )

    for forbidden in (
        "SD.",
        "SmfFileIndexer",
        "std::vector",
        "new ",
        "malloc(",
    ):
        require(forbidden not in hub_cpp,
                f"HUB MIDI projection must remain allocation/IO free: {forbidden}")

    require("playerExpectsMidiProjection" in hub_cpp and
            "projectionIsSyncing" in hub_cpp and
            "projection.ready()" in hub_cpp,
            "HUB MIDI must explicitly gate incoherent snapshots")
    require("MIDI LAYERS: SYNCING" in hub_cpp,
            "HUB MIDI must expose projection synchronization instead of stale rows")

    require("toggleMidiLayer" in hub_h and
            "toggleMidiSolo" in hub_h and
            "adjustMidiRoute" in hub_h,
            "HUB MIDI must keep explicit mute/solo/route operations")
    require("replaceMutedMask" in hub_cpp and
            "HubMidiSoloState" in hub_cpp,
            "HUB MIDI solo must restore the previous mute mask without stopping transport")
    require("smfTrackOutputRouteState" in hub_cpp and
            "routeCanBeEdited" in hub_cpp,
            "HUB MIDI route editing must use the persisted per-file route state")
    require("smfTrackLevelState().adjustLevel" in hub_cpp and
            "FN<>VOL" in hub_cpp,
            "HUB MIDI Fn arrows must adjust selected track level")

    player_handle = function_block(
        page_cpp,
        "bool SmfPlayerPage::handleEvent(UIEvent& event)",
        "void SmfPlayerPage::draw",
    )
    require("event.key == 'h' || event.key == 'H'" in player_handle and
            "kOpenMidiFromPlayerContext" in player_handle,
            "Player H must remain the explicit HUB MIDI shortcut")

    print("SMF panel completion source regressions: PASS")


if __name__ == "__main__":
    main()
