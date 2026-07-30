#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


ino = ROOT / "GroovePuter.ino"
text = ino.read_text(encoding="utf-8")

replace_once(
    ino,
    "TaskHandle_t g_audioTaskHandle = nullptr;\nstatic AudioMutationGate g_audioMutationGate;\n",
    "TaskHandle_t g_audioTaskHandle = nullptr;\n"
    "static AudioMutationGate g_audioMutationGate;\n"
    "static uint32_t g_lastUiDrawUs = 0;\n"
    "static uint32_t g_peakUiDrawUs = 0;\n",
)

replace_once(
    ino,
    "void drawUI() {\n  if (g_miniDisplay) g_miniDisplay->update();\n}\n",
    "void drawUI() {\n"
    "  const uint32_t startedAt = micros();\n"
    "  if (g_miniDisplay) g_miniDisplay->update();\n"
    "  g_lastUiDrawUs = micros() - startedAt;\n"
    "  if (g_lastUiDrawUs > g_peakUiDrawUs) {\n"
    "    g_peakUiDrawUs = g_lastUiDrawUs;\n"
    "  }\n"
    "}\n",
)

text = ino.read_text(encoding="utf-8")
start_marker = "  auto handleWithFallback = [&](UIEvent evt) {"
end_marker = "\n  auto applyCtrlLetter ="
start = text.index(start_marker)
end = text.index(end_marker, start)
replacement = r'''  auto handleWithFallback = [&](UIEvent evt) {
    Serial.printf("[DIAG] handleWithFallback: key=0x%02X (%c), scancode=%d, app_event=%d\n",
      (uint8_t)evt.key, evt.key >= 32 && evt.key < 127 ? evt.key : '.', evt.scancode, evt.app_event_type);
    evt.event_type = GROOVEPUTER_KEY_DOWN;

    bool handled = false;
    {
      AudioMutationScope mutationScope(g_audioMutationGate);
      handled = g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false;
    }
    if (handled) {
      drawUI();
      return;
    }

    bool needsDraw = false;
    {
      // Guard only control-plane mutations. Releasing the gate before drawUI()
      // prevents a full display redraw from intentionally pausing audio output.
      AudioMutationScope mutationScope(g_audioMutationGate);
      char c = evt.key;
      if (c == '\t' && g_miniDisplay) {
        UIEvent app_evt{};
        app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
        app_evt.app_event_type = GROOVEPUTER_APP_EVENT_MULTIPAGE_DOWN;
        needsDraw = g_miniDisplay->handleEvent(app_evt);
      } else if (c == '\n' || c == '\r') {
        if (g_miniDisplay) g_miniDisplay->dismissSplash();
        needsDraw = true;
      } else if (c == '[') {
        if (g_miniDisplay) g_miniDisplay->previousPage();
        needsDraw = true;
      } else if (c == ']') {
        if (g_miniDisplay) g_miniDisplay->nextPage();
        needsDraw = true;
      } else if (c == 'i' || c == 'I') {
        g_miniAcid->randomize303Pattern(0);
        needsDraw = true;
      } else if (c == 'o' || c == 'O') {
        g_miniAcid->randomize303Pattern(1);
        needsDraw = true;
      } else if (c == 'p' || c == 'P') {
        g_miniAcid->randomizeDrumPattern();
        needsDraw = true;
      } else if (c == '1') {
        g_miniAcid->toggleMute303(0);
        needsDraw = true;
      } else if (c == '2') {
        g_miniAcid->toggleMute303(1);
        needsDraw = true;
      } else if (c == '3') {
        g_miniAcid->toggleMuteKick();
        needsDraw = true;
      } else if (c == '4') {
        g_miniAcid->toggleMuteSnare();
        needsDraw = true;
      } else if (c == '5') {
        g_miniAcid->toggleMuteHat();
        needsDraw = true;
      } else if (c == '6') {
        g_miniAcid->toggleMuteOpenHat();
        needsDraw = true;
      } else if (c == '7') {
        g_miniAcid->toggleMuteMidTom();
        needsDraw = true;
      } else if (c == '8') {
        g_miniAcid->toggleMuteHighTom();
        needsDraw = true;
      } else if (c == '9') {
        if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteClap();
        else g_miniAcid->toggleMuteRim();
        needsDraw = true;
      } else if (c == '0') {
        if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteRim();
        else g_miniAcid->toggleMuteClap();
        needsDraw = true;
      } else if (c == 'k' || c == 'K') {
        g_miniAcid->setBpm(g_miniAcid->bpm() - 2.5f);
        needsDraw = true;
      } else if (c == 'l' || c == 'L') {
        g_miniAcid->setBpm(g_miniAcid->bpm() + 2.5f);
        needsDraw = true;
      } else if (c == '-' || c == '_') {
        g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, -3);
        needsDraw = true;
      } else if (c == '=' || c == '+') {
        g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, 3);
        needsDraw = true;
      } else if (c == ';' || c == '\'') {
        needsDraw = true;
      } else if (c == ' ') {
        if (g_miniAcid->isPlaying()) g_miniAcid->stop();
        else g_miniAcid->start();
        needsDraw = true;
      }
    }

    if (needsDraw) drawUI();
  };
'''
ino.write_text(text[:start] + replacement + text[end:], encoding="utf-8")

replace_once(
    ino,
    "       // Serial.printf(\"[PERF] CPU: avg %.1f%% / peak %.1f%% (underruns %u)\\n\",\n"
    "       //     cpuAvg, cpuPeak, (unsigned)underruns);\n"
    "       // Serial.printf(\"       DSP: v:%uus d:%uus s:%uus f:%uus\\n\",\n"
    "       //     (unsigned)dv, (unsigned)dd, (unsigned)ds, (unsigned)df);\n",
    "       const uint32_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n"
    "       const uint32_t largestInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n"
    "       Serial.printf(\"[PERF] audio=%.1f%% peak=%.1f%% underruns=%u ui=%uus uiPeak=%uus freeInt=%u largest=%u dsp=%u/%u/%u/%u\\n\",\n"
    "           cpuAvg, cpuPeak, (unsigned)underruns,\n"
    "           (unsigned)g_lastUiDrawUs, (unsigned)g_peakUiDrawUs,\n"
    "           (unsigned)freeInt, (unsigned)largestInt,\n"
    "           (unsigned)dv, (unsigned)dd, (unsigned)ds, (unsigned)df);\n"
    "       g_peakUiDrawUs = g_lastUiDrawUs;\n",
)

ui = ROOT / "src/ui/miniacid_display.cpp"
replace_once(
    ui,
    "        if (splash_active_) {\n            gfx_.flush();\n            return;\n        }\n",
    "        if (splash_active_) {\n            gfx_.flush();\n            gfx_.endWrite();\n            return;\n        }\n",
)

tests = ROOT / "tests/test_source_regressions.py"
test_text = tests.read_text(encoding="utf-8")
insert_marker = "\ndef main() -> None:\n"
if insert_marker not in test_text:
    raise RuntimeError("test_source_regressions.py: main marker not found")
new_test = r'''

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
'''
test_text = test_text.replace(insert_marker, new_test + insert_marker, 1)
test_text = test_text.replace(
    "    test_atlas_compiler_matches_manifest_contract()\n",
    "    test_atlas_compiler_matches_manifest_contract()\n"
    "    test_ui_redraw_does_not_hold_audio_pause()\n"
    "    test_splash_closes_display_transaction()\n",
    1,
)
tests.write_text(test_text, encoding="utf-8")

# Guard the migration result before it can be committed.
ino_text = ino.read_text(encoding="utf-8")
ui_text = ui.read_text(encoding="utf-8")
test_text = tests.read_text(encoding="utf-8")
assert "if (needsDraw) drawUI();" in ino_text
assert "[PERF] audio=" in ino_text
assert "gfx_.flush();\n            gfx_.endWrite();\n            return;" in ui_text
assert "test_ui_redraw_does_not_hold_audio_pause" in test_text

print("audio stall migration applied")
