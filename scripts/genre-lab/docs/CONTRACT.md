# Контракт данных

## bars.jsonl — общий формат для корпуса И дублей

Одна строка = один такт.

```json
{"src":"take:house.deep:3","variant":"house.deep","bar_idx":0,"take":3,"bpm":124,
 "events":{
   "kick":  [[0,0.9],[4,0.9],[8,0.9],[12,0.9]],
   "snare": [[4,0.8],[12,0.8]],
   "hat":   [[2.08,0.6],[6.08,0.6]],
   "bass":  [[3,0.8,36,2.0],[10,0.7,43,1.5]]
 }}
```

| поле | тип | правило |
|---|---|---|
| src | str | корпус: `jamendo:<id>`; дубль: `take:<variant>:<take>` |
| variant | str | `<family>.<variant>`, append-only, как recipe ID |
| bar_idx | int | номер такта внутри трека / фразы, с 0 |
| take | int | **только у дублей**; отсутствие = корпус |
| events.<role> для ударных | `[pos, vel]` | роли kick / snare / hat / ohat (ключи по ролям, как в схеме; «events.drum» здесь = любая ударная роль). pos — шаг 0..16 (float, можно −0.4 для ранних ударов), vel 0..1 |
| events.bass | `[pos, vel, pitch, dur]` | pitch — MIDI, dur — длина в шагах |

Неизвестные роли → ошибка разбора. Пустые роли можно не писать.

## Что должен выдавать GroovePuter (`gen_probe --emit-takes-jsonl`)

- Для каждого проверяемого recipe/variant: **8 дублей**. Дубль k = k-е нажатие G от сброшенной таблицы попыток для кортежа `(mode, recipe, P-level, pattern_address)`. Пользовательского seed в устройстве нет.
- Каждый дубль — **фраза из 4 тактов** (или вся фраза phrase engine, если она 1/2/4/8).
- Детерминированность: тот же кортеж и тот же билд → побайтно тот же файл. Это золотой тест (host, `tools/gen_probe/test_takes_jsonl.sh`).
- Таймингы в v1: `timing:"grid"`, целые шаги. Свинг и микротайминг не записаны, см. раздел «Тайминг и провенанс» ниже.
- Отображение голосов GroovePuter на роли:

| роль | голос GroovePuter (VoiceId) | источник |
|---|---|---|
| kick | BD = `VoiceId::DrumKick`, лейн 0 | `scenes.h:224` (`VoiceId`), `sequencer_hub_page.cpp:20` |
| snare | SD = `DrumSnare`, лейн 1, и CP = `DrumClap`, лейн 7 (объединяются, vel = максимум) | то же |
| hat | CH = `DrumHatC`, лейн 2 | то же |
| ohat | OH = `DrumHatO`, лейн 3. **Только OH.** | то же |
| bass | Synth A (303A) = `VoiceId::SynthA`, см. таблицу басов ниже | `strong_rhythm_migration.cpp:1002`, `:1123` |

Ride / crash в секвенсоре нет (лейна нет; `CYMBAL` есть только в `mini_drumvoices.h:24`, не в лейнах). MT, HT, RM в v1 не пишутся.

### Таблица басов: рецепт → голос баса

Басовая роль (`BassRhythm`) в strong-маршруте всегда пишется в Synth A; Synth B получает аккорд или мелодию
(`strong_rhythm_migration.cpp:1002`, `:1123`). Для идиом то же: `bass` = Synth A (`genre_idiom.h:652-661`).
Все 37 строк `kProfiles` (`generation_profile.cpp:406-446`) имеют басовый вид, поэтому событие `bass` пишется для всех.
Пустой басовый план (`ValidButEmpty`) даёт пустой список, это допустимо.

| режим | рецепт | басовый вид | строка | голос баса |
|---|---|---|---|---|


## profiles.json

`{"meta": {...}, "variants": {"house.deep": VariantProfile}}`. Поля — `src/gplab/profile.py::VariantProfile`. Это артефакт для ревью: его можно читать глазами и диффать между прогонами.

## genre_lab_profiles.h

Сгенерированный C++17-заголовок, только `constexpr`, без кучи. Квантование `uint8` (значение/255). Не редактировать вручную: правка = новый прогон + новый заголовок. Провенанс (версия инструмента, дата, sha профилей, корпус) — в шапке файла.

Это **слой данных**. Как превратить `cells`/`stepProb` в идиомы GroovePuter, решает отдельный PR (AGENT_TASKS, PR-4).


## Тайминг и провенанс (v1, gen_probe --emit-takes-jsonl)

- `"timing":"grid"` в каждой строке. Свинг (`feel.swingMask`, `swingPct`) и микротайминг (`steps[s].timing`) в v1 **не** записаны:
  реально звучащий путь найден (`src/dsp/miniacid_engine.cpp:3108-3169`, `processSequencerEvents`: swing + `timing` в тиках по 24 на шаг),
  но его замер не проверен прослушиванием и не сверен с устройством. `timingTicksForStep_` и `grooveOverrideTicksForStep_`
  нигде не вызываются, поэтому их нельзя брать как источник.
- Длина баса `dur` — в шагах сетки: слайд-связанный шаг продолжает ноту (как в аудите). Это не звучащий gate.
- Поля провенанса верхнего уровня (`recipe_id`, `mode`, `p_level`, `pattern_address`, `build_sha`, `timing`) не входят в схему.
  gplab их игнорирует (`schema.py`, `Bar.from_json` читает только `src`, `variant`, `bar_idx`, `take`, `bpm`, `events`).
- Адрес `pattern_address` для пути MATERIAL всегда `0xFFFF` (`kLogicalPhraseAttemptChannel`, `generated_phrase_p1r_materializer.h:36`).
  Дубль k — k-е нажатие G от сброшенной таблицы попыток, ordinal = k (`src/state/generation_request_state.h:192-234`).

## Метод дублей: undo или sequential (доказательство)

- `gen_probe --emit-takes-jsonl` по умолчанию: `--sequence undo`. Каждый дубль k: свежий проект, k пробных нажатий G с Ctrl+Z, затем измеряемое нажатие. Номер попытки = k.
- `--sequence sequential`: один проект, одно reset, нажатия G подряд, как на устройстве.
- Проверка `tools/gen_probe/test_takes_jsonl.sh`, PASS 4: для House, Techno, UKG и Acid BASE события (`events`) каждого такта у sequential и undo побайтно совпадают для k = 0 и 1 (8 из 8 тактов в каждом рецепте).
- Sequential за два дубля упирается в место в Song: длина 8 → 12 → 16, третье нажатие не добавляет такт (`#SEQ`, ошибка «no room»). Значит, k ≥ 2 через sequential не получить, и устройство тоже не выдаст восемь дублей подряд в одном проекте без очистки.
- **Решение:** `undo` — метод по умолчанию, потому что он даёт все восемь дублей. Доказательство равенства покрывает только k = 0, 1.

## Легаси-счётчик аудита (факт, не исправлен)

- Измерено (копия в scratchpad, без изменения репо): после измеряемого нажатия `next_ordinal` для House, Acid, Techno, UKG = 1, 3, 6, 10, 15, 21, 28, 36. Значит, номера дублей аудита: 0, 2, 5, 9, 14, 20, 27, 35, как и предполагалось.
- Легаси-цикл `tools/gen_probe/genrefeatures.cpp` не сбрасывает таблицу попыток между дублями. Исправление вне этого PR.

## Тайминг: DEFERRED

- `timing:"grid"` в v1. Формула `processSequencerEvents` (`src/dsp/miniacid_engine.cpp:3108-3169`) найдена, но сверка смещений с движком на воспроизведении требует отдельного host-харнесса. Он не входит в этот PR.

## Какой G эмулируется (чтение кода, origin/main)

**Измеряется MATERIAL G (phrase page); GENRE G — отдельный путь, см. PR-B2.** Каждая строка несёт `"g_path":"material"`.

- **GENRE G** (`src/ui/pages/genre_page.cpp:421-436` → `applyCurrent(true)` `:347`, `:388-400` → `regenerateWithQuantizedCommit`, `src/generation/migration/quantized_generation_commit_impl.h:664`). Пишет **один такт в текущий рабочий слот** (`impl.h:641`, `:651`, `:817`): перезаписывает, Song не растёт. Ключ попыток — физический адрес слота (`patternAddressFor`, `impl.h:293-296`), не 0xFFFF.
- **MATERIAL G** (`src/ui/pages/phrase_page.cpp:1215-1216` → `generatePhraseToSong` `:1480` → `GeneratedPhraseSong::generate`). Дописывает **4-тактную фразу в Song** в строку `songLength()` (`phrase_page.cpp:493-494`, `resolvedAppendRow`). Ключ попыток: `(mode, recipe, P, 0xFFFF)` (`generated_phrase_song.h:1013-1031`). Это путь эмулятора.

## Почему Song останавливается на 16 тактах

Это **ёмкость слотов**, не лимит Song (`Song::kMaxPositions = 128`, `scenes.h:170`).
- Фраза занимает `bars` подряд **свободных** слотов страницы (16 на страницу): `findSafeContiguousEmptySlots` (`src/dsp/phrase_generator.h:192-210`).
- Слот свободен, если он пуст, не резидентен для материального голоса и не ссылается ни одна строка Song (`localSlotIsSafeForPhrase`, `phrase_generator.h:173-188`; дескриптор `material_slot_access.h:41`).
- Длина 8 → 12 → 16 при двух дублях: 8 слотов занимает исходная сцена (по измерению), 2 × 4 такта заполняют страницу. Третье нажатие: `NoContiguousPatternSlots` → «NO ROOM» (`phrase_page.cpp:1434`).
- MAKE ROOM (`R`) освобождает только такты **не в Song** (`src/ui/docs/keys.md:526`, `phrase_page.cpp:1139-1146`). Такты из Song он не трогает.

**Вывод для устройства:** на MATERIAL G третий дубль подряд в одной Song не запишется, пока пользователь не уберёт такты из Song. Значит, «8 подряд без Undo» на устройстве без очистки невозможно.

## Метод дублей: undo ≡ clear (P3, k = 0..7, четыре BASE)

- `--sequence clear`: одно reset, 8 нажатий G подряд, после каждого нажатия очищаются строки Song, на которые ссылается такт, его pattern-слоты и дескрипторы материала (`clearTakeRows`, `genrefeatures.cpp`). Undo не используется, таблица попыток не сбрасывается.
- Результат: `events` всех 32 тактов совпадают с `undo` у House, Techno, UKG и Acid (32/32 на каждый рецепт). Тест `tools/gen_probe/test_takes_jsonl.sh`, PASS 5 (P3, k=0..7). PASS 4 — sequential против undo только для k=0,1.
- Решение: **undo остаётся по умолчанию** (совпадает с результатом clear и даёт все 8 дублей). Очистка Song на устройстве — действие пользователя, не автоматическое.

## Утечка состояния между жанрами (House k=0, P3)

- Свежий процесс против процесса, где перед House прошли Techno → UKG → Acid (`--warmup`): события совпадают 4/4 тактов.
- Прогрев всеми режимами 0–8 перед House (порядок легаси): события тоже совпадают.
- **Вывод:** утечки между жанрами в этих путях нет. Дубли не нужно дополнять состоянием; их снимают из свежего процесса.

## Умолчание P-уровня

- Эмиттер по умолчанию использует **P3** (`kDefaultGenerationLevel`, `src/state/generation_request_state.h:19-21`). Раньше здесь стоял P2 и не совпадал с устройством; исправлено.
- Легаси-`genrefeatures` работает на P3 по умолчанию: его дубль 0 совпадает с эмиттерным P3 k=0 по бочке, малому и хэтам (бар 1).

## Легаси-счётчик аудита (подтверждено)

- Легаси-дубль 1 совпадает с эмиттерным P3 **k=2** (ordinal 2), а не k=1. Измерено через `next_ordinal`: House, Acid, Techno, UKG → измеряемые ordinal **0, 2, 5, 9, 14, 20, 27, 35**.
- Причина: `resetGenerationAttemptState()` в легаси-цикле не вызывается (`genrefeatures.cpp`).
- Не исправлено в этом PR.

## GENRE G (`--g-path genre`, PR-B2)

- Путь: `genre_page.cpp:421-436` → `applyCurrent(true)` (`:347`) → `regenerateWithQuantizedCommit` (`quantized_generation_commit_impl.h:664`). Один такт в текущие рабочие слоты (`:641,651,817`), строки Song не добавляются.
- Ключ попыток: `(mode, recipe, P, patternAddressFor(captureTarget))` (`:236-247`, `:259-268`, `:293-296`).
- Умолчание `--g-address` — адрес, который даёт `captureTarget` на свежей загрузке: **0** (измерено, `#GENRE ... device_address=0`). Другой адрес отвергается.
- Один дубль GENRE — **один такт** (`bar_idx` 0). Строки несут `"g_path":"genre"`.
- Проверено: детерминизм (PASS 6), `next_ordinal = k+1` (PASS 7), GENRE undo-style = sequential (PASS 8), утечка между жанрами нет (PASS 10).
- **GENRE и MATERIAL совпадают только при k = 0** (PASS 9: позиции и ноты баса). Для k ≥ 1 различаются позиции барабанов; ноты баса совпадают не во всех дублях. Это разные пути генерации, одинаковость не ожидалась.
- **GENRE для Techno и UKG меняет высоты баса, но не ритм:** `gplab diversity` даёт distinct/8 = 1/8 на всех ролях. Позиции шагов одинаковы во всех дублях (вариативность — в нотах). Метрика измеряет позиции, это ожидаемо.
