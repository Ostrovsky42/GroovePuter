#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_pattern_drums_split_local_and_midi() -> None:
    text = (ROOT / "src/dsp/pattern_drum_event_tap.h").read_text(encoding="utf-8")
    start = text.index("void triggerPattern")
    end = text.index("void triggerKick", start)
    block = text[start:end]
    local = block.index("GroovePuterOutput::allowsInternal")
    trigger = block.index("triggerLocal")
    publish = block.index("publishPatternDrumTrigger")
    require(local < trigger < publish,
            "Pattern Drums must gate local trigger independently before MIDI publication")
    require("Track::Drums" in block and "SourceClass::Pattern" in block,
            "Pattern Drums must use the canonical Drum owner")


def test_sampler_is_internal_source_layer_not_output_owner() -> None:
    text = (ROOT / "src/sampler/drum_sampler_track.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/sampler/drum_sampler_track.h").read_text(encoding="utf-8")
    start = text.index("void DrumSamplerTrack::triggerPad")
    end = text.index("void DrumSamplerTrack::stopPad", start)
    block = text[start:end]
    owner = block.index("GroovePuterOutput::allowsInternal")
    pool = block.index("pool_.trigger")
    require(owner < pool,
            "Sampler local ownership must be decided before starting a sample voice")
    require("Track::Drums" in block,
            "Sampler must inherit Drums output ownership")
    require("setEnabled" not in block and "preload" not in block,
            "OutputMode must not mutate sampler source enable or load state")
    require("void stopAll() { pool_.stopAll(); }" in header,
            "transition cleanup must stop voices without clearing assignments")

    process_start = text.index("void DrumSamplerTrack::process")
    process_block = text[process_start:]
    require("GroovePuterOutput" not in process_block,
            "Sampler process path must not read OutputMode per frame")


def test_perform_drums_reuse_existing_local_owner() -> None:
    cpp = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/internal_synth_output.h").read_text(encoding="utf-8")

    start = cpp.index("if (event.target == MusicalEventTarget::Drums)")
    end = cpp.index("// <=0.9.5 PERFORM", start)
    block = cpp[start:end]
    require("allowsInternalNoteOn(event)" in block,
            "PERFORM Drums NoteOn must use canonical output ownership")
    require("triggerRegisteredLocalDrumVoice" in block,
            "PERFORM Drums must reuse the existing local drum engine")
    require("samplerTrack->triggerPad" in block,
            "PERFORM Drums must preserve the optional internal sampler source layer")
    require("samplerTrack->setEnabled" not in block and "preload" not in block,
            "PERFORM output switching must not unload or reconfigure sampler assets")
    require("liveDrumPadMask_" in header,
            "PERFORM Drum sampler cleanup must be bounded to lanes owned by this sink")
    require("samplerTrack->pad(lane).loop" in block and
            "samplerTrack->stopPad(lane)" in block,
            "looping sample key-up must release only its owned pad")
    require("case MusicalEventType::AllNotesOff" in block,
            "PERFORM Drums must retain explicit sampler panic cleanup")


def test_transition_cleanup_reuses_existing_dispatcher() -> None:
    control = (ROOT / "src/midi/midi_control_event_queue.h").read_text(encoding="utf-8")
    scheduled = (ROOT / "src/midi/scheduled_musical_event_queue.h").read_text(encoding="utf-8")
    runtime = (ROOT / "src/output/output_mode_runtime.h").read_text(encoding="utf-8")

    for name, text in (("control", control), ("scheduled", scheduled)):
        require("packedOutputDisableEpochs" in text and
                "midiDisableEpoch" in text,
                f"{name} queue must observe canonical MIDI-disable epochs")
        require("allowsMidiNoteOn" in text and "continue;" in text,
                f"{name} queue must discard stale pre-switch NoteOn at pop time")
        require("takePendingAllNotesOffMask" in text,
                f"{name} queue must reuse existing scoped panic handoff")

    require("liveNoteOff" in runtime,
            "removing Synth local ownership must release an active PERFORM note")
    require("samplerTrack->stopAll" in runtime,
            "removing Drum local ownership must terminate sampler loops")
    require("setMode(track, nextMode)" in runtime,
            "runtime cleanup must publish through the single canonical owner")
    forbidden_calls = (
        "UsbMidiOutput",
        "TinyUSB",
        "SD.",
        "SD::",
        "preload(",
        "loadWav",
        "malloc(",
        "calloc(",
        "realloc(",
        "make_unique",
        "make_shared",
    )
    require(all(token not in runtime for token in forbidden_calls),
            "output transition helper must stay allocation/IO/USB free")


def test_scene_persistence_is_transactional_and_composed() -> None:
    sampler = (ROOT / "src/sampler/sample_scene_persistence.h").read_text(encoding="utf-8")
    sdl = (ROOT / "platform_sdl/scene_storage_sdl.cpp").read_text(encoding="utf-8")
    persistence = (ROOT / "src/output/output_scene_persistence.h").read_text(encoding="utf-8")

    require("OutputSceneWriteState outputFilter_" in sampler,
            "Cardputer sampler Scene writer must compose OutputMode into the existing stream")
    require("OutputSceneReadState outputState_" in sampler,
            "Cardputer sampler Scene reader must capture OutputMode in the existing stream")
    failed_start = sampler.index("bool failed()")
    failed_end = sampler.index("bool eof() const", failed_start)
    failed_block = sampler[failed_start:failed_end]
    require("outputState_.commit()" in failed_block,
            "Cardputer OutputMode must commit only after SceneManager completed the filtered read")
    require("outputState_.commit()" not in sampler[:failed_start],
            "Cardputer output state must not mutate runtime during parsing")

    require("manager.loadScene(serialized)" in sdl,
            "SDL must validate Scene content before committing OutputMode")
    main_load = sdl.index("bool SceneStorageSdl::readScene(SceneManager& manager)")
    main_end = sdl.index("bool SceneStorageSdl::writeScene(const std::string& data)", main_load)
    main_block = sdl[main_load:main_end]
    require(main_block.index("manager.loadScene(serialized)") <
            main_block.index("outputModes.commit()"),
            "SDL main Scene must commit OutputMode only after successful manager load")
    auto_load = sdl.index("bool SceneStorageSdl::readSceneAuto(SceneManager& manager)")
    auto_end = sdl.index("bool SceneStorageSdl::hasSceneAuto()", auto_load)
    auto_block = sdl[auto_load:auto_end]
    require(auto_block.index("manager.loadScene(serialized)") <
            auto_block.index("outputModes.commit()"),
            "SDL autosave must commit OutputMode only after successful manager load")

    require("\"out\"" in persistence and "values[3]" in persistence,
            "Output persistence must remain a bounded three-track extension")
    require("present ? values[i] : 0u" in persistence,
            "missing output field must restore hidden legacy compatibility")


def test_alt_o_ui_uses_existing_audio_guards_and_dirty_state() -> None:
    synth = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
    drum = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")

    require("event.alt" in synth and "GROOVEPUTER_O" in synth,
            "Synth pages must expose Alt+O")
    require("Mode::Layer" in synth and "hasExplicitMode(track)" in synth,
            "first Synth Alt+O on legacy state must canonicalize to LAYER")
    require("audio_guard_(apply)" in synth and
            "applyModeWithLocalCleanup" in synth,
            "Synth Alt+O must use the existing AudioGuard and lifecycle helper")
    require("markSceneMutated" in synth,
            "Synth output changes must mark the project dirty")
    require("SYNTH %c OUT:%s" in synth,
            "Synth output change must give an explicit toast")

    output_key = drum.index("if (isDrumOutputCycleKey(ui_event))")
    active_tab_gate = drum.index("if (activePageIndex() != 0")
    require(output_key < active_tab_gate,
            "Drums Alt+O must work from GRID/FEEL/AUTO/SAMPLES, not only main grid")
    require("Mode::Layer" in drum and "hasExplicitMode(track)" in drum,
            "first Drums Alt+O on legacy state must canonicalize to LAYER")
    require("withAudioGuard" in drum[output_key:active_tab_gate] and
            "applyModeWithLocalCleanup" in drum[output_key:active_tab_gate],
            "Drums Alt+O must use the existing AudioGuard and lifecycle helper")
    require("markSceneMutated" in drum[output_key:active_tab_gate],
            "Drums output changes must mark the project dirty")
    require("DRUMS OUT:%s" in drum,
            "Drums output change must give an explicit toast")


def test_status_chrome_exposes_compact_track_output() -> None:
    chrome = (ROOT / "src/ui/ui_status_chrome.h").read_text(encoding="utf-8")

    require('"src/output/output_ownership.h"' in chrome,
            "status chrome must read the canonical OutputOwnership owner")
    for token in ('"[I]"', '"[M]"', '"[L]"', '"[-]"'):
        require(token in chrome,
                f"status chrome must expose compact output token {token}")
    require("UiStatusContext::SynthA" in chrome and
            "Track::SynthA" in chrome and
            "UiStatusContext::SynthB" in chrome and
            "Track::SynthB" in chrome and
            "UiStatusContext::Drums" in chrome and
            "Track::Drums" in chrome,
            "status chrome must map Synth A/B and Drums to their canonical owners")
    require("hasExplicitMode(track)" in chrome and "UiStatusOutput::Legacy" in chrome,
            "legacy Scenes must show a compact unset marker without a fourth mode")
    require("sceneRevisionSnapshot" in chrome and "UiStatusDirtyStamp" in chrome,
            "cached status chrome must refresh on every output Scene mutation")
    require("uiStatusCanonicalTrackOutput(status)" in chrome,
            "formatted status line must use the canonical track output")


def test_sampler_layer_label_remains_source_layer() -> None:
    sampler = (ROOT / "src/ui/pages/sampler_page.cpp").read_text(encoding="utf-8")
    require("LAYER" in sampler and "samplerTrack" in sampler,
            "existing sampler LAYER control must remain present as sampler source state")
    require("OutputMode" not in sampler and "Alt+O" not in sampler,
            "SamplerPage LAYER must not be silently redefined as MIDI output ownership")


def test_no_new_routing_framework() -> None:
    files = [
        ROOT / "src/dsp/pattern_drum_event_tap.h",
        ROOT / "src/sampler/drum_sampler_track.cpp",
        ROOT / "src/input/internal_synth_output.cpp",
        ROOT / "src/output/output_mode_runtime.h",
    ]
    combined = "\n".join(path.read_text(encoding="utf-8") for path in files)
    require("new MusicalEventQueue" not in combined,
            "0.9.6 must not allocate a second MIDI queue")
    require("UsbMidiOutput" not in combined and "TinyUSB" not in combined,
            "local output ownership must stay independent from physical USB routing")


def test_eye_transport_preserves_display_framebuffer_allocation() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    header = (ROOT / "src/eye_pair_sync/eye_output_mode.h").read_text(
        encoding="utf-8")
    implementation = (ROOT / "src/eye_pair_sync/eye_output_mode.cpp").read_text(
        encoding="utf-8")
    setup = sketch[sketch.index("void setup()") :]

    reserve_framebuffer = setup.index("g_display.setRotation(1);")
    initialize_eye_transport = setup.index("eye_output_mode_init();")
    start_display = setup.index("g_display.begin();")

    require("#define GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW 0" in header,
            "Cardputer ESP-NOW must remain an explicit opt-in until it fits "
            "the accepted DRAM budget")
    require("#define EYE_SYNC_VERSION_OUTPUT_MODE 2" in header and
            "#define EYE_SYNC_VERSION_GVEP 2" in header,
            "Dual-Eye wire formats must be v2")
    require("int64_t effect_t0_us" in header and
            "uint8_t reserved;" in header and
            "sizeof(eye_output_mode_packet_t) == 23" in header,
            "Output Mode must use the authoritative 23-byte v2 layout")
    require("sizeof(eye_gvep_packet_t) == 23" in header,
            "GVEP must use the authoritative 23-byte v2 layout")
    require("GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW" in implementation and
            "#define GVEP_USE_ESPNOW 1" in implementation,
            "Arduino ESP-NOW includes and transport must share the DRAM gate")
    feature_guard = setup.rfind(
        "#if GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW", 0, initialize_eye_transport)
    require(feature_guard >= 0,
            "Cardputer setup must not initialize ESP-NOW in the default build")
    require(start_display < initialize_eye_transport,
            "Cardputer framebuffer must be allocated before Wi-Fi/ESP-NOW "
            "fragments the internal heap")


if __name__ == "__main__":
    test_pattern_drums_split_local_and_midi()
    test_sampler_is_internal_source_layer_not_output_owner()
    test_perform_drums_reuse_existing_local_owner()
    test_transition_cleanup_reuses_existing_dispatcher()
    test_scene_persistence_is_transactional_and_composed()
    test_alt_o_ui_uses_existing_audio_guards_and_dirty_state()
    test_status_chrome_exposes_compact_track_output()
    test_sampler_layer_label_remains_source_layer()
    test_no_new_routing_framework()
    test_eye_transport_preserves_display_framebuffer_allocation()
    print("Output ownership source contract: PASS")
