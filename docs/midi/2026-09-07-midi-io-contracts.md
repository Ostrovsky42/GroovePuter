# MIDI IN/OUT и nanoKEY2 — закреплённые контракты v2 (после RX2)

Статус: нормативная спецификация следующего этапа; полная реализация НЕ завершена.
Дата: 2026-09-07. База: `de6b5a3d`; ветка `feature/20260907-midi-io-nanokey2`.
Этот документ в feature-worktree заменяет черновик v1 из основного checkout.
Сохранены его продуктовые решения; уточнены доказательства, recovery и delivery.
«Закреплено» означает требование к реализации, а не подтверждение тестом.
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


## Что действительно проверено

Пользователь сообщил «да, все работает!» после RX2. Это качественное
подтверждение USB MIDI RX на конкретном комплекте Cardputer + переходник +
nanoKEY2. Ранее пользователь сообщил VID:PID `0944:0115`, IN `0x81`,
OUT `0x02`. OUT обнаружен, но передача на него не подтверждена.
Диагностическая прошивка не содержит PERFORM/ARP/UART и заменяет GroovePuter.

RX2 собирался установленным M5Stack core 3.2.2 / ESP-IDF 5.4 с отключённым
CDC-on-boot и PSRAM. Точное воспроизведение:
[probe README](../../tools/hardware/nanokey2_h0/README.md).
Нет записанных 100 пар нот, 20 reconnect, замера VBUS, latency и runtime
memory под audio/SD: эти пункты остаются открыты. Нулевые четырёхбайтовые
блоки не означают NoteOff; отдельные RAW/NOTE сохраняют последнее валидное
наблюдение, а не последний нулевой блок.

| Основание в ветке | Реальный статус |
|---|---|
| `90b53692`: identity, SPSC, MidiIoState | Частичная основа; UART generation и fencing переходов не завершены |
| `925d9a40`: endpoint dispatcher | Прототип; 8-slot cache вытесняет историю и допускает повторную отправку |
| `e70fc634` + незакоммиченные RX2 изменения | Standalone USB RX; не production integration |
| Основной `TeeMidiTransport` | По-прежнему связан с USB; независимость ещё не обеспечена |

В прототипе повтор sequence после восьми новых sequence больше не находит
свой acceptedMask. Это вывод из кода, ещё не результат отдельного regression
run. Новый план начинает с воспроизводящего теста, не с подключения прототипа.

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

H0-RX уже имеет качественное пользовательское подтверждение и разрешает
разработку host-интеграции на этом комплекте. Аппаратная приёмка H0-STABLE
остаётся отдельным gate перед признанием поддержки готовой; она должна установить схему питания VBUS с конкретным переходником,
совместимость ESP-IDF host API с установленным Arduino core, расход памяти,
дескрипторы nanoKEY2 и восстановление загрузчика. Не считать OTG-переходник
источником 5 В и не обещать работу от батареи до измерения.
Если проверка выявляет неподходящее питание или core, H0-STABLE завершается отчётом с конкретным
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
Внешний producer вызывает submit ровно один раз в монотонном порядке sequence;
retry выполняет только dispatcher. Повтор/регресс sequence отвергается до
обоих transport. Sequence — uint64_t в пределах boot; переиспользовать его
при reconnect запрещено. Payload принятого события неизменяем.

На endpoint — FIFO 64 события, из них последние 16 недоступны новым NoteOn.
Outstanding запись никогда не вытесняется ради новой. При давлении новый
NoteOn этого endpoint отклоняется с диагностикой; принятые другим endpoint
события не повторяются. Critical overflow закрывает admission и запускает
отдельный cleanup, не требующий места в полной FIFO. Clock не копится в note
FIFO: используется существующий realtime publisher с fencing generation.
При длительном отказе transport (100 ms от первой неуспешной попытки головы
FIFO, конфигурируемая константа с тестом на границе) endpoint переходит в
Recovering, backlog атак удаляется после reconciliation owners. Новый сеанс
начинается с cleanup, а не с проигрывания старых NoteOn.
SMF producer не перематывает общий поток из-за отказа одного endpoint.
Readiness — возможность обслуживания хотя бы одного выбранного выхода;
UI отдельно показывает качество каждого выхода.

Музыкальный scheduler и route projection остаются общими. Допустимы два
endpoint state экземпляра существующего output-кода после отделения scheduler
и capability side effects; нельзя просто добавить второй router sink со
старым общим retry. Выходы не конкурируют за глобальное владение MIDI-каналом.

NoteOff использует резерв и bounded recovery. Deferred NoteOff старой атаки
должен быть сериализован в transport раньше нового NoteOn того же endpoint/channel/note.
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
Overflow critical queue увеличивает out-of-band recovery epoch, так что
очистка не зависит от возможности enqueue. Producer прекращает note admission.
Control снимает старые вклады и подтверждает epoch; producer сбрасывает parser
и публикует новый input generation, после чего admission возобновляется.
Поступивший во время подтверждения новый recovery epoch нельзя потерять.
Consumer не переписывает producer head; старые entries извлекаются и отбрасываются
по generation. UART проходит тот же протокол, хотя физического detach у него нет.
Input generation отдельно от USB session и output generation: изменение
input filter не сбрасывает исправный USB OUT.
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

## C11. Интерфейсы границ (планируемые API)

Все объявления ниже — целевые API, а не описание уже существующих методов.
Namespace `GroovePuterMidi`; определения input identity остаются в
`midi_input_event.h`. MIDI channel внутри 0..15, CC value передаётся в поле
`velocity`; у channel-wide событий `id.key = 0`.

```cpp
enum class ResetReason : uint8_t {
    Detach, InputDisabled, FilterChanged, Overflow, SourcePanic, TargetChanged
};
struct InputSession { InputSource source; uint32_t generation; };

// midi_input_parser.h: caller owns one parser per source and feeds its queue.
struct ParseResult {
    bool hasInput;
    MidiInputEvent input;
    bool hasRealtime;
    uint8_t realtimeStatus;
};
class MidiInputParser {
public:
    void reset(InputSession session);
    ParseResult usbPacket(const uint8_t (&packet)[4], uint32_t atMicros);
    ParseResult uartByte(uint8_t byte, uint32_t atMicros);
};
```

UART running status сохраняется при F8..FF; realtime возвращается отдельно.
SysEx пропускается до F7, embedded realtime не теряется; system common сбрасывает
running status. Неполный USB packet отбрасывается платформой, remainder счётчик
растёт; CIN, status, длина и 7-bit data проверяются, cable != 0 игнорируется.
USB SysEx/unsupported CC не попадают в PERFORM. Realtime status фильтрует
существующий clock adapter, а не PerformanceKeyboard.

В `PerformanceKeyboard` добавить публичные методы (существующие char APIs
сохранить как QWERTY adapters):

```cpp
bool handleMidiInput(const GroovePuterMidi::MidiInputEvent& event);
void resetMidiInput(GroovePuterMidi::InputSession session,
                    GroovePuterMidi::ResetReason reason);
void setExternalInputEnabled(bool enabled);
```

`handleMidiInput` получает уже проверенную session/filter; false означает
отклонённое новое удержание/неподдерживаемый target, не требование retry.
`resetMidiInput` идемпотентен и удаляет только указанное поколение источника.
Физические, sustained и latched вклады принадлежат `performance_input_state.h`;
голосообразование и расписание остаются в PerformanceKeyboard.

В `midi_endpoint_dispatch.h` заменить публичный retry API:

```cpp
struct EndpointSession { MidiEndpoint endpoint; uint32_t generation; };
struct SubmitResult { uint8_t queuedMask; uint8_t rejectedMask; };
// MidiEndpointEvent.sequence меняется с uint32_t на uint64_t.
// NoteOn/Off остаются; clock/control идут существующими typed queues.
class MidiEndpointDispatcher {
public:
    MidiEndpointDispatcher(IMidiTransport& usb, IMidiTransport& uart);
    void configure(EndpointSession session, bool enabled);
    SubmitResult submit(const MidiEndpointEvent& event);
    void service(uint32_t nowMicros, uint8_t eventBudgetPerEndpoint);
};
```

`queuedMask` — admission в локальную очередь, НЕ подтверждённая отправка.
Endpoint wire ledger обновляется только при полном принятии сообщения transport.
`IMidiTransport` rejection не должен частично публиковать MIDI message.
Для UART partial driver write хранится offset одного принятого сообщения;
повторяет остаток только transport, не dispatcher. Accepted в USB Host означает
копирование в принадлежащий host buffer; ошибка позднего completion запускает
endpoint recovery, а не повторную доставку другому endpoint.
`configure` задаёт новое поколение и закрывает старое; retries предыдущего
поколения и callback после закрытия не изменяют новое. Output engine сохраняет
owner token live/Pattern/SMF до wire projection; raw channel/note недостаточно
для scoped cleanup. Clock/control delivery должны иметь те же endpoint fencing
и независимость, но не новый scheduler.

USB lifecycle worker единолично владеет handle/interface/transfer и serializes
callback события с session token. Detach: запрет resubmit → halt/flush → дождаться
completion/cancel всех transfers → free → release → close. Ошибка шага не даёт
права free inflight; cleanup повторяется bounded service, UI показывает Fault.
Endpoint выбирается внутри конкретных configuration/interface/alternate setting,
а не последним совпавшим endpoint из всего descriptor. Host OUT упаковывает
MIDI 1.0 четыре байта в собственном bounded buffer; RX/TX completion независимы.

## C12. Бюджет памяти до допуска Host

Обязательны этапы M0–M3 из [плана памяти](../superpowers/plans/2026-09-07-midi-memory.md).
M0/M1 устанавливают сравнимые per-capability free/minimum/largest и resident/peak
затраты основной прошивки и Host. M2 обеспечивает запас до P4; M3 проверяет
реальную интеграцию до H1a. Порог статики 191488 не повышается.

Начальный reserve по caps = max(16384 байт, 2 × измеренный временный пик
разрешённой конкурентной операции). До admission free покрывает добавочный
peak и reserve, largest block покрывает максимальный требуемый allocation.
Проверка allocator остаётся обязательной; heap snapshots не резервируют память.
Перекрывающиеся capability pools не суммировать, task stacks не учитывать дважды.

Показание ESP.getFreeHeap() standalone probe не является стоимостью Host.
Меньший расход от SD/SMF teardown подтверждается измерением и безопасным
протоколом владения всеми files/consumers. Удаление функций, уменьшение
музыкальных лимитов и запрет совместных режимов не выполняются молча.

## Основания и изменения контракта

Аппаратные сведения выше — наблюдения пользователя и локального probe, не
сертификация всех адаптеров. Доступные API проверяются по установленным headers
M5Stack core, не по документации другой версии. Внешние схемы и manuals при
аппаратной приёмке проверяются отдельно; предположения о питании не становятся
заявлением о поддержке.

Изменение C01–C12 требует обновить этот документ, поведенческий тест и план
одним checkpoint. Численные лимиты — проектные стартовые значения; их изменение
требует нового измерения памяти, а не только правки констант.
