# GroovePuter Documentation

This directory contains both **current product documentation** and a large **historical engineering / research record**.

The distinction matters: a document can be valuable evidence without describing the current UI or release behavior.

## Start here

### User

- [`user/QUICKSTART.md`](user/QUICKSTART.md) — **0.9.18 First Five Minutes**: the shortest path from boot to a generated and developed take.
- [`../MANUAL.md`](../MANUAL.md) — current 0.9.18 user workflow manual.
- [`../src/ui/docs/keys.md`](../src/ui/docs/keys.md) — canonical current Cardputer ADV key map.
- [`SONG_PAGE_QUICKSTART.md`](SONG_PAGE_QUICKSTART.md) — focused Song-page editing reference.
- [`releases/0.9.15-midi-ui-polish-2026-10-02.md`](releases/0.9.15-midi-ui-polish-2026-10-02.md) — external-keyboard / PERFORM routing and UI behavior incorporated into the accepted foundation.
- [`releases/0.9.15-hardware-acceptance.md`](releases/0.9.15-hardware-acceptance.md) — USB Host hardware acceptance procedure and evidence.

The current tagged source baseline is **v0.9.18**. It adds genre idioms, phrase ideas, DnB, GRAB, FORM and the `GENRE -> GEN -> FEEL` generation workflow. The tag points at the merged 0.9.18 source, but there is no separate GitHub Release object/package for v0.9.18.

**v0.9.16 — Foundation Freeze** remains the latest packaged GitHub Release and the hardware-accepted foundation. The first-user documentation above follows the newer tagged source while keeping release/hardware claims tied to their exact evidence.

`reference/EXTERNAL_MIDI_COMPATIBILITY.md` is preserved compatibility evidence from the USB-MIDI-device line. It does not replace the later USB Host keyboard acceptance evidence.

### Developer

- [`PRODUCT_POSITIONING.md`](PRODUCT_POSITIONING.md) — product boundaries and priority direction.
- [`ci/CI_ORCHESTRATION.md`](ci/CI_ORCHESTRATION.md) — ownership of the seven canonical GitHub Actions pipelines.
- [`architecture/`](architecture/) — architectural contracts and design notes.
- [`testing/`](testing/) — acceptance and regression procedures.
- [`releases/`](releases/) — release-specific evidence and hardware records.
- [`../PLAN.md`](../PLAN.md) — repository-level execution/product plan where applicable.

### Research

- [`0.9.14/`](0.9.14/) — musical-development / semantic work incorporated into the Foundation Freeze line.
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

## Product-facing documentation shape

The compatibility-safe public facade is converging toward:

```text
docs/
  user/
    QUICKSTART.md
    ...

  developer/
    ...

  research/
    ...

  archive/
    ...
```

The repository is **not** being mass-moved into that layout in one change because hundreds of historical links and evidence paths already exist. Navigation is being made understandable before physical reorganization.

## What belongs where

**User-facing docs** answer actions a musician takes: install, play, generate, keep, edit, develop, save and connect MIDI.

**Developer docs** explain ownership, build/test procedures, realtime constraints, storage and release gates.

**Research docs** may describe experiments, unknowns, rejected models, evidence and purpose-specific semantic claims. They are allowed to be detailed, but should not be presented as shipped behavior without release evidence.
