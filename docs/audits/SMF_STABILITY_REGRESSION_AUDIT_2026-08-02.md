# Аудит регрессии стабильности SMF

Дата: 2026-08-02

Ветка: `feature/seqtrak-master-clock-input`

Последний сохраненный commit: `d4f7322dfe7b09defb35864820c6cc2897956330`

Состояние проверки: рабочее дерево содержит еще 20 измененных файлов
(`+712/-224`) и новые диагностические файлы. Проверенная на устройстве прошивка
не совпадает ни с `main`, ни с hardware RC `9cbd7d3...`.

## Вердикт

Текущую прошивку нельзя считать стабильной и нельзя коммитить одним общим
изменением.

Исходный SMF parser, SD streaming и scheduler не перестали работать. Основная
регрессия появилась на следующем слое:

1. USB MIDI TX endpoint перестает принимать пакеты, когда host перечислил
   устройство, но никакое приложение не читает ALSA MIDI port.
2. Это состояние было ошибочно превращено из конечной ошибки в бесконечный
   `PAUSE -> cleanup probe -> auto-resume`.
3. Текущий recovery path остается активным неограниченно долго и повторяемо
   заканчивается task watchdog reset в `MidiDispatchTask` на CPU0.
4. Даже успешное auto-resume восстанавливается со старым backlog: в логе есть
   опоздание до 3.50 s и 29 late NoteOn drops.

Отдельная проблема с неполным списком `/midi` была вызвана лимитом heap в UI
browser. Она не является причиной `USB WAIT` и уже устранена в текущем рабочем
дереве потоковым окном списка.

## Главные находки

### P0. Бесконечное USB recovery вызывает watchdog reset

Текущий незакоммиченный код заменил terminal transport failure на постоянное
состояние ожидания:

```text
TX FIFO blocked
  -> 24 send retries
  -> scoped cleanup
  -> 32 cleanup attempts
  -> SMF-WAIT
  -> cleanup probe каждые 250 ms без конечной границы
  -> возможный auto-resume
```

Реализация находится в:

```text
src/platform/cardputer_usb_midi_transport.cpp
src/midi/scheduled_smf_midi_event_queue.h
src/platform/cardputer_smf_player.cpp
```

Аппаратное доказательство:

```text
logs/serial-180s-20260802-022723.log:189
  [SMF-WAIT] ... mounted=1 blockedUs=349659 attempts=32

logs/serial-180s-20260802-022723.log:190
  [SMF-WAIT] transport stalled generation=3 tick=2532

logs/serial-180s-20260802-022723.log:217
  Reset Reason: 6

logs/serial-180s-20260802-022723.log:218
  [WDT-DIAG] task=MidiDispatchTas core=0
```

Такой же порядок наблюдался в
`logs/serial-180s-20260801-235409.log`: endpoint stall, рост
`smfCleanRetry`, исчезновение USB, затем `Reset Reason: 6`.

Уверенность высокая, что дефект находится в новом recovery lifecycle. Точный
low-level участок, из-за которого idle task CPU0 не получает управление в
течение watchdog window, еще не изолирован. WDT marker показывает текущую
задачу во время срабатывания, поэтому его следует считать сильной корреляцией,
но не заменой минимального воспроизводимого unit/state-machine test.

### P1. Тестовая топология менялась незаметно

Cardputer работает как USB MIDI device. CDC serial monitor и MIDI endpoint -
разные интерфейсы одного composite USB device.

```text
scripts/monitor.sh
  -> читает только CDC serial

scripts/midi_sink.sh / scripts/midi_probe.py / QTrack
  -> открывает ALSA MIDI port и реально освобождает TinyUSB TX FIFO
```

`mounted=1` означает только, что MIDI interface configured. Это не означает,
что host читает bulk IN endpoint.

При отсутствии MIDI reader FIFO размером 64 bytes заполняется 16 USB MIDI event
packets. После этого `tud_midi_packet_write()` возвращает `false`. Именно это,
а не SD parser, запускает `smfRetry`, cleanup и `SMF-WAIT`.

Ранее стабильное воспроизведение проверялось в окружении, где QTrack, ALSA sink
или другой receiver держал MIDI port открытым. Позже часть тестов выполнялась
только с serial monitor или после остановки sink. Поэтому латентный endpoint
backpressure стал воспроизводиться постоянно.

Прямое соединение Cardputer и SEQTRAK также не является эквивалентом PC bridge:
оба устройства выступают USB devices. Для такого прогона нужен USB host между
ними или подтвержденный аппаратный host mode.

### P1. Auto-resume не очищает временную историю достаточно строго

Один recovery действительно сработал:

```text
logs/serial-180s-20260802-001731.log
  [SMF-WAIT] USB MIDI endpoint draining again; resuming
  [SMF-WAIT] transport recovered tick=10301 resume=0
```

Но после восстановления diagnostics показывают:

```text
smfStall=2/1
smfLateDrop=29
maxLateUs=3495337
smfMaxBlockUs=11361233
```

Это означает, что восстановление endpoint не равно корректному музыкальному
восстановлению. Старые queue deadlines или старый audio anchor переживают stall,
после чего scheduler получает backlog в несколько секунд.

Перед auto-resume необходимы как минимум:

1. Инвалидация старой queue generation.
2. Удаление pending event из старой временной эпохи.
3. Повторная подготовка stream от сохраненного tick.
4. Новый audio/project anchor.
5. Запрет catch-up NoteOn burst.

До появления отдельного детерминированного теста auto-resume безопаснее
отключить и оставить явный `Space` для продолжения.

### P1. Текущая ветка не является точным hardware RC

Коммитированный RC:

```text
9cbd7d3c1373a42042198923b5021d0d8eedcdd0
```

После синхронизации UI с main:

```text
d4f7322dfe7b09defb35864820c6cc2897956330
```

Отличие `9cbd7d3..d4f7322` состоит только из UI/workflow изменений. Но поверх
`d4f7322` сейчас находятся 20 незакоммиченных файлов, включая сам transport
recovery, SMF player state, browser, boot allocation и WDT diagnostics.

Поэтому фраза "тестируем PR #25 RC" сейчас технически неверна: на устройстве
тестируется отдельная экспериментальная сборка без immutable commit SHA.

### P2. Неполный список MIDI был отдельным UI/heap дефектом

Старый browser накапливал имена каталога в динамических строках и прекращал scan
при малом heap:

```text
[SMF-BROWSE] low-mem break freeHeap=3992
[SMF-BROWSE] scanned=0 dirs=0 files=0
```

Это выглядело как отвал SD, но mount и `SD.open()` оставались успешными.

В текущем browser используется ограниченное окно строк. Последний лог
подтверждает полный scan и возврат памяти после закрытия directory handle:

```text
logs/serial-180s-20260802-022723.log:156-158

path=/midi exists=1
root.open ok=1 isDir=1 freeBefore=4548 freeOpen=3756
scanned=9 dirs=6 files=3 complete=1 freeAfter=4548
```

Следовательно:

```text
SD mount       исправен
directory read исправен
directory File закрывается
heap после scan возвращается
```

### P2. RAM мала, но не является причиной текущей паузы

После загрузки файла наблюдается примерно:

```text
freeInt=4100
largest=2292
audio underruns=0
```

Запас опасно мал и ограничивает дальнейшие функции, однако во время нормального
воспроизведения значения стабильны. `SMF-WAIT` возникает при явном росте USB
retry counters, а не при allocation failure. Поэтому RAM - фактор риска, но не
корневая причина этой паузы.

### P2. SEQ MASTER hardware gate фактически еще не выполнен

Во всех последних приведенных логах:

```text
[MIDI-RX] source=SEQ MASTER state=WAIT ... rx=0/0/0/0
```

Это означает, что прошивка не получила ни одного `F8/FA/FB/FC`. Переключение
клавишей `C` проверяет UI и persistence, но не external clock follower.

До появления `rx>0` и перехода `WAIT -> LOCKING -> LOCKED` нельзя делать вывод
о синхронизации с SEQTRAK.

## Что было сделано по этапам

### 1. Потоковый SMF player

Начальный рабочий слой:

```text
3ea0b0346b4ba5737e85ab2db35bb712daf7dc3b
feat: add faithful realtime SMF player (#15)
```

Задача была правильной: не загружать весь MIDI в DRAM, читать события потоково,
планировать их заранее и отправлять через единственного владельца TinyUSB.

Стабильная пользовательская точка находилась около:

```text
6bf4d6ac76741d3851e17869bbcc0893e7034f50
perf: reduce UI transition logging overhead (#16)
```

### 2. Производительность SD и scheduler

Были добавлены sector-aligned cache, меньший seek overhead, общий cache pool,
lookahead и диагностика `SMF-PERF`. Это сняло реальный SD bottleneck и позволило
плотным многодорожечным файлам быстрее наполнять очередь.

Побочный эффект: быстрый producer перестал случайно растягивать USB bursts и
сделал старый TX FIFO defect заметнее. Это не означает, что SD optimization была
ошибкой; она раскрыла следующий bottleneck.

### 3. Endpoint readiness и backpressure

```text
1932f2e9ed10531de3f807a450ca339b301f5977
fix: wait for USB MIDI endpoint readiness

44ad9d9a1db0dbb2fe97f8983c96278d04c1c575
fix: tolerate USB MIDI backpressure
```

Были увеличены retry budgets и исправлена ошибочная эскалация dropped NoteOn в
полную остановку. Это правильные изменения: NoteOn, который не ушел на wire, не
создает ownership и может быть отброшен без panic.

Но endpoint, который host вообще не читает, не является кратковременным burst.
Retry budget не может решить отсутствие receiver.

### 4. PROJECT tempo synchronization

```text
0999209616c2fc7d220b708d2b55d0d765205ece
feat: synchronize SMF playback with project transport
```

Добавлены project timeline, late NoteOn/NoteOff policy, tempo re-anchor и
transport epoch. Этот слой увеличил сложность lifecycle, но последние логи не
показывают project timeline failure: `timelineMiss=0`, `timelineStale=0`,
`reanchor=0` в ORIGINAL mode.

### 5. SEQTRAK master clock

Между `0999209` и `9cbd7d3` изменены 45 файлов (`+3195/-143`): RX parser,
external event queue, PLL, Start/Stop/Continue, source switch, persistence и UI.

Архитектура еще не получила физический входной gate: последние аппаратные
прогоны имеют `rx=0`. Поэтому этот этап нельзя считать причиной текущего
`SMF-WAIT`, но он существенно увеличил поверхность состояний и затруднил bisect.

### 6. Попытка сделать endpoint stall прозрачным

Последний незакоммиченный этап заменил:

```text
USB MIDI BLOCKED -> Error/Stop
```

на:

```text
USB WAIT -> Pause -> постоянные cleanup probes -> Auto-resume
```

Именно здесь появилась текущая release-blocking регрессия: watchdog и stale
resume. Намерение было правильным, но для realtime dispatch task нельзя оставлять
неограниченный recovery lifecycle без доказанной fairness и временной reset
семантики.

## Что говорят ранние логи

Проблема endpoint существовала до SEQ MASTER:

```text
logs/serial-180s-20260731-191033.log
  smfRetry=435477
  maxLateUs=2938644

logs/serial-180s-20260731-192851.log
  smfRetry=24
  smfCleanRetry растет до 15739
```

Первый вариант делал busy retry почти без границы. Следующий вариант ограничил
обычные send retries, но cleanup все еще мог повторяться бесконечно. Значит,
внешний clock не создал исходный defect; он был в endpoint failure handling.

Когда receiver открыт, картина другая:

```text
smfRetry=0
smfCleanRetry=0
smfStall=0/0
heap стабилен
audio underruns=0
```

Когда reader закрывается во время playback, сначала растут `smfRetry` и
`smfCleanRetry`, затем появляется `SMF-WAIT`. Это прямой A/B признак USB
backpressure.

## Почему прежнее ощущение стабильности было реальным

Прошивка действительно играла MIDI стабильно при выполнении ее неявного
контракта: на USB host был открыт MIDI receiver. Parser и scheduler могли играть
длинные файлы, SD cache успевал, audio task не имел underruns.

Но этот контракт не был формализован в UI и hardware gate. Поэтому две разные
ситуации назывались одинаково "Cardputer подключен по USB":

```text
A. USB enumerated + CDC monitor
   MIDI TX никто не читает -> FIFO блокируется

B. USB enumerated + ALSA/QTrack receiver
   MIDI TX дренируется -> playback стабилен
```

После перехода от B к A стало казаться, что новая версия сломала чтение MIDI.
Фактически новая версия сломала обработку уже существующего endpoint stall.

## Ограничения `midi_probe` verdict

Показанный прогон завершался `Ctrl-C` во время активного playback. Поэтому NoteOn,
активные в момент завершения capture, могли быть объявлены stuck без возможности
увидеть последующий NoteOff. Аналогично паузы между музыкальными фразами могут
выглядеть как gaps.

Корректная процедура:

1. Запустить probe до playback.
2. Запустить файл.
3. Выполнить явный Stop/Panic в прошивке.
4. Оставить probe открытым минимум 500 ms для cleanup.
5. Только затем завершить capture.

`clock none` также не является ошибкой, если GP transport не запущен или realtime
TX подавлен выбранным master mode.

## Процессные причины

1. В одном dirty worktree смешаны browser memory fix, WDT diagnostics, external
   transport semantics и USB auto-recovery.
2. Hardware binary не был привязан к новому commit SHA после каждого изменения.
3. Наличие serial monitor использовалось как косвенный признак наличия MIDI
   receiver.
4. В acceptance test не было отдельного сценария "host mounted, MIDI port not
   open".
5. Автовосстановление добавлено сразу поверх failure handling без отдельного
   state-machine test на очередь, generation и watchdog fairness.

## План восстановления

### Этап 0. Сохранить доказательства

Не делать `reset --hard` в текущем рабочем дереве. Сначала сохранить patch или
временную debug-ветку, включая логи и WDT diagnostics.

### Этап 1. Восстановить точные baseline

Проверять в отдельных `git worktree`, чтобы не повредить текущие изменения:

```text
6bf4d6ac...  player-only stable checkpoint
1932f2e9...  player + endpoint readiness
9cbd7d3c...  committed SEQTRAK hardware RC
d4f7322d...  RC synchronized with main UI
```

Для каждого binary записывать:

```text
git SHA
SHA-256 bin
receiver topology
MIDI file
tempo mode
duration
serial log
MIDI capture
```

### Этап 2. Вернуть безопасное конечное поведение

Минимальная безопасная версия не должна автоматически продолжать playback.

Предлагаемый контракт:

```text
endpoint blocked
  -> ограниченные send retries
  -> ограниченная scoped cleanup
  -> invalidate scheduled generation
  -> сохранить paused tick
  -> прекратить cleanup loop до watchdog
  -> показать USB WAIT - OPEN MIDI RECEIVER

receiver returned
  -> отправить deferred scoped CC123/cleanup
  -> пользователь нажимает Space
  -> rebuild stream и новый anchor от paused tick
```

Это хуже по UX, чем прозрачный auto-resume, но детерминировано и безопаснее
текущего reboot.

### Этап 3. Добавить auto-resume отдельно

Auto-resume допускается только после тестов:

```text
old generation rejected
pending event cleared
stream rebuilt from paused tick
new deadline anchor published
no catch-up NoteOn burst
no NoteOff ownership loss
no WDT for 10 min without receiver
```

### Этап 4. Разделить текущие изменения

Нужны отдельные commits/PR:

1. Потоковый browser фиксированного размера.
2. WDT и boot diagnostics.
3. SEQTRAK Start/Continue adaptation.
4. USB endpoint recovery state machine.

Browser fix можно доказать отдельно. Recovery нельзя прятать в том же commit.

## Обязательная тестовая матрица

### ORIGINAL, receiver открыт

```text
[ ] 60 s playback
[ ] smfRetry остается около нуля
[ ] smfCleanRetry не растет
[ ] smfStall=0/0
[ ] explicit Stop, затем 500 ms capture
[ ] no stuck notes
[ ] no watchdog
```

### ORIGINAL, receiver отсутствует с начала

```text
[ ] bounded transition в Paused/Wait
[ ] нет бесконечного cleanup
[ ] нет watchdog 10 min
[ ] SD browser продолжает полный scan
[ ] UI остается управляемым
```

### Receiver закрывается во время playback

```text
[ ] позиция сохраняется
[ ] old generation invalidated
[ ] нет stale catch-up
[ ] no post-stall NoteOn
[ ] reconnect сам по себе не запускает музыку в первой безопасной версии
```

### SEQ MASTER через реальный USB host/bridge

```text
[ ] rx F8 > 0
[ ] WAIT -> LOCKING -> LOCKED
[ ] FA запускает transport
[ ] FC останавливает transport
[ ] нет realtime echo
[ ] notes продолжают передаваться
[ ] disconnect не вызывает auto-start
```

## Merge gate

Текущий gate не пройден, пока выполняется хотя бы одно условие:

```text
smfCleanRetry растет неограниченно
smfStall recovery оставляет maxLateUs в секундах
Reset Reason: 6 после endpoint stall
WDT-DIAG указывает MidiDispatchTask
hardware test не привязан к exact commit SHA
SEQ MASTER остается rx=0
```

## Итоговая причинная цепочка

```text
Быстрый SD producer
  -> плотнее наполняет MIDI queue
  -> TinyUSB TX FIFO быстрее достигает 16 packets
  -> при отсутствии ALSA/QTrack reader FIFO не дренируется
  -> send retries и cleanup retries
  -> новый незакоммиченный recovery оставляет cleanup pending бесконечно
  -> playback pause
  -> иногда stale auto-resume
  -> повторный stall
  -> task watchdog reset в MidiDispatchTask
```

Это объясняет все основные наблюдения без предположения, что MIDI-файлы
повреждены или SD-карта отваливается.
