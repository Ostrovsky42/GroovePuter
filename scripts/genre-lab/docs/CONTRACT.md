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
