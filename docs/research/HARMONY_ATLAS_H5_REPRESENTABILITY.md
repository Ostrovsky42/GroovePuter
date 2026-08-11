# Harmony Atlas H5 — Stage 15 Representability

**Status:** generated R1 research evidence / H5 checkpoint  
**Target:** `Ostrovsky42/GroovePuter @ fc42763e7798866e61895bf1b8d62339ec59e0a7`  
**Target evidence:** `TARGET_CONTRACT_EVIDENCE`  
**Runtime impact:** none

## Target boundary

H5 compares frozen Harmony Atlas evidence against an exact Stage 15 RC source checkpoint. Target code is not copied into the research ancestry and H5 changes no production file.

- max harmonic events: **8**
- root offset field: **±2 semitones**
- ChordRhythm grid: **16 steps/bar**
- generic phrase ceiling: **4 bars**
- live Stage15 progression request: **1 bar**
- `ChordQuality` consumed for pitch: **false**
- `rootOffsetSemitones` consumed for pitch: **true**

## Harmonic representability (F3)

| Level | Definitions |
|---|---:|
| H1 logical definitions | 190 |
| Raw field-shape encodable | 160 |
| Exact quality-label field encodable | 1 |
| Exact current progression-catalog F3 | 0 |
| Root path exact, ignoring quality | 11 |
| **Audible exact F3** | **0** |

| Quality class | Events | Logical definitions |
|---|---:|---:|
| `EXACT_ENUM_LABEL` | 49 | 32 |
| `CONTEXT_DEPENDENT_TRIAD` | 746 | 185 |
| `UNREPRESENTABLE_QUALITY` | 51 | 29 |

Altered-degree definitions: **71**.
Source harmonic forms >8 events: **1**.

Root-path overlaps (quality intentionally ignored):

```text
Major:011
Major:019
Major:025
Major:026
Major:029
Major:031
Minor:009
Minor:015
Minor:016
Minor:019
Minor:034
```

### Unsupported quality signatures

| H1 semantic quality | Logical support | Events |
|---|---:|---:|
| `MAJOR|SEVENTH|UNSPECIFIED|0` | 16 | 24 |
| `MAJOR|SIXTH|NONE|0` | 6 | 7 |
| `SUSPENDED_2|NONE|NONE|0` | 4 | 4 |
| `MAJOR|ADD_NINTH|NONE|0` | 3 | 3 |
| `POWER_5|NONE|NONE|0` | 2 | 5 |
| `MINOR|ADD_NINTH|NONE|0` | 2 | 2 |
| `MINOR|SIXTH|NONE|0` | 2 | 2 |
| `MAJOR|NONE|NONE|-1` | 1 | 2 |
| `MAJOR|NINTH|UNSPECIFIED|0` | 1 | 1 |
| `MAJOR|SIX_NINE|NONE|0` | 1 | 1 |

## ChordRhythm representability (F5)

| Item | Count |
|---|---:|
| H4 rhythm observations | 950 |
| H4 unique F5 identities | 30 |
| Exact current observations | 0 |
| Exact current unique F5 | 0 |
| 16th-grid compatible observations | 950 |
| Finer-grid gaps | 0 |

| Gap | Logical definitions | Style observations |
|---|---:|---:|
| `GENERIC_PHRASE_GT4_BARS` | 32 | 67 |
| `HARMONIC_EVENTS_GT8` | 32 | 55 |
| `LIVE_ONE_BAR_DURATION` | 190 | 950 |
| `SAME_CHORD_RETRIGGER_SEMANTICS` | 190 | 380 |

## Combined representability (F6)

H4: **945** unique F6 / **950** observations.
Current exact F6: **0 unique / 0 observations**.

Exact F6 requires both audibly exact F3 and exact F5; root-only similarity, enum presence and rescaling are not substitutes.

## Ranked deferred capabilities

Counts overlap and are **not additive**; ranking uses logical definitions, never style/key multiplicity.

| Rank | Capability | Dimension | Logical support | Rhythm observations |
|---:|---|---|---:|---:|
| 1 | `MULTI_BAR_CHORD_RHYTHM_IDENTITY` | Rhythm/F5 | 190 | 950 |
| 2 | `QUALITY_RENDERING_CONSUMPTION` | Harmony/F3 | 190 |  |
| 3 | `SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE` | Rhythm/F5 | 190 | 380 |
| 4 | `TRIAD_POLARITY_OR_EXPLICIT_CONTEXT` | Harmony/F3 | 185 |  |
| 5 | `GENERIC_ALTERED_DEGREE_REACHABILITY` | Harmony/F3 | 71 |  |
| 6 | `CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER` | Rhythm/F5 | 32 | 67 |
| 7 | `MORE_THAN_8_HARMONIC_ONSETS` | Combined F3/F5 | 32 | 55 |
| 8 | `ADDITIONAL_CHORD_QUALITY_VOCABULARY` | Harmony/F3 | 29 |  |
| 9 | `SOURCE_HARMONIC_FORM_GT8` | Harmony/F3 | 1 |  |

## H5 contract

- exact Stage15 target blobs are verified;
- H1/H4 bytes are pinned by SHA-256;
- enum support is separate from audible quality rendering;
- altered-degree field capacity is separate from catalog reachability;
- root-only overlap is diagnostic, never F3 equality;
- F5 exactness preserves duration and retrigger semantics;
- support ranking uses logical source definitions and overlapping counts;
- no production changes, runtime weights or runtime candidates are created.

Next stage: **H6 — curated runtime candidates**. Production integration remains separate.
