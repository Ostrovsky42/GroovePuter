# Harmony Atlas H0 Source Audit

**Status:** generated research evidence / H0 complete  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none

## Result

The pinned source is a generated editorial catalog. The canonical harmonic unit is a logical progression definition, not a generated MIDI file.

| Item | Count |
|---|---:|
| Logical progressions | 190 |
| Major | 50 |
| Minor | 58 |
| Modal | 82 |
| Key pairs | 12 |
| Rhythm/style materializations | 5 |
| Materializations per logical progression | 60 |
| Projected progression MIDI materializations | 11400 |
| Major projected materializations | 3000 |
| Minor projected materializations | 3480 |
| Modal projected materializations | 4920 |

The 190 logical definitions therefore project to 11,400 progression MIDI files before counting the repository's separate triad/extended-chord material. Generated file count is not musical popularity.

## Source dimensions

Styles: `default`, `pop`, `pop2`, `hiphop2`, `soul`

Descriptor vocabulary (23): `Anguished`, `Cadence`, `Dark`, `Dramatic`, `Empowered`, `Excited`, `Fearful`, `Hopeful`, `Joyful`, `Lonely`, `Mysterious`, `New`, `Nostalgic`, `Peaceful`, `Playful`, `Rebellious`, `Relaxed`, `Romantic`, `Sad`, `Spiritual`, `Surprised`, `Tender`, `Triumphant`

Mood tags (21): `Anguished`, `Dark`, `Dramatic`, `Empowered`, `Excited`, `Fearful`, `Hopeful`, `Joyful`, `Lonely`, `Mysterious`, `Nostalgic`, `Peaceful`, `Playful`, `Rebellious`, `Relaxed`, `Romantic`, `Sad`, `Spiritual`, `Surprised`, `Tender`, `Triumphant`

Structural tags: `Cadence`

Catalog tags: `New`

Definitions tagged `New`: **14**. Definitions tagged `Cadence`: **6**.

## Lexical inventory

Unique raw progression chord tokens: **85**.

Raw progression suffix vocabulary: `<empty>`, `5`, `6`, `69`, `7`, `9`, `M`, `M-5`, `M6`, `M7`, `add9`, `dim`, `dom7`, `m`, `m6`, `m7`, `m9`, `madd9`, `sus2`, `sus4`

Altered degree classes: `#IV`, `bI`, `bII`, `bIII`, `bVI`, `bVII`

Explicit double-space rest markers: **0**.

Lexically unclassified progression tokens: **none**.

This is lexical inventory only. H0 does not claim semantic equivalence between suffix spellings and does not normalize Roman-numeral function.

## Declared source chord-type catalog

Major-third suffixes: **20**. Minor-third suffixes: **19**. Union: **34**.

Major-third: `2`, `6`, `69`, `7`, `7+11`, `7+5`, `7-5`, `7-9`, `7sus4`, `9`, `9sus4`, `M7+5`, `add11`, `add4`, `add9`, `maj7`, `maj9`, `sus2`, `sus4`, `sus4add9`

Minor-third: `7sus4`, `9sus4`, `dim6`, `dim7`, `m6`, `m69`, `m7`, `m7+5`, `m7-5`, `m7add11`, `m7b9b5`, `m9`, `mM7`, `mM7add11`, `madd4`, `madd9`, `sus2`, `sus4`, `sus4add9`

## H0 gate

- source revision and critical source blobs are pinned;
- all logical progression families are enumerable;
- generated key/style multiplicity is separated from logical support;
- descriptor/tag vocabulary is inventoried and typed;
- source-declared chord-type vocabulary is inventoried;
- altered-degree classes are preserved as lexical evidence while raw spellings remain available in raw tokens;
- lexical anomalies are explicit rather than silently normalized;
- no production generation, Tonal Projector, Scene, Genre or UI code is changed.

Next stage: **H1 canonical parser / loss-aware normalization**. H1 must preserve altered degrees and chord quality, but it is not part of this checkpoint.
