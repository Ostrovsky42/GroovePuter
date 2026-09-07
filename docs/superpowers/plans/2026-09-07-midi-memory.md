# nanoKEY2 / GroovePuter: план бюджета памяти

Статус: план измерений и реализации, численные аппаратные gates ещё не пройдены.
Основной [план MIDI](2026-09-07-midi-io.md),
[контракты C01–C12](../../midi/2026-09-07-midi-io-contracts.md).
Работа в существующей ветке `feature/20260907-midi-io-nanokey2`.

Цель: сохранить nanoKEY2 → общий PERFORM/ARP/ритмы → UART → SEQTRAK
при достаточной памяти для загрузки, переходов UI и восстановления соединения.
Сначала измерить стоимость ресурсов и их время жизни, затем менять владельцев
и резервирование. Удаление функций или запрет их совместного использования
требует конкретного предложения с измеренной экономией и решения пользователя.

## Исходные сведения и ограничения выводов

Пользователь сообщил `heap: 347360` в standalone RX2 после подключения Korg.
Это текущий `ESP.getFreeHeap()` другой прошивки. В присланном отчёте основной
прошивки фигурируют около 356 свободных байт, SD около 30 КБ, SMF около 9 КБ.
Эти числа — ориентиры для воспроизведения: сборка, capabilities и точки
измерения должны совпадать, прежде чем считать их сравнительным бюджетом.
Из одного показания нельзя вычислить стоимость Host или неизбежность OOM.

Release без PSRAM; существующий ELF DRAM gate ≤191488 байт сохраняется.
Этот порог ограничивает учитываемую скриптом статику, но не доказывает запас
heap. Не вычислять доступную кучу простым вычитанием из номинальной SRAM.
Разные capability heaps могут перекрываться: INTERNAL/8BIT/DEFAULT/DMA
нельзя складывать как независимые объёмы.

## M0. Воспроизвести бюджет основной прошивки

Файлы: `src/platform/cardputer_runtime_diagnostics.{h,cpp}`, `GroovePuter.ino`,
`scripts/build_cardputer_memory_baseline.sh`,
`scripts/check_cardputer_dram_budget.sh`;
результаты в `docs/midi/2026-09-07-midi-io-evidence.md`.

- [ ] Зафиксировать HEAD и dirty diff, core/FQBN, ELF/map и hash бинарника.
  Проверить состав исправлений из соседней сессии; отдельно отметить, какие
  исправления UI allocation уже присутствуют в проверяемой сборке.
- [ ] Расширить существующий fixed-size MemorySnapshot: free, minimum free,
  largest block для INTERNAL|8BIT, INTERNAL|DEFAULT и INTERNAL|DMA, failed
  allocation size/caps, stack high-water каждой задачи. Проверить единицы
  stack API по установленному core: не умножать на sizeof(StackType_t) автоматически.
- [ ] Измерить перед/после M5, display, AudioTask, DSP delays, SD mount,
  SMF begin, scene load и первого кадра UI. Повторить три холодных старта.
- [ ] Проверить открытие всех реально доступных страниц, генерацию,
  загрузку/сохранение сцены, SMF load/play/stop и sample playback.
  Записать пиковое одновременное выделение, а не только heap после возврата.
- [ ] Для каждого потребителя составить запись: владелец, static/heap/stack,
  bytes/caps/alignment, момент allocation/free, обязательность, одновременные
  пользователи. Данные ELF и runtime не учитывать дважды.

Выход: воспроизводимые таблица владельцев и минимумы heap/largest block;
конкретные allocation failures имеют размер, caps и точку вызова.
Диагностика использует фиксированные записи; форматирование и integrity scan
не выполняются в audio/USB callbacks. Её собственная память учитывается.

## M1. Измерить добавочную стоимость Host и проверить выбор роли

Файлы: `tools/hardware/nanokey2_h0/nanokey2_h0.ino`,
`src/platform/cardputer_usb_midi_transport.cpp`, `scripts/build.sh`,
установленные USB/FreeRTOS headers и startup-код core.

- [ ] В одной probe-сборке снять одинаковые capability snapshots перед host
  install, после install/register, после enumeration/claim/transfer alloc,
  при приёме аккорда, после detach/cleanup и повторного attach.
- [ ] Отдельно учесть задачу UI 4096 байт из probe, её TCB и display allocations.
  Она не является обязательной стоимостью production Host и не переносится
  в основную прошивку автоматически.
- [ ] Исправление snapshot при будущей реализации должно копировать весь
  PacketDiagnostics под общей блокировкой; сейчас RAW/NOTE читаются вне неё.
  Отрисовку выполнять после снятия блокировки.
- [ ] Проверить 20 циклов attach/detach: resident bytes, peak, largest block,
  число handles/transfers. Отличать остающиеся allocations установленной
  библиотеки от утечки на каждый новый сеанс.
- [ ] Проверить инициализацию core до setup: можно ли выбрать Host/Device
  до старта стека в одном MIDI-only binary. Сопоставить ELF и heap отдельных
  Host/Device сборок; не считать стоимость замены стеков простым сложением.

Выход: measured resident/peak Host cost и максимальная непрерывная allocation
по каждому caps; решение о build/boot механизме до P4. Если единый бинарник
невозможен без смены framework, это явное ограничение C02 для согласования.

## M2. Обеспечить бюджет до production-интеграции

Файлы: `src/platform/cardputer_smf_player{,_registry}.{h,cpp}`,
`src/platform/cardputer_sd.cpp`, `GroovePuter.ino`,
`src/input/performance_keyboard.{h,cpp}`, `src/midi/midi_input_queue.h`,
`src/midi/midi_endpoint_dispatch.h`, `src/midi/midi_note_ownership_table.h`.
Изменять только подтверждённых M0/M1 владельцев; дополнительные пути UI/DSP
включить в evidence после локализации allocation.

- [ ] Посчитать target ABI sizeof/alignment: две ingress очереди (65 storage
  slots каждая), две output FIFO64, source-held64+QWERTY19, generated16,
  endpoint ownership, recovery state, buffers и task stacks/TCBs.
  Host sizeof не считать точным размером ESP32. Для очереди считать
  storage × sizeof(event) + metadata, включая padding uint64 sequence.
- [ ] Сохранить один scheduler и один PERFORM engine. Проверить, не резервирует
  новый endpoint второй комплект музыкальных таблиц или стек dispatch task.
  Pool/static allocation лишь меняет место затрат, не создаёт свободную RAM.
- [ ] Сначала исправить подтверждённые утечки и ненужные дубли. Затем сделать
  ленивыми измеренные неиспользуемые heap allocations. В SMF registry уже
  есть ensureStarted(), но boot явно вызывает begin; сам player_ также
  резидентен. Разделить эти затраты до обещания экономии от lazy init.
- [ ] Для освобождения SD/SMF установить протокол владельца: stop admission,
  дождаться завершения операции, закрыть files, снять ссылки/очереди consumers,
  освободить ресурс. SD может одновременно обслуживать samples, scenes и SMF;
  не unmount при активном потребителе или несохранённой записи.
- [ ] Сохранять рассчитанные запасы стеков до измерения худшего сценария.
  Не уменьшать loop stack 32768 только по одной спокойной сессии: учесть
  generation, page creation, callbacks и сценовую загрузку.
- [ ] Проверить lifecycle/failure injection тестами: отказ каждого обязательного
  allocation возвращает контролируемую ошибку без partial init/leak; повторный
  запуск проходит; stop/release не запускает сервис заново; работающий audio
  и уже принятые NoteOff продолжают обслуживаться.

Численный критерий admission для каждого caps c:

`freeBefore(c) >= peakAdditional(c) + reserve(c)`

`largestBefore(c) >= largestRequiredAllocation(c)`

Стартовый инженерный reserve(c) = max(16384 байт, удвоенный измеренный пик
временных allocations допустимой конкурентной операции). Это проектный
порог, не характеристика ESP32 и не гарантия отсутствия фрагментации.
Для каждого пула определить peak и reserve отдельно; общий минимум не
подменяет DMA/DEFAULT. Admission проверяется одним владельцем перехода;
проверка heap из UI не резервирует память от другой задачи. Возврат allocator
проверять даже после успешной оценки. Суммарный peak моделирует все разрешённые
одновременно переходы, включая page change во время USB reconnect.

Выход: текущий runtime плюс измеренная верхняя оценка Host и новые таблицы
укладываются с запасом. Если нет — конкретная таблица вариантов экономии
(байты, функция, поведение), затем решение пользователя об ограничениях.

## M3. Проверить интеграцию перед музыкальной приёмкой

Файлы: production Host adapter/build из P4, существующая диагностика,
evidence. Предусловие: M2 и P1–P4 software gates PASS.

- [ ] Собрать реальную Host GroovePuter; сохранить ELF/map/DRAM gate и полный
  runtime бюджет, сравнить с оценкой M2. Не считать урезанный probe приёмкой.
- [ ] 30 минут audio+ARP+UART с UI navigation, 20 reconnect с sustain/latch;
  повторить с SD samples/SMF, если их совместная работа заявлена поддержанной.
  Включить самый дорогой сценарий M0 — например, загрузку сцены или генерацию.
- [ ] В каждой фазе после admission сохраняется рассчитанный reserve по caps,
  обязательный следующий allocation помещается в largest block; нет failed
  allocations, heap corruption, watchdog resets и новых audio underruns.
  После прогрева повторяющиеся циклы не дают нарастающей потери heap.
- [ ] Сопоставить результаты с Device-регрессией; подтвердить отсутствие
  старых атак и преждевременного free inflight transfer. Затем перейти к H1a.

Выход: memory gate PASS на явно записанном наборе совместных функций и
конкретной сборке. Hardware NOT TESTED остаётся незакрытым gate. Успешный
compile или защита от OOM в одном draw path не заменяют runtime budget.
