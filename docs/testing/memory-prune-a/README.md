# MEMORY-PRUNE-A — отчёт

База: [PR #494](https://github.com/Ostrovsky42/GroovePuter/pull/494), `integration/20261023-long-melody-hgs-energy`, проверенный HEAD `af4d51072e99b7cd7497c332bfc6da8877294c26`. Работа выполнена в отдельной `optimization/20261023-memory-prune-a`; PR #494 не менялся.

## Память

Все значения в байтах. Статические секции взяты из ELF; flash — значение Arduino `Sketch uses`. Heap/largest — реальные `[PERF]` из отдельных 60-секундных captures каждого product-образа на одном устройстве в спокойном STOP после загрузки проекта/экрана. Это не максимальная нагрузка генерации. Подробные SHA-256, секции, нормализованные исходники staged sketch, найденные runtime-символы и все PERF-пробы — в [measurements.json](measurements.json).

| Вариант | .data | .bss | DRAM сумма | Flash | Free heap, последняя проба | Largest block, последняя проба |
|---|---:|---:|---:|---:|---:|---:|
| Исходный af4d5107 | 41996 | 147008 | 189004 | 1627394 | 15188 | 7668 |
| Без Sampler | 41996 | 144864 | 186860 | 1595718 | 18508 | 9716 |
| + без Tape runtime | 41996 | 144848 | 186844 | 1592394 | 18604 | 9204 |
| + без истории waveform | 41996 | 142272 | 184268 | 1591782 | 21164 | 12276 |
| Эксперимент: без TempoDelay | 41996 | 142272 | 184268 | 1590990 | 39604 | 30708 |
| Итог: delay сохранён | 41996 | 142256 | 184252 | 1591658 | 21184 | 12276 |
| Исходный MIDI-only | 42076 | 147368 | 189444 | 1676150 | OPEN | OPEN |
| Итог MIDI-only | 42076 | 142616 | 184692 | 1640354 | OPEN | OPEN |

Итог normal против исходного: **−4752 B статической DRAM, −35736 B flash, +5996 B свободного heap, +4608 B largest block**. `.data` не менялась. DRAM-лимит **191488 B** не повышался; итог 184252 B, запас 7236 B. Largest не аддитивен: после Tape отдельный замер снизился с 9716 до 9204, несмотря на снижение общего расхода; важен итог 12276. Не приписываем удалению Tape уже отключённые ранее буферы.

Последняя строка normal дополнительно содержит удаление 16 B оставшегося Tape bookkeeping и исправление жизненного цикла sampler-метаданных. Файлы staged sketch сохранены в build/*/.arduino-build/sketch; script collector исключает Arduino `#line` при сравнении исходников. Пара waveform/no-delay имеет **одинаковый нормализованный source hash** и различается экспериментальным флагом.

## Удалено из Cardputer ADV

- WAV runtime: RamSampleStore, SampleIndex, SampleLoader, SamplerPool/SamplerVoice, DrumSamplerTrack; startup registry/scan/preload, WAV-render/trigger и страница SAMPLES.
- Оставшиеся владельцы TapeFX/TapeLooper, Tape controls/render/mode reconciliation, Tape page implementation и неиспользуемый FX safety bookkeeping. Большие TapeFX/Looper буферы были отключены ещё в базе: их экономия здесь **0**.
- Двойной waveform audio history, UI trace history и их постоянное копирование. Overlay недоступен и не восстанавливается из старой UI session. Сохранены test tone, AudioDiagnostics и компактный атомарный PCM peak для диагностического экрана.

Sampler/Tape/Waveform по умолчанию остаются в desktop SDL. Принудительная конфигурация SDL без них также собирается и проверена визуально. Capabilities задаются в `src/platform/cardputer_capabilities.h`; старые page IDs не перенумерованы. Старый Sampler ID нормализуется в DRUMS; shortcut сообщает `SAMPLER NOT IN THIS BUILD`.

## Сохранено

HGS/Harmony Graph/Genre Contracts; Melody A/B и 8B, Song/FORM; TAKE/DEVELOP/BREAK; MIDI/SEQTRAK/SMF/GRAB; синтезированные барабаны и основные DSP; Undo, Save/Save As/Load и Melody/page persistence. Нормальный TempoDelay сохраняет алгоритм и оба коротких буфера. FormantSynth и voice compressor сохранены: они реально воспроизводят Voice-партии Song, см. [аудит](voice-formant-audit.md).

## Совместимость проектов

Scene schema, samplerPads/samplerEnabled/Tape поля сохранены. Legacy ID-only pads загружаются и сохраняются без реестра. Stable refs публикуют тихие runtime IDs и сохраняются в совместимом sidecar для возврата проекта в WAV-capable сборку; отсутствие WAV не ломает загрузку синтезированных партий. Удаление WAV-слоя намеренно означает, что прежние WAV-партии на Cardputer не звучат; их assignments сохраняются. На SDL с Sampler они продолжают работать.

Ревью обнаружило существовавший в базе дефект: начало неудачной загрузки очищало refs текущего проекта. Он исправлен в этой ветке: streaming filter завершает подготовку без публикации, после успешного Scene decode storage подтверждает загрузку через существующий `failed()` acknowledgement, и только тогда refs публикуются. Failed/unacknowledged load сохраняет текущие refs; успешная legacy-загрузка очищает старые refs; New/Clear и default fallback в pruned engine сбрасывают sidecar. Регрессии воспроизведены RED и проверены GREEN.

Остаётся существующая архитектурная граница: SceneManager публикует Scene до завершающего `filtered.failed()`. Отказ другого output/filter на этой поздней стадии может нарушить всю транзакцию; этот PR не переписывает общего владельца загрузки. Дефект преждевременной публикации sampler refs устранён, но полная переработка этой границы — отдельный backlog.

## TempoDelay — отдельный эксперимент

Флаг `GROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY=1` выключает только выделение двух буферов, а `process()` становится dry bypass. По умолчанию **0**; на устройстве в конце установлен normal с эффектом.

- Target A/B waveform → no-delay: статические секции одинаковы, flash −792 B, free heap +18440 B, largest +18432 B (наблюдение allocator/boot, не расчёт размера массивов).
- Host impulse на реальном TempoDelay: два буфера суммарно 17640 B payload при 22050 Hz; echo energy 0.08131443 → 0.00000000. Подтверждена потеря эха, dry сигнал остаётся. Имеются воспроизводимые build и host A/B scripts.
- Художественная оценка звучания bypass — OPEN. Удалять эффект из продукта по одной экономии памяти не предлагается.

## TinyUSB — отдельный эксперимент

[Аудит и воспроизводимый archive experiment](tinyusb-audit.md): в исходном ELF MSC/DFU/NCM держат 14608 B буферов + 140 B состояния. Скетчевые флаги не изменяют уже собранный `usbd.c.obj`. Изолированное удаление трёх driver objects из копии архива оставляет 16 unresolved driver references — это доказательство необходимости согласованной пересборки SDK, **не готовая прошивка**. Установленный SDK не изменялся. В продукте USB-классы сохранены; экономия TinyUSB в таблице **0**.

OPEN: полноценная pinned SDK/core пересборка driver table и классов, валидный ELF без этих классов, повторная проверка CDC+MIDI и recovery. Опасный archive-only вариант не линкуется и не прошивался.

## Проверки

- PASS: обычная Cardputer ADV product-сборка с dynamic FatFs/PSRAM disabled и MIDI-only product-сборка; неизменный DRAM gate.
- PASS: P3 real-engine 8B/cardinality/onset/display/persistence; отдельный pruned-host gate (все три компонента off), включая test-tone/peak, New/Clear refs, обе 8B Melody, Save/Load/New и отдельные OS process restart.
- PASS: полный HGS suite, Smart Play suite, существующие sampler persistence ownership и registry tests для desktop-capable режима; исходные memory telemetry/source checks; TempoDelay impulse A/B.
- PASS: full desktop SDL build; pruned SDL app из тех же engine objects; визуально DRUMS без waveform и unavailable Sampler shortcut. Screenshots `/tmp/gp-memory-prune-sdl/drums-no-samples.png`, `sampler-unavailable.png`. Dummy audio не является проверкой звучания.
- PASS: полный `bash tests/run_host_tests.sh`, exit 0 (`/tmp/gp-mpa-host.log`), включая завершающие UI navigation, synth engine switch ASan/UBSan и MORE restore. Отдельный pruned sampler compatibility test также PASS под ASan/UBSan.
- PASS hardware: все normal A/B образы загружены с проверкой hash; после них восстановлен итоговый normal. Все измеренные PERF samples — `underruns=0`, fatal boot markers отсутствуют.
- PASS hardware USB: CDC и upload/recovery возврат порта; ALSA видит `Cardputer MIDI 1`. Отправлены 3 NoteOn/NoteOff + CC123: serial `rx=7`, компьютер захватил MIDI CC cleanup. Это проверяет физический USB device RX и TX, но не MIDI echo: выходные кадры могли оставаться от startup cleanup. Не является тестом SEQTRAK или USB Host nanoKEY2.
- PASS user acceptance: после установки итогового normal пользователь подтвердил «Все работает, отлично пуш» в ответ на проверку обеих Melody 8B, генерации, синтезаторов/барабанов и Save → Reset → Load.
- OPEN: отдельные внешние SEQTRAK/USB Host MIDI сценарии, аппаратное MIDI-only без CDC, длительный worst-case generation/SD-failure stress и художественная оценка отключённого delay. По ним новые PASS не заявлены.

## Повторение

```bash
bash scripts/experiments/build_memory_prune_variant.sh baseline normal
bash scripts/experiments/build_memory_prune_variant.sh sampler normal
bash scripts/experiments/build_memory_prune_variant.sh tape normal
bash scripts/experiments/build_memory_prune_variant.sh pruned normal
bash scripts/experiments/build_memory_prune_variant.sh no-delay normal
bash scripts/experiments/build_memory_prune_variant.sh pruned midi-only
bash tests/run_cardputer_prune_tests.sh
bash scripts/experiments/run_tempo_delay_ab.sh
python3 scripts/experiments/audit_tinyusb_archive.py --help
```

`baseline` строит git archive точного исходного SHA, остальные — текущую ветку. Выводы идут в отдельные build directories; установленный SDK не меняется. Build script записывает SHA/flags/FQBN и patch грязного tracked tree; для полного воспроизведения использовать закоммиченный HEAD. Production code checkpoint: `3b3f2bb8`. Итоговый прошитый BIN SHA-256: `b5a6a0e3dc8ea6604a20f7022ff548cbca2209d59bf4797ec8ac3c4fe45841e3`. Artifact SHA таблицы взяты из measurements.json, а не предполагаются по имени ветки. Merge не выполнялся.
