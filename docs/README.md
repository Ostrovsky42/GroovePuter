# GroovePuter Documentation

This directory contains both **current product documentation** and a large **historical engineering / research record**.

The distinction matters: a document can be valuable evidence without describing the current UI or release behavior.

## Start here

### User

- [`user/QUICKSTART.md`](user/QUICKSTART.md) — first-session guide for the internal `.33–.35` line; not a published release or hardware-acceptance claim.
- [`../MANUAL.md`](../MANUAL.md) — current internal-line user workflow manual; latest published firmware remains v0.9.19.
- [`../src/ui/docs/keys.md`](../src/ui/docs/keys.md) — canonical Cardputer ADV key map for the internal `.33–.35` development line.
- [`user/MIDI_SETUP.md`](user/MIDI_SETUP.md) — profile boundaries, committed-Song MIDI export and what remains hardware-unverified.
- [`user/TROUBLESHOOTING.md`](user/TROUBLESHOOTING.md) — export, SD and MIDI routing failure guidance.
- [`implementation/0.9.25-0.9.35/HARDWARE_TEST_0_9_35.md`](implementation/0.9.25-0.9.35/HARDWARE_TEST_0_9_35.md) — planned owner acceptance checklist; no tests recorded yet.
- [`SONG_PAGE_QUICKSTART.md`](SONG_PAGE_QUICKSTART.md) — focused Song-page editing reference.
- [`releases/0.9.15-midi-ui-polish-2026-10-02.md`](releases/0.9.15-midi-ui-polish-2026-10-02.md) — external-keyboard / PERFORM routing and UI behavior incorporated into the accepted foundation.
- [`releases/0.9.15-hardware-acceptance.md`](releases/0.9.15-hardware-acceptance.md) — USB Host hardware acceptance procedure and evidence.

The latest packaged GitHub Release is **v0.9.19**. The internal development line has implemented committed-Song MIDI export in `.33`; `.34` productization passed the canonical host suite at `fb300ff5`, and `.35` candidate integration is in progress. The final candidate still requires all acceptance lanes on its exact clean SHA. Its current user workflow remains `GENERATE → KEEP/EDIT → DEVELOP → FORM/SONG → PERFORM → SAVE/LOAD → EXPORT`. `GEN` remains a local panel inside GENRE: `Tab` opens it, and the next `Tab` continues to FEEL.

**v0.9.16 — Foundation Freeze** is the earlier hardware-accepted foundation. For the exact v0.9.19 binary and acceptance boundary, use the [release notes](https://github.com/Ostrovsky42/GroovePuter/releases/tag/v0.9.19). The `.33–.35` line is unpublished and has not been hardware-accepted.

`reference/EXTERNAL_MIDI_COMPATIBILITY.md` is preserved compatibility evidence from the USB-MIDI-device line. It does not replace the later USB Host keyboard acceptance evidence.

### Developer

- [`PRODUCT_POSITIONING.md`](PRODUCT_POSITIONING.md) — product boundaries and priority direction.
- [`implementation/0.9.25-0.9.35/RELEASE_READINESS.md`](implementation/0.9.25-0.9.35/RELEASE_READINESS.md) — current unpublished package pipeline, artifact boundaries and remaining candidate gates.
- [`implementation/0.9.25-0.9.35/MIDI_EXPORT.md`](implementation/0.9.25-0.9.35/MIDI_EXPORT.md) — committed-Song SMF export contract and verification evidence.
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
