# Harmony Atlas H1 Renderer Semantics Boundary

**Status:** normative H1 research boundary  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Runtime impact:** none

## Why this boundary exists

H1 normalizes the authored Roman-numeral notation in the pinned `free-midi-chords` source. It does **not** claim that its notation-level `triad_class` or extension labels are a bit-for-bit reconstruction of the pitch sets produced by every historical build of the source pack.

The pinned source has a separate renderer dependency:

```text
src/chords2midi/c2m.py
    -> mingus.core.progressions.to_chords(...)
```

The pinned Makefile builds with:

```text
env PYTHONPATH=python-mingus/ python3 gen.py
```

and its `check` target only requires a local `python-mingus/README.md`, instructing the user to clone `https://github.com/ldrolez/python-mingus.git` when missing. The `free-midi-chords` commit therefore does not itself identify an exact `python-mingus` commit.

The repository `requirements.txt` is also non-exact:

```text
mingus>=0.6.1
```

## H1 claim

H1 may claim:

```text
AUTHORED_ROMAN_NOTATION
    -> deterministic lexical parse
    -> degree + accidental
    -> Roman case
    -> explicit suffix decomposition
    -> loss-aware source spelling
```

H1 may **not** claim:

```text
NORMALIZED_TOKEN
    == historically rendered MIDI pitch set
```

without separately pinning or otherwise proving the renderer dependency used for that build.

Consequently:

- `triad_class` is a notation-level normalized class, not a measured MIDI chord label;
- generic `7` and `9` remain semantically conservative (`seventh_flavor=UNSPECIFIED`);
- explicit `dom7` and `M7` remain distinct;
- no generated MIDI files are decoded to redefine H1 notation semantics;
- no unpinned `mingus` installation is executed by H1 CI.

## Future rendered-semantics validation

If a later research question needs exact rendered pitch-set equivalence, it must use a separate reproducible checkpoint with one of:

1. an exact `python-mingus` commit proven to be the intended renderer revision; or
2. a pinned generated MIDI pack/archive whose artifact hash is treated as the evidence source.

That future validation must remain separate from H1 canonical notation normalization and must not silently rewrite H1 source identities.
