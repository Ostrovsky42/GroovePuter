#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, content: str) -> None:
    (ROOT / path).write_text(content, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# 1. Restore the original groovebox startup page. PERFORM remains additive page 12.
path = "src/ui/miniacid_display.h"
text = read(path)
text = replace_once(
    text,
    "  int page_index_ = 12;\n  int previous_page_index_ = 12;  // For Backspace/` toggle\n",
    "  int page_index_ = 0;\n  int previous_page_index_ = 0;  // For Backspace/` toggle\n",
    "restore startup page",
)
write(path, text)

# 2. Give [ and ] back to the legacy page carousel. Scale moves to comma/period.
path = "src/ui/pages/perform_page.cpp"
text = read(path)
text = replace_once(
    text,
    "        case '[':\n            keyboard_.cycleScale(-1);\n            return true;\n        case ']':\n            keyboard_.cycleScale(1);\n            return true;\n",
    "        case ',':\n        case '<':\n            keyboard_.cycleScale(-1);\n            return true;\n        case '.':\n        case '>':\n            keyboard_.cycleScale(1);\n            return true;\n",
    "move scale controls",
)
text = replace_once(
    text,
    '                           "N:Note [ ]:Scale -/=:Oct",\n',
    '                           "N:Note ,/.:Scale -/=:Oct",\n',
    "update PERFORM footer",
)
write(path, text)

# 3. Render synth voices when transport is stopped. This is the missing live-audio path.
path = "src/dsp/miniacid_engine.cpp"
text = read(path)
voice_start = text.index("    uint32_t tV0 = 0;")
voice_end = text.index("    uint32_t tD0 = 0;", voice_start)
old_voice = text[voice_start:voice_end]
new_voice = '''    uint32_t tV0 = 0;
    if (detailedProfile) tV0 = micros();
    // Synth voices are instruments as well as sequencer voices. Their envelopes
    // must be rendered while transport is stopped so live NoteOn/NoteOff reaches
    // the audio output and release tails can complete naturally.
    if (!mute303 && synthVoices_[0]) {
      float v = synthVoices_[0]->process() * 0.5f;
      v = distortion303.process(v);
      v *= trackVolumes[(int)VoiceId::SynthA];
      sample303 += delay303.process(v);
    } else delay303.process(0.0f);
    if (!mute303_2 && synthVoices_[1]) {
      float v = synthVoices_[1]->process() * 0.5f;
      v = distortion3032.process(v);
      v *= trackVolumes[(int)VoiceId::SynthB];
      sample303 += delay3032.process(v);
    } else delay3032.process(0.0f);
    if (detailedProfile) tVoicesTotal += (micros() - tV0);

'''
if "if (playing)" not in old_voice or "synthVoices_[0]->process()" not in old_voice:
    raise RuntimeError("live audio render block did not match expected structure")
text = text[:voice_start] + new_voice + text[voice_end:]
text = replace_once(
    text,
    "      sample += sample303 + drumsMix;\n    }\n    if (detailedProfile) tDrumsTotal += (micros() - tD0);\n",
    "      sample += drumsMix;\n    }\n    sample += sample303;\n    if (detailedProfile) tDrumsTotal += (micros() - tD0);\n",
    "mix live synth outside transport gate",
)
write(path, text)

# 4. Pin both hardware regressions in source tests.
path = "tests/test_performance_source_regressions.py"
text = read(path)
insert = '''\n\ndef test_live_synth_render_is_not_transport_gated() -> None:\n    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")\n    start = engine.index("uint32_t tV0 = 0;")\n    end = engine.index("uint32_t tD0 = 0;", start)\n    voice_block = engine[start:end]\n\n    require("if (playing)" not in voice_block,\n            "live synth rendering must work while transport is stopped")\n    require("synthVoices_[0]->process()" in voice_block,\n            "Synth A must be rendered in the live-audio path")\n\n    drum_end = engine.index("if (detailedProfile) tDrumsTotal", end)\n    drum_block = engine[end:drum_end]\n    require("if (playing)" in drum_block,\n            "drums must remain transport-gated")\n    require("sample += sample303;" in engine[drum_end - 120:drum_end + 80],\n            "live synth mix must be added outside the drum transport gate")\n\n\ndef test_perform_is_additive_to_legacy_carousel() -> None:\n    display_header = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")\n    perform = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")\n\n    require("int page_index_ = 0;" in display_header,\n            "GroovePuter must boot into the original groovebox page")\n    require("int previous_page_index_ = 0;" in display_header,\n            "legacy Back/page state must start from page zero")\n    require("case '[':" not in perform and "case ']':" not in perform,\n            "PERFORM must not steal legacy [ ] page navigation")\n    require("case ',':" in perform and "case '.':" in perform,\n            "PERFORM scale controls must use non-navigation keys")\n'''
marker = "\ndef main() -> None:\n"
if marker not in text:
    raise RuntimeError("performance regression main marker missing")
text = text.replace(marker, insert + marker, 1)
text = replace_once(
    text,
    "    test_note_mode_is_explicit_and_runtime_only()\n",
    "    test_note_mode_is_explicit_and_runtime_only()\n    test_live_synth_render_is_not_transport_gated()\n    test_perform_is_additive_to_legacy_carousel()\n",
    "register new regressions",
)
write(path, text)

# 5. Update hardware test instructions.
path = "docs/tests/PERFORMANCE_WORKFLOW_CARDPUTER_ADV.md"
text = read(path)
text = text.replace("- Startup: `PERFORM`.", "- Startup: the original Genre/groovebox page.")
text = text.replace("- `[` / `]`: previous / next scale.", "- `[` / `]`: previous / next page in the original carousel.\n- `,` / `.` on PERFORM: previous / next scale.")
text = text.replace("- [ ] Firmware starts on PERFORM.", "- [ ] Firmware starts on the original Genre/groovebox page.")
text = text.replace("- [ ] `Fn + Tab` cycles PERFORM, PATTERN, and ARRANGE.", "- [ ] `[` / `]` still cycles every original GroovePuter page.\n- [ ] `Fn + Tab` provides the additional PERFORM/PATTERN/ARRANGE shortcut.")
write(path, text)

# 6. Restore the permanent read-only workflow and remove this one-shot script.
workflow = '''name: Core regressions

on:
  push:
    branches:
      - main
      - agent/fix-core-reliability
  pull_request:

permissions:
  contents: read

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run core host regressions
        run: bash tests/run_host_tests.sh

  sdl-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install SDL dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \\
            build-essential \\
            libsdl2-dev \\
            libsdl2-gfx-dev

      - name: Build desktop target
        working-directory: platform_sdl
        run: make clean all CXX=g++

  cardputer-adv-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Arduino CLI
        uses: arduino/setup-arduino-cli@v2
        with:
          version: "1.x"

      - name: Install pinned M5Stack dependencies
        run: bash scripts/install_arduino_deps.sh

      - name: Compile Cardputer ADV firmware
        run: bash scripts/build.sh --warnings all
'''
write(".github/workflows/core-regressions.yml", workflow)
Path(__file__).unlink()
