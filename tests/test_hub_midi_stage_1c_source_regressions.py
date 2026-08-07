#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    page = (ROOT / "src/ui/pages/sequencer_hub_page_midi.cpp").read_text(
        encoding="utf-8"
    )
    header = (ROOT / "src/ui/pages/sequencer_hub_page.h").read_text(
        encoding="utf-8"
    )
    structural = (ROOT / "src/midi/smf_structural_inspector.h").read_text(
        encoding="utf-8"
    )
    mute_state = (ROOT / "src/midi/smf_track_mute.h").read_text(
        encoding="utf-8"
    )
    route_state = (ROOT / "src/midi/smf_track_output_route.h").read_text(
        encoding="utf-8"
    )
    routing = (ROOT / "src/midi/smf_routing.h").read_text(encoding="utf-8")
    player_header = (ROOT / "src/platform/cardputer_smf_player.h").read_text(
        encoding="utf-8"
    )
    display = (ROOT / "src/ui/miniacid_display.h").read_text(
        encoding="utf-8"
    )
    cardputer_display = (ROOT / "cardputer_display.cpp").read_text(
        encoding="utf-8"
    )
    makefile = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")

    require(
        "class SequencerHubPage final : public SequencerHubPageBase" in header,
        "HUB MIDI must remain a thin projection over the existing Hub page",
    )
    require(
        "bool midiOverview_{false};" in header
        and "bool midiReturnToPlayer_{false};" in header
        and "bool midiRouteEdit_{false};" in header
        and "int8_t midiRouteDraft_{-1};" in header
        and "uint8_t midiSelected_{0};" in header
        and "uint8_t midiScroll_{0};" in header
        and "uint32_t midiGeneration_{0};" in header,
        "HUB MIDI state must remain page-local and bounded",
    )
    for forbidden in ("std::vector", "std::array", "SmfPlayerSnapshot player_", "static Smf"):
        require(
            forbidden not in header,
            f"HUB MIDI header must not retain runtime snapshot/cache state: {forbidden}",
        )

    require(
        "GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER" in display,
        "MiniAcidDisplay must instantiate the public HUB MIDI wrapper",
    )
    require(
        makefile.count("../src/ui/pages/sequencer_hub_page.cpp") == 1
        and makefile.count("../src/ui/pages/sequencer_hub_page_midi.cpp") == 1,
        "SDL must compile both the existing Hub implementation and MIDI projection once",
    )

    for snapshot_call in (
        "smfStructuralInspectorState().snapshot()",
        "smfTrackInspectorState().snapshot()",
        "smfTrackMuteState().snapshot()",
        "smfTrackOutputRouteState().snapshot(",
        "service->snapshot()",
    ):
        require(
            snapshot_call in page,
            f"HUB MIDI must project existing snapshots: {snapshot_call}",
        )

    require(
        "smfSessionGeneration()" in page
        and "smfSnapshotGenerationsMatch(" in page
        and "routes.generation == generation" in page
        and "SYNCING" in page,
        "HUB MIDI must reject stale snapshots and expose an explicit SYNCING state",
    )
    require(
        "state.toggleTrack(track, projection.generation)" in page,
        "HUB MIDI mute must use the generation-aware physical-track mute path",
    )
    require(
        "smfTrackMuteState().clear(projection.generation)" in page,
        "HUB MIDI All On must use the generation-aware existing mute state",
    )
    require(
        "smfTrackMuteState().selectTrack(" in page
        and "projection.generation" in page,
        "HUB MIDI row selection must follow the existing generation-aware physical track",
    )

    shortcut_pos = page.index("const bool hubShortcut")
    back_pos = page.index(
        "if (event.scancode == GROOVEPUTER_ESCAPE || UIInput::isBack(event))",
        shortcut_pos,
    )
    modifier_pos = page.index("if (event.alt || event.ctrl || event.meta)", back_pos)
    require(
        shortcut_pos < back_pos < modifier_pos
        and "event.key == 'h'" in page[shortcut_pos:back_pos]
        and "returnFromMidiOverview();" in page[shortcut_pos:back_pos]
        and "midiRouteEdit_" in page[back_pos:modifier_pos]
        and "ROUTE CANCELLED" in page[back_pos:modifier_pos]
        and "GROOVEPUTER_ESCAPE" in page[back_pos:modifier_pos],
        "H and physical Escape must exit while Escape first cancels an active route edit",
    )

    require(
        "constexpr uint8_t kVisibleMidiRows = 7u;" in page
        and "constexpr uint8_t kArrangementSegments = kSmfStructuralFormSegments;" in page
        and "drawArrangementRow(" in page
        and "layer.form[segment]" in page
        and "const int cellSize = std::min(maxCellWidth, maxCellHeight);" in page
        and "gfx.fillRect(cellX," in page
        and "cellSize,\n                     cellSize," in page,
        "HUB MIDI must render seven rows with sixteen compact square arrangement cells",
    )
    require(
        "kSmfStructuralFormSegments = 16" in structural
        and "kSmfStructuralBarLimit = 256" in structural
        and "barActivity[(kSmfStructuralBarLimit + 1u) / 2u]" in structural
        and "buildNormalizedForm(" in structural
        and "bar) * kSmfStructuralFormSegments" in structural
        and "layer.noteCount" in structural,
        "load-time structural analysis must publish a bounded normalized sixteen-segment form",
    )
    require(
        "arrangementPlayheadX(" in page
        and "player.currentTick" in page
        and "player.endTick" in page
        and "const int rowsTop = std::min(kOverlayBandHeight, screenHeight);" in page
        and "const int rowsBottom = std::max(rowsTop, screenHeight - kOverlayBandHeight);" in page
        and "gfx.fillRect(playheadX, rowsTop, 2, rowsHeight, kAccent);" in page,
        "all rows and the shared playhead must stay between the overlay bands",
    )
    require(
        "drawOverlayBands(" in page
        and page.count("kOverlayScrim") >= 5
        and "player.filename" in page
        and "player.totalBars" in page
        and "formatPitchRange(" in page
        and "selectedLayer.noteCount" in page,
        "filename/bar and selected routing/note/range text must use dark bands",
    )
    require(
        "rowBackground(isMuted, isSelected)" in page
        and "isSelected ? kAccent" in page
        and "isSelected ? '>'" not in page
        and "isMuted ? \"MUTE\" : \"ON\"" not in page,
        "selection and mute must be graphical rather than arrow or ON/MUTE labels",
    )
    require(
        "kActivityRamp[8]" in page
        and "kMutedActivityRamp[8]" in page
        and page.count("kAccent") == 3,
        "the grid must use one density hue ramp and reserve the accent for selection/playhead",
    )
    require(
        "dirty_tiles_.scan(frame_.data(), w_" in cardputer_display
        and "pushRegion_(region)" in cardputer_display,
        "Cardputer output must continue through the existing dirty-region display foundation",
    )

    require(
        "G%s SW%u" not in page
        and "N%u.%u A%u%%" not in page
        and "P Player U Mixer I Chans" not in page
        and "event.key == 'u'" not in page
        and "event.key == 'i'" not in page,
        "HUB MIDI must not expose raw inspector metrics or duplicate U/I panel navigation",
    )
    require(
        "formatTrackChannel(" in page
        and "formatRouteDestination(" in page
        and "formatPitchRange(" in page
        and "%s>%s N%u %s" in page
        and "ROUTE %s" in page
        and 'hints = soloActive ? "S UNSOLO <>RTE" : "S SOLO <>RTE";' in page,
        "selected-layer footer must expose live Solo and direct route arrows",
    )

    solo_start = page.index("if (event.key == 's' || event.key == 'S')")
    solo_end = page.index("int routeMove = 0;", solo_start)
    solo_block = page[solo_start:solo_end]
    require(
        "replaceMutedMask(" in solo_block
        and "allSmfTracksMask(" in solo_block
        and "MIDI SOLO TRK %02u" in solo_block
        and "MIDI SOLO OFF" in solo_block,
        "S must solo the selected physical track and restore the previous mute mask",
    )
    require(
        "togglePlayPause" not in solo_block
        and "toggleHubMidiTransport" not in solo_block
        and "->pause(" not in solo_block
        and "->stop(" not in solo_block,
        "Solo must never stop, pause, or otherwise own the MIDI transport",
    )
    require(
        "bool replaceMutedMask(uint64_t desiredMask, uint32_t generation)" in mute_state
        and "SmfSessionMutationGuard guard(generation)" in mute_state
        and "const uint64_t newlyMuted = desiredMask & ~previousMask;" in mute_state
        and "pendingReleaseLow_.fetch_and(desiredLow" in mute_state
        and "pendingReleaseLow_.fetch_or(newlyMutedLow" in mute_state,
        "Solo mask replacement must stay generation-safe and preserve NoteOff cleanup",
    )
    require(
        "restoreHubMidiSoloBeforeManualMute(projection.generation)" in page
        and "clearHubMidiSoloTracking();" in page,
        "legacy mute/All On actions must leave Solo state deterministically",
    )

    require(
        "cycleRouteDestination(" in page
        and "GROOVEPUTER_LEFT" in page
        and "GROOVEPUTER_RIGHT" in page
        and "projection.routes.destinationFor(selectedTrack), routeMove" in page
        and "midiRouteEdit_ = true;" in page
        and "smfTrackOutputRouteState().setDestination(" in page
        and "projection.generation" in page
        and "projection.mute.trackCount" in page,
        "Left/Right must enter route editing directly and Enter must apply one generation-aware physical-track route",
    )
    require(
        "event.key == 'c' || event.key == 'C'" in page
        and "compatibility alias" in page,
        "legacy C route entry may remain only as a compatibility alias",
    )
    require(
        "routeCanBeEdited(" in page
        and "SmfPlayerState::Stopped" in page
        and "SmfPlayerState::Paused" in page
        and "PAUSE MIDI FIRST" in page
        and "SEQTRAK ROUTING REQUIRED" in page,
        "route edits must be limited to safe paused/stopped SEQTRAK routing",
    )

    require(
        "kSmfTrackOutputRouteCapacity = 32u" in route_state
        and "kSmfSeqtrakOutputChannelCount = 10u" in route_state
        and "packedRoutes_[kPackedRouteWords]" in route_state
        and "std::atomic<uint32_t> boundGeneration_" in route_state
        and "SmfSessionMutationGuard guard(generation)" in route_state
        and "kInitializingGeneration" in route_state,
        "per-track routes must remain bounded, atomic and session-generation aware",
    )
    for forbidden in ("std::vector", "new ", "malloc", "free("):
        require(
            forbidden not in route_state,
            f"route state must not allocate dynamically: {forbidden}",
        )

    require(
        "routeSmfNoteToSeqtrakDestination(" in routing
        and "routeSmfTrackNote(" in routing
        and "channel <= 6u" in routing
        and "mode == SmfRoutingMode::Seqtrak && destinationOverride >= 0" in routing,
        "explicit CH1-7 routes must use fixed note 60 while CH8-10 preserve pitch",
    )
    require(
        "SmfRoutedNote routeSmfNote(" in player_header
        and "pendingEvent_.trackIndex" in player_header
        and "fileIndex_.trackCount" in player_header
        and "smfTrackOutputRouteState().destinationFor(" in player_header
        and "GroovePuterMidi::routeSmfTrackNote(" in player_header,
        "Cardputer player must apply the selected physical-track route at the existing scheduling boundary",
    )

    require(
        "if (UIInput::isUp(event)) move = -1;" in page
        and "else if (UIInput::isDown(event)) move = 1;" in page
        and "move = -kVisibleMidiRows" not in page
        and "move = kVisibleMidiRows" not in page,
        "Up/Down must own layer selection after Left/Right is reassigned to routing",
    )
    require(
        "bool toggleHubMidiTransport(MiniAcid& miniAcid)" in page
        and "service->togglePlayPause()" in page
        and "transportClockRuntime().snapshot()" in page
        and "G START FIRST / THEN SPACE" in page
        and "return toggleHubMidiTransport(mini_acid_);" in page
        and "MIDI TRANSPORT: PLAYER" not in page,
        "Space in HUB MIDI must use the existing player service transport contract",
    )
    require(
        "void SequencerHubPage::onEnter(int context)" in page
        and "PlayerHubNavigation::kOpenMidiFromPlayerContext" in page
        and "requestPageTransition(PlayerHubNavigation::kPlayerPage)" in page,
        "HUB MIDI must support the bounded Player round-trip without taking scheduler ownership",
    )

    for forbidden in (
        "SD.",
        "SmfFileIndexer",
        "SmfScheduler",
        "loadFile(",
        "loadSelected(",
        "seek(",
        "restart(",
        "->play(",
        "->pause(",
        "->stop(",
        "tud_midi",
        "TinyUSB",
        "USBMIDI",
    ):
        require(
            forbidden not in page,
            f"HUB MIDI must not acquire file, scheduler or USB ownership: {forbidden}",
        )

    require(
        "mode_ == Mode::OVERVIEW" in page,
        "MIDI projection must only replace the Hub overview, not detail editing",
    )
    require(
        "SequencerHubPageBase::draw(gfx)" in page
        and "SequencerHubPageBase::handleEvent(event)" in page,
        "Internal Hub behavior must remain delegated to the existing implementation",
    )

    print("HUB MIDI navigation/solo source regressions: OK")


if __name__ == "__main__":
    main()
