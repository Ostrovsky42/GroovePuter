# Rhythm Browser candidate audit — limited RB-101/102

Base: `8a64104f9e6aa4feb9cb2eb1de37b470208528ae` (accepted source `85b53e6b77bd167c23cf23fd78e0832ba9e6a0ef`). This is a technical admission experiment, not musical listening or hardware acceptance. No Browser UI, audition, apply, Song mutation or MIDI preview is implemented here.

## Vocabulary

`tests/tools/rhythm_browser_vocab_audit.cpp` enumerates the 24 stable `ReferenceVocabulary` IDs. For each it asks the existing `realizeRhythmPhrase` for one P1 bar, seeds 1–16, phrase ordinal 0, with no Genre/Recipe/density context; then `materializeRhythmPattern` with standard drum bindings and explicit ignoring of BassRhythm, ChordRhythm and MelodicRhythm. All 384 requests produced a non-empty drum pattern. Exact per-ID counts and hit ranges are in [the output](2026-11-06-rhythm-browser-vocabulary-audit.txt), SHA-256 `67f6c3415e2c173019acef11ae19ab2bdc4cd46325a7a0cbe13321df50698e58`.

This demonstrates generic materialization for these inputs only. It does not establish stable audible output for all contexts, group labels, meaningful variant differences, appropriate feel, listener approval or admission of 24 UI entries. The ignored tonal roles can affect musical identity. P2/P3 were not audited as Browser variants and must not be advertised merely because realization levels exist.

Reproduce from the repository root:

```sh
g++ -std=c++17 -Wno-c++20-extensions -I. tests/tools/rhythm_browser_vocab_audit.cpp src/generation/generation_context.cpp src/generation/rhythm/rhythm_catalog.cpp src/generation/rhythm/relationship_resolver.cpp src/generation/rhythm/rhythm_realizer.cpp src/generation/rhythm/reference_vocabulary.cpp src/generation/materialization/pattern_materializer.cpp -o /tmp/rb-vocabulary-audit
/tmp/rb-vocabulary-audit
```

## Atlas exact material

The baseline manifest and `tools/atlas/README.md` identify six compiled recipe IDs 6–11 and P1/P2/P3: Chicago Jack, Rolling Acid, Classic 2-Step, Dark Skippy, Deep Chord and Minimal Space. `AtlasRuntime::materializeDrums` now materializes the drum targets 0–7 into caller-owned scratch without receiving synth destinations. It validates the entire compiled pattern before writing and shares the existing drum event mapping with `applyRecipe`. As with `applyRecipe`, it resets scratch groove and automation; it is **not a Scene commit API**, and future apply must preserve user-owned feel and other state under its transaction policy.

Host tests compare every drum step of all 18 variants against the existing `applyRecipe` path and independently check every compiled drum event and total hit count, verify nonempty patterns, and check invalid recipe/variant refusal without changing the supplied scratch or metadata. They do not establish legal permission to redistribute raw Atlas data or prove physical MIDI playback. The compiler manifest records the source archive SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`; `docs/architecture/ATLAS_VOCABULARY_EXTRACTION.md` distinguishes 51 raw-bundle-eligible patterns from 362 blocked pending rights review. Per-pattern rights/provenance for these 18 need explicit verification before release; existing runtime availability alone is not legal proof.

## Audition and output risks to resolve

`MiniAcid::advanceTick` and `triggerDrumVoice_` read the pending audible drum owner, while `activeDrumPattern` also supplies retriggers. A candidate must reach all these paths coherently, at the existing boundary, with ownership and cleanup; `triggerDrumVoice_` refuses when the Song drum pattern index is negative. Scratch defaults must not silently discard accepted pattern groove/automation during audition or apply: their ownership and preservation must be resolved before wiring playback. Do not claim Song preview in that state without a safe explicit policy.

`src/output/output_ownership.h` sends legacy `SourceClass::Preview` only internally, whereas Pattern/Song is routed to internal and MIDI by default. Explicit INTERNAL/MIDI/LAYER modes have different routing. The requested Browser audition must honor the active drum mode and layout through the existing pattern output and MIDI gate ownership; reusing the old Preview source unmodified would silently suppress MIDI. This is a source audit, not verified audio/MIDI parity. A bounded RB-301 spike and tests remain mandatory before wiring a UI that claims to play.

## Focused verification

- `test_atlas_runtime.cpp` and `atlas_runtime.cpp`: GCC 13.3.0, `-Wall -Wextra -Werror`, passed; GCC ASan/UBSan passed with leak detection disabled in the sandbox. Clang was not installed. Existing `tests/test_source_regressions.py` and `tests/test_atlas_sound_profile.py` passed.
- `bash tests/run_output_ownership_tests.sh` passed the existing ownership, queue, Scene persistence and source-contract checks; log SHA-256 `557cde7c21108a2dcf20861bc7a7a87f74f2053bd2a71198870910d898da959d`. This does not exercise the future Browser.
- `bash scripts/build.sh` passed on the source of this change: program 1,651,406 bytes, globals 185,788 bytes. App SHA-256 `cc55209d383de8fb6e9f50df14ff47bf0e4be2126d992e3d8477e29f278bcded`; build log SHA-256 `b2efb105e3524addd94f1bae85a6f388cbfd7ef429b251a64e25faddbb5c58f2`. The existing provisional DRAM gate passed at 185,788 / 191,488 bytes, with policy items 5–7 still pending. The new scratch API is not wired into the application and may be removed by the linker, so this is not a final Browser memory measurement. Logs are in the local `umg-freeze-20261106` evidence directory.
- The full host/core suites were not rerun for this limited slice; the previous baseline results apply only to `85b53e6b`. Hardware and human listening remain unverified.
