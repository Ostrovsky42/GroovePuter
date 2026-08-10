#!/usr/bin/env python3
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
if "case WorkflowMode::Generate: return 2;" not in workflow:
    raise AssertionError("GENERATE must expose exactly GENRE and FEEL")
generate_pages = workflow.split("static constexpr int kGeneratePages[]", 1)[1].split("};", 1)[0]
if "kTexture" in generate_pages or "kGeneration" in generate_pages:
    raise AssertionError("retired TEXTURE/GENERATION ids must not return to normal GENERATE navigation")
if "if (page == kTexture || page == kGeneration) return kFeel;" not in workflow:
    raise AssertionError("legacy TEXTURE/GENERATION page ids must remain FEEL redirects")

print("TextureMode runtime removal regressions: PASS")
