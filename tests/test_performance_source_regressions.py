#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_default_runtime_logging_budget() -> None:
    text = (ROOT / "src/debug_log.h").read_text(encoding="utf-8")

    match = re.search(
        r"#ifndef\s+DEBUG_LEVEL\s*\n\s*#define\s+DEBUG_LEVEL\s+(\d+)",
        text,
    )
    require(match is not None,
            "debug_log.h must keep an explicit default DEBUG_LEVEL")
    require(int(match.group(1)) <= 2,
            "Cardputer runtime must not default to INFO/DEBUG synchronous logging")
    require("-DDEBUG_LEVEL=3/4" in text,
            "verbose diagnostics must remain explicitly opt-in")
    require("DEBUG_PRESET_PERFORMANCE" in text,
            "focused performance profiling preset must remain available")
    require("DEBUG_PRESET_SILENT" in text,
            "silent production preset must remain available")


def test_blocked_note_mode_keys_are_consumed() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    start = keyboard.index("bool PerformanceKeyboard::keyDown")
    end = keyboard.index("bool PerformanceKeyboard::keyUp", start)
    block = keyboard[start:end]

    layout_pos = block.index("if (!isPerformanceKey(physicalKey)) return false;")
    note_mode_pos = block.index("if (!noteModeEnabled_) return false;")
    blocked_pos = block.index("if (!enabled_ || transportPlaying_) return true;")
    note_pos = block.index("noteForKey", blocked_pos)

    require(layout_pos < note_mode_pos < blocked_pos < note_pos,
            "layout membership must be decided before transport blocks NoteOn")
    require("return true;" in block[blocked_pos:note_pos],
            "transport-blocked performance keys must remain consumed")
    require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
            "lower performance row must retain K/L collision keys")
    require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
            "upper performance row must retain I/O/P collision keys")

    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    route_pos = display.index("performance_keyboard_.keyDown(event.key)")
    fallback_pos = display.index(
        "if (event.key == ']') { nextPage(); return true; }", route_pos
    )
    require(route_pos < fallback_pos,
            "NOTE-mode routing must run before legacy global fallback")


def test_performance_all_notes_off_is_target_scoped() -> None:
    sink = (ROOT / "src/input/internal_synth_output.cpp").read_text(
        encoding="utf-8"
    )
    start = sink.index("case MusicalEventType::AllNotesOff")
    block = sink[start:]

    require("engine_.liveNote(voice)" in block,
            "AllNotesOff must inspect only the event target voice")
    require("engine_.liveNoteOff(voice" in block,
            "AllNotesOff must release only the live-owned target voice")
    require("engine_.allLiveNotesOff()" not in block,
            "performance AllNotesOff must not become a global voice release")


def test_note_mode_is_explicit_and_runtime_only() -> None:
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    storage = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("keyboard_.toggleNoteMode();" in page,
            "PERFORM must expose an explicit NOTE-mode toggle")
    require('noteMode ? "NOTE ON" : "NOTE OFF"' in page,
            "PERFORM must expose NOTE-mode state as a stage-readable badge")
    require("bool noteModeEnabled_{true};" in header,
            "PERFORM must remain immediately playable by default")
    require("noteModeEnabled" not in scenes and "noteModeEnabled" not in storage,
            "NOTE mode must remain runtime-only in this PR")


def test_live_synth_render_is_not_transport_gated() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    start = engine.index("uint32_t tV0 = 0;")
    end = engine.index("uint32_t tD0 = 0;", start)
    voice_block = engine[start:end]

    require("if (playing)" not in voice_block,
            "live synth rendering must work while transport is stopped")
    require("synthVoices_[0]->process()" in voice_block,
            "Synth A must be rendered in the live-audio path")

    drum_end = engine.index("if (detailedProfile) tDrumsTotal", end)
    drum_block = engine[end:drum_end]
    require("if (playing)" in drum_block,
            "drums must remain transport-gated")
    require("sample += sample303;" in engine[drum_end - 120:drum_end + 80],
            "live synth mix must be added outside the drum transport gate")


def test_perform_is_additive_to_legacy_carousel() -> None:
    display_header = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
    perform = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")

    require("int page_index_ = 0;" in display_header,
            "GroovePuter must boot into the original groovebox page")
    require("int previous_page_index_ = 0;" in display_header,
            "legacy Back/page state must start from page zero")
    require("case '[':" not in perform and "case ']':" not in perform,
            "PERFORM must not steal legacy [ ] page navigation")
    require("case ',':" in perform and "case '.':" in perform,
            "PERFORM scale controls must use non-navigation keys")


def test_stage_visuals_remain_lightweight_and_state_driven() -> None:
    visuals = (ROOT / "src/ui/components/music_visuals.h").read_text(
        encoding="utf-8"
    )
    perform = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    keyboard = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    player = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")

    require("drawPiano" in visuals and "drawDrumPads" in visuals,
            "PERFORM must retain instrument-shaped piano/pad visuals")
    require("drawProgressBar" in visuals and "drawChip" in visuals,
            "stage UI must retain reusable high-contrast progress/chip primitives")
    require("isPitchClassHeld" in keyboard and "isPhysicalKeyHeld" in keyboard,
            "visuals must read held-note state instead of inventing animation state")
    require("activeVelocity" in keyboard,
            "PERFORM must expose the actual active-note velocity to the UI")
    require("MusicVisuals::drawPiano" in perform and
            "MusicVisuals::drawDrumPads" in perform,
            "PERFORM must switch between piano and native drum pads")
    require("MusicVisuals::drawProgressBar" in player and
            "smfPlayerStateName(state.state)" in player,
            "MIDI Player must keep camera-readable state and progress")

    forbidden = ("Sprite", "Canvas", "createSprite", "pushSprite", "delay(")
    for token in forbidden:
        require(token not in visuals,
                f"music visuals must stay immediate-mode and allocation-free: {token}")
    require("millis()" not in visuals,
            "music visuals must not introduce timer-driven flicker/animation")


def test_smf_player_is_additive_and_keeps_single_usb_owner() -> None:
    ui_config = (ROOT / "src/ui/ui_config.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    player_page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
    player_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8"
    )
    smf_service = (ROOT / "src/midi/smf_player_service.h").read_text(
        encoding="utf-8"
    )
    player_registry = (ROOT / "src/platform/cardputer_smf_player_registry.cpp").read_text(
        encoding="utf-8"
    )
    usb_dispatch = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(
        encoding="utf-8"
    )
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("kPageCount = 14" in ui_config,
            "MIDI Player must remain a real lazy-loaded UI page")
    require("case kSmfPlayerPage:" in display and
            "std::make_unique<SmfPlayerPage>" in display,
            "MiniAcidDisplay must construct the MIDI Player page")
    require("event.alt && (event.key == 'p' || event.key == 'P')" in display,
            "Alt+P must provide a deterministic hardware shortcut to MIDI Player")
    require("player->togglePlayPause()" not in display,
            "display-global Space handling must not bypass MIDI Player policy")
    page_dispatch = display.index("currentPage->handleEvent(event)")
    global_space = display.index("event.key == ' '", page_dispatch)
    require(page_dispatch < global_space,
            "pages must get first refusal before global GroovePuter Space")
    require("return togglePlayerTransport();" in player_page and
            "player_->togglePlayPause()" in player_page,
            "MIDI Player page must own Space Play/Pause")
    player_space = player_page[player_page.index("bool SmfPlayerPage::togglePlayerTransport()"):
                               player_page.index("void SmfPlayerPage::toggleGrooveTransport()")]
    require("miniAcid_.start()" not in player_space and
            "miniAcid_.stop()" not in player_space,
            "MIDI Space must not mutate the GroovePuter transport")
    require("event.key == 'g' || event.key == 'G'" in player_page and
            '"GROOVE PLAY / SPACE MIDI"' in player_page and
            '"GROOVE STOP / MIDI PAUSED"' in player_page,
            "G must expose separate, explicit GroovePuter transport control")
    require(player_page.count("miniAcid_.start();") == 1 and
            player_page.count("miniAcid_.stop();") == 1,
            "only explicit G control may mutate the GroovePuter transport")
    require("player_->togglePlayPause()" not in
            player_page[player_page.index("if (event.key == 't' || event.key == 'T')"):],
            "tempo source switching must not depend on a second UI command")
    require('"GP MASTER: %s > USB CLOCK"' in player_page,
            "PROJECT mode must identify GroovePuter as the outbound clock master")
    require('queued ? "MIDI PANIC / PAUSE" : "PANIC QUEUE BUSY"' in player_page,
            "player panic must report command-queue failure")

    require("requestLoad(path.c_str())" in player_page and
            "requestLoadAndPlay" not in player_page,
            "Enter must load an SMF without creating hidden playback intent")
    load_case = player_service[player_service.index("case CommandType::Load:"):
                               player_service.index("case CommandType::TogglePlayPause:")]
    require("loadFile(command.path);" in load_case and
            "startFromTick" not in load_case,
            "the platform Load command must leave the player stopped")
    player_space = player_page[player_page.index("bool SmfPlayerPage::togglePlayerTransport()"):
                               player_page.index("void SmfPlayerPage::toggleGrooveTransport()")]
    require('"G START FIRST / THEN SPACE"' in player_space and
            "TransportClockSource::GroovePuterInternal" in player_space and
            player_space.index('"G START FIRST / THEN SPACE"') <
            player_space.index("player_->togglePlayPause()"),
            "stopped GP MASTER must not arm SMF before GroovePuter starts")
    groove_toggle = player_page[player_page.index("void SmfPlayerPage::toggleGrooveTransport()"):
                                player_page.index("bool SmfPlayerPage::handleEvent")]
    require("player_->pause()" in groove_toggle and
            "withAudioGuard" in groove_toggle,
            "G stop must pause PROJECT SMF and guard engine transport mutation")
    require("virtual bool pause() = 0" in smf_service and
            "case CommandType::Pause:" in player_service,
            "SMF transport needs an explicit idempotent Pause command")
    require('"GP STOP / MIDI PAUSED"' in player_service and
            '"WAIT SEQTRAK PLAY"' in player_service and
            "projectRelaunchAfterExternalStop_" in player_service,
            "project stop must pause GP MASTER but preserve SEQ MASTER arming")

    require('"MIDI LIBRARY  %.24s"' in player_page and
            "currentPath_.c_str()" in player_page and
            "requestLoad" in player_page,
            "MIDI Player page must expose selectable SD playback and its path")
    require("seekBars(event.shift ? -4 : -1)" in player_page and
            "seekBars(event.shift ? 4 : 1)" in player_page,
            "player seek must preserve +/-1 and Shift +/-4 bar UX")
    require("MIDI PANIC / PAUSE" in player_page,
            "player page must expose scoped panic")
    require("event.key == 'r' || event.key == 'R'" in player_page and
            "player_->restart(SmfPlayerRestartOrigin::MusicStart)" in player_page and
            "R RESTART" in player_page,
            "physical R must restart playback from MUSIC START without Shift")
    require("event.key == 'd' || event.key == 'D'" in player_page and
            "drawPerformance" in player_page and
            "state.performance" in player_page and
            "snapshot_.performance = performance" in player_service,
            "physical D must expose SMF performance without Serial or SD logging")
    require("player_->adjustTempoBpm(deltaBpm)" in player_page and
            "player_->resetTempo()" in player_page and
            "player_->cycleVelocityBoost()" in player_page and
            "applySmfVelocityBoost" in player_service and
            "tempoScalePermille_" in player_service,
            "SMF UI must expose bounded BPM and velocity controls")
    project_bpm = player_page[player_page.index(
        "if (state.tempoMode == SmfTempoMode::Project)"):
        player_page.index("if (event.key == 'o' || event.key == 'O')")]
    require("withAudioGuard" in project_bpm and
            "miniAcid_.setBpm(targetBpm)" in project_bpm and
            "10.0f" in project_bpm and "250.0f" in project_bpm,
            "PROJECT BPM mutation must be guarded and use the engine range")

    reanchor = player_service[player_service.index(
        "void CardputerSmfPlayerService::reanchorProjectTempo"):
        player_service.index("bool CardputerSmfPlayerService::enqueue")]
    invalidate_pos = reanchor.index("invalidateScheduledEvents()")
    first_scan_pos = reanchor.index("prepareStreamAt(streamTick)")
    require(invalidate_pos < first_scan_pos and
            "auto failReanchor" in reanchor and
            "invalidateAndRequestPanic()" in reanchor and
            "TEMPO REANCHOR FAILED" in reanchor and
            "futureTick" not in reanchor and
            "projectOriginSmfTick_ = currentTick" in reanchor,
            "tempo re-anchor must invalidate before scan and cleanup only on failure")
    schedule_ahead = player_service[player_service.index(
        "void CardputerSmfPlayerService::scheduleAhead"):
        player_service.index("void CardputerSmfPlayerService::logPerformance")]
    require("ProjectTransportBlockSnapshot projectTransport" in schedule_ahead and
            "scheduleProjectSmfTick(\n                projectTransport" in schedule_ahead and
            "DroppedLateNoteOn" in schedule_ahead,
            "one PROJECT snapshot must govern each scheduling pass and late NoteOn")
    route_pos = schedule_ahead.index("routeSmfNote(")
    filter_pos = schedule_ahead.index("if (!routed.mapped)")
    queue_pos = schedule_ahead.index("eventQueue_.tryPushNoteOn(")
    require(route_pos < filter_pos < queue_pos and
            "perfUnmappedEventsFiltered_" in schedule_ahead,
            "unmapped SEQTRAK notes must be filtered before queue publication")
    require("trySnapshot(candidate)" in player_service and
            "projectTimelineIsStale(candidate.blockSequence" in player_service and
            "ProjectTransportReadResult::Unavailable" in player_service and
            "GP CLOCK STALE / MIDI PAUSED" in player_service,
            "timeline contention and signed freshness must remain distinct from Stop")
    require("B Files" in player_page and "toggleRouting()" in player_page,
            "player must expose file return and RAW/SEQTRAK routing controls")

    require("MidiImporter importer" in project and "importFile" in project,
            "existing quantized MIDI importer must remain available beside PLAY")

    forbidden_usb = ("USBMIDI", "TinyUSB", "writePacket(", "sendNoteOn(", "sendNoteOff(")
    for token in forbidden_usb:
        require(token not in player_service,
                f"SmfPlayerTask must not own USB/TinyUSB directly: found {token}")
        require(token not in player_registry,
                f"SMF service registry must stay free of USB writes: found {token}")

    require("registerCardputerSmfMidiQueue" in player_registry,
            "Cardputer SMF service must publish only through the scheduled queue")
    require("beginCardputerSmfPlayerService" in player_registry and
            "beginCardputerSmfPlayerService" in
                (ROOT / "GroovePuter.ino").read_text(encoding="utf-8"),
            "SMF runtime must be reserved explicitly during setup")
    require("kMaxTimingEvents = 32" in
                (ROOT / "src/platform/cardputer_smf_player.h").read_text(encoding="utf-8") and
            "timingDocument_ = SmfDocument{}" not in player_service,
            "Cardputer SMF metadata must stay bounded and preserve setup capacity")
    require("catch (const std::bad_alloc&)" in player_service,
            "SMF loading must report allocation failure instead of resetting")
    require("ScheduledSmfMidiEventQueue* g_smfQueue" in usb_dispatch,
            "existing MidiDispatchTask must consume the SMF scheduled queue")
    require("smfSendFailureAction" in usb_dispatch and
            "vTaskDelay(kSmfRetryDelay)" in usb_dispatch and
            "kSmfStaleNoteOnThresholdUs" in usb_dispatch,
            "USB backpressure must use bounded paced retries and stale NoteOn drops")
    require("kSmfStaleNoteOnThresholdUs = kBlockDurationUs" in usb_dispatch and
            "smfLateNoteOnSent" in usb_dispatch and
            "smfLateNoteOffSent" in usb_dispatch and
            "smfTransportEpochDrops" in usb_dispatch,
            "dispatcher must enforce one-block late policy and expose diagnostics")
    final_smf_dispatch = usb_dispatch[usb_dispatch.index(
        "if (g_smfQueue == nullptr ||\n"
        "                !scheduledSmfMidiEventGenerationIsCurrent"):
        usb_dispatch.index("drainControlEvents(2);")]
    require("pendingSmf.type == ScheduledSmfMidiEventType::NoteOn" in
                final_smf_dispatch and
            "!projectSmfNoteOnStillCurrent(pendingSmf)" in final_smf_dispatch and
            final_smf_dispatch.index("!projectSmfNoteOnStillCurrent(pendingSmf)") <
                final_smf_dispatch.index("dispatchSmfEvent(pendingSmf)"),
            "PROJECT NoteOn must recheck playing/epoch after the final busy-wait")
    project_note_gate = usb_dispatch[usb_dispatch.index(
        "bool projectSmfNoteOnStillCurrent"):
        usb_dispatch.index("void logDiagnosticsIfDue")]
    require("if (event.projectTransportEpoch == 0) return true;" in
                project_note_gate,
            "ORIGINAL NoteOn must not depend on the PROJECT timeline seqlock")
    require("g_output.handleSmfNoteOn" in usb_dispatch and
            "g_output.handleSmfNoteOff" in usb_dispatch,
            "MidiDispatchTask must remain the physical SMF note owner")
    require("midiDispatchTask" in usb_dispatch,
            "accepted single USB owner task must remain present")


def main() -> None:
    test_default_runtime_logging_budget()
    test_blocked_note_mode_keys_are_consumed()
    test_performance_all_notes_off_is_target_scoped()
    test_note_mode_is_explicit_and_runtime_only()
    test_live_synth_render_is_not_transport_gated()
    test_perform_is_additive_to_legacy_carousel()
    test_stage_visuals_remain_lightweight_and_state_driven()
    test_smf_player_is_additive_and_keeps_single_usb_owner()
    test_smf_velocity_boost_range_is_session_sticky()
    print("performance + SMF source regressions: OK")


def test_smf_velocity_boost_range_is_session_sticky() -> None:
    routing = (ROOT / "src/midi/smf_routing.h").read_text(encoding="utf-8")
    player = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8"
    )

    require("nextSmfVelocityBoost" in routing,
            "velocity boost cycle must stay in the testable SMF routing core")
    for value in ("case 0: return 8", "case 8: return 16",
                  "case 16: return 24", "case 24: return 32",
                  "case 32: return 48"):
        require(value in routing,
                f"velocity boost cycle is missing {value}")

    cycle_start = player.index("case CommandType::CycleVelocityBoost:")
    cycle = player[cycle_start:player.index("break;", cycle_start)]
    require("nextSmfVelocityBoost(velocityBoost_)" in cycle,
            "Cardputer player must use the shared +0..+48 cycle")

    load = player[player.index("bool CardputerSmfPlayerService::loadFile"):
                  player.index("bool CardputerSmfPlayerService::scanMetadata")]
    require("velocityBoost_ = 0" not in load,
            "loading another MIDI must keep the session velocity boost")


if __name__ == "__main__":
    main()
