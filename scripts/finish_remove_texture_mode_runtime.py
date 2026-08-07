#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


# The first migration pass intentionally writes all modified files before its
# residue assertion. Complete the streaming Scene writer which lives in the
# header and was not covered by the first pass.
sh = read("scenes.h")
for fragment in (
    '  if (!writeLiteral(",\\\"tex\\\":")) return false;\n  if (!writeInt(scene_->genre.textureMode)) return false;\n',
    '  if (!writeLiteral(",\\\"amt\\\":")) return false;\n  if (!writeInt(scene_->genre.textureAmount)) return false;\n',
    '  if (!writeLiteral(",\\\"cur\\\":")) return false;\n  if (!writeBool(scene_->genre.curatedMode)) return false;\n',
    '  if (!writeLiteral(",\\\"sound\\\":")) return false;\n  if (!writeBool(scene_->genre.applySoundMacros)) return false;\n',
):
    if fragment not in sh:
        raise RuntimeError(f"streaming serializer fragment not found: {fragment.splitlines()[0]}")
    sh = sh.replace(fragment, '')
write("scenes.h", sh)

# Replace the generated source gate with a field/call-site aware contract.
# Legacy JSON spellings are allowed only in scenes.cpp as explicit ignored
# decoder input; they must not appear as Scene fields, serializers or runtime API.
test = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

runtime_paths = []
for root in (ROOT / "src",):
    runtime_paths.extend(
        p for p in root.rglob("*")
        if p.is_file() and p.suffix in {".h", ".hpp", ".c", ".cc", ".cpp", ".ino"}
    )
runtime_paths.extend([ROOT / "scenes.h", ROOT / "scenes.cpp"])

hard_forbidden = (
    "enum class TextureMode", "struct TextureParams", "kTextureModeCount",
    "kTexturePresets", "setTextureMode(", "textureMode()", "cycleTexture(",
    "isTextureAllowed(", "firstAllowedTexture(", "nextAllowedTexture(",
    "applyTexture(", "resetTextureBiasTracking(",
    "syncTextureBiasBaselineFromCurrentState(", "lastAppliedCutoffBias_",
    "lastAppliedResBias_", "computeCutoffBiasSteps(", "computeResBiasSteps(",
)

for path in runtime_paths:
    text = path.read_text(encoding="utf-8", errors="ignore")
    for token in hard_forbidden:
        if token in text:
            raise AssertionError(
                f"runtime TextureMode residue: {path.relative_to(ROOT)}: {token}"
            )

scene_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
for field in (
    "uint8_t textureMode", "uint8_t textureAmount", "bool curatedMode",
    "bool applySoundMacros", "scene_->genre.textureMode",
    "scene_->genre.textureAmount", "scene_->genre.curatedMode",
    "scene_->genre.applySoundMacros",
):
    if field in scene_h:
        raise AssertionError(f"legacy TEXTURE state still persisted in scenes.h: {field}")

scene_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
for field in (
    'genreObj["tex"] =', 'genreObj["amt"] =', 'genreObj["cur"] =',
    'genreObj["sound"] =', "target_.genre.textureMode",
    "target_.genre.textureAmount", "target_.genre.curatedMode",
    "target_.genre.applySoundMacros", "loaded->genre.textureMode",
    "loaded->genre.textureAmount", "loaded->genre.curatedMode",
    "loaded->genre.applySoundMacros",
):
    if field in scene_cpp:
        raise AssertionError(f"legacy TEXTURE state still materialized: {field}")

for key in (
    'lastKey_ == "tex"', 'lastKey_ == "textureMode"',
    'lastKey_ == "amt"', 'lastKey_ == "textureAmount"',
    'lastKey_ == "cur"', 'lastKey_ == "curated"',
    'lastKey_ == "sound"', 'lastKey_ == "snd"',
):
    if key not in scene_cpp:
        raise AssertionError(f"legacy decode-only key missing: {key}")

engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
for token in ("applyTexture(*this)", "setTextureMode(", "resetTextureBiasTracking("):
    if token in engine:
        raise AssertionError(f"scene/reset path still projects TEXTURE: {token}")

workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
if "case WorkflowMode::Generate: return 3;" not in workflow:
    raise AssertionError("GENERATE must remain exactly three pages")
if "case kTexture:" not in workflow or "if (page == kTexture) page = kFeel;" not in workflow:
    raise AssertionError("legacy page id 8 must remain a FEEL redirect")

print("TextureMode runtime removal regressions: PASS")
'''
write("tests/test_remove_texture_mode_runtime.py", test)

# Release gate: record the migration as a pre-release compatibility gate, while
# leaving physical listening acceptance explicitly open.
gate_path = ROOT / "docs/releases/PRE_0_9_RELEASE_GATE.md"
if gate_path.exists():
    gate = gate_path.read_text(encoding="utf-8")
    marker = "## TextureMode runtime migration"
    if marker not in gate:
        gate += '''\n\n## TextureMode runtime migration\n\nAutomated gate:\n\n- `TextureMode`, texture presets and runtime projection APIs are absent;\n- legacy texture metadata is decode-only and never re-serialized;\n- Scene load does not re-apply a texture preset over persisted synth/Tape/delay/distortion state;\n- GENERATE remains `GENRE -> FEEL -> GENERATION`;\n- host, SDL, Cardputer ADV, fixed-DRAM and SEQTRAK MIDI-only checks must be green on the exact PR head.\n\nHardware gate before merge/release:\n\n- [ ] load a pre-migration Scene containing historical TEXTURE metadata;\n- [ ] confirm concrete synth/Tape/delay/distortion settings are preserved after load/save;\n- [ ] compare the old project before/after and confirm no material sound regression;\n- [ ] confirm page id 8 redirects to FEEL and normal navigation exposes only three GENERATE pages.\n'''
        gate_path.write_text(gate, encoding="utf-8")

# Remove the temporary trigger marker from the final PR diff.
(ROOT / "docs/acceptance/.keep").unlink(missing_ok=True)

# Final static assertions over the transformed working tree.
for rel in ("src/dsp/genre_manager.h", "src/dsp/genre_manager.cpp"):
    text = read(rel)
    for token in (
        "TextureMode", "TextureParams", "kTexturePresets", "kTextureModeCount",
        "setTextureMode", "textureMode()", "applyTexture(", "cycleTexture(",
        "isTextureAllowed", "firstAllowedTexture", "nextAllowedTexture",
        "lastAppliedCutoffBias_", "lastAppliedResBias_",
    ):
        if token in text:
            raise RuntimeError(f"{rel}: runtime residue {token}")

scene_h = read("scenes.h")
for token in (
    "scene_->genre.textureMode", "scene_->genre.textureAmount",
    "scene_->genre.curatedMode", "scene_->genre.applySoundMacros",
    "uint8_t textureMode", "uint8_t textureAmount", "bool curatedMode",
    "bool applySoundMacros",
):
    if token in scene_h:
        raise RuntimeError(f"scenes.h: persisted residue {token}")

print("TextureMode runtime migration completion: PASS")
