# GroovePuter Documentation

This directory contains both **current product documentation** and a large **historical engineering / research record**.

The distinction matters: a document can be valuable evidence without describing the current UI or release behavior.

## Start here

### User

- [`../README.md`](../README.md) — product overview and current candidate status.
- [`../src/ui/docs/keys.md`](../src/ui/docs/keys.md) — canonical current Cardputer ADV key map.
- [`SONG_PAGE_QUICKSTART.md`](SONG_PAGE_QUICKSTART.md) — focused Song-page editing reference.
- [`releases/0.9.15-midi-ui-polish-2026-10-02.md`](releases/0.9.15-midi-ui-polish-2026-10-02.md) — current external-keyboard / PERFORM routing and UI behavior.
- [`releases/0.9.15-hardware-acceptance.md`](releases/0.9.15-hardware-acceptance.md) — current USB Host hardware acceptance procedure.

`MANUAL.md` in the repository root is an older 0.9.1-era manual and must not be treated as the authoritative key map for the current candidate.

`reference/EXTERNAL_MIDI_COMPATIBILITY.md` is preserved compatibility evidence from the USB-MIDI-device line. It does not describe the full current 0.9.15 USB Host keyboard path.

### Developer

- [`PRODUCT_POSITIONING.md`](PRODUCT_POSITIONING.md) — product boundaries and priority direction.
- [`ci/CI_ORCHESTRATION.md`](ci/CI_ORCHESTRATION.md) — ownership of the seven canonical GitHub Actions pipelines.
- [`architecture/`](architecture/) — architectural contracts and design notes.
- [`testing/`](testing/) — acceptance and regression procedures.
- [`releases/`](releases/) — release-specific evidence and hardware records.
- [`../PLAN.md`](../PLAN.md) — repository-level execution/product plan where applicable.

### Research

- [`0.9.14/`](0.9.14/) — current musical-development / semantic work for the 0.9.14 line.
- [`research/`](research/) — cross-cutting research records.
- [`gf2/`](gf2/) — GF2 semantic-development research.
- [`stages/`](stages/) — historical implementation stages and acceptance checkpoints.
- older top-level `0.9.x` documents — preserved historical checkpoints and contracts.

## Documentation policy

Use this order of authority when two documents appear to disagree:

1. **exact candidate/release source and tests**;
2. **exact release/hardware acceptance document**;
3. **canonical current key map** (`src/ui/docs/keys.md`);
4. current product documentation;
5. historical stage/research documents.

A historical file is not deleted merely because the implementation moved on. It should remain available for provenance, but it should not silently masquerade as current product documentation.

## Current documentation debt

The repository is being productized incrementally. The desired public-facing shape is roughly:

```text
docs/
  user/
    QUICKSTART.md
    MANUAL.md
    MIDI.md

  developer/
    BUILDING.md
    ARCHITECTURE.md
    TESTING.md

  research/
    ...

  archive/
    ...
```

The repository is **not** being mass-moved into that layout in one change because hundreds of historical links and evidence paths already exist. This index is the first compatibility-safe facade: navigation becomes understandable before physical reorganization begins.

## What belongs where

**User-facing docs** should answer actions a musician takes: install, play, generate, keep, edit, develop, save, connect MIDI.

**Developer docs** should explain ownership, build/test procedures, realtime constraints, storage and release gates.

**Research docs** may describe experiments, unknowns, rejected models, evidence and purpose-specific semantic claims. They are allowed to be detailed, but should not be presented as shipped behavior without release evidence.
