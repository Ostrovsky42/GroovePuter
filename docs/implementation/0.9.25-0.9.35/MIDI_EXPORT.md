# 0.9.33 — MIDI export design and implementation plan

Status: implementation not started. This document records the source audit and the contract for the next milestone. No export behavior is claimed yet.

## Scope

Export the committed in-memory Song to a deterministic Standard MIDI File Type 1. The project format remains the editable source of truth. Export is a read-only projection; it does not regenerate material, edit Song, commit temporary Performance macros, alter playback ownership, or touch Undo state.

Export is a control/UI operation and is refused while `MiniAcid::isPlaying()`. Pattern-page reads, melody reads, SD writes and SMF serialization stay outside audio/MIDI callbacks. The export path must not hold an audio guard across filesystem I/O.

## Source audit

- `Scene::songs[2]` contains the two committed Song slots. `Song::positions` stores global pattern references for Synth A, Synth B, Drums and Voice; `Song::length`, `Song::reverse` and the per-cell `stepsOverride` are persisted with the scene.
- One Song row occupies `Scene::feel.patternBars` bars (1, 2, 4 or 8). `SongPosition` has three pitched/exportable pattern lanes plus Voice. Pattern-page IDs resolve to page/bank/slot using the helpers in `scenes.h`.
- Each pattern page stores the two synth banks, drum banks and material descriptors. `PatternPagingService::loadPage(page, scratch, false)` can load into `sceneTransactionScratch()` without changing the resident Scene or publishing a different active page. Export must serialize this access and must not let a failed page read publish partial events.
- Synth material can be a step `SynthPattern` or a `Melody` payload. `Song::stepsOverrideAt(voice,row)` explicitly selects STEPS even when that slot descriptor is Melody. Melody payloads use `MelodyPromotion::loadMaterial()` and the fixed-capacity `RuntimeSynthEventBuffer` representation.
- Pattern event timing is already defined by `projectPatternToRuntimeEvents()`: 96 PPQN, 384 ticks/bar, step timing, swing, tie/slide gate lifetime, and the Genre recipe's `gateLengthRatio`. Export should reuse this projection rather than invent a second note-duration rule.
- Runtime note durations have 16 subticks per 96-PPQN tick. Export should use SMF division 1536 PPQN and map each runtime tick to 16 SMF ticks, preserving those durations exactly rather than rounding note-offs to 96 PPQN.
- `SceneManager::bpm()` is the saved project tempo. `MiniAcid::bpm()` can instead reflect external clock tempo, so export captures the SceneManager value and ignores transient clock state.
- Existing production parser pieces are read-only (`SmfParser`, `SmfFileIndexer`, `SmfTrackStream`). No SMF writer or committed-Song exporter exists.
- `MidiOutputRouteProjection` already maps musical roles and drum voices to channels/notes. The export core will keep this as a separate projection option; profile-specific mapping must not be copied into musical material or persisted into Song.

## Snapshot and projection contract

1. Refuse while playback is active. On the UI/control thread, copy the selected active Song, saved BPM, `feel.patternBars`, relevant feel/gate settings, project identity, scene revision and the separately selected output-route projection into a bounded `ExportSnapshot`.
2. Treat the copied Song and projection as immutable for the duration of the export. Export processes the full stored Song, independent of current playhead or temporary loop range. Honor the Song's stored reverse direction as one finite pass; do not repeat a playback loop indefinitely.
3. Resolve every row and lane from the copied Song. For Synth A/B, STEPS override selects the page's step pattern first. Otherwise use the slot's declared `MaterialKind`; a missing/corrupt page or melody payload aborts the export.
4. Repeat a step pattern once per bar of the Song row. Repeat a Melody by its stored `lengthTicks` within the row, bounded by the row duration. Clip synth note-offs at the Song-row boundary where `applySongPositionSelection()` calls the existing hard barrier. Preserve note-on/off pairs and overlapping equal pitches as independent events.
5. Export the stored note content deterministically. Do not consult runtime RNG or transient macro values. Since SMF has no native probability/ghost-step field, export all authored note events with their stored velocity and document that probability is not represented by SMF.
6. Emit a conductor track with tempo and 4/4 time signature at 1536 PPQN, then separate Synth A, Synth B and Drums tracks. Track names are stable. Voice is spoken/sample material, not pitched MIDI: include a descriptive text marker and UI notice when the Song contains Voice content rather than silently implying it was exported.
7. Map role/channel/drum note data through the selected export projection after resolving musical events. The first UI path will use the currently applied MIDI device profile's route projection, independent of the external-output enable gate. Pending next-boot settings must not be described as active. Unknown CC and transient controller state are not exported.

## File transaction

- Produce a unique visible `.mid` name using the existing filename policy; never replace an existing export.
- Stream to a hidden temporary path in the MIDI directory. Keep only bounded event scratch and writer state in RAM; do not buffer the whole file.
- Check every write count, track-length/seek patch, flush, close and file reopen. Parse the closed temporary file with the bounded production reader, verify declared track structure and End-of-Track, and compare its event invariants with the snapshot.
- Rename the validated temporary file to the final name as the last operation. On any write, close, validation or rename failure, remove the temporary file and report failure; no final-name file may be presented as a successful export.
- This protects against application-level partial success. It does not claim FAT is power-loss transactional.

## Tests required for GO

- A host fake storage sink injects open, short-write, flush/close, readback, validation and rename failures. Each failure leaves no new final-name file.
- A separate host-side SMF parser, independent of the production writer/parser, validates every fixture and reconstructs track, channel, pitch, start, duration, velocity and tempo.
- Fixtures cover empty Song, one/multiple tracks, overlapping same-pitch notes, tempo, Song reverse, full section order, 1/2/4/8-bar rows, STEPS override on a Melody slot, Melody repetition, drum routing, missing/corrupt material, voice marker, and repeated export.
- The FORM/STEP regression fixture materializes a Song with a non-default STEPS override, saves/reloads it, exports it, and verifies row duration and event placement.
- Repeated export of one committed snapshot is semantically identical. Output channels match the captured export projection.
- Source review and tests prove no SD access, heap allocation, writer call or unbounded work is reachable from realtime callbacks.
- Record firmware size, static DRAM and available stack/heap evidence against the 0.9.32 baseline. Hardware remains `NOT TESTED`.

## Known implementation risks

- Pattern pages share one global `sceneTransactionScratch()` buffer. Export must be control-thread serialized and must not overlap another page transaction.
- Song rows can span up to eight bars and reference page-backed Pattern or Melody material. Event ordering and note lifetime must remain bounded while writing a Type 1 track.
- The existing profile route model has an incomplete Custom Performance table. Export must fail clearly or use its documented safe mapping when Custom cannot project a role; it must not guess.
- Cardputer SD `File::flush()` does not expose a rich durability result on all Arduino implementations. Closed-file readback and structural validation are required before promotion.
- The export path may block the UI while traversing up to 128 Song rows. Measure before considering progress UI or asynchronous infrastructure.
