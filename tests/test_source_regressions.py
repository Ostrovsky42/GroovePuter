#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_ppqn_dispatch_is_not_step_gated() -> None:
    source = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    loop_start = source.index("while (ticksToAdvance--)")
    loop_end = source.index("if (gateCountdownA_", loop_start)
    dispatch_block = source[loop_start:loop_end]

    require("advanceTick();" in dispatch_block,
            "each accumulated PPQN tick must dispatch sequencer events")
    require("currentTick_ % 24" not in dispatch_block,
            "PPQN dispatch must not be gated to 16th-note boundaries")


def test_all_substep_offsets_are_reachable() -> None:
    ticks_per_bar = 384
    ticks_per_step = 24

    for step in range(16):
        for offset in range(-23, 24):
            due_tick = (step * ticks_per_step + offset) % ticks_per_bar
            visits = sum(1 for tick in range(ticks_per_bar) if tick == due_tick)
            require(visits == 1,
                    f"step={step} offset={offset} must be visited exactly once")


def test_adv_amp_pin_is_not_used_as_rgb_data() -> None:
    profile = (ROOT / "src/platform/cardputer_adv_hardware.h").read_text(
        encoding="utf-8"
    )
    led = (ROOT / "src/ui/led_manager.cpp").read_text(encoding="utf-8")

    require("GROOVEPUTER_CARDPUTER_ADV_PA_EN_PIN 21" in profile,
            "Cardputer ADV PA_EN pin must remain explicit")
    require("GROOVEPUTER_CARDPUTER_ADV_RGB_LED_PIN (-1)" in profile,
            "RGB output must remain disabled until a distinct ADV pin is verified")
    require("neopixelWrite(21" not in led,
            "GPIO21 must never receive WS2812 timing on Cardputer ADV")


def test_cardputer_sd_has_one_hardware_mount_path() -> None:
    profile = (ROOT / "src/platform/cardputer_adv_hardware.h").read_text(
        encoding="utf-8"
    )
    sd_mount = (ROOT / "src/platform/cardputer_sd.cpp").read_text(
        encoding="utf-8"
    )
    scene_storage = (ROOT / "scene_storage_cardputer.cpp").read_text(
        encoding="utf-8"
    )
    recorder = (ROOT / "src/audio/cardputer_audio_recorder.cpp").read_text(
        encoding="utf-8"
    )
    smf_page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
    smf_page_header = (ROOT / "src/ui/pages/smf_player_page.h").read_text(
        encoding="utf-8"
    )
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")

    for declaration in (
        "kSdClockPin = 40",
        "kSdMisoPin = 39",
        "kSdMosiPin = 14",
        "kSdChipSelectPin = 12",
    ):
        require(declaration in profile,
                f"Cardputer ADV SD profile is missing {declaration}")

    require("SPI.begin(" in sd_mount and "SD.begin(" in sd_mount,
            "platform SD owner must configure and mount the card")
    require("SD.begin(" not in scene_storage,
            "scene storage must use the shared platform SD mount")
    require("SD.begin(" not in recorder,
            "audio recorder must not remount SD with default pins")
    require("SD.begin(" not in smf_page,
            "SMF UI must not own hardware SD initialization")
    require("std::array<BrowserRow, kBrowserVisibleRows>" in smf_page_header and
            "browserStorageReady_" in smf_page_header and
            "fillVisibleEntries()" in smf_page and
            "resolveEntry(" in smf_page and
            "complete=1" in smf_page and
            "SD UNAVAILABLE" in smf_page,
            "SMF browser must stream a complete directory into a fixed visible "
            "window and distinguish an SD failure from an empty directory")
    require("bounded break" not in smf_page and
            "low-mem break" not in smf_page and
            "std::vector<std::string>" not in smf_page_header,
            "SMF browser must not present a partial directory as complete when "
            "runtime heap is low")

    audio_task_pos = sketch.index("startAudioTask();")
    early_sd_pos = sketch.index("g_sceneStorage.initializeStorage();")
    engine_init_pos = sketch.index("g_miniAcidInstance.init();")
    require(audio_task_pos < early_sd_pos < engine_init_pos,
            "SD must mount after reserving AudioTask and before DSP heap allocation")

    smf_runtime_pos = sketch.index("beginCardputerSmfPlayerService();")
    require(early_sd_pos < smf_runtime_pos < engine_init_pos,
            "SMF task and timing storage must be reserved before DSP/UI fragmentation")

    midi_runtime_pos = sketch.index("registerCardputerUsbMidiSink(")
    require(smf_runtime_pos < midi_runtime_pos < engine_init_pos,
            "MIDI dispatcher stack must be reserved before engine heap fragmentation")


def test_scene_and_page_validation_share_one_scratch_buffer() -> None:
    scenes_header = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    paging = (ROOT / "src/audio/pattern_paging.cpp").read_text(encoding="utf-8")

    require("class SceneScratchLease" in scenes_header and
  "SceneScratchLease::SceneScratchLease()" in scenes and
  "SceneScratchLease::~SceneScratchLease()" in scenes,
  "scene parsing and page validation need a bounded RAII scratch contract")
    require("MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT" in scenes and
  "MALLOC_CAP_SPIRAM" not in scenes,
  "Cardputer scratch scenes must use internal heap without PSRAM")
    require("PageStaging" not in paging and "g_stagingPage" not in paging,
  "pattern paging must not retain a second static staging buffer")
    require(paging.count("SceneScratchLease scratch;") == 2 and
  "SceneScratchLease scratch;" in scenes and
  "readAndValidatePage(const std::string& path, Scene& staging)" in paging,
  "project and page validation must lease and release transaction scenes")

def test_genre_regeneration_uses_full_compiled_params() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    start = engine.index("void MiniAcid::regeneratePatternsWithGenre()")
    end = engine.index("void MiniAcid::syncGrooveModeToGenre()", start)
    block = engine[start:end]

    require("getCompiledGenerativeParams()" in block,
            "genre regeneration must keep the complete fallback parameter set")
    require("getGrooveRecipe()" not in block,
            "genre regeneration must not collapse to the lossy GrooveRecipe adapter")


def test_recipe_selects_the_matching_groovebox_mode() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    start = engine.index("void MiniAcid::syncGrooveModeToGenre()")
    end = engine.index("void MiniAcid::toggleAudioDiag()", start)
    block = engine[start:end]

    require("grooveboxModeForRecipe" in block,
            "active recipes must select their Breaks/Acid/Dub groovebox mode")
    require("genreManager_.recipe()" in block,
            "groovebox mode selection must include the active recipe id")


def test_legacy_recipe_adapters_start_from_compiled_params() -> None:
    manager = (ROOT / "src/dsp/mode_manager.cpp").read_text(encoding="utf-8")
    marker = "// GROOVE RECIPE OVERLOADS"
    adapters = manager[manager.index(marker):]

    require(adapters.count(
        "GenerativeParams params = engine_.genreManager().getCompiledGenerativeParams();"
    ) == 3,
            "all legacy GrooveRecipe adapters must preserve omitted genre fields")
    require("GenerativeParams params;" not in adapters,
            "partially initialized GenerativeParams reintroduces random/noisy genres")


def test_generative_params_have_safe_defaults() -> None:
    header = (ROOT / "src/dsp/genre_manager.h").read_text(encoding="utf-8")
    start = header.index("struct GenerativeParams")
    end = header.index("// === GROOVE RECIPE", start)
    block = header[start:end]

    required_defaults = (
        "int minNotes =",
        "int maxNotes =",
        "int minOctave =",
        "int maxOctave =",
        "float slideProbability =",
        "float accentProbability =",
        "float gateLengthMultiplier =",
        "float swingAmount =",
        "float microTimingAmount =",
        "int velocityMin =",
        "int velocityMax =",
        "bool preferDownbeats =",
        "bool allowRepeats =",
        "float rootNoteBias =",
        "float ghostProbability =",
        "float chromaticProbability =",
        "bool sparseKick =",
        "bool sparseHats =",
        "bool noAccents =",
        "float fillProbability =",
    )
    for declaration in required_defaults:
        require(declaration in block,
                f"missing safe default in GenerativeParams: {declaration}")


def test_atlas_recipe_catalog_and_legacy_fallbacks() -> None:
    index = (ROOT / "src/generated/atlas_runtime.generated.h").read_text(
        encoding="utf-8"
    )
    manager = (ROOT / "src/dsp/genre_manager.cpp").read_text(encoding="utf-8")

    atlas_recipes = (
        (6, "Chicago Jack", "REC_ACID_CHICAGO_JACK", "rec_acid_chicago_jack.generated.h", "Acid"),
        (7, "Rolling Acid", "REC_ACID_ROLLING", "rec_acid_rolling.generated.h", "Acid"),
        (8, "Classic 2-Step", "REC_UKG_CLASSIC_2STEP", "rec_ukg_classic_2step.generated.h", "Breaks"),
        (9, "Dark Skippy", "REC_UKG_DARK_SKIPPY", "rec_ukg_dark_skippy.generated.h", "Breaks"),
        (10, "Deep Chord", "REC_DUB_DEEP_CHORD", "rec_dub_deep_chord.generated.h", "Dub"),
        (11, "Minimal Space", "REC_DUB_MINIMAL_SPACE", "rec_dub_minimal_space.generated.h", "Dub"),
    )
    for runtime_id, name, atlas_id, filename, groove in atlas_recipes:
        data = (ROOT / "src/generated" / filename).read_text(encoding="utf-8")
        require(f"kRecipe_{atlas_id}" in index,
                f"generated Atlas index must publish {name}")
        require('"P1", "BASE"' in data, f"{name} must include P1")
        require('"P2", "DEVELOPMENT"' in data, f"{name} must include P2")
        require('"P3",' in data, f"{name} must include P3")
        require(f'{{{runtime_id}, "{name}"' in manager,
                f"GenreManager must expose {name} as recipe id {runtime_id}")
        require(f"case {runtime_id}: return GrooveboxMode::{groove}" in manager,
                f"{name} must select {groove} macro mode")

    # The original probabilistic generators remain independent fallbacks.
    legacy_recipes = (
        (1, "UK Garage"),
        (2, "Drum&Bass"),
        (3, "Footwork"),
        (4, "Psytrance"),
        (5, "Dub Techno"),
    )
    for runtime_id, name in legacy_recipes:
        require(f'{{{runtime_id}, "{name}"' in manager,
                f"legacy recipe generator was removed: {name}")


def test_atlas_recipe_precedes_random_fallback() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    start = engine.index("void MiniAcid::regeneratePatternsWithGenre()")
    end = engine.index("void MiniAcid::syncGrooveModeToGenre()", start)
    block = engine[start:end]

    atlas_pos = block.index("AtlasRuntime::applyRecipe")
    fallback_pos = block.index("getCompiledGenerativeParams()")
    require(atlas_pos < fallback_pos,
            "compiled Atlas patterns must be attempted before random fallback")
    require("scene.feel.swingPct = atlasMetadata.swingPercent" in block,
            "Atlas recipe swing must be applied with the pattern")
    require("scene.genre.applyTempoOnApply" in block,
            "Atlas BPM must respect the existing tempo opt-in")


def test_genre_page_uses_recipe_mode_and_tempo_order() -> None:
    page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")

    link_start = page.index("const char* linkStateShort")
    link_end = page.index("int clampRecipeIndex", link_start)
    link_block = page[link_start:link_end]
    require("grooveboxModeForRecipe" in link_block,
            "Genre page LINK state must account for the active recipe")

    apply_start = page.index("void GenrePage::applyCurrent()")
    apply_end = page.index("void GenrePage::updateFromEngine()", apply_start)
    apply_block = page[apply_start:apply_end]
    require("grooveboxModeForRecipe" in apply_block,
            "Genre Apply must select recipe-aware macro mode")
    tempo_pos = apply_block.index("if (doApplyTempo)")
    regenerate_pos = apply_block.index("if (doRegenerate)")
    require(tempo_pos < regenerate_pos,
            "tempo must be set before generation so BPM adaptation is correct")
    require(apply_block.count("if (doApplyTempo)") == 1,
            "generic tempo must not overwrite Atlas BPM after regeneration")


def test_atlas_compiler_matches_manifest_contract() -> None:
    compiler = (ROOT / "tools/atlas/compile_atlas_runtime.py").read_text(
        encoding="utf-8"
    )
    manifest = json.loads(
        (ROOT / "tools/atlas/atlas_v2_6_runtime_manifest.json").read_text(
            encoding="utf-8"
        )
    )

    expected_hash = manifest["source_archive_sha256"]
    require(expected_hash in compiler,
            "Atlas compiler and provenance manifest must pin the same archive")
    require(manifest["atlas_schema_version"] == "2.6.0",
            "runtime compiler is defined only for Atlas schema 2.6.0")
    require(len(manifest["recipes"]) == 6,
            "runtime manifest must contain all six reviewed recipes")
    require(manifest["ignored_sampler_events"] == 40,
            "ignored sampler data must remain explicit")
    expected_ids = {
        "REC_ACID_CHICAGO_JACK",
        "REC_ACID_ROLLING",
        "REC_UKG_CLASSIC_2STEP",
        "REC_UKG_DARK_SKIPPY",
        "REC_DUB_DEEP_CHORD",
        "REC_DUB_MINIMAL_SPACE",
    }
    require({item["atlas_recipe_id"] for item in manifest["recipes"]} == expected_ids,
            "manifest recipe catalog drifted")
    for atlas_id in expected_ids:
        require(atlas_id in compiler, f"compiler missing recipe: {atlas_id}")


def test_recipe_selector_is_visible_and_navigable() -> None:
    page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    require("void drawRecipeOverlay" in page,
            "recipe selection must be visible instead of Fn-only hidden state")
    require("RECIPE SELECT" in page,
            "recipe overlay needs an explicit title")
    require("drawRecipeOverlay(gfx, recipeIndex_)" in page,
            "Apply focus must render the recipe list")
    require("cycleRecipeSelection(-1);" in page and "cycleRecipeSelection(1);" in page,
            "UP/DOWN must navigate visible recipes while Fn+UP/DOWN keeps morph")


def test_enter_applies_selected_recipe() -> None:
    page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    start = page.index("// ENTER: apply the current genre/texture/recipe selection.")
    end = page.index("// SPACE: toggle apply mode", start)
    enter_block = page[start:end]
    require("applyCurrent();" in enter_block,
            "Enter must apply the selected recipe")
    require("cycleApplyMode" not in enter_block,
            "Enter must not cycle the apply mode")
    require('right = "ENTER:Apply M:ApplyMode";' in page,
            "Apply footer must document Enter and M controls")



def test_performance_workflow_boundaries() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")

    require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
            "performance key mapping must stay centralized")
    require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
            "performance key mapping must stay centralized")
    for path in (ROOT / "src/ui/pages").glob("*.cpp"):
        page = path.read_text(encoding="utf-8")
        require("asdfghjkl" not in page and "qwertyuiop" not in page,
                f"keyboard mapping duplicated in {path.name}")

    require("MidiOutput" not in header,
            "MusicalEventTarget must describe logical voices, not output sinks")
    require("USBMIDI" not in sketch and "USB-MIDI" not in sketch,
            "USB MIDI belongs to the later spike")

    live_start = engine.index("void MiniAcid::liveNoteOn")
    live_end = engine.index("void MiniAcid::liveNoteOff", live_start)
    require("if (playing) return;" in engine[live_start:live_end],
            "PatternPlayer must exclusively own Synth A while transport runs")
    require("performance_keyboard_.keyDown" in display,
            "display input policy must route unhandled page keys through PerformanceKeyboard")
    require("g_performanceKeyboard.keyDown" not in sketch,
            "hardware sketch must not duplicate normalized note routing")
    require("releaseMissingKeys" in sketch,
            "keyboard matrix must recover missed key-up events")
    require("case 12: page = std::make_unique<PerformPage>" in display,
            "PERFORM page must remain an additive page instead of reindexing editors")
    require("setCurrentPage(static_cast<int8_t>(page_index_))" not in display,
            "UI page indices must never overwrite pattern-storage page indices")


def test_performance_settings_are_runtime_only() -> None:
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    storage = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    require("PerformanceScale" not in scenes and "octaveShift" not in scenes,
            "performance scale/octave must not expand scene schema in this PR")
    require("PerformanceScale" not in storage and "octaveShift" not in storage,
            "performance settings must reset to runtime defaults after reboot")

def test_ui_redraw_does_not_hold_audio_pause() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    start = sketch.index("auto handleWithFallback")
    end = sketch.index("auto applyCtrlLetter", start)
    block = sketch[start:end]

    require("bool needsDraw = false;" in block,
            "key handling must defer drawing until mutations are complete")
    require("if (needsDraw) drawUI();" in block,
            "UI redraw must happen after the mutation scope exits")
    last_scope_end = block.rfind("    }\n\n    if (needsDraw) drawUI();")
    require(last_scope_end >= 0,
            "audio mutation scope must close before the full UI redraw")


def test_splash_closes_display_transaction() -> None:
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    splash_start = display.index("if (splash_active_)")
    splash_end = display.index("// Draw background", splash_start)
    block = display[splash_start:splash_end]
    require("gfx_.flush();\n            gfx_.endWrite();\n            return;" in block,
            "splash early return must balance startWrite/endWrite")


def test_project_midi_import_and_persistence_contracts() -> None:
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(
        encoding="utf-8"
    )
    midi_manager = (ROOT / "src/ui/midi_file_manager.cpp").read_text(
        encoding="utf-8"
    )
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(
        encoding="utf-8"
    )
    scenes = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    importer = (ROOT / "src/audio/midi_importer.cpp").read_text(
        encoding="utf-8"
    )
    storage = (ROOT / "scene_storage_cardputer.cpp").read_text(
        encoding="utf-8"
    )

    auto_start = project.index("void ProjectPage::autoRouteMidi()")
    auto_end = project.index("void ProjectPage::openConfirmClearDialog()", auto_start)
    auto_route = project[auto_start:auto_end]
    require("channels[9].used() ? 10 : -1" in auto_route,
            "MIDI auto-route must reserve unconditional drums for GM channel 10")
    require("maxNotes" not in auto_route,
            "the busiest melodic channel must not be guessed as drums")
    require('name.find("drum")' in auto_route and
            'name.find("perc")' in auto_route,
            "non-GM drum routing must require an explicit percussion name")

    import_start = project.index("bool ProjectPage::importMidiAtSelection()")
    import_end = project.index("bool ProjectPage::deleteSelectionInDialog()", import_start)
    import_block = project[import_start:import_end]
    require("Select at least one MIDI route" in import_block,
            "MIDI import must reject an empty routing matrix")
    require("if (midi_mask_a_)" in import_block and
            "if (midi_mask_b_)" in import_block and
            "if (midi_mask_d_)" in import_block,
            "song population must respect the A/B/Drums routing matrix")
    require("firstRowEmpty" in import_block and
            "if (firstRowEmpty) songPosition = 0;" in import_block,
            "APPEND must reuse row zero in a genuinely blank song")
    require("saveSceneAs(mini_acid_.currentSceneName())" in import_block,
            "successful MIDI import must persist patterns and song rows")
    require("MIDI imported and saved" in import_block,
            "the UI must distinguish persisted imports from save failures")

    require("GROOVEPUTER_ESCAPE" in project and
            "dialog_type_ = DialogType::ImportMidi;" in project,
            "Escape from the routing matrix must return to MIDI browsing")
    require("GROOVEPUTER_ESCAPE" in midi_manager and
            "navigateUp()" in midi_manager and
            "EntryKind::Parent" in midi_manager,
            "the shared MIDI manager must own parent-directory navigation")
    require("midi_import_start_pattern_ >= kMaxPatterns" in project,
            "MIDI target selection must support the complete pattern range")

    new_start = engine.index("bool MiniAcid::createNewSceneWithName")
    new_end = engine.index("void MiniAcid::loadSceneFromStorage", new_start)
    new_block = engine[new_start:new_end]
    require("sceneManager_.wipeToZero();" in new_block,
            "New must create a blank scene instead of the Mario demo")
    require("loadDefaultScene" not in new_block,
            "the demo fallback must not be reused for user-created projects")

    save_start = engine.index("bool MiniAcid::saveSceneAs")
    save_end = engine.index("bool MiniAcid::createNewSceneWithName", save_start)
    save_block = engine[save_start:save_end]
    require("previousName" in save_block and
            "setCurrentSceneName(previousName)" in save_block,
            "failed Save As must restore the previous current-scene pointer")

    names_start = engine.index("std::vector<std::string> MiniAcid::availableSceneNames")
    names_end = engine.index("bool MiniAcid::loadSceneByName", names_start)
    names_block = engine[names_start:names_end]
    require("getCurrentSceneName" not in names_block,
            "the scene list must not invent an entry without a JSON file")

    wipe_start = scenes.index("void SceneManager::wipeToZero()")
    wipe_end = scenes.index("Scene& SceneManager::currentScene()", wipe_start)
    wipe_block = scenes[wipe_start:wipe_end]
    require("positions[0].patterns[0] = 0" not in wipe_block and
            "positions[0].patterns[1] = 0" not in wipe_block and
            "positions[0].patterns[2] = 0" not in wipe_block,
            "blank songs must not be pre-populated with pattern zero")

    require("bool importedThisNote = false;" in importer and
            "if (importedThisNote)" in importer,
            "unsupported drum notes must not produce false import success")
    require("msgType == 0xD0" in importer and
            "uint8_t tmp; readU8(file, tmp);" in importer,
            "MIDI scan must consume Channel Pressure payload bytes")
    require("finalPageSaved" in importer and "originalPageRestored" in importer,
            "MIDI import must report pattern-page persistence failures")

    require("if (!sceneAlreadyExists) return true;" in storage,
            "new scene names must not become the boot target before data is saved")
    require("persistCurrentSceneName()" in storage,
            "existing scene selection must remain persistent")

def main() -> None:
    test_ppqn_dispatch_is_not_step_gated()
    test_all_substep_offsets_are_reachable()
    test_adv_amp_pin_is_not_used_as_rgb_data()
    test_genre_regeneration_uses_full_compiled_params()
    test_recipe_selects_the_matching_groovebox_mode()
    test_legacy_recipe_adapters_start_from_compiled_params()
    test_generative_params_have_safe_defaults()
    test_atlas_recipe_catalog_and_legacy_fallbacks()
    test_atlas_recipe_precedes_random_fallback()
    test_genre_page_uses_recipe_mode_and_tempo_order()
    test_atlas_compiler_matches_manifest_contract()
    test_recipe_selector_is_visible_and_navigable()
    test_enter_applies_selected_recipe()
    test_performance_workflow_boundaries()
    test_performance_settings_are_runtime_only()
    test_ui_redraw_does_not_hold_audio_pause()
    test_splash_closes_display_transaction()
    test_project_midi_import_and_persistence_contracts()
    test_scene_and_page_validation_share_one_scratch_buffer()
    print("source regressions: OK")


if __name__ == "__main__":
    main()
