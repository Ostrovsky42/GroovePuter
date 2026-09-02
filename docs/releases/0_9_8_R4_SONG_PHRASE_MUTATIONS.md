# 0.9.8 R4 — Song / Phrase persistent mutation ownership

## Scope

R4 completes the 0.9.8 safe-editing migration for the remaining Song/Phrase
persistent edit surfaces. It extends the accepted R2 `UndoOwner<1536>` contract;
it does not introduce a second history owner or any musical activation machinery.

## Ownership boundary

- **Transport owns TIME**: Song playhead, mode/loop controls, edit-slot selection,
  pattern-slot browsing and ordinary navigation remain runtime state.
- **Persistence owns COMMITTED STATE**: Song arrangement rows/cells and Phrase
  bank changes publish through the canonical one-level `UndoOwner`.
- **UI owns neither**: pages prepare detached values and ask the owner to commit.

The normal R4 mutation path is:

`capture before -> detached PREPARE -> reject no-op -> validate resident target -> UndoOwner COMMIT -> one bounded assignment -> one Scene revision`

Failure before COMMIT leaves Scene, retained Undo and revision/dirty state unchanged.

## Song receipt

`SongUndoPayload` contains:

- resident page address;
- Song A/B slot address;
- one complete `Song` before-image.

The slot is an address, not user-navigation state. Browsing A/B therefore does
not become part of Undo. The receipt remains under the measured ~1 KiB bound and
fits the existing 1536-byte canonical slot.

R4 covers current manual arrangement writes:

- Q-I assignment to a cell/selection;
- Alt Up/Down pattern adjustment;
- Alt+B bank-reference flip;
- insert/delete row;
- Backspace/Tab clear;
- Alt+Backspace Song reset;
- CUT/PASTE for one cell, area or whole Song;
- Pattern editor chaining;
- Drum editor chaining;
- Phrase `W` insert/replace into Song.

The old Song-page vector history is no longer on an active R4 mutation path.
Dynamic vectors used for clipboard contents remain clipboard storage only; they
are not retained Undo state.

## Phrase receipt

`PhraseUndoPayload` retains one fixed `PhraseBank` before-image plus page address.
The bank is 244 bytes and includes `nextPhraseId`, so capture/derive/clear can be
rolled back exactly without snapshotting the Scene.

`PhraseWorkspace` is now a PREPARE-only layer over caller-owned detached values:

- `capturePrepared`;
- `derivePrepared`;
- `clearPrepared`;
- `writeToSongPrepared`.

It has no UndoOwner, revision, AudioGuard, filesystem, JSON or activation owner.

## 0.9.9 boundary

R4 deliberately does **not** claim:

- generated Phrase -> Song materialization;
- Song generation / quantized materialization;
- pending-next-bar activation;
- queued reverse activation;
- scheduler or boundary-liveness machinery.

Generation keeps its accepted specialized PREPARE/commit path. Queued Song
reverse remains a 0.9.9 transition boundary; scheduling a new reverse transition
advances revision only to invalidate an older arrangement receipt, rather than
pretending the queued transition is an R4 Song COMMIT.

## Resource contract

R4 adds no resident history allocation. `UndoOwner` remains the only retained
one-level history slot. Song/Phrase PREPARE values are bounded stack temporaries;
there is no full-Scene snapshot, heap-backed Undo, filesystem staging, JSON
staging, new mutex or scheduler.

## Acceptance

Focused R4 acceptance reruns R2 and R3 first, then checks:

- pure Song and Phrase PREPARE layers;
- receipt size/trivial-copy contracts;
- canonical owner routing for Song/Phrase and chaining writers;
- runtime selector/TIME separation;
- generation/ACTIVATE exclusion;
- Song edit semantics (set/clear/insert/delete/bank flip/reset).

Merge readiness additionally requires the normal exact-head repository gates:
host regressions, SDL, Cardputer ADV normal, fixed-DRAM and Cardputer ADV
SEQTRAK MIDI-only on the same SHA.
