# GroovePuter 0.9.18 — First Five Minutes

This page is for the first session on a M5Stack Cardputer ADV. The goal is simple:
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
The GENERATE workflow now also has a middle **GEN** page between GENRE and FEEL for
choosing generation target and phrase-shape options. You do not need it for this first
groove; the defaults are enough.

## 1. Hear something

1. Press `Space` to start playback.
2. Press `Fn+M`, choose **GENRE**, and enter it.
3. Use `Up/Down` to choose the field and `Left/Right` to change Genre, Variant or
   Rhythm.
4. Press `G`.

While playback is running, the generated material changes at a musical bar boundary
rather than cutting the current bar in half. Press `G` again if you want another take
of the same selected musical direction.

## 2. Make a take you can develop

1. Press `Fn+M` to open the workspace launcher.
2. Open **MATERIAL**.
3. Set `LENGTH` to `4B`.
4. Set `STYLE` to `REWORK`.
5. Leave `TO` on `APPEND` for the simplest first session.
6. Press `G`.

`G` creates a fresh TAKE at the destination shown on screen. The generated TAKE is
real editable project material; it is not an opaque audio preview.

## 3. Develop it

While that fresh TAKE remains unedited, press `D` on **MATERIAL**.

GroovePuter appends the currently supported related development cycle. Depending on
the admitted musical form, the result can be shown as `DEVELOP + BREAK` or
`BREAK ONLY`. Listen through the boundary before judging it.

If `D` refuses, do not troubleshoot internal states. For this first bounded workflow,
make a new unedited `4B` `REWORK` TAKE with `G` and try `D` again.

## 4. If you do not like the result

Press `Ctrl+Z` to undo the last retained edit supported by the current page.

If **MATERIAL** reports `NO ROOM`, press plain `R`. It offers only unused generated
takes that are not currently referenced and that you have not edited. `Enter` allows
those slots to be reused by a later `G` or `D`; `Esc` cancels.

## 5. Save

Open **PROJECT** and use the on-screen Save action. Project Save is different from
`Alt+Enter`, which accepts working Synth material on Synth pages.

After saving, stop and reload the project once during your first session if you want a
quick confidence check that your material and arrangement return together.

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
