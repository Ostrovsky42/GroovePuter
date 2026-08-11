# GroovePuter 0.9.1 Research Roadmap

Status: planning / research only  
Integration branch: `dev_0.9.1`  
Planning base: `12f7ddb9b6e8bfb8bdea5308066cea4d2d303451`

## Purpose

Define three bounded research directions for GroovePuter 0.9.1 without reopening the current release candidate as an uncontrolled feature branch:

1. Harmony Atlas from `ldrolez/free-midi-chords`.
2. Melodic Corpus analysis from `AyHa1810/touhou-midi-collection`.
3. MIDI Player architecture audit inspired by `Zephkek/MIDIPlusPlus`.

These tracks are intentionally independent. Research may run in parallel, but production integration must be reviewed and merged through separate PRs.

## Release boundary

The existence of this roadmap does not make any of these features part of the current hardware candidate.

Before new 0.9.1 musical features are promoted, the current release blockers must be resolved and a new unchanged candidate SHA must pass the normal host, SDL, Cardputer ADV, fixed-DRAM, SEQTRAK MIDI-only and hardware-listening gates.

Every candidate that appears complete must then pass three consecutive clean reviews on one unchanged SHA:

1. scope / ownership / production boundary;
2. runtime / musical semantics / contracts;
3. CI / build / memory / documentation / handoff.

Any finding or SHA movement resets the count to 0/3.

---

# Track A — Harmony Atlas

Source project: <https://github.com/ldrolez/free-midi-chords>

License: MIT. External copyright and license notices must be retained where required.

## Goal

Build a compact evidence-backed harmonic vocabulary for the existing Stage 15 pipeline instead of adding another runtime chord generator.

The source repository already separates useful concepts that match GroovePuter ownership:

- Roman-numeral chord progression identity;
- Major / Minor / Modal families;
- mood tags;
- rhythmic chord styles such as `long`, `pop`, `pop2`, `hiphop2` and `soul`.

Target ownership remains:

```text
ChordProgression -> what harmonic degrees occur
ChordRhythm      -> when harmonic events occur
Tonal Projector  -> absolute MIDI projection
materialization  -> physical Synth A/B pattern writes
```

No Harmony Atlas code may become a second scale mapper or absolute-pitch owner.

## Research deliverables

1. Offline extractor for progression definitions and mood tags.
2. Normalized progression schema based on degree identity, not absolute MIDI notes.
3. Separate normalized chord-rhythm schema.
4. Structural fingerprint and dedup report.
5. Frequency / diversity report by Major, Minor and Modal family.
6. Curated candidate vocabulary small enough for embedded use.
7. Provenance file recording source repository revision and license.
8. Host tests for normalization, deduplication and deterministic export.

## Initial candidate schema

```text
ProgressionDefinition
  id
  family            Major | Minor | Modal
  degree_count
  degrees[]
  chord_qualities[]
  mood_tags[]
  cadence_flags
  source_id

ChordRhythmDefinition
  id
  event_count
  durations[]
  rest_mask
  continuation_mask
  source_style
```

The exact runtime representation is not frozen by this document.

## Non-goals

- importing 13,000 MIDI files into firmware;
- storing every source progression;
- adding a second Tonal Projector;
- choosing progression by raw mood string at audio rate;
- changing current Stage 15 ownership during extraction work.

## Acceptance gate

Harmony Atlas research is acceptable when:

- extraction is reproducible from a pinned source revision;
- no absolute source key is required for the normalized result;
- duplicate and near-duplicate progressions are reported;
- mood tags are metadata/selection evidence, not hidden musical ownership;
- generated runtime pitches still pass exclusively through the shared Tonal Projector;
- no source MIDI payload is required on Cardputer storage.

---

# Track B — Melodic Corpus

Research corpus: <https://github.com/AyHa1810/touhou-midi-collection>

Usage boundary: RESEARCH ONLY.

The collection contains MIDI material from multiple creators and arrangements, and its README describes credit requirements and additional restrictions for some material. GroovePuter must not redistribute these MIDI files, embed copied melodies, or ship source-derived note sequences.

## Goal

Extract abstract melodic behavior statistics that can inform a future Melody Grammar without copying identifiable musical content.

The research target is behavior, not songs.

## Metrics to extract

- melodic interval distribution;
- scale-degree transition distribution after tonal normalization;
- repeated-note probability;
- step / third / leap ratios;
- direction after large leaps;
- contour classes;
- register and register-change statistics;
- note-density distributions;
- phrase-length candidates;
- motif recurrence distance;
- exact vs transformed motif recurrence;
- onset relationship to bass / harmony where tracks are identifiable;
- voice-leading motion statistics;
- cadence-adjacent melodic behavior.

## Required privacy/copyright boundary

Research exports must contain aggregated or abstracted information only.

Do not commit:

- source MIDI files;
- full source melodies;
- long exact note sequences;
- reconstructed song-specific motifs;
- filenames as runtime musical IDs.

A useful research artifact is a probability table, histogram, transition matrix, contour class or anonymous structural fingerprint.

## Proposed research outputs

```text
research/melody/
  corpus_manifest.csv
  aggregate_intervals.csv
  degree_transitions.csv
  contour_classes.csv
  motif_recurrence.csv
  phrase_statistics.csv
  report.md
```

The paths above describe the intended research artifact shape; they are not created by this roadmap commit.

## Production integration gate

A future Melody Grammar may consume only a curated, human-reviewed abstraction derived from the corpus.

It must continue to emit melodic intent/contour/degree behavior. Absolute MIDI projection remains owned by the shared Tonal Projector.

---

# Track C — MIDI Player Audit

Reference project: <https://github.com/Zephkek/MIDIPlusPlus>

License: GPLv3.

## License boundary

MIDIPlusPlus is an architecture/reference source only for GroovePuter's MIT codebase.

Do not copy or adapt GPL implementation code into GroovePuter. Any improvements must be independently implemented from GroovePuter requirements and MIDI/SMF specifications.

## Goal

Audit the existing GroovePuter MIDI Player against the failure classes already relevant on Cardputer and SEQTRAK:

- event preparation vs realtime dispatch;
- tempo-map handling;
- time-signature handling;
- NoteOn / NoteOff lifetime correctness;
- sustain state;
- track mute and solo behavior;
- seek / rewind / restart state reconstruction;
- playback-speed changes;
- AllNotesOff / panic behavior;
- track state after transport jumps;
- stuck-note prevention;
- bounded memory use on Cardputer ADV.

## Audit questions

1. Is SMF parsing separated cleanly from realtime event dispatch?
2. Are tempo changes converted into a deterministic time/tick map before playback?
3. Can muting a track leave active notes sounding?
4. Does unmute resume only future events rather than replay stale note-ons?
5. Does seek/restart reconstruct controller and sustain state safely?
6. Are NoteOff events retained even under filtering/mute operations?
7. Does every stop/error/pathological file path end in deterministic AllNotesOff?
8. Can event storage be bounded before playback begins?
9. Are track mute/solo semantics identical in UI and playback core?
10. Can the player remain MIDI-only without allocating unrelated audio subsystems?

## Deliverables

1. Current-flow diagram for GroovePuter SMF loading and playback.
2. Failure-mode matrix with reproducible host tests.
3. Memory budget for event storage on Cardputer ADV.
4. Independent fixes in small PRs, one behavior change per PR where practical.
5. Updated MIDI Player user documentation after runtime behavior is frozen.

---

# Priority and sequencing

## 0. Release blocker cleanup

Complete the current release-blocker work before using hardware listening to evaluate new research features.

Research branches may proceed in parallel because they do not change production behavior.

## 1. Harmony Atlas

Highest immediate musical leverage because Stage 15 already has explicit ChordRhythm, ChordProgression and Tonal Projector ownership.

First production candidate should be a small curated progression vocabulary, not a large imported corpus.

## 2. MIDI Player audit

Can proceed largely independently from generation work and should focus on correctness, transport safety and bounded memory rather than new UI.

## 3. Melodic Corpus

Begin as offline analysis. Do not wire it into firmware until the current Melody ownership and Tonal Projector path are stable and the extracted abstractions have been reviewed for musical usefulness and source leakage.

---

# Branch and PR policy

`dev_0.9.1` is the integration line.

Use separate branches/PRs for:

```text
research/harmony-atlas
research/melodic-corpus
fix/midi-player-audit
```

Actual branch names should follow the repository's dated `agent/...` convention when work begins.

Rules:

- no research corpus commits directly to `dev_0.9.1`;
- no external MIDI archive in firmware or normal repository history;
- no GPL implementation code copied into the MIT codebase;
- no new pitch projector;
- no new hidden generation owner;
- no merge based only on host tests when hardware behavior is affected;
- production changes require an explicit acceptance checklist and three clean reviews on the final unchanged SHA.

# 0.9.1 horizon

The intended 0.9.1 direction is convergence, not feature accumulation:

```text
release correctness
      -> ownership cleanup
      -> Harmony Atlas evidence
      -> MIDI Player hardening
      -> Melodic Grammar research
```

The success criterion is not the number of new generators. It is a smaller number of clearly owned musical systems backed by stronger evidence and reproducible tests.
