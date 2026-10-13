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
# HGS offline source import

The vocabulary importer reads the pinned Fannon/Tonal source. The progression
importer reads the pinned ldrolez `chords.py` as Python syntax data and resolves
the Roman route using the documented behavior of Mingus 0.6.1. It never imports
or executes the upstream generator or resolver.

For progression import, place the pinned repositories as siblings:

```text
source-cache/
  free-midi-chords/   # ldrolez/free-midi-chords at locked commit
  python-mingus/      # bspaans/python-mingus with tag 0.6.1
```

Then run:

```sh
python3 tools/harmony/import_ldrolez_progressions.py \
  --source source-cache/free-midi-chords --out build/hgs-cache/ldrolez-import
```

`progressions.json` accounts for every row from `prog_maj`, `prog_min`, and
`prog_modal`, preserving source list, line, original spelling, and mood labels.
Mood labels remain corpus annotations; they do not infer genre legality.
The importer rejects source hash, resolver hash, or row-count drift.

Mingus 0.6.1 resolves `I7` as a key-derived seventh and `Idom7` as a dominant
seventh; `ii5` is a power fifth. Its `chord_shorthand` does not define `M-5`,
`add9`, or `madd9`, although these occur in the corpus. Six affected full routes
are retained in the explicit quarantine with their source locator and unsupported
suffix. No fallback triad is substituted. The Mingus files are behavioral
references only and are not copied or executed; the full upstream GPL-3.0 license
is included alongside the source notices.

## Validate the combined source manifest

After exporting/importing Fannon vocabulary and importing ldrolez progressions, combine them:

```sh
python3 tools/harmony/import_ldrolez_progressions.py \
  --source build/hgs-cache/free-midi-chords \
  --out build/hgs-cache/ldrolez-import
python3 tools/harmony/validate_harmony_model.py \
  --vocab build/hgs-cache/harmony-model/fannon_vocab.json \
  --vocab-manifest build/hgs-cache/harmony-model/manifest.json \
  --progressions build/hgs-cache/ldrolez-import/progressions.json \
  --progressions-manifest build/hgs-cache/ldrolez-import/manifest.json \
  --out-dir build/hgs-cache/validated-model
```

The resulting `harmony_manifest.json` separates immutable registries and vocabulary from
the progression corpus. Corpus weight is one per unique normalized route; duplicate source
locators and mood labels aggregate, while repeated events inside a route remain ordered.
Fannon contributes vocabulary only and never contributes transition counts. Source locators
and copied license notices remain in provenance; the content checksum excludes filesystem
paths and timestamps.
