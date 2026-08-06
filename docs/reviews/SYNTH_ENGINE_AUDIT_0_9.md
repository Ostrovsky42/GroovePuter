# GroovePuter 0.9 — аудит синтезаторных движков

**Ветка аудита:** `audit/synth-engines-pre-0.9`  
**База:** `dev`  
**Статус:** ревью исходного кода завершено; измерения звука и аппаратная приёмка ещё не выполнены  
**Целевая платформа:** M5Stack Cardputer ADV, mono, 22 050 Hz, block 512 samples

## 1. Назначение документа

Документ фиксирует фактическое состояние всех синтезаторных движков GroovePuter перед релизом 0.9:

- как формируется звук;
- какие параметры доступны пользователю;
- что параметр реально меняет в DSP;
- как обрабатываются velocity, accent, slide и release;
- что сохраняется в Scene и восстанавливается после перезагрузки;
- где возможны неверный строй, щелчки, шипение, почти полная тишина или неработающие органы управления;
- что должен отдельно оценить музыкант-разработчик на слух.

Это **code audit**, а не акустическое заключение. Указанные звуковые риски должны быть подтверждены записанным выходом Cardputer ADV и анализом WAV/FFT перед закрытием релизных блокеров.

## 2. Актуальный список движков

Пользовательский список содержит шесть движков:

1. `TB303`
2. `SID`
3. `AY`
4. `SH101`
5. `SN76489`
6. `WAVEMORPH`

`OPL2/YM3812/FM` оставлен только как legacy-идентификатор и фактически заменяется на `TB303`. Он не является отдельным рабочим движком 0.9.

Основные файлы:

- `src/dsp/swappable_synth_voice.{h,cpp}`
- `src/dsp/mini_tb303.{h,cpp}`
- `src/dsp/sid_synth_voice.{h,cpp}`
- `src/dsp/sid_synth.{h,cpp}`
- `src/dsp/ay_synth_voice.{h,cpp}`
- `src/dsp/sh101_synth_voice.{h,cpp}`
- `src/dsp/sn76489_synth_voice.{h,cpp}`
- `src/dsp/wave_morph_synth_voice.{h,cpp}`
- `src/ui/pages/tb303_params_page.cpp`
- `src/dsp/miniacid_engine.cpp`
- `scenes.{h,cpp}`

## 3. Шкала критичности

- **P0 — release blocker:** неверные ноты, потеря пользовательских параметров, зависшие ноты, почти полная тишина или опасная мутация сохранённого патча.
- **P1 — исправить до 0.9 RC:** слышимый щелчок, сильная разница громкости, misleading control, aliasing/шипение на штатном диапазоне, операция в real-time потоке с риском dropout.
- **P2 — допустимо только при явной документации:** ограничение модели, неоднозначное название, фиксированный внутренний параметр или упрощённая эмуляция.

## 4. Общий аудиотракт

### 4.1. Контракт платформы

Cardputer ADV работает с:

```text
sample rate: 22 050 Hz
block size:  512 samples
Nyquist:     11 025 Hz
```

Каждый синтезатор рендерится даже при остановленном transport, чтобы live keyboard и release-tail продолжали звучать.

### 4.2. Путь сигнала одного синтезатора

```text
engine.process()
  × 0.5
  → per-voice TubeDistortion
  → track volume
  → per-voice TempoDelay
  → sum Synth A + Synth B
  → общий mix с drums/sampler/voice
  × 0.65
  → master LPF
  → master DC blocker
  → soft limiter
  → master volume 0..1.8
  → final soft limiter
  → TPDF dither
  → int16 mono
```

Следствия:

- движки не имеют единого loudness contract до общего множителя `0.5`;
- внутренний gain каждого движка напрямую определяет, будет ли он значительно тише или громче соседей;
- даже при цифровой тишине в финальный сигнал добавляется примерно однобитный TPDF dither; нулевой WAV не ожидается, но шум не должен быть слышим на штатной громкости;
- delay-tail продолжает звучать после mute, потому что в delay отправляется ноль, но его буфер продолжает обрабатываться;
- release-tail продолжает звучать после Stop — это текущая ожидаемая семантика.

### 4.3. Общие release blockers

#### P0-A. Шестой параметр не сохраняется

Scene codec сохраняет только пять generic-значений:

```text
param 0 → cutoff
param 1 → resonance
param 2 → envAmount
param 3 → envDecay
param 4 → oscType (0..100)
```

`param 5` не записывается и не восстанавливается. В трёх движках это `Decay`:

- SH101 — Decay;
- SN76489 — Decay;
- WAVEMORPH — Decay.

После save/reload пользователь может увидеть или услышать возврат Decay к дефолту.

**Необходимое исправление:** versioned full generic state с `engineType + paramCount + normalized params[]`, миграция legacy `SynthParameters`, round-trip тест каждого движка.

#### P0-B. Загрузка Scene перезаписывает ручной TB303 patch

`MiniAcid::applySceneStateFromManager()` сначала восстанавливает сохранённые параметры, затем безусловно вызывает `GenreManager::applyGenreTimbre()`. Для каждого активного TB303 жанровый timbre заново записывает oscillator/cutoff/resonance/env/decay.

Результат: сохранённый ручной TB303 patch может открыться с другим звуком. Остальные движки пропускаются, поэтому persistence ведёт себя по-разному в зависимости от TYPE.

**Необходимое решение:** жанровый timbre применять только при явном Apply/Generate или хранить отдельный флаг владения параметрами. Load должен быть идемпотентным.

#### P0-C. Live NoteOff может не снять зажатую ноту

Live NoteOn ограничивает MIDI note диапазоном C1..B4 и запоминает ограниченное значение. Live NoteOff сравнивает его с исходным неограниченным MIDI note.

Пример:

```text
NoteOn 84 → внутри звучит 71
NoteOff 84 → 84 != 71 → release не вызывается
```

**Необходимое исправление:** одинаковая нормализация note identity в NoteOn и NoteOff либо хранение исходного MIDI note отдельно от частоты DSP.

#### P0-D. DST может сделать голос почти неслышным

Когда FEEL Drive выключен и per-voice DST выключен, `applyTextureFromScene_()` может установить drive процессора в `0.1`. Последующее нажатие `DST` только включает процессор и не восстанавливает рабочее значение drive.

При `mix=1` вход сначала умножается на `0.1`, поэтому вместо перегруза получается приблизительно десятикратное ослабление.

**Необходимое исправление:** единая функция `setVoiceDistortionEnabled()` должна атомарно устанавливать enable, drive и mix. Нужен тест bypass/on loudness.

#### P0-E. AY имеет неверный строй на штатной частоте дискретизации

AY-код вычисляет chip period через `sampleRate / (16 × frequency)`. Sample rate ошибочно используется как master clock PSG.

На Cardputer ADV 22 050 Hz:

```text
запрошено 110 Hz → примерно 106.01 Hz  (-64 cents)
запрошено 220 Hz → примерно 229.69 Hz  (+75 cents)
запрошено 440 Hz → примерно 459.38 Hz  (+75 cents)
запрошено 880 Hz → примерно 689.06 Hz  (-423 cents)
выше ~1378 Hz   → все ноты схлопываются в одну частоту
```

Это прямой release blocker «кривые ноты».

**Необходимое исправление:** использовать отдельный AY/YM chip clock и корректный period, затем переводить chip output в host sample rate; либо отказаться от chip-period квантования и честно назвать движок AY-inspired.

## 5. UI и параметры

Страница `SYNTH A/B PARAMS` имеет две вкладки.

### MAIN

Четыре ручки всегда соответствуют generic параметрам `0..3`. Подписи и значения берутся из текущего движка, поэтому одинаковое физическое место имеет разный смысл.

### MORE

- `TYPE` — выбор движка;
- для TB303: отдельные `OSC` и `FLT`;
- для остальных: generic параметры `4` и `5`, если они существуют;
- `DST` и `DLY` — общие post-engine эффекты для всех движков.

### Найденная несостыковка TB303 normalized API

`MiniAcid::set303ParameterNormalized()` передаёт `TB303ParamId` через generic интерфейс. Generic TB303 реализует только индексы 0..3. Поэтому normalized запись `Oscillator` с ID 4 и `FilterType` с ID 5 не выполняется.

Практический эффект: ручное переключение OSC/FLT через специальный TB303 API работает, но жанровый вызов `set303ParameterNormalized(Oscillator, ...)` выглядит рабочим только в исходнике и не меняет осциллятор.

## 6. TB303

### 6.1. Архитектура звука

- wavetable oscillator;
- selectable oscillator mode;
- optional mode-controlled sub oscillator;
- selectable filter profile поверх Chamberlin/Diode/Ladder core;
- filter envelope;
- optional lo-fi quantization/noise/DC coloration;
- bass boost;
- per-voice distortion и tempo delay после движка.

### 6.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Cutoff | 60..2500 Hz | 800 | базовая частота фильтра |
| Reso | 0..0.85 | 0 | resonance/feedback фильтра |
| Env | 0..2000 Hz | 400 | глубина filter envelope |
| Decay | 20..2200 ms | 420 | спад filter envelope |
| Oscillator | saw/sqr/super/pulse/sub | saw | форма/слой oscillator |
| Filter | lp1/acid/moog/warm/soft/retro/drive | lp1 | core и coloration profile |
| Volume | 0..1 | 0.8 | **не используется в `process()`** |

### 6.3. Articulation

- velocity влияет на amp;
- accent повышает amp и filter envelope;
- slide сохраняет фазу и плавно меняет частоту;
- release закрывает gate, но хвост определяется filter envelope.

### 6.4. Риски

#### P0/P1. Dead Volume

`MainVolume` создаётся как Parameter, но не входит в generic `parameterCount`, не показан в UI и не участвует в выходном gain.

Решение: удалить параметр как ложный контракт либо реально подключить и сохранить.

#### P1. Sub подмешивается дважды

При `subEnabled_` sub добавляется в `oscillatorSample()`, а затем второй раз в `process()`. Один `subPhase_` продвигается в двух местах. Возможны неожиданный уровень баса, изменение строя фазы и щелчок при переключении режима.

#### P1. Phase increment привязан к глобальному `kSampleRate`

Несколько oscillator-функций используют `kSampleRate`, а не фактический `sampleRate` голоса. На текущем Cardputer они совпадают, но любое изменение sample rate или desktop-конфигурации меняет высоту тона.

#### P1. Heap allocation в audio path

Смена filter type вызывает создание нового filter object через `std::make_unique`; проверка выполняется из per-sample filter path. Первый sample после изменения типа может выполнить allocation в real-time потоке и вызвать dropout.

#### P1. Genre oscillator write не работает

Genre timbre пытается записать Oscillator через normalized generic index 4, который TB303 игнорирует.

#### P2. Pulse label/реализация

Комментарий заявляет pulse около 30%, но используется square wavetable. Требуется проверить фактическую duty table и либо исправить DSP, либо название.

### 6.5. Музыкальная проверка

- C1/C2/C3/B4 на всех oscillator modes;
- slide C2→G2→C3 без повторного attack;
- accent на одинаковой velocity;
- sub OFF/ON с анализом RMS и основной частоты;
- каждый filter profile при resonance 0/50/100%;
- переключение filter во время удерживаемой ноты — отсутствие dropout;
- saved patch до/после reboot должен быть идентичен.

## 7. SID

### 7.1. Архитектура звука

Текущая реализация — упрощённый pulse oscillator с one-pole filter. Исходник прямо фиксирует, что это не полноценная SID-модель.

### 7.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Cutoff | 0..12000 Hz | 4000 | cutoff one-pole filter |
| Reso | 0..255 | 0 | меняет alpha, но не создаёт настоящий resonance peak |
| P-Width | 0..4095 | 2048 | pulse duty; DSP зажимает минимум до 64 |
| F-Mode | LP/BP/HP/OFF | LP | упрощённый выбор output |

### 7.3. Articulation

- частота обратно округляется до ближайшего MIDI note;
- accent игнорируется;
- slide игнорируется;
- NoteOn сбрасывает phase в 0;
- NoteOff немедленно выключает voice без release envelope.

### 7.4. Риски

#### P1. Щелчки

Жёсткий phase reset при NoteOn и моментальный zero при NoteOff могут давать discontinuity.

#### P1. Misleading Reso/BP

`Reso` не является резонансом в обычном музыкальном смысле. BP вычисляется как приближённая смесь, а не настоящий band-pass.

#### P1. Разница громкости

SID core дополнительно умножает output на `0.25`, после чего общий mixer ещё на `0.5`. Он может быть заметно тише SH101/WAVEMORPH.

#### P1. Верхняя часть Cutoff недоступна физически

При 22 050 Hz Nyquist равен 11 025 Hz, а slider идёт до 12 000 Hz. DSP зажимает значение, поэтому верхняя часть движения не меняет cutoff.

#### P2. Mode и LoFi no-op

`setMode()` и `setLoFiAmount()` ничего не делают. Это допустимо только при явной документации.

#### P2. Velocity 0 всё равно звучит

Минимальный amp зажат до 0.05.

### 7.5. Решение по позиционированию

До 0.9 выбрать одно:

1. назвать движок `SID-LITE`/`SID PULSE`, честно описать ограничения;
2. либо реализовать envelope, корректные filter modes, accent и slide.

## 8. AY

### 8.1. Архитектура звука

- три square tone channels;
- ratios: root, detuned voice и sub-like voice;
- 17-bit LFSR noise;
- 4-bit amplitude quantization;
- anti-click amplitude slew;
- четыре envelope modes.

### 8.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Noise | 0..1 | 0.10 | одновременно noise mix и noise clock rate |
| Decay | 20..1500 ms | 220 | envelope decay |
| Chorus | 0..1 | 0.20 | detune tone B/C |
| Env | Hold/Decay/Pluck/Gate | Decay | форма envelope |

### 8.3. Articulation

- accent игнорируется;
- slide не выполняет portamento, а только не сбрасывает `ampSlew`;
- velocity влияет на gain;
- release закрывает gate и даёт decay-tail.

### 8.4. Риски

#### P0. Неверный строй

См. общий blocker P0-E.

#### P1. Сильный aliasing

Все tone channels — raw square без PolyBLEP или band-limited tables. На B4 и при chorus возможен отчётливый цифровой свист/шипение.

#### P1. Noise управляет двумя независимыми величинами

Один slider одновременно увеличивает mix и поднимает noise clock. Музыкант не может отдельно выбрать тембр и количество шума.

#### P2. Gate не является sustain

В режиме `Gate` envelope во время удержания медленно затухает примерно с четырёхсекундным коэффициентом. Название может вводить в заблуждение.

## 9. SH101

### 9.1. Архитектура звука

- PolyBLEP saw/pulse;
- selectable Saw/Pulse/Mix;
- sub oscillator;
- noise;
- attack/decay/sustain/release amp envelope;
- filter envelope;
- two-stage resonant low-pass approximation;
- saturation и DC blocker.

### 9.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Wave | Saw/Pulse/Mix | Mix | основная форма |
| Sub | 0..1 | 0.35 | sub mix |
| Noise | 0..1 | 0 | noise mix |
| Cutoff | 80..6500 Hz | 1800 | base filter cutoff |
| Reso | 0..0.92 | 0.30 | filter feedback |
| Decay | 30..2400 ms | 420 | одновременно amp и filter decay |

### 9.3. Articulation

- accent повышает gain и filter envelope;
- slide — реальный legato portamento;
- первый note со slide всё равно запускает envelope;
- release около 70 ms фиксирован;
- sustain фиксирован около 0.62.

### 9.4. Оценка

Это наиболее завершённый subtractive engine текущей группы.

### 9.5. Риски

- **P0 persistence:** параметр 5 Decay теряется после reload;
- **P1:** slide coefficient задан per-sample и меняет фактическое время glide при другом sample rate;
- **P2:** Decay управляет сразу двумя envelopes, что не видно из label;
- **P2:** pulse width, attack, release и filter-env amount фиксированы.

## 10. SN76489

### 10.1. Архитектура звука

- три PolyBLEP square channels;
- частоты квантованы через clock 3 579 545 Hz и 10-bit divider;
- selectable stack ratios;
- white/periodic LFSR noise;
- 4-bit envelope amplitude;
- anti-click slew и DC blocker.

### 10.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Stack | Uni/Oct/Fifth/Chord | Oct | ratios трёх tone channels |
| Tone2 | 0..1 | 0.65 | level channel 2 |
| Tone3 | 0..1 | 0.45 | level channel 3 |
| Noise | 0..1 | 0 | noise mix |
| NMode | W/P × Hi/Mid/Low/T3 | W-Mid | тип и clock noise |
| Decay | 20..2000 ms | 260 | one-shot envelope |

### 10.3. Articulation

- accent повышает gain;
- slide плавно двигает target, hardware-divider retune выполняется раз в 16 samples;
- envelope затухает даже при удерживаемом gate;
- release использует фиксированный быстрый coefficient.

### 10.4. Риски

- **P0 persistence:** параметр 5 Decay теряется после reload;
- **P1:** если квантованная tone frequency превышает безопасный диапазон, phase increment зажимается до 0.49. Верхние ноты могут схлопываться/перестраиваться около Nyquist;
- **P1:** slide time sample-rate dependent;
- **P2:** `Uni` фактически использует небольшой detune 1.003/0.997;
- **P2:** `Oct` означает root + octave down + two octaves down, а не octave up;
- **P2:** one-shot envelope должен быть явно указан в UI/manual.

## 11. WAVEMORPH

### 11.1. Архитектура звука

- восемь таблиц по 128 samples;
- линейная интерполяция внутри таблицы;
- morph между выбранной таблицей и следующей;
- raw square sub oscillator;
- two-stage low-pass с feedback;
- ADSR-like amp behavior с фиксированным attack/sustain/release;
- DC blocker.

### 11.2. Параметры

| Параметр | Диапазон | Default | Реальное действие |
|---|---:|---:|---|
| Wave | Sine/Tri/Saw/Pulse/Organ/Vowel/Metal/Digital | Saw | начальная wavetable |
| Morph | 0..1 | 0.15 | crossfade в следующую таблицу |
| Sub | 0..1 | 0.20 | raw square sub mix |
| Cutoff | 80..7000 Hz | 2400 | static filter cutoff |
| Reso | 0..0.90 | 0.18 | feedback |
| Decay | 30..2400 ms | 520 | amp decay к sustain |

`Digital + Morph` циклически переходит обратно к `Sine`; это должно быть описано пользователю.

### 11.3. Риски

- **P0 persistence:** параметр 5 Decay теряется после reload;
- **P1:** таблицы не имеют mip levels/band-limited variants. Saw/Pulse/Metal/Digital дают aliasing на верхних нотах;
- **P1:** raw square sub тоже не band-limited;
- **P1:** slide time sample-rate dependent;
- **P2:** filter envelope отсутствует; Cutoff статический;
- **P2:** attack, sustain и release не настраиваются.

## 12. Смена движка

`SwappableSynthVoice` выполняет примерно 10 ms equal-power crossfade и повторно запускает текущую ноту на новом движке.

Положительное:

- уменьшает щелчок при смене TYPE;
- сохраняет звучание удерживаемой ноты во время перехода.

Риски:

- параметры между разными движками семантически не преобразуются — новый движок стартует с defaults;
- generic invalid index возвращает param 0 у большинства движков, что может скрыть UI bug;
- OPL2 silently превращается в TB303;
- Atlas hybrid recipes по-прежнему просят `OPL2`, поэтому фактический Synth B становится TB303, а комментарии/ожидания говорят о chord-root FM voice.

## 13. Громкость и headroom

До общего mixer gain движки имеют разные внутренние ceilings:

- SID дополнительно ×0.25;
- AY target amp около ×0.30;
- SN76489 target amp около ×0.48;
- SH101 имеет выходной коэффициент ×1.15;
- WAVEMORPH ×1.20;
- TB303 использует собственный amp/filter/makeup path.

После этого все получают одинаковый ×0.5. Поэтому одинаковая velocity и track volume не гарантируют близкую perceived loudness.

**Цель 0.9:** при neutral patch, A3, velocity 100, FX OFF разница integrated RMS между движками не более ±3 dB; peak не выше -3 dBFS до master limiter.

## 14. Обязательные автоматические тесты

Существующий `tests/test_new_synth_voices.cpp` покрывает только SH101 и SN76489 и проверяет в основном finite output, peak и release-tail. Не покрыты TB303, SID, AY и WAVEMORPH.

Нужно добавить:

1. **Pitch test** для каждого движка:
   - A2/A3/A4;
   - оценка fundamental frequency;
   - допуск ±5 cents для tonal modes;
   - отдельный ожидаемый hardware-divider допуск для PSG.
2. **Silence/release test:**
   - FX OFF;
   - после release RMS ниже -70 dBFS за установленное время;
   - отсутствие NaN/Inf.
3. **Click test:**
   - максимальный sample delta на NoteOn/NoteOff;
   - отдельный threshold для chip engines.
4. **Parameter activity test:**
   - изменение каждого slider должно статистически менять output;
   - option endpoints должны давать различимые состояния;
   - TB303 Volume либо участвует, либо удалён.
5. **Persistence matrix:**
   - уникальные значения всех параметров каждого движка;
   - dump/load;
   - engine + param count + все normalized values идентичны.
6. **Scene-load idempotence:**
   - load → dump → load не меняет TB303 patch;
   - жанр не перезаписывает ручные параметры без явной команды.
7. **Live note identity:**
   - NoteOn/Off ниже C1, внутри диапазона и выше B4;
   - после NoteOff active voice обязан завершиться.
8. **Distortion bypass/on:**
   - DST OFF ≈ unity;
   - DST ON не должен давать падение RMS более 3 dB при neutral drive;
   - finite output на drive max.
9. **Engine switch:**
   - no allocation failure;
   - bounded sample discontinuity;
   - корректный engine name после legacy OPL2 fallback.
10. **Aliasing metric:**
   - верхняя штатная нота B4;
   - энергия вне ожидаемых harmonic bins;
   - отдельные baseline для chip и wavetable engines.

## 15. Аппаратный протокол для музыканта

### 15.1. Подготовка

- Cardputer ADV;
- наушники и линейная запись выхода;
- FX: DST OFF, DLY OFF, Tape OFF, LoFi OFF;
- master volume фиксирован;
- track volume Synth A/B = 100%;
- один и тот же mono MIDI source;
- тестировать Synth A и Synth B отдельно.

### 15.2. Набор нот

Для каждого движка записать:

```text
C1  32.70 Hz
A1  55.00 Hz
A2 110.00 Hz
A3 220.00 Hz
A4 440.00 Hz
B4 493.88 Hz
```

Каждая нота:

- velocity 64 и 100;
- 2 seconds hold;
- 2 seconds release/silence;
- отдельно accent и slide.

### 15.3. Что оценивать

По шкале 0..5:

- точность высоты;
- стабильность высоты;
- щелчок NoteOn;
- щелчок NoteOff;
- шум/свист на sustain;
- aliasing на A4/B4;
- музыкальность low register;
- response каждого параметра;
- равномерность slider;
- громкость относительно остальных движков;
- пригодность для bass/lead/arp/chord-like роли.

### 15.4. Проверка каждого slider

Для каждого параметра записать три состояния:

```text
MIN → CENTER → MAX
```

Критерии:

- изменение слышно;
- направление изменения соответствует label;
- нет участка, где 20% и более хода ничего не меняет;
- нет внезапной тишины, runaway feedback или постоянного DC;
- option labels соответствуют слышимому результату.

### 15.5. Save/reload

Для каждого движка:

1. установить легко узнаваемые значения всех параметров;
2. сохранить проект;
3. перезагрузить устройство;
4. открыть тот же проект;
5. записать тот же A3;
6. сравнить UI values и WAV до/после.

Критерий: значения идентичны, а WAV не имеет неожиданного изменения тембра/decay.

## 16. Рекомендуемый порядок исправлений

### PR 1 — Pitch and note lifecycle

- исправить AY clock/period;
- унифицировать live NoteOn/Off identity;
- добавить fundamental-frequency tests;
- добавить stuck-note tests.

### PR 2 — Versioned synth persistence

- сохранять все generic params;
- мигрировать legacy TB303 fields;
- прекратить genre overwrite при load;
- round-trip matrix всех шести движков.

### PR 3 — TB303 correctness

- убрать двойной sub;
- исправить normalized extended params;
- решить dead Volume;
- убрать allocation из process path;
- сделать phase increment зависимым от instance sample rate.

### PR 4 — FX silence and gain staging

- атомарный DST enable/drive/mix;
- loudness normalization по движкам;
- проверить constrained delay timing;
- mixer peak/RMS tests.

### PR 5 — SID articulation and truthful UI

- attack/release smoothing;
- определить судьбу accent/slide;
- переименовать misleading Reso/BP либо реализовать корректнее;
- решить название `SID` против `SID-LITE`.

### PR 6 — Aliasing pass

- WAVEMORPH band-limited/mip strategy либо ограничение harmonic content;
- AY anti-alias strategy;
- SN upper-note policy;
- аппаратные FFT baselines.

## 17. Release gate GroovePuter 0.9

0.9 не должен называться готовым, пока не выполнено всё ниже:

- [ ] AY проходит pitch test;
- [ ] ни один live NoteOff не оставляет зависшую ноту;
- [ ] все параметры каждого движка проходят save/reload;
- [ ] TB303 patch не меняется сам при загрузке проекта;
- [ ] DST не вызывает почти полную тишину;
- [ ] ни один видимый slider не является dead control;
- [ ] engine loudness нормализован или явно компенсирован;
- [ ] SID NoteOn/Off не даёт неприемлемых щелчков;
- [ ] WAVEMORPH/AY/SN upper register принят музыкантом;
- [ ] Cardputer ADV build и fixed-DRAM gate зелёные;
- [ ] полный hardware listening matrix приложен к release notes.

## 18. Вопросы музыканту-разработчику

1. SID должен быть эмуляцией или характерным «SID-inspired» voice?
2. Нужны ли SID accent и slide, или UI должен показывать их как unavailable?
3. AY должен соблюдать реальный chip clock/period или быть более ровно темперированным AY-inspired синтом?
4. Для SN76489 желательна hardware-accurate квантованная высота или musical pitch с chip coloration?
5. WAVEMORPH Digital→Sine wrap при Morph является желательным циклом?
6. Должен ли `Decay` SH101 одновременно управлять amp и filter envelope?
7. Нужны ли общие target loudness и headroom одинаковые для всех движков?
8. Должен ли per-voice mute сохранять delay-tail или давать немедленную тишину?
9. Допустим ли audible one-LSB dither на встроенном усилителе?
10. Какие aliasing/lo-fi артефакты считаются характером, а какие дефектом релиза?

## 19. Краткий итог

Сильнейшая текущая база — SH101; SN76489 также структурно завершён, но требует проверки верхнего диапазона и persistence. WAVEMORPH музыкально перспективен, но нуждается в anti-aliasing и сохранении шестого параметра. TB303 функционально богат, однако имеет несколько несогласованных API и lifecycle-проблем. SID пока является прототипом с misleading labels и жёсткими границами нот. AY в текущем виде не проходит базовый критерий точности нот.

До исправления P0 пунктов документ следует считать **release-blocking audit**, а не описанием готовых инструментов.