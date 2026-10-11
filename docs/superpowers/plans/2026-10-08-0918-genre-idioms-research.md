# 0.9.18 genre idioms: research for bass + lead pairs

Date: 2026-10-08. Input for the idiom prototype (House, Acid, LoFi first).

## Why

Owner after hardware listening: Rave is good; the other genres work and are
smooth but "boring, no catchy melody", "doesn't sound like the genre", "bass and
lead are not one". Priorities: Acid/Outrun/Darksynth, House/Techno/UKG,
LoFi/HipHop/FunkSoul.

Cause in the generator: bass and lead come from one shared cell vocabulary,
chosen independently; the lead only avoids bass onsets (it never answers them);
every bar of a phrase is rebuilt with no memory of bar 1, so no motif returns.

## What the sources agree on

- **Hook = repetition + small change.** A short phrase is played, repeated,
  changed at the end, and played again (AABA / A A' A B). The repeat is what
  makes it stick; the change keeps it alive. In electronic music the hook is
  usually a bright repeating lead or the bassline itself.
- **Call and response.** Non-overlapping phrases answer each other: bass
  "call", lead "response" in the gaps, often higher and busier.
- **Busy bass, simple lead (and the reverse).** Acid, darksynth and techno
  basses run in 16ths, so the lead is sparse or a long tone; where the bass is
  sparse (lofi, hip hop, outrun) the lead carries the melody.
- **A simple pattern plus one or two flourishes beats a complex one** (303
  advice: octave jump + slide on one or two notes per bar).

## Notation

Steps `0..15` per bar (step 0 = beat 1, 4 = beat 2, 8 = beat 3, 12 = beat 4;
"and" = 2/6/10/14). Pitches are degrees relative to the **current chord root**:
`R` root, `b3`, `3`, `4`, `5`, `b7`, `7`, `9`, `+12` octave up. `~` slide into
the next note, `!` accent, `_` held (tie).

## Genre pairs

### House (deep/classic, 118-126 BPM, swing 55-60%)
- Bass: **offbeat** `2,6,10,14` R, one octave pop: `2:R 6:R 10:+12 14:R`.
  Variation adds pickups between hits (`3`, `11`, `15`), "four extra notes
  change the whole vibe". Kick stays alone on `0,4,8,12`.
- Lead/chords (Synth B): **minor 7 / 9 stabs**, short, filling gaps between
  kick and bass: `3, 7, 11` or the 3-3-2 `0, 3, 6` + `11`. Voicing m7/m9.
- Hook: the stab rhythm is the hook; same rhythm every bar, chord changes
  underneath; bar 4 drops a stab or adds `15`.

### Techno (hypnotic, 128-135)
- Bass: rolling 16ths off the kick (`1,2,3,5,6,7,9,10,11,13,14,15`) on R, or
  offbeat like house.
- Lead: **one-bar 16th riff on 1-2 pitches** (R and b7 or R and +12), accent
  groups 3-3-2 (`!0 3 !6 8 !11 14`). Identity = sameness; variation only in
  accents/length, a note dropped in bar 4.

### UK garage / 2-step (130-135, swing 58-63%)
- Kick on `0, 2, 8, 9, 10` (rolling), snare `4, 12`.
- Bass: deep, **answers gaps in the drums** rather than following every kick:
  stabs on "and of 2" `6`, "e of 3" `9`, "and of 4" `14`, plus `0`; pitch
  bend/slide up into one note: `0:R 6:b7 9:R~ 14:5`.
- Chords: m7 / m9 / m11 stabs, short, on offbeats; typical Am7-Gmaj7-Fmaj7-Gmaj7.

### Acid (303, 120-135)
- Bass: 16-step line, mostly R with **octave jumps, b3, 5, b7**; slide into
  the octave notes, accents on 2-3 steps:
  `!0:R 2:R~ 3:+12 5:R 6:b3 8:R !10:R~ 11:+12 13:5 14:b7`.
  One or two flourishes per bar, not more.
- Lead (Synth B): second line **in the bass rests**, higher, sparse:
  `4:5 7:b7 12:+12 15:b3` (call/response with the bass).
- Hook: bars 1 and 3 identical, bar 2 transposed to its chord, bar 4 an
  octave run or a slid fill.

### Outrun / synthwave (80-118)
- Bass: **steady 8ths** `0,2,4,...,14`, R/+12 alternating or R-R-R-+12,
  following the chords (i-VI-III-VII).
- Lead: **long notes**, 2-4 per bar, wide (5th/octave leaps), stepwise in
  between: `0:R_ 6:3 8:5_ 14:4`; arpeggio (R 3 5 +12 in 16ths) as the
  alternative lead.
- Hook: A A' A B with the last bar landing on 5 or R.

### Darksynth (100-150)
- Bass: **driving 16ths**, all steps R, octave on `8` and `14`, **b2** as the
  tension note once per bar.
- Lead: menacing short riff, tresillo-like `0,3,6,8,11,14`, degrees
  `R b3 R b2 R 5`; or a long minor-third wail.

### LoFi hip hop (70-90, swing 55-65%)
- Chords: **maj7 / m7 / 9**, held (whole bar or 2 per bar, the second slightly
  early at `15`); ii-V-I, I-vi-ii-V turnarounds.
- Bass: R on `0`, **approach note** on `7` or `14` (step/chromatic into the
  next chord root), long notes.
- Lead: lazy, 2-4 notes per bar, long, on chord tones **3, 7, 9**, mostly
  descending steps, entering late (`2`, `4`, `7`).

### Hip hop / boom bap (85-95)
- Kick `0` and around "and of 2"/beat 3 (`6`/`8`/`10`), snare `4, 12`.
- Bass: **locks to the kick**: notes on the kick steps, R with a b7 or 5
  pickup, fairly long.
- Lead: a **1-2 bar loop** (sample-like), minor pentatonic, 3-5 notes,
  identical every bar; change only every 4th bar.

### Funk / soul (90-115)
- Bass: **16th syncopation, two-bar pattern**, octave pops and ghost notes,
  outlining the 7th chord (R 5 b7 +12): `0:R 3:(ghost) 6:+12 7:b7 10:R 13:5 14:+12`.
- Lead: short clav/horn stabs on 16th offbeats in the bass gaps (`1, 4, 9, 12`),
  call/response with the bass.

## Rules for the generator

1. **Bass and lead are chosen as a pair** (one template = both parts), so they
   interlock by construction: lead mostly in bass rests, both on the downbeat
   at most once.
2. **Phrase form A A' A B** over 4 bars (2 bars: A B; 8 bars: A A' A B A A' A B'):
   A = template; A' = same rhythm, pitches re-fitted to the bar's chord;
   B = same start, changed end that lands on R or 5 (cadence).
3. **Pitches are degrees on the chord of the bar**, mapped into the project
   key, so a template works in every key and follows the progression.
4. **Flourishes are rationed**: slides/accents/octave jumps 1-3 per bar.
5. **Templates are varied, not just picked**: per press, rotate the pitch
   choice inside a slot (R vs +12, 5 vs b7), drop or add one note, swap the
   B-bar ending; LIVELY and NOTES keep working on top.

## Plan

- Prototype: 4-6 pair templates for House, Acid, LoFi; phrase form; render
  before/after for listening.
- Then Outrun, Darksynth, Techno, UKG, HipHop, FunkSoul.
- Rave stays as it is (owner: good).

## Sources

- [MusicRadar: producer's guide to the TB-303](https://www.musicradar.com/news/producers-guide-roland-tb-303)
- [MusicTech: Chicago-style 303 acid bassline](https://musictech.com/guides/essential-guide/how-to-create-a-chicago-style-acid-house-bassline)
- [The Producer School: tech house bass lines](https://theproducerschool.com/blogs/featured-blogs/master-modern-tech-house-bass-lines-complete-tutorial-guide)
- [Attack Magazine: dusted deep house](https://www.attackmagazine.com/technique/beat-dissected/dusted-deep-house/)
- [Music Production Wiki: UK garage](https://musicproductionwiki.com/articles/how-to-make-uk-garage)
- [Native Instruments: lo-fi chord progressions](https://blog.native-instruments.com/lo-fi-chord-progressions/)
- [Music Production Wiki: lo-fi hip hop](https://musicproductionwiki.com/articles/how-to-make-lo-fi-hip-hop)
- [Native Instruments: what is boom bap](https://blog.native-instruments.com/what-is-boom-bap/)
- [Talking Bass: ghost note basslines](https://www.talkingbass.net/bass-technique-ghost-notes/)
- [Mode Audio: the joy of arps (synthwave)](https://modeaudio.com/magazine/the-joy-of-arps-creating-a-synthwave-score)
- [Secrets of Songwriting: repetition and good melodies](https://www.secretsofsongwriting.com/2010/04/21/the-hook-and-other-repeating-elements/)
- [MusicRadar: repeated hooks](https://www.musicradar.com/how-to/songwriting-repeated-hooks)
- [Amped Studio: call and response bassline](https://ampedstudio.com/create-a-call-response-bassline)
- [Sinee: hypnotic techno](https://sinee.de/en/raw-hypnotic-techno-how-to-make-a-track)
