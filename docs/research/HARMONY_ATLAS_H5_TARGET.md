# Harmony Atlas H5 Target Boundary

**Status:** bootstrap target contract  
**Runtime impact:** none

H5 compares frozen Harmony Atlas H1/H4 evidence against the exact Stage 15 RC checkpoint:

```text
Ostrovsky42/GroovePuter
fc42763e7798866e61895bf1b8d62339ec59e0a7
```

This target is `TARGET_CONTRACT_EVIDENCE`, not an ancestor of the research stack. H5 must not copy it into production or silently widen it.

Pinned target blobs:

```text
src/generation/roles/chord_progression.h               0a4e014fbe1b671a0cb50e22bee7c473720758a6
src/generation/roles/chord_progression.cpp             f83d0c7859a4e9fee8f1e878a2de0ad5401a873a
src/generation/roles/chord_rhythm.h                    481c70a5c16ca4bda29b5eee946789f3f6ecbf2d
src/generation/roles/chord_rhythm.cpp                  9a50e369e8c68d3f09116e4b0c793e9c898af3ef
src/generation/rhythm/rhythm_types.h                   5a3d415d8ae2f4bdc35c9d3391cea3ef40bce613
src/generation/tonal/tonal_materializer.cpp            2ab2b5e2d05090d3f6e855d2d59abe4f23a4f1b6
src/generation/migration/strong_rhythm_migration.cpp   5641a2e02dfde652434271d12ed391254db5bcb5
```

Measured target boundaries used by H5:

- `ChordQuality`: Triad, Minor7, Major7, Dominant7, Sus4, Minor9, Major9, Diminished;
- `kMaxHarmonicEvents = 8`;
- `rootOffsetSemitones` is bounded to ±2 and is consumed by TonalMaterializer;
- current progression catalog restricts nonzero offsets to specific hard-coded progression IDs;
- `ChordRhythmPlan` is a 16-step one-bar onset/continuation/release plan;
- the live Stage 15 bridge requests `phraseBars = 1` and derives progression event count from ChordRhythm onsets;
- current TonalMaterializer validates `ChordQuality` but does not use it to calculate pitch.

Therefore H5 must distinguish field/schema capacity, current catalog reachability and audible representability. Enum presence alone is not evidence of audible quality support.
