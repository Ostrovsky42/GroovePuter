# 0.9.14 M0-A — Musical play baseline

Base: D1-C1 (`feature/20260929-m0-musical-play-baseline`). Nature of this checkpoint:
**measurement, not redesign.** No production musical behaviour was changed by M0-A.

**Evidence limits, up front.** Everything below is *structural* evidence computed on
deterministic host corpora. **No human listening has happened yet**; the listening cards
are prepared but unanswered. Thresholds in the pair comparison are descriptive and
arbitrary. Most numbers are for one identity (ordinal 11) or 24/48 sampled identities, not
the full identity space. Where a claim depends on listening it is marked *(needs ears)*.

## Reproduce

```bash
bash tools/m0/build_m0a.sh                 # builds tools/m0/m0a_corpus.cpp, runs it
                                           # -> build/m0a/m0a_report.txt (this doc's numbers)
                                           # -> build/m0a/corpus/structural_corpus.tsv
                                           # -> build/m0a/listening/*.mid, manifest.tsv, listening_cards.md
bash tests/run_0_9_14_m0a_tests.sh         # gate: product firewall + determinism + SMF validity
```

The corpus is deterministic (identical FNV hash over two independent runs; asserted by the gate).
Timbre is removed by construction: exported data has lanes, attack timing, continuation
(separately), pitch class (root-relative), bar ordinal, BPM, harmonic-event boundaries,
articulation flags — no voice/FX/timbre parameters. The SMF files use GM drum notes and a plain
bass/chord program; they are listening aids, not a production exporter (`tools/m0` is host-only
and a source regression forbids firmware from including it).

## 1. Capability audit: TAKE · KEEP · DEVELOP · BREAK · RETURN

| ACTION | CURRENT OWNER | AUDIBLE NOW? | PRODUCT GAP |
|---|---|---|---|
| **TAKE** | Genre page `G` = "NEW TAKE" → `regenerateWithQuantizedCommit` (commits at the next bar, "TAKE -> NEXT BAR"), with UNDO/REDO TAKE and an attempt table ("TAKE LIMIT") | **Yes**: a new take is a new phrase heard at the next bar; undoable | Takes are unrelated to each other (see §7: 0% of later-take bars match an earlier bar). Attempt number/limit leaks to the user. |
| **KEEP** | Accepted Material / ACCEPT (modified Enter), Melody slots (Q..I), DISCARD restores, single-slot undo receipt; source anchor in `developmentLineage_` | Yes for *storing*; **nothing protects a kept idea from experimentation** except manual slot discipline | No "keep this" gesture that is also the future RETURN target; KEEP and RETURN are unconnected. |
| **DEVELOP** | Front-panel `D` ("DEVELOP") issues `Revoice`; `V` ("VARY") issues `Connect` (`material_development_ux.h`). `growMaterial(..., GrowthMode::Develop)` returns **"DEVELOP GROWTH DEFERRED"** and is never called from UI | **Barely.** Measured on real generated bass bars with the UI's default request: `D` changes the octave of every note and nothing else (pitch classes 0 changed, ticks 0 changed); `V` changes neither pitch nor tick. Both always give CONTINUES. | The word DEVELOP names a register shuffle. Real development (a related-but-changed next section) has **no owner acting on CURRENT**. |
| **BREAK** | `BarFunction::Break` inside trajectory 8 ("Statement · RepeatGhosts · Break · Return"), reachable only via law `SparseDrift` at depth **P3** on archetypes admitted to phrase evolution. A `BREAK` *role label* exists on the Phrase page but no audio owner consumes it (only UI/workspace read it). | **Weakly.** Where it fires the Break bar mostly thins drums (e.g. Funk 15→8 hits, UKG 20→14, DnB 23→18, Dub 16→9) while bass/chord attacks barely move. Naturally selected only for Dub 8-bar at P3 (10/48 identities); never in the other sampled genres. | Not user-invokable (no phrase-law control in UI); not a contrast in bass/harmony. |
| **RETURN** | `BarFunction::Return` (trajectories 5, 6, 8) | **Rhythm only.** Return bar vs bar 0: drums IDENTICAL, bass rhythm IDENTICAL, chord rhythm identical (except Funk), harmonic *timing* identical; bass **pitch classes often differ** (Darksynth 3 steps, Dub 2, UKG 2, DnB 1; identical for Techno and Funk at the sampled identity) because the progression keeps moving. | Nothing returns to the *kept* idea; "return" = "the same rhythm again". Full material return does not exist. |

## 2. Realized genre audit

| GENRE | STRUCTURAL SIGNAL (realized, timbre-free) | REALIZED EVIDENCE | IMPORTANT GAP |
|---|---|---|---|
| **Acid** | `straight_acid` / `rolling_acid`; bass-rhythm ids OFFBEAT PUSH / SYNCOPATED HOOK; fewer bass attacks than Techno, off-beat weighted | Distinct from Techno in bass rhythm and bass pitch, **not** in drums. **Slide-into-attack = 0 and accent = 0 across 24 identities at P2 and P3** (articulation owner exists — `BassArticulationStyleId` AccentPulse/Legato/Dynamic — but sampled realizations are Plain) | Acid's classic identity (slide/accent) is *declared but not realized* in sampled output; after timbre removal Acid = bass rhythm + pitch only. Acid never selects a phrase-evolution archetype (always Loop). |
| **Techno** | `straight_drive`/`machine_syncopation`; bass ROLLING DRIVE; harmonic events 1/bar; no bar-to-bar change under Loop | At ordinal 11: bars identical in every lane (0 changed cells). With admitted archetype + non-Loop law: 1–2 bars change, **drums only**, bass/chord unchanged. Static harmony visible (bass pitch classes unchanged across bars) | Multi-bar development is "same bar with another drum mask" (RepeatReply: 2/4 bars differ in drums, 0 in bass). |
| **House** | `stacked_quarters`; ROLLING DRIVE bass | **kick = 6/bar (not a pure quarter pulse)**, hats 6, offbeat 0.00, 2 harmonic events/bar; bass pitch moves each bar (8 steps differ bar-to-bar even under Loop) | Never selects a phrase-evolution archetype in 48 identities (always Loop, no Return/Break). "Quarter pulse" is a prior, not a constraint. Deep House distinction not evidenced. |
| **Dub** (Reggae/5, DeepChord Reggae/10) | `straight_drive`; bass SPARSE ANCHOR; 1.0 bass attacks/bar; chord lane enters/exits on alternate bars | Sparse bass and chord alternation are *structural* and survive timbre removal (Dub 8-bar bar-change 1.29, 2-bar 3.00, 4-bar 0.00 — two identical 4-bar halves) | **Production-as-form is absent from pattern data**: reverb/compression/transient automation nodes in generated patterns = **0**. Any dub delay/reverb form needs an owner that does not exist. Only genre whose 8-bar phrase is admitted (P1R length policy rejects 8 bars for the other seven). |
| **Funk/Soul** | `sparse_fast_break`; BASS KICK ANSWER; snare backbeat at steps 4 and 12 in 4/4 bars | Kick@0 in 4/4 bars, backbeat present, bass@0 in 0/4. Nice: DevelopReturn/RepeatReply are reachable and drums differ 1–2 of 4 bars | **No owner for a metric hierarchy / "The One" / pocket.** A tick-0 event is not The One (nothing here constrains bass/kick/accent interlock). |
| **UK Garage** | Positive control: `skippy_2step` (ordinal 11) and `machine_syncopation` (ordinal 0) — two-step organisation visible in kick/snare masks; DnB vs UKG drums differ by 7.5 hits/bar | Structural difference from DnB is strong in drums. Two archetypes for one genre ⇒ genre→archetype is a *prior*; timing-shifted drum steps/bar = 0 at ordinal 11 (swing not visible there) | Swing is realized as step timing only where the feel path applies it; not evidenced at this identity. |
| **DnB** | `ghosted_roll`; hats 8.5/bar, kick 4, snare 2 | Distinct drum density from UKG. Law is always `RepeatReply` (48/48). **Bass-vs-drum hierarchy: `HalfTimePocket` exists as a bass-rhythm identity but was not selected at the sampled identity** (bass 5 attacks vs 15 drum hits/bar; relation reported GAP) | No realized owner *establishes* slower-bass-against-faster-drums. Do not infer it from the genre name. |

Timbre-removal pairs (4-bar, ordinal 11; threshold-based, **not a classifier**):

| pair | label | dimensions that differ |
|---|---|---|
| Techno vs Darksynth | STRUCTURAL DIFFERENCE OBSERVED | **bass rhythm, bass pitch, harmony; drums identical (same archetype)** |
| House vs Techno | STRUCTURAL DIFFERENCE | drums, bass pitch, harmony |
| House vs Dub | STRUCTURAL DIFFERENCE | drums, bass rhythm, bass pitch, harmony |
| Techno vs Funk | STRUCTURAL DIFFERENCE | bass rhythm, bass pitch, harmony |
| UKG vs DnB | STRUCTURAL DIFFERENCE | drums, bass pitch, chord |
| Acid vs Techno | STRUCTURAL DIFFERENCE | bass rhythm, bass pitch (articulation stripped: same result — none was realized) |

Reading: distinctness survives timbre removal, but for Techno/Darksynth and Acid/Techno it lives in
the **bass lane only**; the drums are the same archetype. Whether that is *audibly* distinct is
*(needs ears)*.

## 3. Multi-temporal activity (measured, not scored)

Mean changed cells between consecutive windows (drum lanes + bass + chord attacks):

| genre (phrase) | bar | 2-bar | 4-bar | beyond |
|---|---|---|---|---|
| Acid 4-bar, Loop | 0.00 | 0.00 | – | – |
| Techno 4-bar, Loop | 0.00 | 0.00 | – | – |
| House 4-bar, Loop | 0.00 (bass pitch moves) | 0.00 | – | – |
| Funk 4-bar, DevelopReturn | 2.00 | 1.50 | – | – |
| UKG 4-bar, RepeatReply | 2.67 | 4.00 | – | – |
| DnB 4-bar, RepeatReply | 4.67 | 7.00 | – | – |
| Dub 8-bar, Loop | 1.29 | 3.00 | 0.00 | – |

The old failure is visible: for the Loop genres *all* structure change is 0 at bar/2-bar/4-bar
level; for admitted archetypes change lives at the bar and 2-bar level only. Per-bar deltas show
that **development is drum-centric**: e.g. DnB RepeatReply bars 1→2 differ by 7 drum cells and 0
bass/chord cells.

## 4. Phrase-law causal test (only the law varies)

Same genre, recipe, identity ordinal, feel and length; the law is swapped through the same owner call
production uses (`evolveRhythmPhrase` with the law's trajectory) on a copy of the prepared
execution. Bars differing from Loop, drums/bassRhythm/bassPitch/chord (4-bar, P2):

* Techno: RepeatReply 2/0/0/0, DevelopReturn 1/0/0/0 · Darksynth 4/0/0/0, 4/0/0/0
* Dub: 2/0/0/0, 1/1/1/1 · Funk: 2/2/2/0, 1/1/1/0 · UKG: 2/0/0/0, 1/0/0/0 · DnB: 4/0/0/0, 4/1/1/0
* P3 SparseDrift (Break): Techno 1/0/0/0, Darksynth 1/0/0/0, Dub 4/1/1/1, Funk 2/1/1/0, UKG 1/0/0/0, DnB 4/0/0/0

The law **does causally change actual output** (not just an enum), primarily in the drum lanes.
`NOT_APPLICABLE` (archetype not admitted / no eligible trajectory): Acid and House at every
tested identity; `SparseDrift` at P2 for all; every 8-bar request except Dub (P1R length policy
rejects 8 bars for the other seven genres — a typed rejection, not a Legacy route).

Natural production selection (48 identities): Acid and House are always Loop; DnB is always
RepeatReply; Techno ≈ 52% Loop / 29% DevelopReturn / 19% RepeatReply; UKG ≈ 27% Loop; Funk ≈ 48% Loop;
`SparseDrift`/Break appears only for Dub 8-bar at P3 (10/48).

## 5. One-minute test (production mechanisms only, ~60 s at the engine BPM of 100)

| genre (unit) | mechanism | change per level (bar / 2-bar / 4-bar / 8-bar / 16-bar) | bars after the first unit already heard |
|---|---|---|---|
| Acid (4-bar) | leave it looping | 0.00 / 0.00 / 0.00 / 0.00 / – | **100%** |
| Techno (4-bar) | leave it looping | 0.00 / 0.00 / 0.00 / 0.00 / – | **100%** |
| Dub (8-bar) | leave it looping | 1.45 / 3.00 / 0.00 / 0.00 / 0.00 | **100%** |
| Acid/Techno/Dub | press TAKE every unit | large changes at the 4–8-bar boundary (e.g. Dub 8-bar 15.75), 0% repeats | 0% |

Classification: **LONG-FORM GAP.** A loop is fully predictable after the first phrase; successive
takes are *unrelated* rather than developed, so structure does not accumulate. (Randomizing more
notes was deliberately not tried.)

## 6. Hard constraint vs soft bias (classification only — no policy change)

| Rule | Class | Evidence |
|---|---|---|
| Techno static harmony | GENERATION PRIOR | static progression ids exist; realized Techno bass pitch stays fixed; not enforced against other picks |
| House quarter pulse | SOFT BIAS / GENERATION PRIOR | realized kick = 6/bar; archetype prior, not a constraint |
| Dub protected space | HARD STRUCTURAL CONSTRAINT (at rhythm realization) | `protectedSpaceFor(archetype, role)` is passed to the bass rhythm owner; violation not tested here |
| Acid slide/accent | GENERATION PRIOR (articulation policy) — *not realized in sampled output* | 0 slides/accents over 24 identities × P2/P3 |
| Funk primary metric anchor | NOT WORTH MODELING *until an audible feature needs it*; currently no owner | no constraint found |
| DnB halftime bass relation | GENERATION PRIOR (bass-rhythm weighting) | `HalfTimePocket` exists, not selected at sampled identity |
| UKG two-step organisation | GENERATION PRIOR at archetype selection; HARD once the archetype is fixed | two archetypes seen for one genre |
| Timbre / delay / reverb behaviour | TIMBRAL / PRODUCTION CHOICE | absent from pattern data |
| When to break / return | USER INTENT | no control exposed |

## 7. Instrument vs laboratory audit (240×135 path; paper audit from source, not a user study)

A hypothetical 15-minute session that touches Genre → Synth sequencer → Material/Phrase → Song
meets, before it has made a musical decision beyond "another take":

* **recipe identity**: mode + recipe + morph target/amount + depth (P1/P2/P3) + apply mode
  ("NEW TAKE", "NEW TAKE + TEMPO", …) + attempt limit ("TAKE LIMIT");
* **source mode**: 6 labels (`NONE`, `STEPS`, `GENERATED`, `DERIVED`, `SMF`, `LIVE`);
* **Material state / ownership**: `REF`, `REF MUT`, `OWNED`, `EMPTY`; roles `MAIN/VAR/BREAK/END` (labels only);
* **slot ownership**: Pattern/Melody slots Q..I, Song slots/rows, banks/pages;
* **NEXT/GO lifecycle**: ~12 distinct toasts (`NEXT READY`, `NEXT BUSY: SONG`, `NEXT NOT READY`,
  `NO NEXT MATERIAL`, `GO: QUEUED/ACTIVATED/FAILED`, `GO DISARMED`, `NEXT DISCARDED`, `DISCARD: …`),
  three keys (Enter GO, Esc cancel, Alt+Backspace DISCARD) plus `^Z`;
* **semantic/debug state**: one leak — the legacy `IdeaClassification` toast **"NEXT READY:
  VARIATION / NEW MATERIAL / REFINED"** (baseline, tracked in the M0-A source regression).

That is roughly seven families and 30+ distinct labels/keys of internal vocabulary, against zero
musical verbs for BREAK/RETURN. The new D1-C machinery adds **zero** front-panel surface (the
observation is read-only, unreferenced by UI/Genre/policy code — enforced by
`tests/test_0_9_14_m0a_source_regressions.py`).

### Target session on paper (not implemented)

`TAKE` (existing G, next-bar) → `KEEP` (one gesture: this idea is now the thing you can return to)
→ `DEVELOP` (a related-but-changed next 2/4 bars of the *kept* idea) → `BREAK` (a contrast) →
`RETURN` (the kept idea, recognisably). The player must never need: provenance, R1/R2/R3, lineage
status, MaterialVersion, ReferenceRole, capability, trajectory ids, recipe identity, attempt
number, source-mode words. Internal terms (CONTINUES, UNKNOWN, EXACT, VARIATION, capability,
preservation, origin witness, predecessor, source relation, phraseGenerationIdentity) stay test/debug
only.

## 8. Listening artifacts

`build/m0a/listening/` (not committed): 17 SMF files + 3 explicit GAP rows in `manifest.tsv`,
`listening_cards.md` (six questions per file: YES / NO / UNCLEAR + note), and two one-minute files
per genre (`oneminute_<genre>_loop.mid`, `_takes.mid`). Files exist for Acid (loop only),
Techno/Dub/UKG/DnB (loop, reply, develop→return, break→return, each 4 bars, played twice).
GAP: Acid reply/develop/break (archetype not admitted). Ordinals used per file are recorded in the
manifest. The cards are **unanswered** — that is the next human step.

## 9. Answers

**Q1 — Does existing Genre structure survive timbre removal meaningfully?** Yes for the drum lane and
bass rhythm of most pairs, structurally (§2 table). It is weakest exactly where the distinction was
supposed to be about sound: Techno/Darksynth and Acid/Techno differ only in the bass lane. *(audibility needs ears)*

**Q2 — Mostly structural vs production-dependent?** Mostly structural: UK Garage, DnB, House (drum
organisation), Funk (backbeat). Heavily production-dependent: Acid (its slide/accent identity is not in the
realized pattern), Dub (delay/reverb form has no data owner), Darksynth-vs-Techno.

**Q3 — Does phrase evolution produce perceptible development over 4/8 bars?** It produces *structural*
change at 4 bars for admitted archetypes (drums mainly, 1–4 of 4 bars). Over 8 bars: only Dub is admitted and
its two halves are identical at the 4-bar level. Perceptibility needs ears; the drum-only, few-cell magnitude
suggests it is subtle.

**Q4 — Can existing owners DEVELOP from CURRENT?** No. `preparePhraseExecution` takes genre settings, a
generation identity and a length — no Material — so the law machinery **generates a new phrase**; it cannot
develop the current object. `growMaterial(Develop)` is explicitly deferred and never called from UI. The
existing DEVELOP key is Revoice.

**Q5 — Does BREAK have a real causal/audible owner?** Causal: yes (`BarFunction::Break`, trajectory 8, thins
drums). Audible/reachable: barely — P3 only, law SparseDrift only, one genre naturally, no user control, bass and
chord mostly untouched.

**Q6 — Does RETURN restore enough of the idea?** It restores the **rhythm** (drums, bass rhythm, chord rhythm,
harmonic timing identical) but not reliably the pitch content, and it returns to bar 0 of the phrase, not to a
kept idea. Probably heard as "the beat came back"; whether as a *return* needs ears.

**Q7 — Anything meaningful beyond the bar timescale?** At 2 and 4 bars for admitted archetypes, yes (structure).
Beyond 4 bars: nothing — loops are 100% predictable, takes are unrelated, and 8-bar phrases exist for one genre.

**Q8 — Single production change that most improves a 15-minute session?** Make one section-level action
(DEVELOP/BREAK/RETURN as one cycle) apply the existing bar-function programmes to the material the user just kept, with
the bar-function mutation reaching bass and chord lanes, not only drums. Everything else (more evidence, more claims) is secondary.

**Q9 — Can all semantic internals remain invisible?** Yes by construction for D0–D1-C (read-only, unreferenced by
UI). Existing exceptions to redirect: the "NEXT READY: VARIATION" toast and the Phrase page's source/ownership/role labels.

**Q10 — Next production checkpoint?** See decision below: realization first, then a DEVELOP/BREAK/RETURN action.

## 10. Stop / Continue / Redirect

**CONTINUE** (justified by audible/product evidence): TAKE with next-bar commit and undo; the phrase bar-function
trajectories (they change output causally); owner-derived provenance *as a source for RETURN to a kept idea* (D1-B/B1
evidence is exactly what a return target needs); the host corpus + SMF pipeline as a regression/listening instrument.

**STOP** (no current musical consumer): more preservation dimensions; repeated lineage A→A′→A″; persistent lineage
graph / ReturnTarget persistence; universal motif or harmonic equivalence; a Genre validity checker; NEW_IDEA; any further origin-evidence fields.

**REDIRECT** (right mechanism, wrong abstraction level): `D`/`V` keys (Revoice/Connect) → primitives *beneath* a real
DEVELOP; `IdeaClassification` toast → audible/plain feedback or nothing; Phrase-page `BREAK` role label → bind to
`BarFunction::Break` or drop; phrase law chosen by profile lottery → a user-level DEVELOP/BREAK/RETURN verb; provenance/anchor
machinery → "return to the thing I kept".

## 11. Decision

**M0-A → FIX REALIZATION FIRST** *(provisional until the listening cards are answered)*.

Why (structure, not code cleanliness): form declarations exist and even causally change output, but they reach the
audible result weakly — development is drum-only and a few cells wide, Acid's articulation is declared but absent, Break/Return
are rare/rhythm-only, 8-bar phrases exist for one genre, and two genres never enter phrase evolution at all. Exposing
DEVELOP/BREAK/RETURN now would put a control on a mechanism whose output is barely different.

What would flip it: if listening answers to cards for `*_develop_return` / `*_break_return` are mostly YES on
"related but changed" and "recognizably a return", the answer becomes **IMPLEMENT MUSICAL PLAY P0**. If listeners
find even the strongest contrast (DnB/Dub P3) indistinct *and* the timbre-removed pairs unrecognisable, revisit
**REVISE MUSICAL MODEL**.
