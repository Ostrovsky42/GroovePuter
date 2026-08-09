# Groove Vocabulary — Stage 5 Acceptance

Status: implementation candidate / hardware correction iteration / not complete until CI + three clean reviews.

Stage 5 migrates only the already-strong rhythmic paths from legacy rhythmic placement to the Stage 1–4 Groove Vocabulary stack. It does not attempt to rehabilitate weak genres and does not introduce later-stage semantic voice ownership.

## Base and stack

Stage 5 is stacked on the Stage 4 materializer/shadow implementation and refreshed with the current `dev_0.9_test` UI/runtime state before live migration begins.

The prerequisite sync branch also normalizes historical removed page IDs so a persisted session cannot reopen the deleted standalone GENERATION/TEXTURE or synth-parameter pages.

## Runtime boundary

The only live Stage 5 opt-in point is GENRE `MATERIALIZE` / `MATERIALIZE+BPM`.

Execution order is intentionally conservative:

1. run the complete existing `MiniAcid::regeneratePatternsWithGenre()` path;
2. keep that result as the authoritative rollback source and as owner of synth pitch/timbre, Atlas tempo metadata and unsupported genres;
3. select an explicit Stage 5 route from `Scene::genre`;
4. realize one ReferenceVocabulary archetype through `RhythmPhraseRealizer`;
5. materialize a scratch drum candidate;
6. for Dub Techno / Deep Chord only, relocate the already-generated legacy Synth B events onto the realized `ChordRhythm` onset topology;
7. commit the migrated material only if every required step succeeds.

There is no user-facing backend switch.

## GENRE / VARIANT selection contract

`GENRE` and `VARIANT` are not independent generators. A recipe is a curated variant of one visible genre family; selecting an incompatible genre must normalize the variant back to `BASE` rather than allowing the recipe to silently mask the selected genre.

Current visible grouping:

- Acid: `BASE`, `Chicago Jack`, `Rolling Acid`;
- Rave: `BASE`, `Psytrance`;
- Reggae / Dub family: `BASE`, `Dub Techno`, `Deep Chord`, `Minimal Space`;
- Broken / Breaks family: `BASE`, `UK Garage`, `Drum&Bass`, `Footwork`, `Classic 2-Step`, `Dark Skippy`;
- Outrun, Techno, Electro, TripHop and Chip: `BASE` only in the current UI.

This grouping is a UI-selection contract only. It does not migrate weak recipes into Stage 5; unsupported recipes still take the exact legacy generation path.

## Explicit allow-list

Stage 5 may migrate only:

- base Acid;
- base Techno (`GenerativeMode::Darksynth` is the retained internal enum value);
- base Rave;
- Drum&Bass recipe;
- Dub Techno recipe;
- Chicago Jack recipe;
- Rolling Acid recipe;
- Deep Chord recipe.

All other base modes and recipes remain legacy, including the weak paths reserved for Stage 12. Cross-recipe morphs remain legacy because weighted vocabulary selection is not owned by Stage 5.

Routing by broad `GrooveboxMode` is forbidden because it would accidentally absorb Reggae, Trip-Hop, UK Garage, Footwork or related modes before their dedicated migration/rehabilitation stage.

## Archetype pools

### Acid

- `straight_acid`
- `rolling_acid`
- `syncopated_acid`
- `sparse_acid`

### Techno

- `straight_drive`
- `offbeat_open_hat`
- `hypnotic_sparse`
- `broken_techno`

### Rave

- `straight_drive`
- `offbeat_open_hat`
- `broken_techno`
- `shuffled_4x4`

### Drum&Bass

- `two_step_roll`
- `ghosted_roll`
- `sparse_fast_break`
- `halftime_switch`

### Dub Techno

- `one_drop_space`
- `steppers`
- `sparse_skank`
- `chord_response`

### Chicago Jack

- `straight_acid`
- `sparse_acid`

### Rolling Acid

- `rolling_acid`
- `syncopated_acid`

Chicago Jack and Rolling Acid deliberately use disjoint Stage 5 archetype pools after hardware listening found the previous overlap audibly too similar.

### Deep Chord

- `chord_response`

Deep Chord is deliberately chord-centric in the correction iteration. Broader Dub grammars such as `one_drop_space` and `steppers` remain available to Dub Techno but no longer define Deep Chord.

## Determinism

Stage 5 adds no hidden `rand()` dependency and no persisted generation seed.

The deterministic context uses:

- selected generative mode;
- selected recipe/morph metadata;
- explicit strong-route ID;
- existing global pattern address.

The current pattern address is the variation coordinate. Re-entering the same genre/recipe/pattern context must produce the same archetype and realization. Different pattern addresses must provide useful deterministic variation over the allow-listed pool.

A future explicit generation ordinal may add intentional reroll semantics; Stage 5 does not smuggle reroll behavior through legacy random output.

## Drum ownership

On a successful migrated route Stage 5 replaces all eight physical drum voice patterns with the scratch Vocabulary result. This is intentional: leaving legacy tom/clap/aux events in otherwise migrated material would contaminate the relational groove.

Stage 5 does **not** overwrite:

- `PatternGroove`;
- drum automation lanes;
- global FEEL state.

On unsupported route, invalid context, realization failure or materialization failure the legacy drum output remains unchanged.

## Dub / Deep Chord stab compatibility

The architecture brief requires Dub / Deep Chord drums **and stab rhythm** to migrate in Stage 5.

Stage 5 does not create `VoiceRole::Chord` ownership to satisfy that requirement. Instead it uses a deliberately narrow compatibility adapter:

- legacy procedural generation already treats physical Synth B as voice 1 / lead-or-arp;
- the existing Atlas Deep Chord corpus uses target 8 for the low/bass line and target 9 (Synth B) for chord/stab events;
- therefore only Dub Techno and Deep Chord may bind `ChordRhythm` to physical Synth B in Stage 5;
- the Vocabulary plan supplies onset coordinates only;
- note pitch, velocity, timing, probability, accent, slide, FX and timbre are copied from the already-generated legacy Synth B events in chronological order;
- if no legacy Synth B pitch event exists, the compatibility binding fails and neither migrated drums nor stab topology is committed.

This binding is runtime-local and intentionally temporary. Stage 7 replaces physical compatibility assumptions with semantic `VoiceRole` runtime ownership.

Synth A is never bound by Stage 5; bass pitch/rhythm materialization remains reserved for Bass Generator v2 and later performance-policy stages.

## Persistence boundary

Stage 5 must not add any persisted field for:

- backend selection;
- vocabulary/archetype ID;
- generation seed;
- phrase ordinal;
- physical/semantic role binding.

`Scene::genre` remains the persisted selection input. Generated patterns remain ordinary existing pattern data.

## Automated acceptance

The Stage 5 host matrix must pass under GCC, Clang and ASan+UBSan and must verify:

- exact strong-route allow-list;
- unsupported route fallback;
- cross-recipe morph fallback;
- invalid mode/address/P-level rejection;
- no mutation on all fallback/failure paths;
- deterministic same-context output;
- deterministic variation across pattern addresses;
- legal P1/P2/P3 realization;
- preserved PatternGroove and automation;
- GENRE-scoped VARIANT selection and incompatible-variant normalization;
- disjoint Chicago Jack / Rolling Acid archetype pools plus structural divergence across the listening-address sample;
- Deep Chord remains on `chord_response` and produces non-empty chord onsets;
- Dub Techno and Deep Chord Synth B onset masks exactly equal realized `ChordRhythm` masks;
- legacy Synth B pitch/performance event sequence survives relocation;
- empty Dub/Deep Chord pitch source rolls back both drums and Synth B;
- non-Dub routes leave Synth B unchanged;
- source ownership guard forbids broad `GrooveboxMode` routing, Scene persistence, Synth A compatibility binding and fixed-note synth materialization.

Repository gates on the same final SHA:

- Stage 1 focused suite;
- Stage 2 focused suite;
- Stage 3 reference vocabulary suite;
- Stage 4 materializer/shadow suite;
- Stage 5 host matrix;
- SDL build;
- Cardputer ADV normal build;
- Cardputer ADV fixed-DRAM gate;
- SEQTRAK MIDI-only build;
- synth persistence suite;
- Phrase Core suite;
- current two-page GENERATE/Four-axis UI suite.

Any red introduced before the known inherited post-suite core-regression assertion is a Stage 5 blocker.

## Hardware listening gate

Hardware listening uses normal GENRE `MATERIALIZE`, not Stage 3A audition mode.

First hardware pass on the pre-correction Stage 5 head:

- Acid base — PASS;
- Techno base — PASS;
- Rave base — PASS;
- Drum&Bass — PASS;
- Dub Techno — PASS;
- Deep Chord — FAIL, insufficient chord-specific identity;
- Chicago Jack / Rolling Acid — FAIL, insufficient audible separation;
- Chip — weak legacy path, not a Stage 5 regression;
- Classic 2-Step / Dark Skippy — weak and too similar legacy material, reserved for later weak-genre rehabilitation.

The first hardware pass also exposed the independent `GENRE`/global-`VARIANT` selector bug: a strong recipe could mask the selected genre. The correction iteration scopes variants to genre families and resets incompatible selections to `BASE`.

Correction retest is intentionally narrow:

1. Deep Chord — several pattern addresses, confirm a clearly chord/stab-centric topology and preserved usable Synth B pitch character;
2. Chicago Jack — several pattern addresses;
3. Rolling Acid — the same addresses, confirm audible distinction from Chicago Jack;
4. GENRE/VARIANT UI — verify incompatible recipes cannot be carried across genre changes;
5. one short regression smoke each for Acid base, Techno base, Rave base, Drum&Bass and Dub Techno.

Stage 5 is not accepted if the corrected strong routes are technically valid but still audibly collapse onto the same musical identity.

## Explicit non-goals

Stage 5 does not implement:

- BarEvolution;
- semantic VoiceRole runtime;
- VoiceRole persistence;
- bass pitch generation;
- bass performance policy;
- phrase/motif vocabulary;
- weak-genre rehabilitation;
- vocabulary expansion beyond the curated Stage 3 reference set.

Those remain Stage 6–13 responsibilities.

## Completion rule

After the implementation and all required gates are stable on one unchanged SHA, perform three consecutive clean reviews at increasing depth:

1. scope / ownership / diff review;
2. state safety / determinism / runtime-memory review;
3. CI / embedded / DRAM / acceptance review.

Any finding resets the count to `0/3` and requires a new final SHA before reviews restart.
