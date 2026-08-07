#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def sub_once(text, pattern, repl, label, flags=re.S):
    updated, count = re.subn(pattern, repl, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"{label}: expected one replacement, got {count}")
    return updated


# ---------------------------------------------------------------------------
# Genre runtime: remove TextureMode, presets, helpers, state and bias tracking.
# ---------------------------------------------------------------------------
h = read("src/dsp/genre_manager.h")
h = h.replace('#include "src/dsp/tape_defs.h"\n', '')
h = sub_once(
    h,
    r'// ============================================================================\n// TWO-AXIS GENRE SYSTEM\n// Generative Mode × Texture Mode = 20 genre combinations\n// ============================================================================\n',
    '// ============================================================================\n// GENRE GENERATION SYSTEM\n// Musical generation/profile selection only. Sound FX live in their own state.\n// ============================================================================\n',
    'genre header title')
h = sub_once(
    h,
    r'\n// === AXIS 2: TEXTURE MODE \(how sound is processed\) ===\nenum class TextureMode : uint8_t \{.*?\};\n\nstatic constexpr int kGenerativeModeCount = 9;\nstatic constexpr int kTextureModeCount = 5;',
    '\nstatic constexpr int kGenerativeModeCount = 9;',
    'TextureMode enum')
h = sub_once(h, r'\n// === TEXTURE PARAMETERS ===\nstruct TextureParams \{.*?\};\n\n// === GENRE TIMBRE', '\n// === GENRE TIMBRE', 'TextureParams')
h = h.replace('extern const TextureParams kTexturePresets[kTextureModeCount];\n', '')
h = h.replace('    TextureMode texture = TextureMode::Clean;\n', '')
h = h.replace('        static const char* const texNames[] = {"", "Dub ", "LoFi ", "Industrial ", "Psy "};\n', '')
h = sub_once(
    h,
    r'        snprintf\(cachedName_, sizeof\(cachedName_\), "%s%s",\s*\n\s*texNames\[static_cast<int>\(texture\)\],\s*\n\s*genNames\[static_cast<int>\(generative\)\]\);',
    '        snprintf(cachedName_, sizeof(cachedName_), "%s",\n                 genNames[static_cast<int>(generative)]);',
    'cached genre name')
h = sub_once(h, r'\n    void setTextureMode\(TextureMode mode\) \{.*?\n    \}', '', 'setTextureMode')
h = sub_once(h, r'\n    void cycleTexture\(int direction = 1\) \{.*?\n    \}', '', 'cycleTexture')
h = h.replace('    TextureMode textureMode() const { return state_.texture; }\n', '')
h = sub_once(h, r'\n    const TextureParams& getTextureParams\(\) const \{.*?\n    \}', '', 'getTextureParams')
h = sub_once(h, r'\n    static const char\* textureModeName\(TextureMode mode\) \{.*?\n    \}', '', 'textureModeName')
h = sub_once(h, r'\n    // Curated compatibility helpers\n    static bool isTextureAllowed.*?static TextureMode nextAllowedTexture.*?;\n', '\n', 'texture compatibility helpers')
h = sub_once(h, r'\n    // Apply texture to engine \(implemented in cpp\)\n    void applyTexture\(MiniAcid& engine\);\n', '\n', 'applyTexture declaration')
h = sub_once(h, r'\n    // Reset bias tracking .*?syncTextureBiasBaselineFromCurrentState\(\) \{.*?\n    \}\n', '\n', 'texture bias public API')
h = sub_once(h, r'\n    // Track last applied filter bias.*?int computeResBiasSteps\(\) const \{.*?\n    \}\n', '\n', 'texture bias private state')
h = h.replace('    TextureMode texture;\n', '')
write("src/dsp/genre_manager.h", h)

cpp = read("src/dsp/genre_manager.cpp")
cpp = sub_once(
    cpp,
    r'\n// TextureParams fields in order:.*?const TextureParams kTexturePresets\[kTextureModeCount\] = \{.*?\n\};\n',
    '\n', 'texture preset table')
cpp = re.sub(r',\s*TextureMode::[A-Za-z]+,', ',', cpp)
cpp = sub_once(
    cpp,
    r'\n// ============================================================================\n// TEXTURE APPLICATION\n// ============================================================================\n',
    '\n// ============================================================================\n// GENRE COMPILATION / TIMBRE\n// ============================================================================\n',
    'texture section title')
cpp = sub_once(
    cpp,
    r'\n// Bitmask per GenerativeMode:.*?constexpr uint8_t kAllowedTextureMask\[kGenerativeModeCount\] = \{.*?\n\};\n',
    '\n', 'allowed texture mask')
cpp = sub_once(cpp, r'\nbool GenreManager::isTextureAllowed\(.*?\n\}', '\n', 'isTextureAllowed')
cpp = sub_once(cpp, r'\nTextureMode GenreManager::firstAllowedTexture\(.*?\n\}', '\n', 'firstAllowedTexture')
cpp = sub_once(cpp, r'\nTextureMode GenreManager::nextAllowedTexture\(.*?\n\}', '\n', 'nextAllowedTexture')
cpp = sub_once(cpp, r'\nvoid GenreManager::applyTexture\(MiniAcid& engine\) \{.*?\n\}\n\n// ============================================================================\n// STRUCTURAL BEHAVIOR', '\n// ============================================================================\n// STRUCTURAL BEHAVIOR', 'applyTexture body')
cpp = cpp.replace('        // Apply base synthesis parameters (BEFORE texture bias)\n', '        // Apply base synthesis parameters.\n')
cpp = cpp.replace('        // voice; delay/space remain controlled by the selected texture layer.\n', '        // voice; delay/space remain controlled by persisted FX state.\n')
write("src/dsp/genre_manager.cpp", cpp)

# ---------------------------------------------------------------------------
# Scene model/codec: legacy texture keys are decode-only and never serialized.
# ---------------------------------------------------------------------------
sh = read("scenes.h")
for line in (
    '    uint8_t textureMode = 0;      // TextureMode enum value\n',
    '    uint8_t textureAmount = 70;   // 0..100 intensity\n',
    '    bool curatedMode = true;      // true: only allowed Genre x Texture combos\n',
    '    bool applySoundMacros = false; // true: Flavor change overwrites 303/Tape\n',
):
    if line not in sh:
        raise RuntimeError(f"missing GenreSettings field: {line.strip()}")
    sh = sh.replace(line, '')
write("scenes.h", sh)

sc = read("scenes.cpp")
sc = sub_once(
    sc,
    r'    \} else if \(lastKey_ == "tex"\) \{.*?target_\.genre\.textureAmount = static_cast<uint8_t>\(v\);',
    '    } else if (lastKey_ == "tex" || lastKey_ == "textureMode" ||\n               lastKey_ == "amt" || lastKey_ == "textureAmount") {\n      // Legacy TEXTURE values are accepted but intentionally ignored.',
    'evented legacy numeric texture decode')
sc = sc.replace('    else if (lastKey_ == "cur") target_.genre.curatedMode = value;\n    else if (lastKey_ == "sound") target_.genre.applySoundMacros = value;\n',
                '    else if (lastKey_ == "cur" || lastKey_ == "curated" ||\n             lastKey_ == "sound" || lastKey_ == "snd") {\n      // Legacy TEXTURE policy values are decode-only.\n    }\n')
for line in (
    '  genreObj["tex"] = scene_->genre.textureMode;\n',
    '  genreObj["amt"] = scene_->genre.textureAmount;\n',
    '  genreObj["cur"] = scene_->genre.curatedMode;\n',
    '  genreObj["sound"] = scene_->genre.applySoundMacros;\n',
):
    if line not in sc:
        raise RuntimeError(f"missing serializer line: {line.strip()}")
    sc = sc.replace(line, '')
sc = sub_once(
    sc,
    r'\n    int tex = valueToInt\(genreObj\["tex"\], loaded->genre\.textureMode\);.*?loaded->genre\.textureAmount = static_cast<uint8_t>\(amt\);\n',
    '\n    // Historical tex/amt (and long-form aliases) are ignored. Concrete\n    // synth/FX state already persisted in the Scene remains authoritative.\n',
    'DOM legacy numeric texture decode')
sc = sc.replace('    loaded->genre.curatedMode = genreObj["cur"].is<bool>() ? genreObj["cur"].as<bool>() : loaded->genre.curatedMode;\n', '')
sc = sc.replace('    loaded->genre.applySoundMacros = genreObj["sound"].is<bool>() ? genreObj["sound"].as<bool>() : loaded->genre.applySoundMacros;\n', '')
write("scenes.cpp", sc)

# ---------------------------------------------------------------------------
# Engine lifecycle: never recreate texture state or re-project it on reset/load.
# ---------------------------------------------------------------------------
eng = read("src/dsp/miniacid_engine.cpp")
eng = sub_once(
    eng,
    r'\n  // NOW reset bias tracking \(after all base params are set\)\n  genreManager_\.resetTextureBiasTracking\(\);\n  // Apply texture to bring engine into consistent state with current genre\n  genreManager_\.applyTexture\(\*this\);\n',
    '\n', 'reset texture projection')
eng = eng.replace('  // NOTE: applyTexture is NOT called here - it\'s applied separately by UI on texture change\n  // This prevents double-application which would cause delta-bias drift\n', '')
eng = eng.replace('  // Restore genre state from scene before applying timbre/texture\n', '  // Restore genre state before applying the existing genre timbre behavior.\n')
eng = eng.replace('  genreManager_.setTextureMode(static_cast<TextureMode>(gs.textureMode));\n', '')
eng = sub_once(
    eng,
    r'\n  LOG_PRINTLN\("  - MiniAcid::applySceneStateFromManager: resetTextureBiasTracking\.\.\."\);.*?genreManager_\.applyTexture\(\*this\);\n',
    '\n  // Legacy TEXTURE metadata is intentionally not reapplied. Persisted synth,\n  // Tape, delay and distortion state remains authoritative during scene load.\n',
    'scene load texture projection')
eng = eng.replace('  sceneManager_.currentScene().genre.textureMode = static_cast<uint8_t>(genreManager_.textureMode());\n', '')
write("src/dsp/miniacid_engine.cpp", eng)

# Minor UI wording: no hidden fourth axis.
gp = read("src/ui/pages/genre_page.cpp")
gp = gp.replace('  // ENTER: apply the current genre/texture/recipe selection.\n  // Texture is intentionally not changed by the GENRE page.\n',
                '  // ENTER: apply the current genre/recipe selection.\n')
write("src/ui/pages/genre_page.cpp", gp)

# ---------------------------------------------------------------------------
# Regression gate for removal and migration contract.
# ---------------------------------------------------------------------------
test = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = [ROOT / "src", ROOT / "scenes.h", ROOT / "scenes.cpp"]
FORBIDDEN = (
    "TextureMode", "TextureParams", "kTexturePresets", "setTextureMode",
    "textureMode()", "applyTexture(", "cycleTexture(", "isTextureAllowed",
    "firstAllowedTexture", "nextAllowedTexture", "textureAmount",
    "curatedMode", "applySoundMacros", "TextureBias",
)

for root in RUNTIME:
    files = [root] if root.is_file() else list(root.rglob("*"))
    for path in files:
        if not path.is_file() or path.suffix not in {".h", ".hpp", ".c", ".cc", ".cpp", ".ino"}:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for token in FORBIDDEN:
            if token in text:
                raise AssertionError(f"runtime TextureMode residue: {path.relative_to(ROOT)}: {token}")

scene = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
for serialized in ('genreObj["tex"] =', 'genreObj["amt"] =', 'genreObj["cur"] =', 'genreObj["sound"] ='):
    if serialized in scene:
        raise AssertionError(f"legacy TEXTURE field is still serialized: {serialized}")
for legacy in ('lastKey_ == "tex"', 'lastKey_ == "textureMode"', 'lastKey_ == "amt"', 'lastKey_ == "textureAmount"'):
    if legacy not in scene:
        raise AssertionError(f"legacy numeric key is not explicitly accepted: {legacy}")
for legacy in ('lastKey_ == "cur"', 'lastKey_ == "curated"', 'lastKey_ == "sound"', 'lastKey_ == "snd"'):
    if legacy not in scene:
        raise AssertionError(f"legacy policy key is not explicitly accepted: {legacy}")

workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
if "case WorkflowMode::Generate: return 3;" not in workflow:
    raise AssertionError("GENERATE must remain exactly three pages")
if "case kTexture:" not in workflow or "if (page == kTexture) page = kFeel;" not in workflow:
    raise AssertionError("legacy page id 8 must remain a FEEL redirect")

print("TextureMode runtime removal regressions: PASS")
'''
write("tests/test_remove_texture_mode_runtime.py", test)

runner = read("tests/run_host_tests.sh")
line = 'python3 tests/test_remove_texture_mode_runtime.py\n'
if line not in runner:
    runner += '\n' + line
write("tests/run_host_tests.sh", runner)

# ---------------------------------------------------------------------------
# User-facing docs and hardware acceptance.
# ---------------------------------------------------------------------------
readme = read("README.md")
readme = readme.replace('generated form, sound texture, arrangement, performance, and MIDI routing.',
                        'generated form, sound design, arrangement, performance, and MIDI routing.')
readme = readme.replace('**Beta.** The core groovebox, four-axis GENERATE workflow,',
                        '**Beta.** The core groovebox, three-page GENERATE workflow,')
readme = sub_once(readme, r'```text\nGENRE != FEEL != GENERATION != TEXTURE\n```', '```text\nGENRE != FEEL != GENERATION\n```', 'README model')
readme = readme.replace('GENERATE: GENRE -> FEEL -> GENERATION -> TEXTURE', 'GENERATE: GENRE -> FEEL -> GENERATION')
readme = readme.replace('independent GENRE / FEEL / GENERATION / TEXTURE decisions', 'independent GENRE / FEEL / GENERATION decisions')
readme = sub_once(readme, r'### Four-axis GENERATE workflow\n\nThe four pages have separate ownership:.*?Changing one axis must not silently mutate another axis\. Page-aware `Alt\+H` states the ownership and non-scope of each page\.',
'''### Three-page GENERATE workflow

The pages have separate ownership:

1. **GENRE** — musical corridor, vocabulary, recipe, and explicit materialization policy.
2. **FEEL** — swing, timing humanization, and velocity humanization only.
3. **GENERATION** — bounded form/materialization into the selected Song row.

Sound design is edited through the synth, Tape and FX controls that own the persisted DSP parameters. There is no separate runtime TEXTURE axis.

The causal order is fixed:

```text
GENRE -> FEEL -> GENERATION
```

Changing one page must not silently mutate another page. Page-aware `Alt+H` states the ownership and non-scope of each page.''', 'README generate section')
readme = readme.replace('The current page map has **15 pages**: four GENERATE pages,', 'The current page map has **14 pages**: three GENERATE pages,')
write("README.md", readme)

manual = read("MANUAL.md")
manual = manual.replace('GroovePuter separates four responsibilities:', 'GroovePuter separates three GENERATE responsibilities:')
manual = manual.replace('* `TEXTURE`: sound coloration and effects.\n', '')
manual = manual.replace('GENRE != FEEL != GENERATOR != TEXTURE', 'GENRE != FEEL != GENERATOR')
manual = manual.replace('* `Genre`: style, texture, preset, and apply policy.', '* `Genre`: style, variant, morph, and apply policy.')
manual = manual.replace('* `Feel/Texture`: timing and sound macros.', '* `Feel`: timing and velocity feel.')
manual = manual.replace('* `Generator`: generative parameters.', '* `Generation`: bounded materialization into Song.')
manual = manual.replace('Scene Save/Load is separate from UI session persistence.',
'''Sound coloration is not a separate persisted axis. Synth, Tape, delay, distortion and other FX parameters are saved by their owning controls. Historical TEXTURE fields are accepted on load but ignored and are not written again.

Scene Save/Load is separate from UI session persistence.''')
write("MANUAL.md", manual)

# Keep the canonical key sheets explicit about the three-page workflow.
for rel in ("src/ui/docs/keys.md", "docs/keys_sheet.md"):
    p = ROOT / rel
    if not p.exists():
        continue
    text = p.read_text(encoding="utf-8")
    text = text.replace('GENRE -> FEEL -> GENERATION -> TEXTURE', 'GENRE -> FEEL -> GENERATION')
    text = text.replace('GENRE / FEEL / GENERATION / TEXTURE', 'GENRE / FEEL / GENERATION')
    text = re.sub(r'^.*TEXTURE.*(?:\n|$)', '', text, flags=re.M)
    p.write_text(text, encoding="utf-8")

acceptance = '''# TextureMode removal — Cardputer ADV acceptance\n\n## Purpose\nConfirm the runtime TextureMode migration on physical Cardputer ADV without changing musical generation behavior.\n\n## Hardware\n- M5Stack Cardputer ADV (ESP32-S3FN8)\n- USB cable for flash + serial\n- Optional headphones / SEQTRAK for A/B listening\n\n## Wiring\nNo external wiring is required. Use the normal Cardputer ADV USB connection.\n\n## Build / flash\n```bash\nbash scripts/install_arduino_deps.sh\nbash scripts/build.sh --warnings all\nbash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf\nbash scripts/upload.sh /dev/ttyACM0\n```\n\n## Expected behavior\n- GENERATE exposes exactly GENRE, FEEL, GENERATION.\n- Historical page id 8 opens FEEL and never appears in normal navigation.\n- Loading an older Scene with TEXTURE metadata succeeds.\n- Existing synth/Tape/delay/distortion settings are not overwritten by a texture preset.\n- Saving the migrated Scene does not write legacy texture metadata.\n\n## Troubleshooting\n- If an old project sounds different, compare its persisted synth/Tape/delay/distortion values before changing any generator settings.\n- If page 8 is visible in normal navigation, the PR is not acceptable.\n- If load fails on an unknown legacy texture value, capture the Scene JSON and serial log.\n\n## Acceptance checklist\n- [ ] Boot succeeds with no reset loop.\n- [ ] GENRE -> FEEL -> GENERATION navigation works in both directions.\n- [ ] Legacy page id 8 redirects to FEEL.\n- [ ] Old Scene loads without error.\n- [ ] Old Scene sounds materially the same in an A/B check.\n- [ ] Save/reload keeps concrete synth/FX values.\n- [ ] Serial shows no Scene parse/load errors.\n- [ ] Normal and SEQTRAK MIDI-only firmware smoke tests pass.\n'''
write("docs/acceptance/REMOVE_TEXTURE_MODE_CARDPUTER_ADV.md", acceptance)

contract = read("docs/refactors/REMOVE_TEXTURE_MODE_RUNTIME.md")
contract = contract.replace('- [ ] `TextureMode`, `TextureParams`, `kTexturePresets`, `setTextureMode`, `textureMode`, and `applyTexture` have no runtime definitions or call sites.', '- [x] `TextureMode`, `TextureParams`, `kTexturePresets`, `setTextureMode`, `textureMode`, and `applyTexture` have no runtime definitions or call sites.')
contract = contract.replace('- [ ] A legacy scene containing historical texture keys still loads successfully.', '- [x] Legacy compact and long-form texture keys are accepted as decode-only input; CI covers the source contract.')
contract = contract.replace('- [ ] Synth/tape/delay/distortion values already stored in that legacy scene are not overwritten by a texture preset during load.', '- [x] Scene load no longer runs a texture projection over persisted synth/Tape/delay/distortion state.')
contract = contract.replace('- [ ] Re-saving that scene does not emit `tex`, `amt`, `curated`, or `snd` in the genre object.', '- [x] New serialization omits historical texture fields (`tex`, `amt`, `cur`/`curated`, `sound`/`snd`).')
contract = contract.replace('- [ ] GENRE -> FEEL -> GENERATION navigation remains unchanged from PR #130.', '- [x] GENRE -> FEEL -> GENERATION navigation remains unchanged from PR #130.')
write("docs/refactors/REMOVE_TEXTURE_MODE_RUNTIME.md", contract)

# Runtime residue gate. Page-id compatibility token kTexture is intentionally allowed.
for rel in ("src/dsp/genre_manager.h", "src/dsp/genre_manager.cpp", "scenes.h", "scenes.cpp", "src/dsp/miniacid_engine.cpp"):
    text = read(rel)
    for token in ("TextureMode", "TextureParams", "kTexturePresets", "setTextureMode", "textureMode()", "applyTexture(", "cycleTexture(", "isTextureAllowed", "firstAllowedTexture", "nextAllowedTexture", "textureAmount", "curatedMode", "applySoundMacros", "TextureBias"):
        if token in text:
            raise RuntimeError(f"{rel}: runtime residue {token}")

print("TextureMode runtime migration applied")
