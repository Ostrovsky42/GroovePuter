# MIDI I/O and nanoKEY2 — evidence log

This log records observed results, not promises of support. The normative
behavior is [the MIDI I/O contracts](2026-09-07-midi-io-contracts.md).

## Baseline — 2026-09-07

Worktree: `feature/20260907-midi-io-nanokey2`.
Base: `de6b5a3d`; preceding MIDI commits: `90b53692`, `925d9a40`,
`e70fc634`.

| Check | Result | Scope |
|---|---|---|
| `bash tests/run_host_tests.sh` | PASS | Full host suite; existing C++20/type-limit warnings remain |
| `bash tests/run_midi_io_contract_tests.sh all` | PASS | State/output/packet/parser native tests |
| `bash tests/run_performance_closure_tests.sh` | PASS | Existing performance closure |
| Memory runtime build | PASS | normal/runtime; fixed DRAM 190856 B, provisional gate headroom +632 B |
| RX2 qualitative USB receive | PASS, user reported | Cardputer + USB-C adapter + nanoKEY2 |
| nanoKEY2 identity | Observed | VID:PID `0944:0115`, IN `0x81`, OUT `0x02` |
| nanoKEY2 → PERFORM → UART → SEQTRAK | Not tested | RX2 is standalone; it contains neither PERFORM nor UART |
| VBUS, latency, 100 pairs, 20 reconnect, memory soak | Not measured | Remain H0-STABLE/H1 evidence |

The standalone probe is built with M5Stack core 3.2.2, ESP-IDF 5.4, PSRAM
disabled and CDC-on-boot disabled. Its exact command and boot recovery notes
are in [the probe README](../../tools/hardware/nanokey2_h0/README.md).

## M1 Host memory probe — awaiting screen observations

The flashed `build/nanokey2-h0-m1` probe captures internal 8-bit and DMA
free/largest-block pairs after `HOST-INSTALLED`, `HOST-CLIENT`, `HOST-RX-READY`,
the first validated note, and `HOST-DETACHED`. Record all five phase/value pairs
from the display before drawing a conclusion about Host cost or a reconnect leak.

Observed after Host installation (phase label not recorded by the user):
`i8=342684/278516`, `dma=334952/278516`, `ESP.getFreeHeap()=342684`.
This is one probe point, not an integration budget or a Host delta.

Observed after keyboard detach: `i8=345608/278516`, `dma=337876/278516`,
`ESP.getFreeHeap()=347340`. Relative to the first observation this is
`+2924` bytes for both capability queries and the same largest block. This
single cycle shows no monotonic loss; it does not establish Host resident or
active-RX cost because RX-ready/first-note observations are still absent.

Observed at `HOST-RX-READY` and again at `HOST-FIRST-NOTE`:
`i8=342684/278516`, `dma=334952/278516`, `ESP.getFreeHeap()=342684`.
The first validated note did not add an observable allocation. The Host
resident cost is therefore paid no later than RX-ready, but its exact delta
still needs a pre-install point captured in the same boot.

## M0 Main firmware — first cold boot observation

The diagnostic main firmware was flashed with FQBN
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
The user captured this cold boot on 2026-09-07. Values below are
`freeInt/largInt` for `INTERNAL|8BIT`; they are observed state, not a heap
budget that can be added across capability classes.

| Boundary | Free / largest (B) | Delta free from preceding point | Interpretation |
|---|---:|---:|---|
| after M5 + I2S | 137972 / 73716 | — | baseline after hardware and direct I2S |
| after display | 72432 / 31732 | -65540 | display is the largest measured resident consumer |
| after AudioTask | 63352 / 31732 | -9080 | task/stack and audio runtime |
| after two DSP delays | 44648 / 31732 | -18704 | both `TempoDelay` buffers |
| after SD mount | 14504 / 7668 | -30144 | SD runtime sharply reduces both free heap and contiguous space |
| after SMF begin | 5076 / 2292 | -9428 | SMF leaves no viable allocation headroom |
| after MIDI sink | 5076 / 2292 | 0 | existing USB-device sink made no observed additional allocation here |
| before restored UI page | 1876 / not logged | -3200 from SMF point, including engine/scene/sample-scan path | page allocation is already in the danger zone |
| after page 10 allocation | 1640 / not logged | -236 | page succeeded once, but only 1.6 KiB remained |

Consequences: the primary memory risk precedes USB Host. The active full
configuration has only 2,292 B of contiguous `INTERNAL|8BIT` space after SMF,
and 1,640 B free after the restored UI page. A production Host integration is
therefore blocked by M2; it must first provide measured lifetime changes and
an admission reserve, rather than relying on the standalone Host probe's
larger heap. The `MEM-BASE` periodic runtime lines were not retained in this
serial capture, so the next M0 pass must leave the monitor connected for at
least ten seconds after `setup() complete` and exercise UI/scene/SMF paths.

## Known red test before P2

`tests/test_midi_endpoint_dispatch_eviction.cpp` is intentionally outside the
green runner until P2. It demonstrates that `MidiEndpointDispatcher` overwrites
its eight-slot accepted-delivery cache. A retry of sequence 41 after sequences
42–49 asks UART to send sequence 41 twice. P2 must move this case into the
ordinary `output` group and make it green by removing evicting retry history.
