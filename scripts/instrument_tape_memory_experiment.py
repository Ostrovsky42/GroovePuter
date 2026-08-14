#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit(
        "usage: instrument_tape_memory_experiment.py <source-root> <A|B|C>")

root = Path(sys.argv[1])
variant = sys.argv[2].upper()
if variant not in {"A", "B", "C"}:
    raise SystemExit("tape memory variant must be A, B, or C")

ino_path = root / "GroovePuter.ino"
engine_path = root / "src/dsp/miniacid_engine.cpp"


def replace_once(path: Path, anchor: str, replacement: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(anchor)
    if count != 1:
        raise SystemExit(
            f"tape memory instrumentation: expected one {label} anchor "
            f"in {path}, found {count}")
    path.write_text(text.replace(anchor, replacement, 1), encoding="utf-8")


# Variant A is the exact current runtime. Variant B removes the TapeFX object
# from MiniAcid. Variant C removes both TapeFX and TapeLooper objects, so the
# looper buffer can never be allocated. These edits are made only in the
# temporary build tree created by the experiment script.
if variant in {"B", "C"}:
    replace_once(
        engine_path,
        "    tapeFX(std::make_unique<TapeFX>()),\n",
        "    tapeFX(nullptr),\n",
        "TapeFX constructor",
    )
    replace_once(
        engine_path,
        "  if (!tapeControlCached_ || macroChanged) {\n"
        "    tapeFX->applyMacro(tapeState.macro);",
        "  if (tapeFX && (!tapeControlCached_ || macroChanged)) {\n"
        "    tapeFX->applyMacro(tapeState.macro);",
        "TapeFX macro sync",
    )
    replace_once(
        engine_path,
        "  if (!tapeControlCached_ || minimalChanged) {\n"
        "    tapeFX->applyMinimalParams(tapeState.space, tapeState.movement, tapeState.groove);",
        "  if (tapeFX && (!tapeControlCached_ || minimalChanged)) {\n"
        "    tapeFX->applyMinimalParams(tapeState.space, tapeState.movement, tapeState.groove);",
        "TapeFX minimal sync",
    )
    replace_once(
        engine_path,
        "  const bool tapeFxEnabled = tapeState.fxEnabled;",
        "  const bool tapeFxEnabled = tapeState.fxEnabled && tapeFX != nullptr;",
        "TapeFX render guard",
    )

if variant == "C":
    replace_once(
        engine_path,
        "    tapeLooper(std::make_unique<TapeLooper>()),\n",
        "    tapeLooper(nullptr),\n",
        "TapeLooper constructor",
    )
    replace_once(
        engine_path,
        "  if (!tapeControlCached_ || looperModeChanged) {\n"
        "    tapeLooper->setMode(tapeState.mode);",
        "  if (tapeLooper && (!tapeControlCached_ || looperModeChanged)) {\n"
        "    tapeLooper->setMode(tapeState.mode);",
        "TapeLooper mode sync",
    )
    replace_once(
        engine_path,
        "  if (!tapeControlCached_ || looperSpeedChanged) {\n"
        "    tapeLooper->setSpeed(tapeState.speed);",
        "  if (tapeLooper && (!tapeControlCached_ || looperSpeedChanged)) {\n"
        "    tapeLooper->setSpeed(tapeState.speed);",
        "TapeLooper speed sync",
    )
    replace_once(
        engine_path,
        "  if (!tapeControlCached_ || looperVolChanged) {\n"
        "    tapeLooper->setVolume(tapeState.looperVolume);",
        "  if (tapeLooper && (!tapeControlCached_ || looperVolChanged)) {\n"
        "    tapeLooper->setVolume(tapeState.looperVolume);",
        "TapeLooper volume sync",
    )
    replace_once(
        engine_path,
        "  const bool looperActive = (tapeState.mode != TapeMode::Stop);",
        "  const bool looperActive = tapeLooper && (tapeState.mode != TapeMode::Stop);",
        "TapeLooper render guard",
    )
    replace_once(
        engine_path,
        "      diag.trackSource(sample303, drumsMix, samplerSample, 0.0f, vocalSample, tapeLooper->getPeak(), 0.0f);",
        "      diag.trackSource(sample303, drumsMix, samplerSample, 0.0f, vocalSample,\n"
        "                       tapeLooper ? tapeLooper->getPeak() : 0.0f, 0.0f);",
        "TapeLooper diagnostics guard",
    )
    replace_once(
        engine_path,
        "  if (tapeState.mode != tapeLooper->mode()) {",
        "  if (tapeLooper && tapeState.mode != tapeLooper->mode()) {",
        "TapeLooper state mirror guard",
    )

# Capture the true post-static-construction heap before setup performs any
# device initialization. The values live on the setup task stack, not heap, and
# are printed only after Serial becomes available.
replace_once(
    ino_path,
    "void setup() {\n"
    "  // Enable the Cardputer ADV power amplifier. This pin is not RGB data.\n",
    "void setup() {\n"
    "  const uint32_t tapeExpStaticFreeInt = heap_caps_get_free_size(\n"
    "      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n"
    "  const uint32_t tapeExpStaticLargestInt = heap_caps_get_largest_free_block(\n"
    "      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);\n"
    "  const uint32_t tapeExpStaticFree8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);\n"
    "  const uint32_t tapeExpStaticLargest8 = heap_caps_get_largest_free_block(\n"
    "      MALLOC_CAP_8BIT);\n"
    "  // Enable the Cardputer ADV power amplifier. This pin is not RGB data.\n",
    "setup static snapshot",
)

replace_once(
    ino_path,
    "  Serial.println(\"\\n\\n!! BOOTING !!\");\n",
    "  Serial.println(\"\\n\\n!! BOOTING !!\");\n"
    f"  Serial.println(\"[TAPE-MEM] variant={variant}\");\n"
    "  Serial.printf(\"[tape-exp-after-static-construction] freeInt=%u largInt=%u free8=%u larg8=%u\\n\",\n"
    "                (unsigned)tapeExpStaticFreeInt,\n"
    "                (unsigned)tapeExpStaticLargestInt,\n"
    "                (unsigned)tapeExpStaticFree8,\n"
    "                (unsigned)tapeExpStaticLargest8);\n",
    "static snapshot print",
)

replace_once(
    ino_path,
    "  logHeapCaps(\"after-audio-task\");\n",
    "  logHeapCaps(\"after-audio-task\");\n"
    "  logHeapCaps(\"tape-exp-after-audio-task\");\n",
    "audio task checkpoint",
)
replace_once(
    ino_path,
    "  markBootStage(83, \"after early SD init\");\n",
    "  markBootStage(83, \"after early SD init\");\n"
    "  logHeapCaps(\"tape-exp-after-sd\");\n",
    "SD checkpoint",
)
replace_once(
    ino_path,
    "  markBootStage(51, \"after MiniAcid::init\");\n",
    "  markBootStage(51, \"after MiniAcid::init\");\n"
    "  logHeapCaps(\"tape-exp-after-miniacid-init\");\n",
    "MiniAcid checkpoint",
)
replace_once(
    ino_path,
    "  markBootStage(71, \"after MiniAcidDisplay alloc\");\n",
    "  markBootStage(71, \"after MiniAcidDisplay alloc\");\n"
    "  logHeapCaps(\"tape-exp-after-ui\");\n",
    "UI checkpoint",
)

print(f"Tape memory experiment variant {variant} instrumented in {root}")
