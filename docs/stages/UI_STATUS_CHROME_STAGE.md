# UI Status Chrome Stage

## Purpose

Wave 1 / A1 replaces the competing per-page header text with one compact,
global context line inside the existing 16-pixel header:

```text
GEN PAT PLAY B3/4 INT AUD
PLYR SMF ARM B8/128 EXT MIDI
```

The fields are ordered by immediate musical priority:

1. current page context;
2. active musical source;
3. transport state;
4. bar position;
5. clock source;
6. primary output path.

This stage does not add dirty tracking or change keyboard routing. Those remain
separate Wave 1 tasks A2 and B.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for build/flash and serial monitoring
- Yamaha SEQTRAK is optional for checking `EXT` clock and MIDI playback states

## Wiring

External wiring: none.

The test does not use PORT.A. Existing Cardputer ADV I2C invariants remain:

```text
SDA GPIO2
SCL GPIO1
```

## Build / Flash

```bash
bash tests/run_host_tests.sh
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_ui_status_chrome.cpp \
  -o build/host-tests/test_ui_status_chrome
build/host-tests/test_ui_status_chrome
bash scripts/build.sh --warnings all
```

Flash with the existing repository workflow after a successful Cardputer ADV
build.

## Expected behavior

The top 16 pixels show one line on every page without reducing the existing
content area.

Token meanings:

| Token | Meaning |
|---|---|
| `PAT` | internal pattern source |
| `SONG` | internal Song source |
| `SMF` | MIDI-file player source |
| `PLAY` / `STOP` / `PAUS` / `ARM` | transport state |
| `B3/4` | current bar and active length |
| `INT` | GroovePuter internal clock |
| `EXT` | external SEQTRAK clock |
| `FILE` | original SMF tempo map |
| `AUD` | internal audio is the primary sink |
| `MIDI` | SMF MIDI output is the primary sink |
| `LM` | LiveMix lock is enabled |

Status derivation and string formatting run only when the compact snapshot
changes. The cached line is painted every UI frame because the existing renderer
redraws the page and header every frame; removing that full-frame behavior is a
separate rendering project and is not part of A1.

## Troubleshooting

### Header still shows the old page title

Confirm `MiniAcidDisplay::update()` still calls
`UI::drawLiveMixLockBadge(gfx_, mini_acid_)` after the current page is drawn.
That compatibility hook now delegates to `drawStatusChrome()`.

### SMF state is not shown

The SMF player owns the chrome while it is loading, armed, playing or paused.
A loaded stopped player is shown as `SMF STOP` while the MIDI Player page is
selected. Outside that page, stopped SMF does not hide an active Pattern or Song
source.

### Clock displays `INT` instead of `EXT`

Check the persisted transport clock source and the external-follow state on the
Project page. `EXT` is read from `transportClockRuntime()`; it is not inferred
from cable presence.

### Text is clipped

The formatter intentionally uses short fixed tokens and a 48-byte static buffer.
Do not add long labels to the permanent line. Additional detail belongs in a
context overlay or help page.

## Acceptance checklist

- [ ] Chrome is visible on every musical and system page.
- [ ] No page content is shifted, cropped or resized.
- [ ] `PAT`, `SONG` and `SMF` follow the active source rules above.
- [ ] Transport state changes without a full-page navigation event.
- [ ] Pattern and Song bar counters advance on their existing musical boundary.
- [ ] SMF bar/total values come from `SmfPlayerSnapshot`.
- [ ] `INT`, `EXT` and `FILE` match the actual clock owner.
- [ ] CARBON, CYBER and AMBER remain readable using existing palette colors.
- [ ] LiveMix is represented by `LM` without drawing into the content area.
- [ ] `tests/test_ui_status_chrome.cpp` passes with `-Werror`.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes with `--warnings all`.
- [ ] Five-minute maximum-density hardware run shows no new underruns.
