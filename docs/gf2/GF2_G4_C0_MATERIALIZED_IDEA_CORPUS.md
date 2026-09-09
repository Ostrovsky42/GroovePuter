# GF2 G4-C0 — Materialized Musical-Idea Corpus

Status: **RESEARCH CHECKPOINT — NO PRODUCTION CHANGES**

## 1. Question

G4-C0 answers one question before any 0.9.11 production design:

> Can the current generator materialize genuinely different musical ideas inside one genre while preserving genre identity, or does semantic/combinatorial diversity collapse after production dimensions are removed?

The checkpoint deliberately does **not** introduce a `MusicalIdea` owner, new schema, new genre, new recipe, new parameter, new UI control, or new generation behavior.

Pilots:

- Acid
- House
- Dub Techno
- Drum & Bass

Research base:

`integration/20260909-0.9.10-pattern-phrase-runtime-ui-gf2`

`fcd0d77da5ed6ef38419547477ab26e77ec6ff26`

Accepted exact-head corpus evidence before this report:

- research SHA: `128c7eb4a4b80f03d1e858ba76c3e9ad5d63e511`
- workflow: `GF2 G4-C0 materialized corpus`
- run: `34325672274`
- job: `102382238832`
- result: **GREEN**
- deterministic repeat: **byte-identical summary**
- artifact: `g4-c0-materialized-corpus`, ID `10093803369`
- artifact digest: `sha256:042f7ebfedaf0ebaf9fea10f0a75e6a17e2d4acd042e8f23b7a42bd2c3660485`

The workflow tracks this report path as well, so the final report head is intended to receive the same exact-head gate.

---

## 2. Method

The corpus does not reproduce generation logic in Python. It executes the production materialization seam:

```text
resolveStrongRhythmFrozenSelection()
        ↓
migrateStrongRhythmFrozenMaterial()
        ↓
DrumPatternSet + SynthPattern A/B
```

For Phrase measurements it executes:

```text
preparePhraseExecution()
        ↓
materializePreparedPhraseBar()
```

Corpus size for one-bar material:

```text
4 genres
× 128 phrase identities
× P1 / P2 / P3
= 1536 production materializations
```

Every one of the 1536 cases materialized successfully.

For every identity, P1/P2/P3 retained the same composition semantic fingerprint:

```text
Acid       128 / 128 stable
House      128 / 128 stable
Dub Techno 128 / 128 stable
DnB        128 / 128 stable
```

This is direct evidence that P1/P2/P3 are realization/transformation levels of one frozen selection, not new musical-idea selection.

### 2.1 Four observation layers

The same material is observed at four progressively stricter layers.

#### SEMANTIC

The selected composition tuple:

```text
rhythm archetype
bass rhythm
chord rhythm
progression
melodic rhythm
motif shape
phrase law / bars
harmonic rate
secondary role
```

This measures what the generator *says it selected*.

#### SURFACE

Existing GF2 material fingerprint, including physical expression such as velocity, accents, timing, FX and absolute pitch.

This measures final material difference, but intentionally over-counts differences that are not new ideas.

#### ROLE STRUCTURE

Production dimensions are removed while retaining structural role information:

```text
drum lane attack masks
synth attack masks
synth lifetime/occupancy
relative pitch motion
```

Absolute transposition, FEEL, velocity, FX and instrument identity are not idea witnesses here.

#### CLICK STRUCTURE

Strict first-filter projection:

> Replace every remaining sound with the same-pitch click.

Only real attack times survive. Role identity, pitch, velocity, accent, FX and timbre disappear.

This is a **stress test**, not a complete genre classifier. Equal click masks can still hide different kick↔bass relationships, note lifetimes and phrase functions. Conversely, a semantic identity that repeatedly collapses at ROLE or CLICK level is strong evidence of false diversity.

### 2.2 Continuation correction

The first research pass incorrectly treated every `SynthStep.note >= 0` as a new attack. On this exact base, `TonalPatternAdapter` represents a continuation by copying the active note into a later step with `slide=true`.

The accepted measurement therefore separates:

```text
ATTACK    = note >= 0 && !slide
OCCUPANCY = note >= 0
```

For these four pilot profiles on this base, idea-level bass articulation resolves to Plain and chord/melodic adaptation does not supply a slide-into-onset mask, so `slide=true` is valid as continuation occupancy for this corpus.

The full corpus was rerun after this correction and remained deterministic.

---

## 3. Materialized diversity

### 3.1 One-bar corpus

| Genre | Level | Semantic unique | Surface unique | Role unique | Click unique | Four-floor kick |
|---|---:|---:|---:|---:|---:|---:|
| Acid | P1 | 127 | 115 | 115 | 30 | 72 / 128 |
| Acid | P2 | 127 | 120 | 114 | 18 | 72 / 128 |
| Acid | P3 | 127 | 120 | 92 | **2** | 72 / 128 |
| House | P1 | 127 | 113 | 113 | 22 | 91 / 128 |
| House | P2 | 127 | 126 | 125 | 31 | 91 / 128 |
| House | P3 | 127 | 128 | 124 | 37 | 91 / 128 |
| Dub Techno | P1 | 126 | 102 | 101 | 23 | 28 / 128 |
| Dub Techno | P2 | 126 | 112 | 105 | 24 | 28 / 128 |
| Dub Techno | P3 | 126 | 119 | 79 | 9 | 28 / 128 |
| DnB | P1 | 126 | 126 | 126 | 24 | 0 / 128 |
| DnB | P2 | 126 | 126 | 125 | 21 | 0 / 128 |
| DnB | P3 | 126 | 126 | 123 | 12 | 0 / 128 |

The key result is not a single diversity number. It is the **distance between layers**.

Examples:

```text
Acid P3       127 semantic → 92 role → 2 click
House P2      127 semantic → 125 role → 31 click
DubTechno P3  126 semantic → 79 role → 9 click
DnB P3        126 semantic → 123 role → 12 click
```

The engine has real structural capacity: ROLE diversity remains high for most pilots. But semantic tuple count substantially overstates the number of materially distinct structural statements.

### 3.2 Direct false-diversity witnesses

The corpus produced three useful kinds of witness.

#### Surface different, role structure same

Same structural idea, different physical realization.

Acid identity 1 P2 vs P3:

```text
same semantic identity
same role fingerprint
different surface fingerprint
```

This is expected variation, not a new musical idea.

#### Semantic different, role structure same

Different selected labels, same material structural result.

House identity 12 P1 vs identity 13 P1:

```text
different chord/progression/motif/phrase metadata
same surface fingerprint
same role fingerprint
same click mask
```

This is direct materialized false diversity.

Acid and DnB also contain semantic-different / role-same witnesses.

#### Role different, click structure same

Different role relationships can occupy the same global attack slots.

This demonstrates why CLICK must remain a stress test rather than the sole idea identity.

---

## 4. Cross-genre stripped-timbre test

For each P-level the corpus compares the set and frequency of strict same-click masks between genres.

`collision rate` below means the proportion of all `128 × 128` cross-genre identity pairs that materialize the exact same global click mask.

| Level | Pair | Shared click masks | Collision rate |
|---|---|---:|---:|
| P1 | Acid ↔ House | 18 | **6.07%** |
| P1 | Acid ↔ Dub Techno | 3 | 1.12% |
| P1 | Acid ↔ DnB | 2 | 0.14% |
| P1 | House ↔ Dub Techno | 3 | 2.30% |
| P1 | House ↔ DnB | 2 | 0.17% |
| P1 | Dub Techno ↔ DnB | 0 | **0%** |
| P2 | Acid ↔ House | 14 | **5.08%** |
| P2 | Acid ↔ Dub Techno | 0 | **0%** |
| P2 | Acid ↔ DnB | 1 | **5.47%** |
| P2 | House ↔ Dub Techno | 2 | 0.21% |
| P2 | House ↔ DnB | 1 | 1.68% |
| P2 | Dub Techno ↔ DnB | 1 | 0.73% |
| P3 | Acid ↔ House | 2 | **11.27%** |
| P3 | Acid ↔ Dub Techno | 0 | **0%** |
| P3 | Acid ↔ DnB | 1 | **12.91%** |
| P3 | House ↔ Dub Techno | 1 | 0.51% |
| P3 | House ↔ DnB | 4 | 2.24% |
| P3 | Dub Techno ↔ DnB | 0 | **0%** |

### Interpretation

#### Acid ↔ House

There is substantial overlap after complete click stripping. At P3 Acid has only **two** global click masks and both exist in House.

This does **not** prove Acid and House are the same genre. Acid identity can legitimately live in bass pitch/lifetime/articulation relationships that the strict click projection removes. It does prove that P3's very large semantic space is not producing a correspondingly broad global temporal vocabulary.

#### Acid ↔ DnB P2/P3

Only one click mask is shared, but that mask is common in both corpora, producing high pair-collision rates. Set Jaccard alone would hide this. Frequency therefore matters in structural audits.

#### Dub Techno ↔ DnB

They are strongly separated by global attack topology: zero shared masks at P1 and P3, one at P2.

Therefore the Dub Techno concern is **not** generic collapse into DnB or Acid. Its problem is internal genre ownership: most current Dub Techno material does not preserve a techno four-floor skeleton after timbre removal.

---

## 5. Genre-identity findings

### 5.1 Acid

P2 archetype distribution:

| Archetype | Count | Four-floor |
|---|---:|---:|
| 405 StraightAcid | 40 | 40 |
| 406 RollingAcid | 32 | 32 |
| 407 SyncopatedAcid | 29 | 0 |
| 408 SparseAcid | 27 | 0 |

Exactly 72/128 samples are four-floor because only 405/406 preserve that skeleton.

**Identity verdict: PARTIAL / NOT YET PROVEN AS ONE EXPLICIT STRUCTURAL CONTRACT.**

Acid already has real role-level idea diversity, but P3 compresses to two global attack masks. More importantly, idea-level tonal articulation on this base is Plain-only, so an important acid structural dimension is not represented as a coherent idea choice even though articulation may exist elsewhere in materialization/runtime.

Do not solve this by merely adding randomness. The missing question is which bass timing/lifetime/articulation relationships are invariant enough to make Acid recognisable after production is removed.

### 5.2 House

P2 archetype distribution:

| Archetype | Count | Four-floor |
|---|---:|---:|
| 401 StraightDrive | 29 | 29 |
| 402 OffbeatOpenHat | 15 | 15 |
| 419 ShuffledFourFour | 19 | 19 |
| 711 StackedQuarters | 28 | 28 |
| 713 FunkHouseBridge | **37** | **0** |

The entire 37/128 non-four-floor population comes from `FunkHouseBridge` 713. It is not a fringe candidate; its House selection weight is high.

**Identity verdict: AT RISK / FAILS A STRICT FOUR-FLOOR HOUSE CONTRACT.**

If House identity in GroovePuter is intended to require a stable four-floor dance pulse, the current candidate set violates that prohibition. If that invariant is not intended, the genre contract must explicitly say what replaces it; current semantic diversity alone is not sufficient evidence.

Role-level diversity itself is strong. The problem is not lack of material variation.

Previous tonal-profile research also found House's idea-level tonal contour space strongly collapsed: bass RootAnchor and melody Static. That further concentrates House diversity in rhythm selection rather than a coherent multi-role idea space.

### 5.3 Dub Techno

P2 archetype distribution:

| Archetype | Count | Four-floor |
|---|---:|---:|
| 409 OneDropSpace | 38 | 0 |
| 410 Steppers | **28** | **28** |
| 411 SparseSkank | 25 | 0 |
| 412 ChordResponse | 37 | 0 |

Only 28/128 samples — **21.9%** — preserve a four-floor kick skeleton, exactly the Steppers population.

The other three archetypes are structurally dub/reggae-oriented even before timbre and FX enter the picture.

**Identity verdict: FAIL under the intended `techno skeleton + dub space` contract.**

This is the strongest G4-C0 genre finding.

Dub Techno does have genuine role-level idea diversity (`105` unique role structures at P2). Therefore the remedy is not "more diversity" and not "more delay/reverb". The current route/recipe needs an evidence-backed identity reclassification or compatibility correction so that techno identity is structural and dubness appears through space, chord timing, bass behavior and phrase logic.

No evidence here justifies creating a new persisted genre branch.

### 5.4 Drum & Bass

All four expected breakbeat archetypes are materially represented:

| Archetype | Count |
|---|---:|
| 413 TwoStepRoll | 34 |
| 414 GhostedRoll | 40 |
| 415 SparseFastBreak | 23 |
| 416 HalftimeSwitch | 31 |

Role-level diversity is the strongest of the pilots: `126 / 125 / 123` unique role structures at P1/P2/P3.

**Identity verdict: DRUM LAYER STRONG; FULL DnB IDENTITY NOT YET PROVEN.**

The unresolved issue is bass organization. The DnB base profile still draws from the generic drive bass bag (`KickLock`, `OffbeatPush`, `RollingDrive`, `SyncopatedHook`) and does not select `HalfTimePocket`. An explicit requested bass ID also bypasses family auto-compatibility arbitration.

Therefore the required fast-drum / slower-or-half-time-bass relationship is not protected as a genre prohibition. Good drum identity cannot substitute for this missing cross-role invariant.

---

## 6. Phrase development and causal ownership

P2 phrase corpus:

| Genre | Ready | Explicit trajectory | Role-developing | Click-developing | Click-static |
|---|---:|---:|---:|---:|---:|
| Acid | 128 | **0** | 125 | **0** | 128 |
| House | 128 | **0** | 103 | **0** | 128 |
| Dub Techno | 128 | **0** | 115 | 20 | 108 |
| DnB | 128 | **41** | 87 | 12 | 116 |

This exposed a causal mismatch that tuple-level research could not prove.

`phraseTrajectoryForLaw()` maps the declared laws to real trajectories, but `preparePhraseExecution()` admits those trajectories only for archetypes enabled by the Stage-12 phrase-evolution overlay.

The Stage-12 whitelist includes:

```text
404, 420, 712, 714,
413, 414, 415, 416,
417, 418
```

For the four pilots:

```text
Acid       405–408   → none admitted
House      401/402/419/711/713 → none admitted
Dub Techno 409–412   → none admitted
DnB        413–416   → all admitted
```

Therefore Acid, House and Dub Techno can select semantic metadata named `RepeatReply`, `DevelopReturn` or `SparseDrift` while the explicit phrase-law bar-function programme remains unreachable for their selected archetypes.

Their observed role-level bar differences are caused by other per-bar consumers/temporal coordinates, not by the declared phrase law.

**Verdict: phrase-law semantic labels are currently false causal diversity for three of the four pilots.**

DnB is the only pilot in this corpus where the declared phrase law has a production-reachable explicit trajectory owner.

This is not a request to enable every archetype in Stage 12. It is evidence that 0.9.11 must make phrase-development semantics truthful: either the idea admits a real phrase function, or the semantic layer must not claim one.

---

## 7. Physical time

Bar-relative numbers hide large musical differences.

At suggested BPM, P2 total attack activity is approximately:

| Genre | Suggested BPM | Avg attacks/bar | Attacks/sec | Every-2-beats harmonic change |
|---|---:|---:|---:|---:|
| Acid | 132 | 20.50 | 11.27 | 0.909 s |
| House | 122 | 22.04 | 11.20 | 0.983 s |
| Dub Techno | 120 | 14.94 | 7.47 | 1.000 s |
| DnB | 174 | 24.39 | 17.68 | 0.689 s |

The same symbolic `Every2Beats` decision therefore ranges from about 0.69 s to 1.00 s per harmonic change across these pilots.

This confirms that future G4 evidence must report both:

```text
musical-grid measures
AND
physical-time measures
```

At minimum:

- attacks per bar
- attacks per second
- bass attacks per second
- average bass lifetime
- seconds per harmonic change

A genre contract expressed only in bars/steps can hide materially different temporal behavior.

---

## 8. What G4-C0 proved

### Proven

1. **Representation capacity exists.** The engine can materialize many different role structures without adding a new generator.
2. **P1/P2/P3 preserve selection identity.** They are variation/transformation layers, not idea selection.
3. **False diversity exists materially, not only in selection metadata.** Different semantic tuples can collapse to identical role/surface structures.
4. **Click stripping exposes major compression.** Semantic tuple count is not a credible proxy for musical-idea count.
5. **Dub Techno identity is structurally misaligned with a techno-skeleton contract.** Only the Steppers subset preserves four-floor.
6. **House contains a high-weight non-four-floor candidate.** A strict four-floor House invariant is not currently enforced.
7. **DnB has strong drum identity but no protected half-time/slower bass relationship.**
8. **Phrase-law causality is missing for Acid, House and Dub Techno archetypes.** DnB alone has explicit Stage-12 phrase-law execution among the four pilots.
9. **Physical time matters.** Identical beat-relative harmonic rates differ substantially in seconds and attack rates.

### Not proven

1. The exact perceptual number of musical ideas per genre.
2. That click-mask uniqueness alone defines an idea.
3. That every non-four-floor Acid idea is invalid Acid.
4. That Dub Techno needs a new persisted genre branch.
5. That more random combinations would improve diversity.
6. That the current semantic tuple should become a persisted `MusicalIdea` object unchanged.

---

## 9. Recommended 0.9.11 G4 contract

Do not start with a new framework. Start by making the existing decisions obey four semantic levels.

```text
GENRE IDENTITY
    invariants and prohibitions that define the genre

MUSICAL IDEA
    one coherent structural statement inside that genre

IDEA VARIATION
    local transformations that preserve that statement

PHRASE DEVELOPMENT
    temporal development of the same statement without rerolling it
```

### 9.1 Genre identity

Must be expressed as relationships/prohibitions where possible, not as a list of production presets.

Examples of candidate contracts to verify, not blindly implement:

```text
House:
  preserve the dance-pulse invariant if four-floor is authoritative

Dub Techno:
  techno temporal skeleton survives timbre/FX stripping
  dub identity lives in space, chord placement, bass/lifetime and phrase behavior

DnB:
  fast break layer coexists with a slower/half-time bass organization

Acid:
  identify which bass timing/lifetime/articulation relations survive timbre stripping
```

### 9.2 Musical idea

A different idea needs at least one dominant structural witness such as:

- different bass onset/lifetime topology
- different kick↔bass relationship
- different chord timing/role
- different melodic statement topology
- different phrase activity/development logic

The following alone do **not** create a new idea:

- transposition
- velocity
- FEEL
- timbre/FX
- ghost notes alone
- one event changed in isolation

### 9.3 Idea variation

P1/P2/P3 and local reroll may alter expression and bounded topology, but must preserve the idea's defining anchors.

A `SustainAndDrop` idea cannot silently become `RollingDrive` and still claim to be the same idea merely because the semantic owner did not change.

### 9.4 Phrase development

Phrase must **develop, not reroll**.

For every non-Loop semantic phrase law that is presented as a musical decision, there must be a causal production path to at least one meaningful temporal function, for example:

- answer
- omission/rest
- displacement
- build
- reduction
- return
- pickup
- delayed resolution
- register movement

If an archetype does not admit such a programme, do not pretend the selected label caused the material.

### 9.5 Activity

Low activity must select or realize a coherent sparse idea. It must not be implemented as an arbitrary event-deleted version of a dense idea.

### 9.6 Evidence format

Do not introduce one `diversityScore`.

Keep independent verdicts:

```text
IDENTITY: PASS / FAIL / PARTIAL
IDEA: SAME / DIFFERENT / family label
PHRASE CAUSALITY: PASS / FAIL
```

And retain multiple structural projections:

```text
SEMANTIC
SURFACE
ROLE + LIFETIME
CLICK
PHYSICAL TIME
```

---

## 10. Pilot disposition for production planning

| Pilot | Genre identity | Genuine idea capacity | Phrase-law causality | G4 priority |
|---|---|---|---|---|
| Acid | Partial / needs explicit structural contract | **Proven at role level** | **Fail** | Define idea anchors; do not add randomness |
| House | At risk under strict four-floor contract | **Proven at role level** | **Fail** | Resolve 713 compatibility + tonal idea collapse |
| Dub Techno | **Fail** under techno-skeleton contract | **Proven at role level** | **Fail** | Highest identity/reclassification priority |
| DnB | Drum identity strong; bass invariant missing | **Proven at role level** | **Partial/real: explicit trajectories exist** | Protect drum↔bass temporal relationship |

---

## 11. G4-C0 verdict

```text
G4 PROBLEM                         PROVEN
MATERIALIZED STRUCTURAL CAPACITY  PROVEN
SEMANTIC-TUPLE DIVERSITY          NOT A VALID IDEA METRIC
GENUINE ROLE-STRUCTURE DIVERSITY  PROVEN
FALSE DIVERSITY                   PROVEN
P1/P2/P3 AS IDEA RESELECTION      REJECTED BY EVIDENCE
PHRASE-TIME CAPACITY              EXISTS
PHRASE-LAW CAUSALITY              MISSING FOR 3/4 PILOTS
ACTIVITY-AS-COHERENT-IDEA         NOT PROVEN / MISSING
DUB TECHNO IDENTITY ISSUE         PROVEN
NEW GENRE BRANCHES                NOT JUSTIFIED
PRODUCTION G4 IMPLEMENTATION      NOT STARTED
```

The current generator does **not** need more expressive capacity first.

The 0.9.11 problem is to organize existing capacity into truthful, coherent spaces:

> genre identity constrains the space; a musical idea selects a coherent statement; variation changes that statement locally; Phrase develops it over time.

That contract should be designed before any production owner or schema is introduced.
