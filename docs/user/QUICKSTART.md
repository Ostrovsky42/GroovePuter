# GroovePuter 0.9.21 (unreleased) — First Five Minutes

This page describes the 0.9.21 (unreleased) release candidate; it is not a published or hardware-accepted release. Use it for a first session on a M5Stack Cardputer ADV. The goal is simple:
hear a groove, make a take, develop it, recover from a bad choice, and know where to
ask for help without learning GroovePuter's internal architecture first.

## Six keys to know first

| Key | What it means in the first session |
|---|---|
| `Space` | Play / Stop |
| `Fn+M` | Open the workspace launcher; choose **GENRE** or **MATERIAL** |
| `G` | Generate the thing you are looking at |
| `D` on **MATERIAL** | Develop the fresh TAKE |
| `Ctrl+Z` | Undo the last retained edit |
| `Alt+H` | Show help for the current page |

These are beginner rules, not global overrides. GroovePuter is contextual: the page on
screen decides the exact scope of an action. In particular, `D` means **DEVELOP** in
this quick start only on the **MATERIAL** page; other expert pages keep their own `D`
actions.

`Alt+V` opens GENRE directly; `Fn+M` reaches it through the workspace launcher.
Inside GENRE, `Tab` opens the middle **GEN** panel for generation target and
phrase-shape options; another `Tab` continues to FEEL. You do not need this panel for
this first groove; the defaults are enough.

## 1. Hear something

1. Press `Space` to start playback.
2. Press `Fn+M`, choose **GENRE**, and enter it.
3. Use `Up/Down` to choose the field and `Left/Right` to change Genre, Variant or
   Rhythm. For the later MATERIAL `D` step, choose any genre except **Acid** and
   **House**; those two do not support the current bounded development cycle.
4. Press `G`.

While playback is running, the generated material changes at a musical bar boundary
rather than cutting the current bar in half. Press `G` again if you want another take
of the same selected musical direction.

## 2. Make a take you can develop

1. Press `Fn+M` to open the workspace launcher.
2. Open **MATERIAL**.
3. Set `LENGTH` to `4B`.
4. Set `STYLE` to `P3` (Rework).
5. Leave `TO` on `APPEND` for the simplest first session.
6. Press `G`.

`G` creates a fresh TAKE at the destination shown on screen. The generated TAKE is
real editable project material; it is not an opaque audio preview.

## 3. Develop it

While that fresh TAKE remains unedited, press `D` on **MATERIAL**.

GroovePuter appends the currently supported related development cycle. Depending on
the admitted musical form, the result can be shown as `DEVELOP + BREAK` or
`BREAK ONLY`. Listen through the boundary before judging it.

If `D` refuses, first confirm the selected genre is not Acid or House, the TAKE is
new and unedited, and its source context has not changed. Making another TAKE in an
unsupported genre will not make it developable.

## 4. If you do not like the result

Press `Ctrl+Z` to undo the last retained edit supported by the current page.

If **MATERIAL** reports `NO ROOM`, press plain `R`. It offers only unused generated
takes that are not currently referenced and that you have not edited. `Enter` lets the next `G` reuse those slots; `Esc` cancels.

## 5. Save

Open **PROJECT** and use the on-screen Save action. Project Save is different from
`Alt+Enter`, which accepts working Synth material on Synth pages.

After saving, use PROJECT Load to reopen that project once. Confirm the material and
arrangement return, then make and save one more edit to prove the session can continue.

## Export and sharing

The candidate does not expose a documented user-facing WAV or MIDI file export
workflow for a GroovePuter composition. The MIDI Player imports and plays MIDI files;
it does not export the current Song. To record external MIDI, route GroovePuter to a
MIDI-capable DAW or instrument and use that device's recording workflow. This is a
known productization gap, not a claim that project Save exports a shareable file.

## The one rule worth remembering

> `G` generates the thing you are looking at.

On GENRE it generates the full musical context. On a Synth page it can generate the
selected synth lane. On DRUMS it generates drums. On SONG or MATERIAL it acts on that
page's material workflow. The screen title tells you the scope.

For any page, press `Alt+H` before opening the full manual.

## Next references

- [`../../MANUAL.md`](../../MANUAL.md) — complete user workflow manual.
- [`../../src/ui/docs/keys.md`](../../src/ui/docs/keys.md) — complete Cardputer ADV key map.
- [`../README.md`](../README.md) — documentation index and release evidence.
