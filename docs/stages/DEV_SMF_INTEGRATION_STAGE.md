# Dev SMF integration stage

## Purpose

Integrate the bounded SMF channel inspector from PR #30 and the isolated runtime SMF track-mute feature from PR #24 into the `dev` branch without modifying `main`.

## Source boundaries

```text
PR #30 feature diff
b0b513f07bf4514e834708d430c38c725ee5ee0c
  -> 719ec5188e2e2c62c2fd2e3e87a3dd45b14e6dde

PR #24 track-mute-only diff
76ef225308fc70267c3945de4feb485214f37937
  -> 35c08dd451ed65125480bf965c8b549a1b0336a0
```

The PR #24 extraction is restricted to:

```text
docs/stages/SMF_TRACK_MUTE_STAGE.md
src/midi/smf_stream.cpp
src/midi/smf_track_mute.h
src/ui/pages/smf_player_page.cpp
tests/test_smf_stream.cpp
```

## Acceptance checklist

```text
[ ] both feature diffs apply three-way on current dev
[ ] host tests pass before publication
[ ] Core regressions run on final dev
[ ] channel inspector and track mute coexist in MIDI Player
[ ] main remains unchanged
```
