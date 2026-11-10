#!/usr/bin/env python3
"""LOOPER phases 0-1 (+ BARS 8 / 192 events): source contracts (docs/design/LOOPER_PLAN.md).

- looper*.h: no allocation, no String/vector, no FS, no locks;
- unrelated PARTNER files and src/audio are untouched (git, when available);
- GroovePuter.ino: the looper and the PARTNER both observe PERFORM input
  (fixed two-way fan-out), the looper borrows the PARTNER player,
  serviceLooper() runs after serviceCallResponseLog();
- the PARTNER stays: its INPUT group (HELPER 14..NEW RIFF IF 19), its
  mode/settings handlers; the LOOPER group sits above it at 5..13 (variant A:
  PerformInputRow, group headings are not rows; HELPER shows RIFF for Mode::Loop);
- PERFORM: Ctrl+Z (APP_EVENT_UNDO) is passed on when the looper has nothing to
  undo (the global "UNDO: EMPTY / NOT HERE" toast stays), Del clears only on
  the live surface, X keeps the layer;
- help and keys.md name the looper keys.
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


for name in ("looper.h", "looper_layer.h", "looper_clock.h"):
    code = strip_comments(read(f"src/input/{name}"))
    for banned in (r"\bnew\b", r"\bmalloc\s*\(", r"std::vector", r"\bString\b", r"\bSD\.",
                   r"\bFile\b", r"mutex", r"portENTER", r"xSemaphore", r"std::string"):
        assert not re.search(banned, code), f"{name}: {banned}"

layer = read("src/input/looper_layer.h")
# BARS 8 / 192 events: 4-byte events (onset 7 bits), the open table's 16-bit
# pool index, passes of more than 32 notes played as consecutive plans.
assert "kLoopCapacity = 192" in layer
assert "kMaxLoopSteps = 128" in layer
assert 'static_assert(sizeof(LoopNote) == 4' in layer
assert "static_assert(kLoopCapacity <= 255" in layer
assert "kOpenIndexMask = 0x00FFu" in layer
assert "uint8_t* nextStep = nullptr" in layer
looper = read("src/input/looper.h")
assert "sizeof(Looper) <= 864" in looper
assert "bars != 1 && bars != 4 && bars != 8" in looper
assert "bars_ == 1 ? 2 : bars_ == 2 ? 4 : bars_ == 4 ? 8 : 1" in looper
assert "startSegment(now, position)" in looper

# Untouched files (looper polyphony base: 44bc76b3).
try:
    changed = subprocess.run(
        ["git", "diff", "--name-only", "44bc76b3", "--",
         "src/input/call_response_listener.h",
         "src/input/call_response_phrase_capture.h", "src/input/call_response_planner.h",
         "src/input/call_response_performance_plan.h",
         "src/audio", "platform_sdl", "src/ui/undo_ux.h"],
        cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    assert changed == [], f"must stay untouched: {changed}"
except (OSError, subprocess.CalledProcessError):
    print("git unavailable: untouched-file check skipped")

ino = read("GroovePuter.ino")
assert "g_performanceKeyboard.setInputObserver(&g_performanceInputFanout);" in ino
assert re.search(r"g_performanceInputFanout\.set\(&GroovePuterCallResponse::callResponseListener\(\),\s*"
                 r"&GroovePuterLooper::looper\(\)\);", ino)
fanout = strip_comments(read("src/input/performance_input_fanout.h"))
for banned in (r"\bnew\b", r"\bmalloc\s*\(", r"std::vector", r"std::function"):
    assert not re.search(banned, fanout), f"fanout: {banned}"
assert "looper().setPlayer(&GroovePuterCallResponse::callResponseListener().responder())" in ino
body = ino[ino.index("void loop() {"):]
cr = body.index("serviceCallResponseLog();")
lp = body.index("serviceLooper();")
assert cr < lp < cr + 80, "serviceLooper() right after serviceCallResponseLog()"
# The PARTNER log body and its live-input cancel are unchanged.
assert "GroovePuterCallResponse::callResponseListener().cancelAnswer();" in ino

page = read("src/ui/pages/perform_page.cpp")
handle = page[page.index("bool PerformPage::handleEvent(UIEvent& event) {"):]
undo = handle.index("GroovePuterUndoUx::isUndoEvent(event)")
keydown = handle.index("if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;")
assert undo < keydown
assert "if (!GroovePuterLooper::looper().canUndo()) return false;" in handle[undo:keydown]
assert "if (toolsLayerVisible_) return false;" in handle  # Del: live surface only
assert "looper().panicStop()" in handle and "PANIC: PARTNER SILENT, LOOP STOPPED" in handle
assert "callResponseListener().silence();" in handle  # X still silences the PARTNER
for key in ("case 'z': case 'Z'", "case 'c': case 'C'", "case 'v': case 'V'", "case 'b': case 'B'",
            "case 'm': case 'M'"):
    assert key in handle, key
assert "case PerformanceToolContext::Input: return kInputRowCount;" in page
header = read("src/ui/pages/perform_page.h")
assert "kMaxRowByContext[5] = {5, 5, 5, 5, kInputRowCount - 1}" in header
assert "kLooperRowFirst = performInputRow(PerformInputRow::Looper);" in header
assert "kLooperRowLast = performInputRow(PerformInputRow::ToMelody);" in header
enum_body = header[header.index("enum class PerformInputRow"):header.index("Count,", header.index("enum class PerformInputRow"))]
order = re.findall(r"^\s*([A-Z][A-Za-z]+)(?: = 0)?,", enum_body, flags=re.M)
assert order == ["MidiIn", "ListenChannel", "SendTo", "PlayAs", "LeadStyle", "Looper", "RecordSource", "LoopBars",
                 "LoopChannel", "NoteLength", "HoldLayer", "Layer", "MuteLayer", "ToMelody", "Helper",
                 "ReplyBars", "ReplyChannel", "FreezeRiff", "Variation", "NewRiffIf"], order
assert "kPartnerRowFirst = performInputRow(PerformInputRow::Helper);" in header
assert "kPartnerRowLast = performInputRow(PerformInputRow::NewRiffIf);" in header
# PARTNER rows and handlers stay.
for text in ('drawRow(ir(InRow::Helper), "HELPER", helperModeName(partner), true);',
             'drawRow(ir(InRow::Variation), "VARIATION"', 'drawRow(ir(InRow::NewRiffIf), "NEW RIFF IF"',
             "cyclePartnerSetting(row, direction);", "partner.cycleMode(direction);",
             'partner.mode() == PartnerMode::Loop ? "RIFF" : partner.modeName()',
             '"--- MIDI IN ---"', '"--- HELPER ---"', '"--- LOOPER ---"'):
    assert text in page, text
# Gone from INPUT: TARGET/OUT/USB rows and the old labels; the USB role is a hint.
tools_input = page[page.index('drawHeading(0, "--- MIDI IN ---");'):page.index('drawRow(ir(InRow::ToMelody)')]
for gone in ('"TARGET"', '"OUT"', '"USB"', '"LIVE MIDI"', '"ROUTE"', '" LP ', '"PARTNER"',
             '"LOCK LAYER"', '"PARTNER ON"', '"PARTNER ACTIVE"'):
    assert gone not in tools_input, gone
# Words: DX ROUTE (value of LOOP CH 0), HOLD (one word for B), HELPER (never PARTNER, for the player).
assert 'std::snprintf(value, sizeof(value), "DX ROUTE");' in tools_input
assert '"HOLD LAYER"' in tools_input and '"HELPER ON"' in tools_input
assert "LOOP: HOLD, B TO RELEASE" in page and "DX ROUTE: SAME CH AS PERFORM DX" in page and "AS KEYS" not in page and "LOOP: HELPER OFF FIRST" in page
assert "UNLOCK" not in page and "LOCK LAYER" not in page and ", LOCK," not in page
assert "LOOP: DX ROUTE, NO INTERNAL SOUND" in page and "dxRouteNoted_" in page
# Counter: no U letter (undo = brighter), Z REC while the loop is empty and idle; one LOOP: prefix.
assert 'loop.canUndo() ? "U" : ""' not in page and 'counterColor = COLOR_TEXT' in page and '"Z REC"' in page
for old in ("LOOP EMPTY:", "BARS: NO ROOM", "LOOP FULL:", "LOOP STOPPED:", "AS KEYS", "--- PARTNER ---"):
    assert old not in page, old
# TO MELODY: GRAB's guard, an explicit question, Undo hint.
export = page[page.index("void PerformPage::exportLooperToMelody() {"):page.index("bool PerformPage::looperKey")]
assert "SynthSequencerPage::voiceHasUnsavedEdits(miniAcid_, voice)" in export
assert "SAVE OR DISCARD %c FIRST" in export
assert export.index("voiceHasUnsavedEdits") < export.index("loopToMelody(")
assert "REPLACE MELODY %c WITH LOOP? ENTER" in export and "CTRL+Z ON SYNTH %c" in export
assert "USB: PROJECT>MIDI" in page
assert "void onExit() override;" in read("src/ui/pages/perform_page.h")

help_text = read("src/ui/global_help_content.h")
perform = help_text[help_text.index("kPerformLines"):help_text.index("kPlayerLines")]
for line in ("Z           Loop REC/DUB", "C           Loop PLAY/STOP", "V           Loop UNDO pass",
             "B           Loop HOLD layer", "Del         Loop CLEAR (V undo)",
             "M           Loop layer 1/2",
             "X           Panic: HELPER+loop stop"):
    assert line in perform, line
assert "Loop: 1-8 bars, 192 notes" in perform
assert "Loop: TO MELODY, Enter x2" in perform and "LP > MELODY" not in perform
assert '"LOOP LENGTH IN BARS"' in page
keys = read("src/ui/docs/keys.md")
assert "`LOOP BARS` 1/2/4/8" in keys and "`N/192`" in keys and "`LAYER`" in keys and "`MUTE LAYER`" in keys
assert "`TO MELODY`" in keys and "LP > MELODY" not in keys and "`LP " not in keys
assert "OFF/ANSWER/DUET/RIFF" in keys and "`--- HELPER ---`" in keys
assert "LOOP: HELPER OFF FIRST" in keys
# Phase 3: BARS keeps the notes (no LoopNotEmpty), the second layer is written.
assert "LoopNotEmpty" not in looper and "layer_.resize(passSteps(), steps, bars_)" in looper
assert "layer_.noteOn(tag, activeLayer()," in looper
assert "muteMask(), carry, &next" in looper
assert "### LOOPER" in keys and "`Z`" in keys and "LOOP: HOLD, B TO RELEASE" in keys

print("looper source regressions: OK")
