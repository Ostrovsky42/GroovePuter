# Deferred items for 0.9.25–0.9.35

This list separates known unfinished work from verified baseline behavior. Items remain deferred until a later milestone explicitly accepts them.

## Carried from 0.9.24

- **H1 — LIVELY form intensity:** remains blocked. The attempted CALM/NORMAL/LIVELY event ordering did not meet the intended counts and was reverted. No sound-quality claim is made.
- **H10 — form listening kit:** remains skipped because it depended on H1. No owner listening has occurred.
- **Hardware acceptance of 0.9.24:** not performed. It is not a prerequisite for host-side work in this goal.

## Baseline gaps for later milestones

- **Live route/profile rebinding:** active USB output consumes a startup snapshot. Project → MIDI now blocks Device Profile and External MIDI output preview/save while playing; stopped edits remain next-boot settings and do not update running MIDI lanes. Keep dynamic rebinding deferred until a bounded, ownership-aware release/activate transaction is designed and tested.
- **Custom Performance route table:** current persisted settings cannot fully describe A/B/DX/drum Performance routes. Custom projection fails closed; do not infer mappings from Pattern routes.
- **Integrated route and endpoint lifecycle traces:** component tests cover ownership, pending releases, tee failover and UART queue pressure, but tests still need to prove the complete dispatcher path across route changes, per-endpoint loss/rejoin, project replacement and Song transitions.
- **SMF v5 fixture:** source has a v5 reader, but the reviewed legacy fixtures directly cover v3 and v4 only. Add a v5 fixture before changing `.gpp` storage.
- **MIDI export fidelity:** `.33` exports the committed Song as SMF, but temporary Performance macros, Voice audio, Drum FX and internal Drum automation are not rendered. Conditional/probability steps are flattened and marked. This is documented behavior, not a lossless audio/render export.
- **M5Launcher compatibility:** the local package generator emits a separate app-only payload marked `HARDWARE_UNVERIFIED`; exact installation compatibility has not been established on a Cardputer ADV.
- **Final release provenance:** build identity, release-version resolution, clean-source verification and rollback-package support are implemented in `.34`. Exploratory `CDCOnBoot=default` FS1B firmware and packaging evidence exist, but were built from a dirty pre-final source. Rebuild and validate all artifacts from the exact clean `.35` SHA.
- **User documentation alignment:** README, Quickstart, Manual, key map, MIDI setup and troubleshooting describe the internal `.33–.35` line while keeping v0.9.19 identified as the latest published release. `.34` source-regression expectations now match those documents; recheck candidate status and install wording during `.35` handoff.
- **Hardware and musical acceptance:** all physical keyboard, internal audio, USB MIDI, DIN MIDI and SEQTRAK behavior remains **NOT TESTED** until the owner accepts the 0.9.35 candidate.

## 0.9.29 follow-up

- **Macro listening and physical control feel:** SDL verifies readable values and actions only. Whether the four mappings sound useful and whether the Cardputer keyboard is comfortable remains pending for owner acceptance of 0.9.35.
- **Per-loop mutation and permanent commit:** MUTATION is deterministic per source event, and live macro output does not add events or edit the Phrase. Per-loop variation and explicit materialization are not included.
- **Phrase integration in the host suite:** the focused P3 gate covers it; the canonical host suite does not currently invoke `run_pattern_phrase_p3_tests.sh`.

## 0.9.30 follow-up

- **Live chord-aware macro TENSION:** Graph and Legacy harmony already constrain generated material before commit, and current Song chord snapshots reject stale participants. The sequencer macro owner still receives only project key/scale. Keep TENSION on its bounded next-onset heuristic until a fixed, coherent control-side chord publication is available; do not read Song harmony storage in the sequencer callback. See `HARMONIC_FOLLOW.md`.
- **Legacy M1-P1 expectation test:** `run_0_9_9_m1_p1_tests.sh` expects `SparseCall` and `Mirror` for the Electro profile. It fails at the exact 0.9.24 baseline because the current profile selects `AngularCell` and also differs on the next literal expectation. This test contract is not updated in 0.9.30; see the verification record.

## 0.9.31 follow-up

- **Arrangement listening:** INTRO, MAIN, DEVELOPMENT, BREAK, BUILD, DROP and OUTRO are bounded starting contours over existing energy levels and source rows. Their musical usefulness has not been evaluated by listening and remains pending for owner acceptance of 0.9.35.
- **Per-section identity and motif development:** role names are transient FORM labels; materialized Song rows remain ordinary editable positions. The templates do not create independent motif variations or persist section-role metadata.
- **FORM and the active loop range:** FORM preserves the user's current loop bounds. If the inserted arrangement extends beyond the loop end, expand the range to hear the new rows while loop mode is enabled. Do not silently change loop intent without a transactional Undo contract.

## 0.9.32 follow-up

- **Owner listening and performance feel:** host transforms and SDL readability are checked, but macro combinations, perceived timing, internal sound and physical controls need owner evaluation with the 0.9.35 candidate.
- **Pitch Bend, aftertouch and motion/gesture input:** deferred through 0.9.35. Cardputer ADV includes a BMI270 six-axis IMU ([M5Stack specification](https://docs.m5stack.com/en/core/Cardputer-Adv)), but this roadmap does not add motion behavior or claim that its firmware path has been verified. Revisit only after integrated hardware acceptance.
- **Macro-to-CC mapping and automatic expression:** deferred; macros remain musical transformations and do not send controller messages.
- **Live profile rebinding:** still unsupported. The active output uses its startup route snapshot; Project → MIDI locks profile/output changes while playing, and stopped changes take effect on the next startup.
- **Hardware MIDI claims:** USB enumeration, DIN behavior and Yamaha SEQTRAK reception remain `NOT TESTED` until owner acceptance of 0.9.35.
- **CC retry deadline after extreme idle:** the wrap-safe 20 ms limiter can encounter signed half-range ambiguity if a cached deadline remains unused for more than roughly half the `uint32_t` millisecond range. Normal sends initialize/retry the deadline; this extreme-idle case is deferred and needs a focused clock-injection test before changing the limiter.
- **Custom-route fake/UI channel agreement:** the route fake uses channels 5/7 while the Custom UI fallback displays 8/9. Existing tests cover the custom fallback and UI label separately but do not assert their exact shared projection. Tighten this fixture when expanding custom-route support.
