# Dirty-region display foundation

## Purpose

Reduce Cardputer-ADV SPI/display traffic without adding a second framebuffer or changing page rendering behavior.

The UI may continue drawing a complete RGB565 frame. `CardputerDisplay::flush()` compares fixed 16 x 16 tiles with the previous completed frame and transfers only changed horizontal tile runs. It falls back to the existing full-screen transfer for the first frame, large changes, or highly fragmented changes.

## Hardware list

- M5Stack Cardputer ADV
- USB-C data cable
- Built-in 240 x 135 display
- Built-in ES8311 audio codec and speaker/headphone output

## Wiring

No external wiring is required.

The test uses the built-in display and audio path. Cardputer ADV remains configured with PSRAM disabled.

## Implementation constraints

- No second framebuffer.
- No allocation inside `flush()`.
- Tile history is fixed-size and supports displays up to 320 x 240.
- Unchanged frames perform no SPI image transfer.
- Full-width dirty runs use one contiguous `pushImage()` call.
- Narrow runs are transferred one framebuffer row at a time inside the existing display transaction.
- At least 50 percent dirty tiles or more than 18 horizontal runs triggers a full refresh.

## Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Host validation:

```bash
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_dirty_tile_tracker.cpp \
  -o build/host-tests/test_dirty_tile_tracker
build/host-tests/test_dirty_tile_tracker
```

## Expected behavior

### Screen

- Boot and page transitions still repaint the complete screen.
- Static page areas remain visually stable: no stale tiles, seams, or partially updated text.
- Transport playhead, waveform overlay, status badges, mutes, and toasts continue updating.
- Switching CYBER/CARBON themes performs a correct complete refresh through the fallback policy.

### Serial and audio

- Normal boot reaches the existing ready stage.
- No new display-related error is printed.
- `[PERF]` `underruns` must not continually increase while a static page is playing and only the playhead/status changes.
- UI draw timing may include tile hashing, but display bus occupancy should be lower on mostly static frames.

## Troubleshooting

### Stale or missing screen area

Call `CardputerDisplay::forceFullRefresh()` after any operation that changes display state outside the framebuffer. Normal page drawing should not require this.

### More crackle or slower UI

The likely cause is excessive fragmentation producing many narrow row transfers. Lower `DirtyTileTracker::kMaxPartialRuns` so fragmented frames fall back to one full transfer sooner.

### Every frame still appears to be full-screen

A page or skin is changing at least half of the tiles each frame. Confirm whether its animation is intentional before changing thresholds. The fallback preserves existing behavior and correctness.

### Cardputer build fails around `pushImage`

Keep the partial path row-based. The framebuffer uses full-screen stride, so a narrow multi-row rectangle is not contiguous and must not be passed as one ordinary `pushImage()` buffer.

## Acceptance checklist

- [ ] Host `test_dirty_tile_tracker` passes.
- [ ] SDL build passes.
- [ ] Cardputer-ADV build passes with warnings enabled.
- [ ] First boot frame and every page transition render completely.
- [ ] Leave transport stopped on a static page for 10 seconds: no flicker or stale text.
- [ ] Run transport for 2 minutes while changing no controls: playhead and overlays update correctly.
- [ ] Open and dismiss help, launcher, and toast overlays: covered areas restore correctly.
- [ ] Switch CYBER/CARBON themes: the whole screen updates correctly.
- [ ] While playing, navigate through PERFORM, PATTERN, ARRANGE, and SMF Player: no sustained increase in `[PERF] underruns` compared with the same build before this change.
