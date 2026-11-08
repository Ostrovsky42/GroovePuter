# G2 seed contract finding — 2026-10-26

Status: protocol mismatch confirmed; amendment requested, NOT APPROVED. Original preregistered JSON and its checksum are unchanged. No replacement corpus has been run.

## Authoritative path

1. `GeneratedPhraseSong::generate` allocates the session attempt for `(genre, recipe, P-level, kLogicalPhraseAttemptChannel)`. Each fresh process starts at ordinal 0.
2. `GeneratedPhraseP1R::phraseIdentityForAttempt` maps this ordinal modulo the existing uint16 identity range.
3. `preparePhraseExecution` receives genre/materialization/identity, not the mode-manager project seed.
4. `resolveStrongRhythmFrozenSelectionForPhraseBars` uses `projectSeedFor(settings, route)`. That hash includes genre, recipe, two compatibility zero bytes and route. It does not read `GrooveboxModeManager::generationSeed`. Realization varies with the attempt ordinal.
5. Existing `docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md` section 5 explicitly defines its evaluation identities as attempt ordinals 0–7. This historical scope does not automatically amend the newer ratified G2 preregistration.

Thus the host probe sets a real seed API, but that seed is ineffective for this specific P1R Legacy route. Eight duplicate Techno outputs are a protocol mismatch, not proof of broken reroll. No production seed plumbing fix is justified from these observations alone.

## Decision requested

Prospectively preregister native identities 0–7 with exact input mapping and unchanged genre/BPM/thresholds, retaining all original failures and artifacts. No post-listening selection, no new laws, no genre substitution. Until explicit approval, do not run a replacement acceptance corpus or relabel the first run PASS. Dub admission remains a separate failing preparation path.

## Focused verification

- `python3 tests/test_0_9_9_phrase_p1r_source_regressions.py`: PASS (source firewall, semantic seam, WHAT contract, heap/publication guards).
- `g++ -std=c++17 -Wall -Wextra -Werror -Wvla -I. tests/test_generation_request_state.cpp -o build/g2-native-workflow/test_generation_request_state` then binary: PASS. This covers session attempt allocation and current defaults; it does not certify music quality or hardware.
