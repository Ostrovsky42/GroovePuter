# 0.9.10 MEMORY-R1 FS1B — filesystem ownership / realtime census

Date: 2026-09-08
Branch: `feature/20260908-0.9.10-memory-r1-dram-recovery`

Purpose: determine whether enabling `CONFIG_FATFS_USE_DYN_BUFFERS` would move
`ff_memalloc` / `ff_memfree` into an AudioTask or ISR hard-realtime path.

This is a source-ownership census. It does **not** prove SD latency, heap
fragmentation, hardware stability, or recovery bytes.

## Result

No production `f_open` / `f_close` owner was found in the Cardputer AudioTask or
an ISR path on the inspected R1 line.

The automated contract `tests/test_memory_r1_fs1b_realtime_fs_contract.py`
guards the two hard-realtime owners directly:

- `GroovePuter::audioTask()`
- `MiniAcid::generateAudioBuffer()`

and rejects filesystem open/close plus general-purpose allocation tokens there.

Recording is the important split-lifetime case: `start()` opens the file before
the render path, `writeSamples()` writes through the already-open handle from
AudioTask, and `stop()` closes it outside the render operation. Dynamic FatFs
therefore adds allocation/free to record start/stop, not each audio block.

SMF is the other important ownership split: `taskLoop()` consumes commands on
the dedicated SMF task, delegates to `handleCommand()`, and the Load command
reaches `loadFile()` -> `source_.open()`. The open allocation is therefore owned
by the SMF task rather than AudioTask.

## Census

| Subsystem | Open context | Read/write context | Close context | Open frequency | AudioTask? | ISR? | FS1B classification |
|---|---|---|---|---|---|---|---|
| SD volume | shared Cardputer SD owner (`cardputer_sd`) during mount/retry | shared filesystem | platform unmount/lifetime | boot / explicit retry | no source evidence of mount in AudioTask | no | ACCEPTABLE SOURCE OWNERSHIP; hardware allocation peak still required |
| SMF playback | dedicated SMF task: `taskLoop -> handleCommand(Load) -> loadFile -> source_.open` | dedicated SMF task streaming | SMF task/service cleanup | load / stop / replacement | **no** | no source path found | ACCEPTABLE |
| Audio recording | control path `CardputerAudioRecorder::start()` -> `SD.open(FILE_WRITE)` | `AudioTask -> writeSamples()` on already-open `File` | control path `stop()` | record session start/stop | write only; **no open/close** | no | ACCEPTABLE FOR ALLOCATOR PLACEMENT; I/O latency remains separate |
| Sample load | loader/admission path opens source before sample becomes resident | decode/read during load; playback uses resident sample memory | loader closes before/after admission | sample load/reload | no open/close in `generateAudioBuffer()` | no | ACCEPTABLE SOURCE OWNERSHIP |
| Scene load/save | SceneStorage control/persistence path | synchronous read/write/validate/commit | SceneStorage transaction cleanup | explicit load/save / restore | no source path in render owner | no | ACCEPTABLE SOURCE OWNERSHIP |
| Pattern paging/copy | PatternPagingService control path | bounded read/write/copy | paging operation cleanup | page save/load/copy | explicitly forbidden by hard-realtime contract | no | ACCEPTABLE SOURCE OWNERSHIP |
| MIDI import/scan | `MidiImporter::importFile()` / `scanFile()` -> `SD.open(FILE_READ)` | synchronous parser; import may persist page state | same import/scan function closes | explicit import/scan | no source path in render owner | no | ACCEPTABLE SOURCE OWNERSHIP |
| Voice cache helper | helper owns open/close including EOF cleanup | helper stream read | explicit/EOF cleanup | playback-dependent if admitted | no production AudioTask caller found in inspected render owner | no source path found | NOT AN AUDIO ALLOCATOR PATH ON CURRENT R1 SOURCE; hardware exercise still required |

## Hard-realtime contract actually proven

`GroovePuter::audioTask()` may call the recorder's `writeSamples()` while a
recording is active, but does not call recorder `start()` or `stop()` and does
not perform `SD.open`, `close`, `f_open`, `f_close`, malloc/free/new/delete,
SceneStorage, PatternPagingService, or VoiceCache operations.

`MiniAcid::generateAudioBuffer()` is independently checked for the same
filesystem/allocation classes. This matters because checking only `audioTask()`
would miss a delegated hard-realtime owner.

The SMF contract additionally proves the command ownership chain:

```text
SMF taskLoop
  -> handleCommand
     -> CommandType::Load
        -> loadFile
           -> source_.open
```

## What this does not prove

- A source-token gate cannot prove that every future indirect call remains safe;
  keep the contract test with FS1B.
- It does not prove that writes through already-open files are realtime-safe.
  That is I/O latency, not dynamic-buffer allocation placement.
- It does not prove allocator fragmentation after repeated attach/open/close
  cycles.
- It does not prove five-handle runtime behavior on the integrated MIDI line.
- It does not prove free/largest/local-minimum recovery.

Those are hardware acceptance gates. Until they are rerun on the integrated R1
image, FS1B remains a production **candidate**, not an accepted R1 recovery.
