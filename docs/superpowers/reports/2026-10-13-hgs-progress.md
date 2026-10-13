# Unified Harmonic System v2 execution progress

## HGS2-0 — legacy replay witness and focused runner

- Worktree: `/home/gg/.codex/worktrees/performance-smart-play`.
- Branch: `feature/smart-play-return`.
- Frozen starting code SHA: `5fbccbe6e06c161343a9f172ef2a770d43a122f0` (SP-6/SP-7
  implementation is committed; no HGS2 production changes existed at this point).
- Pinned HGS v2 source: upstream commit
  `70ec2011098487c6daf551bd8cb6ff38a9294578`, blob
  `63d4ff762b404c3e952a1c79f6068dddf19063f9`.
- HGS v1 is historical prior data only; v2 architecture controls implementation.
- Added `tests/run_hgs_tests.sh` with exact named target `legacy`; no arguments run every
  registered target, unknown targets fail, `CXX` and `HGS_CXXFLAGS` are respected, and test
  binaries are written below `build/hgs-tests/`.
- Added `tests/test_hgs_legacy_replay.cpp` and frozen semantic-field fixture
  `tests/fixtures/hgs_legacy_replay.tsv`. The fixture contains 3,840 successful source/trace
  rows: seeds 0..63, AUTO across all eight valid RhythmFamily values, each legacy explicit
  ProgressionId 1..7, and phrase lengths 1/2/4/8. It also records six invalid request/source
  outcomes. The serialization names every semantic event member and never compares struct
  padding.
- Compile-time guards freeze the existing enum ordinals and baseline sizes:
  every ProgressionId, ChordQuality, RhythmFamily and GenerationDomain ordinal,
  ChordProgressionStatus 0..3; `HarmonicEvent=3`, `ChordProgressionSource=14`,
  `ChordProgressionSourceRequest=16`, `ChordProgressionSourceResult=15`,
  `ChordProgressionPlan=26`, `ChordProgressionRequest=16`,
  `HarmonyEventKey=16`, `HarmonySnapshot=56` bytes on host GCC 13.3.
- `bash tests/run_hgs_tests.sh legacy` — PASS before any HGS2 production change.
- `HGS_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' CXX=g++ bash tests/run_hgs_tests.sh legacy` — PASS.
- Unknown-target rejection — PASS (exit 2); `git diff --check` — PASS.
- Pinned v2 design SHA-256: `d3a288a2c57dbdda1e04c8cd295429a699ffb5ec55c0dae4fd8b47e4398ef7c2`.
- Frozen replay fixture SHA-256: `f872619624e74d88766a6d8f423a86c153b5d3ef0d70a5215e42c716a8833da8`.
- No production harmonic routing changed. Phase A remains mandatory; conditional HGS2-9/10
  remain pending listening evidence. Production mapping is still pending; the base contract
  explicitly excludes treating every device genre as a full HGS profile.
- Hardware and listening acceptance remain `NOT_RUN`.
- HGS2-0 commit: `a32a83b3` (`test: freeze legacy harmonic replay`).

## HGS2-1a — pinned offline chord/scale vocabulary — complete

- Added `source_lock.json` for Fannon commit
  `352f28004a3dc0f1a130682b925c4673f8a3d13b` and ldrolez commit
  `79d8dfcd7072bd9027231575b1d6c5f1ed809335`. Locks include exact source/package-lock/license
  hashes. Full Fannon, ldrolez and Tonal.js MIT notices are in `tools/harmony/licenses/`.
- Fannon source hashes: `src/generateChords.ts` 61abf9e8…f36aa51,
  `src/generateScales.ts` fa9edf91…f30c70bce, `package-lock.json`
  7fbbe80b…de4bbf11. Resolved @tonaljs/tonal 4.10.0, chord-type 4.8.0,
  scale-type 4.8.1; all MIT. `npm ci --ignore-scripts --no-audit --no-fund` ran in
  ignored `build/hgs-cache/`, not the project root. Python uses system 3.12.3 stdlib; no venv
  or third-party Python dependency is needed.
- Added a safe TypeScript exporter that parses only the pinned source alias arrays and calls
  Tonal.js at C. It does not import/execute either upstream generator and writes no MIDI.
  Source commit, files, package versions/licenses and 24/17 input cardinalities are checked.
- Added root-independent normalizers, explicit registry IDs, deterministic merge/provenance,
  digest validation and quarantine output. Existing IDs 0..9 are unchanged; IDs 10..22 add 13
  chord formulas and IDs 10..16 add 7 scale formulas. Source aliases/root/export variants merge
  by exact pitch-class formula. The fixture compiles to 23 unique quality formulas and 17
  scales, with 0 quarantined rows.
- `runtime_bootstrap_count=10` keeps the generated firmware vocabulary on the existing
  bootstrap prefix. `python3 tests/test_harmonic_registry_generation.py` confirms byte-for-byte
  parity with the checked-in generated header; the broad offline IDs do not expand runtime
  selection in this checkpoint.
- `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`); six Python vocabulary tests pass.
- Exported twice and imported twice from the pinned checkout; `cmp` confirms both export JSON
  and normalized vocabulary/manifest bytes match. Normalized content SHA-256:
  `955909d61666537c8ae756a2f895267e793f149c0da8e1474c657ad869b2f16e`.
- `git diff --check` — PASS. No HGS runtime graph or genre behavior changed.
- NEXT_TASK=HGS2-1b: source-specific Roman progression parser from ldrolez `chords.py`.
- HGS2-1a commit: pending local commit.

## HGS2-1b — source-specific progression parser — complete

- Start SHA: `0c3ded40c4b366a56e7a2e1940650e1dc11fb388`.
- Locked ldrolez source commit `79d8dfcd7072bd9027231575b1d6c5f1ed809335`, including
  `chords.py` and the `c2m.py` slash-bass caller. Locked the actual Mingus 0.6.1 tag
  commit `f131620eb7353bcfbf1303b24b951a95cad2ac20`; pinned hashes cover
  `progressions.py`, `chords.py`, and the full GPL-3.0 license notice. No upstream
  Python module is imported or executed; parser input extraction uses Python AST literals.
- `normalize_roman.py` preserves source token spelling, degree, accidental, registry
  quality ID, source dialect, key-derived context, slash-bass root and interval.
  Major/modal source lists resolve against major keys; minor lists resolve against a
  natural-minor key. Thus source case is preserved as metadata, while Mingus key context
  determines the implicit chord quality.
- Fixture tests pin `I7`=major seventh, `Idom7`=dominant seventh, and `ii5`=power fifth.
  Transposed C/D absolute-chord fixtures normalize to the same route as Roman `I V vi IV`.
  Repeated route tokens remain repeated; mood labels remain source annotations.
- Import covers 195 source rows: 189 accepted, 6 quarantined. The quarantines are exactly
  the routes containing `M-5`, `add9`, or `madd9`, which are absent from Mingus 0.6.1's
  resolver dictionary. Full rows, raw strings, labels and source line locators are retained;
  there is no triad fallback. Corpus fixture and importer agree on counts.
- **Ruling:** quarantine the resolver-unknown suffixes instead of assigning guessed formulas.
  This follows the pinned resolver and no-silent-fallback contract; cost if wrong: six
  routes are omitted from transition priors until a deliberate alias/resolver decision.
- Importer outputs twice compared byte-for-byte. Latest normalized content SHA-256:
  `3d15aea7f38da67c7daeb369f0190314e27ebee01bb6d8f452a4c8977d67880d`.
- `python3 tests/test_harmony_roman_import.py` — PASS (4 tests; all 195 rows accounted).
  `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`, `roman`); registry generated
  header byte parity — PASS; unknown HGS runner target returns exit 2; `git diff --check` — PASS.
- HGS-only offline importer. Hardware/listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-1c: normalized corpus provenance/schema and reproducible manifest.

## HGS2-1c — normalized source manifest — implementation complete

- Start SHA: `5c17c3c07855da56fcf2eba1a01a28c8ef40e366`.
- Added `validate_harmony_model.py` to combine the pinned vocabulary and progression
  importer outputs into schema v1 `harmony_manifest.json`. It validates source commits,
  exact source file hashes, normalized input checksums, schema versions, stable registry IDs,
  formulas/masks/cardinality, route references, source locators, and distributed license
  notice hashes before writing output.
- The final manifest carries quality/scale registries, aliases and canonical route tokens,
  sorted mood tags, source row locators and source/license provenance. Fannon is explicitly
  `vocabulary_only`; it cannot add rows to progression corpus totals. Every unique normalized
  route receives weight 1; exact duplicate routes merge locators/tags; ordered repeated
  tokens remain in place. Input order does not affect compiled output.
- Full pinned source result: 195 rows; 189 accepted, 6 quarantined, 188 unique routes,
  1 duplicate. `content_sha256=993d1045ca03180c8d30f69b2e397aff50fb8ba802c28b95564b8b6b3f3a638b`.
  Full result is pinned as small cardinality/checksum fixture, not a generated firmware table.
- Actual validator CLI run twice against pinned source outputs; `cmp` confirmed identical
  `harmony_manifest.json` bytes (file SHA-256
  `139c3349d855fc3cd8de714581159735881d55f28a7125726b47e1dc4fbf549b`).
- `python3 tests/test_harmony_source_manifest.py` — PASS (5 tests, including full pinned
  fixtures, deterministic permutation, one-weight dedup, ordered repeats, vocabulary/corpus
  separation, malformed ID/schema/reference/checksum/provenance rejection).
  `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`, `roman`, `manifest`); registry
  generated header parity — PASS; Python `py_compile` — PASS; `git diff --check` — PASS.
- HGS2-1c does not change runtime or firmware. Hardware/listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-2: pitch-set algebra and exact bounded pitch-class voice-leading.

## HGS2-2 — pitch-set algebra and exact bounded PC voice-leading — implementation complete

- Start SHA: `80322bc24474c7df29d4d9293e2743079078bdf4`.
- Added checked 12-bit transposition/inversion, common-tone count, symmetric set-difference
  count, six-bin interval-class vector, semitone/fifths root distances, and exact injective
  pitch-class voice-leading cost. Invalid high bits or roots return `Invalid` and leave
  output values unchanged.
- PCVL uses a 4096-byte fixed subset cost table, no allocation or recursion. It assigns every
  tone on the smaller side to a distinct tone on the larger side, then applies lambda=3 per
  unmatched tone. Optional caller-owned counters report state/transition work. Six-tone
  candidate case uses exactly 64 subset states; pure helper accepts all cardinalities0..12.
- Tests exhaust all4096 masks for T12/T composition and I_n involution, validate ICV pair
  totals, all144 root-distance pairs, transposition invariance, invalid input behavior, and
  exact PCVL against a brute-force injective oracle for all mask pairs whose cardinality is
  at most4. Also checked C→Cm=1, C→Em=1, C→Am=2, empty→triad=9, unequal cardinality, and
  the repeated-target counterexample (nearest-per-source gives1; injective cost is3).
- `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`, `roman`, `manifest`, `algebra`,
  `pcvl`). `HGS_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' CXX=g++
  bash tests/run_hgs_tests.sh algebra pcvl` — PASS. `clang++` unavailable in this host.
- GCC 13.3 `-O2 -fstack-usage`: `pcVoiceLeadingCost` frame160 bytes,
  `intervalClassVector`48 bytes; scratch object exactly4096 bytes. Object `.text`=2002 bytes
  on host x86-64 (not an ESP32 flash estimate).
- HGS2-2 is pure host-tested math; no DSP, output or project ownership changed. Hardware and
  listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-3: harmonic relation scalar features and overlapping semantic tags.

## HGS2-3 — relation features and semantic tags — implementation complete

- Start SHA: `9550dd272a304365fbfe1a7b5127ac06bd6f854d`.
- Added bounded `HarmonicRelationFeatures` (16 bytes) and `analyzeRelation()`. It validates
  both chord quality/mask pairs and both tonal scale/mask pairs before calculating features;
  invalid input leaves caller output unchanged. Metrics include common tones, set difference,
  exact PC voice-leading, interval-vector distance, root/bass motion, out-of-scale tone counts,
  complexity delta, context identity, scale overlap/difference, and pivot coverage.
- Added stable relation tag bits 0..14. Predicates are annotations and do not impose legality:
  PRL requires major/minor skeleton match and compatible added tones; diatonic-functional
  requires a same-scale root change with both chords in scale; dominant-to-target uses the
  root/family rule, while secondary-dominant additionally requires source tones outside the
  source scale. Borrowed-parallel is limited to parallel major/minor contexts; modal-mixture
  requires same tonic, changed scale, and newly foreign target tones. Chromatic-mediant uses
  third-related roots and at least one contextual outside tone; augmented/passing annotations
  require the bounded root/common-tone predicates pinned in their tests. Pivot compatibility
  records coverage in the shared scale; `PIVOT_CHORD` requires a chord fully inside that
  intersection.
- Tests cover all 15 tags under all 12 chromatic transpositions, Cmaj7→Em7, G7→Cm,
  E7→Am, overlapping C→Am PRL_R + diatonic-functional, borrowed/modal cases, pivot coverage,
  augmented and diminished relations, shared-root and incompatible extension false positives,
  invalid masks, and unchanged output on invalid input.
- `bash tests/run_hgs_tests.sh` — PASS (legacy, vocab, roman, manifest, algebra, pcvl, relation).
  `bash tests/run_hgs_tests.sh relation algebra pcvl legacy` — PASS.
  ASan/UBSan `relation algebra pcvl` — PASS. `git diff --check` — PASS.
- GCC 13.3 `-O2 -fstack-usage`: `analyzeRelation` frame 160 bytes; called PCVL frame 160 bytes;
  fixed PCVL scratch remains 4096 bytes. Host-only 32-candidate microbenchmark: 10,000 rounds
  × 32 candidates, 320,000 successful analyses in 148,106 μs (~14.8 μs per 32-candidate
  batch on this host). This is not an ESP32 timing claim.
- HGS2-3 adds no DSP, MIDI, material routing, genre policy, or hardware-facing integration.
  Hardware/listening acceptance remains `NOT_RUN`.
- NEXT_TASK=HGS2-4: compile compact corpus transition priors and runtime lookup.

## HGS2-4 — compact corpus prior compiler and runtime lookup — implementation complete

- Start SHA: `4dcc0a187e2f79abc53818fcfca82f2fe6f5a234`.
- Added deterministic offline transition counting from the validated normalized manifest:
  unigram, bigram, trigram, quality-family transition, root-motion/quality transition,
  cadence-ending, and modal/borrowed-edge counts. Each unique normalized route contributes
  exactly one vote; repeated events inside it retain order and multiplicity. Fannon vocabulary
  and mood words contribute no observations. Canonical token IDs sort explicit fields and are
  invariant to source ordering/key transposition. Content hashes exclude paths/timestamps and
  retain the validated source-manifest digest and source commit provenance.
- Added fixed byte-encoded runtime tables and bounded `corpusPrior()` lookup. Backoff is exact
  trigram → bigram → coarse quality/root-motion class → 0. A zero prior is documented and
  tested as missing evidence, not a legality decision. Token table uses 8 bytes/entry; trigram,
  bigram and coarse entries use 7, 5 and 4 bytes, avoiding ABI padding assumptions.
- Pinned-corpus result: 188 unique routes, 64 canonical tokens, 358 unique trigram successors
  across 209 contexts and 460 weighted observations. Top-N=2 retains 288 successors / 389
  observations; N=4 retains 343 / 445; N=8 retains 357 / 459. N=8 selected: it preserves all
  but one observed successor and costs 98 bytes more than N=4. Generated table totals 4,544
  explicit data bytes (512 token, 2,499 trigram, 1,285 bigram, 248 coarse). This data-byte count
  is not a whole-firmware flash measurement.
- Runtime test verifies exact trigram priority and bigram/coarse/zero fallbacks; Python tests
  hand-count repeated route transitions/cadences, prove source order reproducibility, reject
  invalid weights/tokens/top-N settings, and regenerate the checked-in table from pinned
  fixtures byte-for-byte.
- `bash tests/run_hgs_tests.sh` — PASS (all registered targets including corpus).
  ASan/UBSan `corpus relation algebra pcvl` — PASS. Two independent model generations compared
  byte-for-byte. GCC 13.3 `-O2 -fstack-usage`: `corpusPrior` frame 80 bytes. `git diff --check`
  — PASS.
- No production ChordProgression routing, genre policy, DSP, or hardware integration changed.
  Hardware/listening acceptance remains `NOT_RUN`.
- NEXT_TASK=HGS2-5a: build concrete genre corridors and explicit catalog binding.

## HGS2-5a — genre corridors and catalog binding — implementation complete

- Start SHA: `6b9ef9b900c30acc8dc582b83dc88d320dbc1fe4`.
- Added source-owned 17-style matrix and hard budgets, generated compact policy table, and
  inspection report. `HarmonicStyleId` IDs are explicit 0..16, independent of persisted
  `GenerativeMode`. Quality/scale admission IDs are clipped to the current bootstrap prefix
  (quality/scale IDs0..9); Q3 can add the supported augmented triad but offline 13/altered/
  sus2 formulas and non-bootstrap named scales remain unavailable to runtime.
- Implemented integer feature coefficient formula and a hard candidate gate for quality,
  scale, contextual outside tones, section change/modulation limits, portals, color budget,
  functional-preparation capability, and project structural STATIC/HOLD restrictions.
  Weights do not make a candidate illegal. Test confirms Salsa C→Am passes despite PRL weight0;
  Acid explicitly disallows functional-preparation requests.
- Added exact catalog mapping for Base modes and contract-listed recipe IDs; nontrivial genre
  morph and unmapped valid recipe/mode return `Unsupported`, malformed persisted mode/recipe/
  morph target returns `Invalid`, and failure leaves output untouched. All 17 policies remain
  directly accessible to host profile tests; eight designated styles are marked host-only and
  are not added to device UI.
- Mapping test exhausts 16 modes × 22 recipes × morph amounts 0/128/255, plus same-target morph
  and invalid values. Python tests compare all weight/budget rows to the contract, validate
  capability clipping and coefficient formulas, reject invalid ranges, and check deterministic
  generated header/report output.
- `bash tests/run_hgs_tests.sh` — PASS (all targets through genre).
  ASan/UBSan `genre` — PASS. `python3 -m py_compile` and `git diff --check` — PASS.
  Cardputer ADV `scripts/build.sh --warnings all` — PASS; image 1,546,414 bytes, global RAM
  187,172 bytes. Existing Arduino core and legacy warnings remain in the build log.
- This is policy/configuration infrastructure only; no progression producer consumes it yet.
  It produces no new audible behavior. Hardware/listening acceptance remains `NOT_RUN` and is
  premature until the bounded route planner and playback adapter are connected.
- NEXT_TASK=HGS2-5b: stable 14-field song genome and reserved independent RNG domains.

## HGS2-5b — stable song genome and independent RNG domains — implementation complete

- Start SHA: `f5bd52e4e35ffd9e86d0182151c4ad986cb06416`.
- Added a fixed 14-byte `SongVariantGenome`, derived from project seed, full 32-bit stable
  song identity, harmonic style and model version. It has no section input, so section changes
  cannot silently re-roll the song's profile. Each field uses its own stable salt in reserved
  `HarmonicGenome` domain 19; route, modulation, tie-break, vertical-color and arp domains
  remain independent at IDs 20..24. Existing domain IDs 0..18 remain unchanged.
- Genome biases are sampled inside style policy caps. Invalid styles produce a zero genome;
  invalid/non-harmonic RNG domains return seed zero. Tests sweep 256 project seeds × all 17
  styles, verify policy caps, repeatability, identity/style/version sensitivity, and that route
  seed does not change when other domains are evaluated.
- Added golden vectors for project seeds 0, 1, 42 and `0xffffffff`, with model version 7,
  UK Garage style, stable song identity `0x1234abcd`, and the pinned corpus model checksum.
- `bash tests/run_hgs_tests.sh` — PASS, including the legacy replay suite and all HGS targets.
  `git diff --check` — PASS. GCC 13.3 host test — PASS. Clang was unavailable in this
  environment, so the second-compiler check could not run.
- Cardputer ADV `scripts/build.sh --warnings all` — PASS; image 1,546,502 bytes, global RAM
  187,172 bytes. Build emitted existing Arduino core/legacy warnings. No HGS route is connected
  to production generation, so this build has no new audible HGS behavior. Hardware and
  listening acceptance remain `NOT_RUN` and premature.
- NEXT_TASK=HGS2-6a: bounded candidate and tonal-context generation.

## HGS2-6a — bounded candidate and tonal-context generation — implementation complete

- Start SHA: `ec662daaebe6715e3a93ab9aa5417bb71dc19caaf`.
- Added fixed-cap candidate collection: up to32 chord candidates and8 tonal contexts. Root
  candidates are deduplicated and ordered from current/home roots, current/home scale tones,
  then bounded fifth/third/parallel/relative/neighbor/tritone/portal relations. Quality choices
  use a three-item per-root shortlist over the ten currently supported runtime qualities; every
  emitted state is explicitly analyzed against its target tonal context.
- Current HOLD is reserved first; exact home state/context is reserved for RETURN before the
  round-robin optional fill. Candidate uniqueness includes root, quality, bass and context.
  Genre hard gates and runtime quality/scale capability masks are applied before publication.
  Empty capabilities produce a valid empty result; unsupported style or malformed input fails
  without replacing candidate output. Corpus zero is retained as “no evidence” and does not
  remove otherwise legal candidates.
- Tests cover empty, one-item and saturated candidate lists; the 32-slot RETURN home reserve;
  zero-prior legal candidates; quality capability clipping; STATIC HOLD; high-risk genome not
  bypassing hard caps; malformed style/state and unchanged output on failure. ASan/UBSan
  `candidates` — PASS. Full `bash tests/run_hgs_tests.sh` — PASS.
- Fixed-memory costs: candidate payload is at most768 bytes (32×24), tonal contexts at most
  96 bytes, and caller-owned PCVL scratch is4096 bytes. GCC 13.3 `-O2 -fstack-usage` reports
  `collectCandidates` frame1104 bytes; PCVL scratch is supplied by the caller. This host stack
  measurement is not an ESP32 timing claim. ADV firmware build passed at 1,546,590 bytes flash
  and 187,172 bytes global RAM; existing Arduino/legacy warnings remain.
- Candidate planning is still host-only and is not wired into current progression playback.
  Hardware/listening acceptance remains `NOT_RUN`; there is no audible HGS change to evaluate.
- NEXT_TASK=HGS2-6b: route fingerprints and bounded recent-harmony memory.

## HGS2-6b — normalized route fingerprints and session memory — implementation complete

- Start SHA: `95ca6f6fcf00fcd4cf68c481ff63833a87b93303`.
- Added shared `ResolvedHarmonicEvent` / `ResolvedRouteView` types and five versioned
  64-bit FNV-1a signatures: root motion and bass relative to immutable home, quality sequence,
  semantic relation/context, cadence, and scale context relative to home. Every serialized value
  is written byte by byte; no struct padding, `std::hash`, or absolute tonic enters normalized
  signatures. Route count, event pointer, state/scale masks, cadence IDs, home identity and
  strictly increasing timing slots are checked before hashing. Invalid routes leave output
  untouched.
- Added fixed eight-entry `RecentHarmonyMemory` plus eight compact local edge tokens. Preview
  penalty is a pure read, explicit accepted-route/edge commit functions are the only append
  operations, and reset clears the session ring. Exact route signature matches receive the
  versioned weights4/3/2/2/1; repeated edges add a bounded weight2 term. Both scale by
  repetition tolerance. No approximate hash-distance comparison is used. Set is40 bytes;
  memory is336 bytes.
- Tests prove C–Am–F–G and transposed D–Bm–G–A normalize identically, while changed quality,
  mode interpretation and cadence affect their intended signatures. Ring wrap after nine
  accepted inserts, immutable preview, deterministic snapshot scoring, error output preservation
  and high repetition tolerance are covered. ASan/UBSan `fingerprint candidates relation` —
  PASS; full HGS host suite — PASS; `git diff --check` — PASS.
- This remains offline session logic; no generator owner calls it yet and no playback behavior
  changed. Hardware/listening acceptance remains `NOT_RUN`.
- NEXT_TASK=HGS2-6c: integer route scorer, bounded route chunks and home-context RETURN search.

## HGS2-6c — integer route planner and exact-home RETURN — implementation complete

- Start SHA: `65426fecab55d011d68e920d6634ccee9c56dab6`.
- Added deterministic integer candidate scoring using genre coefficients, SongVariantGenome,
  corpus prior, home distance/gravity, section intent, repeat fingerprints and normalized
  recent-edge penalties. Candidate tie selection and route selection use independent reserved
  RNG domains20 and21. Positive candidate weights clamp to1..255. Bounded diagnostics retain
  selected relation features, score and corpus prior outside the audio render path.
- Added route chunks of at most4 resolved events with timing-slot ordinals and ending
  change/modulation counters for continuity across chunks. TAKE keeps home context; KEEP keeps
  current context; DEVELOP/BREAK apply existing policy hard limits and carry actual changes,
  rather than treating every slot as a change. No transport or DSP owner was added.
- RETURN uses deterministic bounded Dijkstra over admitted relation neighbors: depth≤4,
  expanded nodes≤128, positive relation costs, stable tie seed, and exact home chord plus full
  tonal-context identity as its goal. A distant F-sharp major test returns through a legal
  intermediate chord; same root with the wrong home context does not count as success. Too few
  slots, unreachable home, invalid input and capacity failures leave published output intact.
- Added fixed `RouteScratch` (3,410 bytes) and caller-owned candidate workspace (4,962 bytes);
  GCC13.3 `-O2 -fstack-usage` reports `planRouteChunk` at688 bytes. PCVL storage remains in
  caller workspace. These are host compiler measurements; device execution time is not measured.
- Route tests cover deterministic replay, zero/1/2/3/4/32 timing budgets, saturation limits,
  section context rules, chunk continuity/budget carry, exact-home/capability failure, output
  preservation and diagnostics. Full HGS host suite — PASS. ASan/UBSan `route fingerprint
  candidates` — PASS. Cardputer ADV firmware build — PASS at1,546,630 bytes flash and187,172
  bytes global RAM; existing Arduino/legacy warnings remain.
- The route API is still offline and has no production generation caller. Hardware/listening
  acceptance remains `NOT_RUN`; first audible device test follows HGS2-7 production integration
  and its host acceptance.
- NEXT_TASK=HGS2-7a: Phase A capability projection at the existing materializer boundary.
