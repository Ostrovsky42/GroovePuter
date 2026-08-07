#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

SAFE_FROM_SOURCE = [
    "README.md",
    "docs/acceptance/REMOVE_TEXTURE_MODE_CARDPUTER_ADV.md",
    "docs/refactors/REMOVE_TEXTURE_MODE_RUNTIME.md",
    "scenes.cpp",
    "scenes.h",
    "src/dsp/miniacid_engine.cpp",
    "src/ui/docs/keys.md",
    "tests/run_host_tests.sh",
    "tests/test_axis_hardware_feedback_source_regressions.py",
    "tests/test_global_help_source_regressions.py",
    "tests/test_remove_texture_mode_runtime.py",
    "tests/test_source_regressions.py",
]
subprocess.run(["git", "fetch", "origin", "agent/remove-texture-mode"], cwd=ROOT, check=True)
subprocess.run(["git", "checkout", "origin/agent/remove-texture-mode", "--", *SAFE_FROM_SOURCE], cwd=ROOT, check=True)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


def remove_function(text, marker):
    start = text.find(marker)
    if start < 0:
        raise RuntimeError(f"missing function marker: {marker}")
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"missing function brace: {marker}")
    depth = 0
    end = None
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None:
        raise RuntimeError(f"unterminated function: {marker}")
    while end < len(text) and text[end] in " \t\r\n":
        end += 1
    return text[:start] + text[end:]

# Preserve the newer GenreSceneView architecture from dev_0.9_test while
# removing the obsolete TextureMode axis and projection API.
h = read("src/dsp/genre_manager.h")
h = h.replace('#include "src/dsp/tape_defs.h"\n', '')
h = re.sub(r'\nenum class TextureMode : uint8_t \{.*?\};\n', '\n', h, flags=re.S)
h = h.replace('static constexpr int kTextureModeCount = 5;\n', '')
h = re.sub(r'\nstruct TextureParams \{.*?\};\n', '\n', h, flags=re.S)
h = h.replace('extern const TextureParams kTexturePresets[kTextureModeCount];\n', '')
h = re.sub(r'\nconst char\* textureModeName\(TextureMode mode\);\n', '\n', h)
h = re.sub(r'\nbool isTextureAllowed\(GenerativeMode genre, TextureMode texture\);\nTextureMode firstAllowedTexture\(GenerativeMode genre\);\nTextureMode nextAllowedTexture\(GenerativeMode genre,\n\s*TextureMode current,\n\s*int direction = 1\);\n', '\n', h)
for snippet in [
    '    void setTextureMode(TextureMode mode);\n',
    '    void cycleTexture(int direction = 1);\n',
    '    TextureMode textureMode() const;\n',
    '    const TextureParams& getTextureParams() const;\n',
    '    void applyTexture(MiniAcid& engine);\n',
    '    void resetTextureBiasTracking() {\n        lastAppliedCutoffBias_ = 0;\n        lastAppliedResBias_ = 0;\n    }\n',
    '    void syncTextureBiasBaselineFromCurrentState();\n',
    '    static const char* textureModeName(TextureMode mode) {\n        return GenreCatalog::textureModeName(mode);\n    }\n',
    '    static bool isTextureAllowed(GenerativeMode genre, TextureMode texture) {\n        return GenreCatalog::isTextureAllowed(genre, texture);\n    }\n',
    '    static TextureMode firstAllowedTexture(GenerativeMode genre) {\n        return GenreCatalog::firstAllowedTexture(genre);\n    }\n',
    '    static TextureMode nextAllowedTexture(\n            GenerativeMode genre, TextureMode current, int direction = 1) {\n        return GenreCatalog::nextAllowedTexture(genre, current, direction);\n    }\n',
    '    int lastAppliedCutoffBias_ = 0;\n',
    '    int lastAppliedResBias_ = 0;\n',
    '    TextureMode texture;\n',
]:
    h = h.replace(snippet, '')
h = h.replace('// The two integers below track already-applied filter deltas; they are transient\n// DSP bookkeeping and are never a second copy of genre settings.\n', '')
write("src/dsp/genre_manager.h", h)

cpp = read("src/dsp/genre_manager.cpp")
cpp = re.sub(r'\nconst TextureParams kTexturePresets\[kTextureModeCount\] = \{.*?\n\};\n', '\n', cpp, flags=re.S)
cpp = re.sub(r',\s*TextureMode::[A-Za-z]+,', ',', cpp)
for marker in [
    'int textureIndex(TextureMode mode)',
    'TextureMode sceneTextureMode(const GenreSettings& settings)',
    'const char* textureModeName(TextureMode mode)',
    'bool isTextureAllowed(GenerativeMode genre, TextureMode texture)',
    'TextureMode firstAllowedTexture(GenerativeMode genre)',
    'TextureMode nextAllowedTexture(GenerativeMode genre,',
    'void GenreSceneView::setTextureMode(TextureMode mode)',
    'void GenreSceneView::cycleTexture(int direction)',
    'TextureMode GenreSceneView::textureMode() const',
    'const TextureParams& GenreSceneView::getTextureParams() const',
    'void GenreSceneView::syncTextureBiasBaselineFromCurrentState()',
    'void GenreSceneView::applyTexture(MiniAcid& engine)',
]:
    cpp = remove_function(cpp, marker)
cpp = re.sub(r'\nconstexpr uint8_t kAllowedTextureMask\[kGenerativeModeCount\] = \{.*?\n\};\n', '\n', cpp, flags=re.S)
write("src/dsp/genre_manager.cpp", cpp)

# Keep dev_0.9_test's newer Genre page and release docs, but remove stale wording
# in the manual without replacing its newer test-branch content wholesale.
manual = read("MANUAL.md")
manual = manual.replace('GENRE != FEEL != GENERATOR != TEXTURE', 'GENRE != FEEL != GENERATOR')
manual = manual.replace('GENRE != FEEL != GENERATION != TEXTURE', 'GENRE != FEEL != GENERATION')
manual = manual.replace('GENRE -> FEEL -> GENERATION -> TEXTURE', 'GENRE -> FEEL -> GENERATION')
write("MANUAL.md", manual)

genre_page = read("src/ui/pages/genre_page.cpp")
genre_page = genre_page.replace('// ENTER: apply the current genre/texture/recipe selection.', '// ENTER: apply the current genre/recipe selection.')
write("src/ui/pages/genre_page.cpp", genre_page)

# Source contract must be clean before committing.
for rel in ["src/dsp/genre_manager.h", "src/dsp/genre_manager.cpp", "scenes.h", "src/dsp/miniacid_engine.cpp"]:
    text = read(rel)
    for token in ["TextureMode", "TextureParams", "kTextureModeCount", "kTexturePresets", "applyTexture(", "setTextureMode(", "textureMode()", "resetTextureBiasTracking("]:
        if token in text:
            raise RuntimeError(f"{rel}: TextureMode residue: {token}")

print("dev_0.9_test TextureMode integration transform: PASS")
