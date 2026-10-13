# Offline harmonic vocabulary tools

These tools import musical vocabulary as deterministic data. They do not run or copy either
source project's MIDI generation code into GroovePuter. No generated MIDI pack is stored here.

## Pinned inputs

`source_lock.json` pins the Fannon chord/scale pack and ldrolez progression source by full
repository commit and SHA-256 for the exact files used. It also records the resolved Tonal.js
packages and their licenses. Full notices are included under `licenses/`.

The Fannon importer reads only `chordTypesVariant1` from `src/generateChords.ts` and
`scaleTypesVariant1` from `src/generateScales.ts`, then asks the pinned Tonal.js version to
resolve their formulas at root C. It does not execute those TypeScript files: their top-level
code creates and writes MIDI files. Export variants, concrete roots and octave spellings are
not statistical observations. Formula identity is the sorted 12-bit pitch-class interval set.

## Reproduce the Fannon export

Python processing uses the system Python 3 standard library; no virtual environment or Python
package is needed. Resolve the pinned checkout and npm lock only under the ignored build cache:

```sh
REPO_ROOT="$(git rev-parse --show-toplevel)"
FANNON_SOURCE="${REPO_ROOT}/build/hgs-cache/fannon-midi-chords-and-scales-pack"
mkdir -p "${REPO_ROOT}/build/hgs-cache"
gh repo clone Fannon/midi-chords-and-scales-pack "${FANNON_SOURCE}"
git -C "${FANNON_SOURCE}" checkout 352f28004a3dc0f1a130682b925c4673f8a3d13b
(cd "${FANNON_SOURCE}" && npm ci --ignore-scripts --no-audit --no-fund)
(cd "${FANNON_SOURCE}" && ./node_modules/.bin/ts-node \
  "${REPO_ROOT}/tools/harmony/export_fannon_vocab.ts" \
  --source "${FANNON_SOURCE}" \
  --out "${REPO_ROOT}/tests/fixtures/harmony/fannon_vocab.json")
python3 tools/harmony/import_fannon_vocab.py \
  --source "${FANNON_SOURCE}" \
  --export-json "${REPO_ROOT}/tests/fixtures/harmony/fannon_vocab.json" \
  --out "${REPO_ROOT}/build/hgs-cache/harmony-model"
```

Run the block from the worktree root. The exporter validates source commit, file digests, cardinality and
resolved package versions before writing the JSON.

The importer verifies the same source metadata, normalizes enharmonic/root variants to formula
IDs, merges source aliases/locators, and writes sorted content plus a manifest checksum. Unknown
formulas receive no ID; they appear in `quarantine` until a stable registry entry is explicitly
added. Content checksums do not contain timestamps.

## Stable registries and runtime boundary

`quality_registry.json` and `scale_registry.json` are append-only ID registries. IDs 0..9 remain
the SP-1 runtime bootstrap prefix. New Fannon formulas are assigned explicit IDs after that
prefix for offline vocabulary coverage; this task does not expand the runtime candidate set.
`build_bootstrap_tables.py` emits only the declared `runtime_bootstrap_count` prefix, preserving
the current generated firmware table byte-for-byte.

Run the fixture checks with:

```sh
python3 tests/test_harmony_vocab_import.py
python3 tests/test_harmonic_registry_generation.py
```

The pinned ldrolez `chords.py` is intentionally not imported or executed here. Its literals,
Roman suffixes, and source dialect are handled in HGS2-1b using AST/data-only extraction.
