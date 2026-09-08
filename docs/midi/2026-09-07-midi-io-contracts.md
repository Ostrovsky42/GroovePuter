# MIDI IN/OUT и nanoKEY2 — закреплённые контракты v1

Статус: спецификация для реализации, не заявление о поддержке на железе.
Дата: 2026-09-07. Исходный HEAD: `de6b5a3d`.
План: [этапы реализации](../superpowers/plans/2026-09-07-midi-io.md).

## Цель и границы

Korg nanoKEY2 через переходник USB-C вводит ноты в общий движок исполнения
GroovePuter: CHORD, ARP, LATCH, STRUM, RATCHET и EUCLIDEAN. Результат идёт
через Unit MIDI UART OUT в SEQTRAK. Встроенные клавиши остаются доступны.
USB и UART имеют независимые входы и выходы; подключение не меняет намерение
пользователя о маршруте. Поддерживаются также UART-клавиатура и USB MIDI от ПК.

В первой версии: один USB MIDI 1.0 class-compliant прибор, один виртуальный
кабель 0, один UART, один общий PERFORM target и общий профиль получателей.
Разные профили для двух выходов, USB hubs, MIDI 2.0, SysEx forwarding,
Program Change, pitch bend и произвольный MIDI THRU не входят в v1.
Наличие у USB-прибора OUT endpoint не означает, что он является синтезатором.

## Подтверждённая исходная архитектура

- `src/platform/cardputer_usb_midi_transport.cpp`: TinyUSB device, один
  MidiDispatchTask; вход обрабатывает только realtime transport.
- `src/midi/tee_midi_transport.h`: USB влияет на результат отправки UART;
  это несовместимо с новым контрактом независимой доставки.
- `src/platform/cardputer_uart_midi_transport.{h,cpp}`: UART1, RX GPIO1,
  TX GPIO2, 31250 8N1; очередь выхода и deferred NoteOff. Приём ещё не реализован.
- `src/input/performance_keyboard.{h,cpp}`: удержания идентифицируются `char`,
  `releaseMissingKeys()` сканирует все удержания. Внешние ноты нельзя добавлять
  в этот список без изменения идентичности источника.
- `src/midi/usb_midi_output.{h,cpp}`: музыкальные lanes, receiver mode,
  Pattern/SMF/live ownership. `midi_note_ownership_table.h` уже использует
  разреженную таблицу 128 ячеек на endpoint.
- Нынешний глобальный DRAM gate: 191488 байт, предварительное исключение,
  а не доказанный запас runtime heap. Release без PSRAM.

## C01. Порты, роли и наблюдаемые факты

Направления IN/OUT всегда заданы относительно GroovePuter.
USB-роль `Off | Device | Host` независима от направлений.
Device подключается к ПК; Host обслуживает nanoKEY2. На одном native USB
разъёме эти роли одновременно не работают. Host может иметь IN и OUT,
но разрешены только реально обнаруженные MIDI endpoints.

USB snapshot содержит requestedRole, activeRole, phase, canReceive,
canSend, sessionGeneration и lastError. Phase:
`Off | Starting | Waiting | Enumerating | Ready | Quiescing | Fault`.
UART snapshot содержит enabled, rxActivity, txQueued, txWritten,
overflow и cleanupPending. Не показывать UART receiver как Connected:
ни успешная запись, ни тишина RX не доказывают наличие/отсутствие SEQTRAK.

## C02. Выбор роли и аппаратный допуск H0

В v1 роль выбирается явно в PROJECT/MIDI и применяется после reboot.
После старта выбранной роли attach/detach/enumeration обрабатываются автоматически.
Автопереключение Host/Device по отсутствию трафика запрещено. Автовыбор роли
требует отдельного доказательства CC/VBUS detection по схеме Cardputer ADV.
При этом live-изменение входов/выходов не требует смены USB-роли.

H0 обязан установить точную схему питания VBUS с конкретным переходником,
совместимость ESP-IDF host API с установленным Arduino core, расход памяти,
дескрипторы nanoKEY2 и восстановление загрузчика. Не считать OTG-переходник
источником 5 В и не обещать работу от батареи до измерения.
Если питание или core не подходят, H0 завершается отчётом с конкретным
препятствием; host-интеграция не объявляется готовой. Проверка нового
питающего адаптера или смена framework — отдельное изменение объёма работ.

## C03. Явная маршрутизация

Настройки: USB IN → PERFORM, UART IN → PERFORM, PERFORM external enable,
USB OUT enable, UART OUT enable, ClockSource = Internal/USB/UART.
Фильтр входного канала: Omni или один канал 1..16; внутри кода 0..15.
Профиль nanoKEY2: USB Host, USB IN on, UART IN off, UART OUT on,
USB OUT off, Internal clock, Omni, SEQTRAK profile.
Выбор этого пресета явный; enumeration его не применяет.

Существующий проект после миграции сохраняет Device и нынешние USB/UART
выходы; clock source сохраняется (старый SeqtrakExternal означает USB).
Оба выхода используют общий текущий route/profile snapshot.
Смена профиля сохраняет существующее правило reboot.

## C04. Общий вход исполнения

Идентификатор удержания = `(source, sessionGeneration, channel, key)`.
Для QWERTY key — физическая клавиша; для MIDI key — исходный номер ноты.
MIDI NoteOn velocity=0 нормализуется в NoteOff. MIDI note и velocity
проверяются в диапазоне 0..127; ноты не преобразуются в ASCII/QWERTY.
External note range 0..127; octave-кнопки nanoKEY2 уже меняют входную ноту.
QWERTY сохраняет текущие octave/scale mapping и диапазон 12..95.
Входная MIDI-нота — абсолютный pitch: без дополнительного octave offset
или автоматического scale quantize. Выбранная scale используется существующим
алгоритмом CHORD. Выход преобразований за 0..127 пропускается, не оборачивается.

Один общий PERFORM движок и transport timeline; нового arp scheduler нет.
Внешний PERFORM остаётся активен при смене UI-страницы; QWERTY сохраняет
текущие ограничения страниц. Глобальный panic применяется ко всем источникам.
`releaseMissingKeys()` освобождает только QWERTY.

Повторный NoteOn того же input key обновляет velocity, но не увеличивает
число удержаний. Совпадающий pitch разных источников представлен в общем
пуле один раз, со счётчиком вкладов; освобождение одного не гасит другой.
При новом владельце звучащий pitch не атакуется повторно. Для последующих
ARP атак velocity берётся от последнего принятого активного вклада.

Для DRUMS внешние MIDI notes 36..42 соответствуют семи live lanes 0..6;
остальные игнорируются с диагностикой. Wire note/channel определяет профиль.
Этот mapping явно показывается в документации, не выдаётся за GM drum map.

## C05. Sustain, latch и отключение источника

CC64 >=64 удерживает отпущенные ноты только своего source/session/channel;
CC64 <64 освобождает их. CC123 снимает удержания своего input channel,
CC120 также немедленно удаляет его latched/sustained вклады.
Остальные CC считаются unsupported и не пересылаются. Sustain применяется
на входе PERFORM, поэтому не отправляется автоматически на SEQTRAK.

Обычный NoteOff сохраняет существующую музыкальную семантику LATCH.
Detach, input disable, channel-filter change, input overflow и source panic
удаляют также sustained/latched/pending-latch вклады затронутого источника.
Готовый агрегированный latch без происхождения недопустим.
При изменении пула отменяются устаревшие generated события и пересчитывается
voicing на том же timeline; локальные удержания остаются. Нельзя сохранить
старый аккорд только потому, что он был вычислен до отключения Korg.

Смена PERFORM target очищает все live удержания/latch и scoped live output
на прежнем target, затем принимает только новые нажатия. Pattern/SMF не гасит.

## C06. Независимая доставка на выходы

Один MidiDispatchTask потребляет музыкальное событие один раз и делает
независимые попытки на разрешённые endpoints. Каждый endpoint имеет свои
wire ownership, pending cleanup, generation и backpressure state.
Успех означает принятие в локальный transport, а не подтверждение звука.

USB reject не задерживает UART. UART reject не повторяет доставленный USB
NoteOn. Retry имеет идентичность `(endpoint, generation, eventSequence)`.
SMF producer не перематывает общий поток из-за отказа одного endpoint.
Readiness — возможность обслуживания хотя бы одного выбранного выхода;
UI отдельно показывает качество каждого выхода.

Музыкальный scheduler и route projection остаются общими. Допустимы два
endpoint state экземпляра существующего output-кода после отделения scheduler
и capability side effects; нельзя просто добавить второй router sink со
старым общим retry. Выходы не конкурируют за глобальное владение MIDI-каналом.

NoteOff использует резерв и bounded recovery. Deferred NoteOff старой атаки
должен быть принят раньше нового NoteOn того же endpoint/channel/note.
Переполнение запрещает новые NoteOn на затронутом endpoint до cleanup;
второй endpoint продолжает работать. Scoped cleanup не гасит владельцев
Pattern/SMF на той же ноте; channel panic допустим только как явная аварийная
потеря всего состояния данного endpoint/channel с последующей сверкой owners.

## C07. Переходы и восстановление

| Событие | Действие | Следующее состояние |
|---|---|---|
| USB start | Инициализировать только выбранный стек | Waiting |
| Attach в Host | Прочитать дескрипторы, проверить MIDI/cable/endpoints | Enumerating |
| Enumeration успешен | Новая sessionGeneration, очистка parser | Ready |
| Неподдерживаемый прибор/ошибка | Счётчик и причина, UART продолжает | Fault |
| USB detach | Инвалидировать session, снять входные вклады, забыть недоставимые USB retries | Waiting |
| Output disable/change | Закрыть admission, закончить scoped cleanup старого маршрута | Disabled или CleanupPending |
| Output enable/reconnect | Новый generation, cleanup handshake, затем новые события | Ready |
| Input overflow | Инвалидировать session, снять её вклады, очистить parser/queue | Recovering, затем Ready |
| USB role change | Сохранить next-boot роль и показать Reboot required | Активная роль прежняя |

События предыдущего generation не могут попасть в новый сеанс.
На восстановленном выходе не проигрывается backlog старых атак. После cleanup
прямые удерживаемые live-ноты сверяются и атакуются один раз; ARP/Pattern/SMF
продолжаются со следующего актуального события. MIDI Start/Continue не
генерируется только из-за переподключения.
Если отключённый приёмник продолжает звучать, физически отправить ему NoteOff
невозможно: UI показывает cleanup pending до доступности или явного сброса.

## C08. Clock и петли

Ровно один clock master: Internal, USB IN или UART IN. Остальные realtime
пакеты не управляют timeline. Clock отделён от note input enable/filter.
Смена master инвалидирует прежнюю оценку и queued transport generation.
При потере внешнего clock действует существующая follower timeout policy,
без скрытого переключения на Internal; конкретные сроки фиксируются тестом
существующего follower до интеграции UART.

В v1 внешний clock не ретранслируется. Internal clock отправляется на
явно включённые выходы по device capabilities. Raw echo/THRU отсутствует.
Для полного MIDI-кольца с SEQTRAK требуется отключённый receiver THRU:
для обычного DIN MIDI невозможно надёжно распознать возвращённую собственную
ноту по содержимому. Input notes с SEQTRAK по умолчанию off, clock можно включить.

## C09. Потоки, границы и память

USB callbacks только публикуют события в очередь; UART parser живёт в
MidiDispatchTask. Control/loop task единолично меняет PerformanceKeyboard.
Разные producers имеют разные SPSC очереди; нельзя сделать старую SPSC MPSC.
Смена output настроек передаётся командой dispatcher, не прямой записью UART
из UI. Состояния для UI публикуются snapshot, без чтения изменяемых таблиц.

Начальные лимиты: 64 события на входной порт, 16 slots reserved для release,
64 внешних удержания суммарно плюс 19 QWERTY; generated pool максимум 16 нот,
выбор в порядке входных удержаний с удалением дубликатов. Превышение held
capacity отклоняет только новый NoteOn; release не требует свободного slot.
Overflow critical queue ставит отдельный несбрасываемый до обработки recovery
flag, так что panic не зависит от возможности enqueue.
Drain budget 32 input события на порт за итерацию; действующий UART TX budget
32 bytes сохраняется. После budget задача уступает CPU.

В audio callback запрещены allocation, USB/UART I/O и блокировки.
Host stack allocations разрешены на lifecycle worker, не в audio callback.
DRAM gate 191488 не повышать ради прохождения. Фиксировать sizeof всех новых
таблиц, min free heap, largest block, stack high-water и SD/playback нагрузку.
Достаточность runtime памяти требует измерений H0 и финального H1, не только ELF.

## C10. Диагностика и критерий готовности

PROJECT/MIDI отдельно показывает active/pending USB role, RX ready/activity,
TX enabled/blocked/cleanup, маршруты, clock source и ошибки питания/enumeration
только если их причина установлена. Unknown USB error не подписывается No power.
Логи: endpoint, session/route generation, accepted/dropped notes, retries,
critical overflow, source resets и timestamp перехода.

Полная готовность требует host tests, SDL, Device+CDC, MIDI-only Device и Host
builds с DRAM gate, а также физический nanoKEY2 → ARP → UART → SEQTRAK прогон.
Не считать mock enumeration аппаратной приёмкой.

## Источники и степень уверенности

- [M5Stack Unit MIDI](https://docs.m5stack.com/en/unit/Unit-MIDI): SEPARATE,
  вход/выход Grove, общий SAM2695/MIDI OUT.
- [Cardputer ADV](https://docs.m5stack.com/en/core/Cardputer-Adv): схема и плата;
  конкретная подача VBUS через переходник остаётся проверкой H0.
- [Espressif USB Host](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_host.html):
  host lifecycle; доступность API проверять по установленной версии core.
- [Korg nanoKEY2 manual](https://cdn.korg.com/us/support/download/files/c1f942dee757c6f016ad96c675b9381f.pdf):
  USB питание и MIDI; реальные descriptors/capabilities записать в H0.

Изменение любого C01..C10 требует обновить этот документ, тест контракта и
план в одном checkpoint. Аппаратное предположение нельзя превращать в контракт
поддержки без evidence.
