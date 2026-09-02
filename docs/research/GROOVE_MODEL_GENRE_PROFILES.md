# GroovePuter: музыковедческие профили четырёхосевой модели

Этот файл является **Частью 3** исследования. Выбор жанров, сравнительная сетка, тест инварианта, рекомендации по гранулярности и реестр источников находятся в [`GROOVE_MODEL_AXIS_ANALYSIS.md`](./GROOVE_MODEL_AXIS_ANALYSIS.md).

## Правила чтения

- Шаги маски идут слева направо как `1 e & a 2 e & a 3 e & a 4 e & a`.
- `X` — required, `o` — preferred, `.` — neutral, `#` — forbidden.
- Маска задаёт **коридор допустимого**, а не готовый паттерн.
- Все микросмещения указаны в тиках при `96 PPQN`: четверть = 96 тиков, шестнадцатая = 24 тика; `±1` означает `±1/96` четверти.
- Swing указан как отношение: `50%` — straight, `66.7%` — триольная граница.
- Внутри одного утверждения используется ровно одна категория провенанса.
- `C1` — внутренний Atlas v2.6 runtime slice: 6 recipes, 18 pattern slots, 535 runtime events. Он служит проверочным якорем, а не статистически репрезентативной выборкой жанра.

---

## 1. Detroit Techno

### A. GENRE

- BPM-коридор: `118–138`, типичная зона `126–132`. `[РЕДАКТОРСКОЕ]` Основание: устойчивый клубный quarter-note pulse, допускающий и более медленные ранние, и более быстрые функциональные формы Detroit techno.
- Сетка: `16 шагов/такт`, прямая; `32 шага/такт` допустимы только для вторичных перкуссионных деталей. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...X...X...X...  [РЕДАКТОРСКОЕ]
snare/clap ....o.......o...  [РЕДАКТОРСКОЕ]
closed hat ..o...o...o...o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       .o...o...o...o..  [РЕДАКТОРСКОЕ]
bass       X..o....X..o....  [РЕДАКТОРСКОЕ]
lead       ....o...o.......  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `4–6`; snare/clap `0–2`; closed hat `4–12`; open hat `0–2`; perc `1–6`; bass `2–7`; lead `0–4`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Dorian, Aeolian, minor pentatonic; chromatic passing tones допустимы как редкие связки, но не как основной словарь. `[РЕДАКТОРСКОЕ]`
- Связь bass/kick не хранится в GENRE по контракту; разрешённый режим задаётся в GENERATION ниже.

### B. FEEL

- Тип: straight или lightly swung; swing `50–56%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick и основные quarter anchors `0`; snare/clap `0…+1`; hats `0…+1`; perc `-1…+1`; bass `-1…0` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: kick steps `1,5,9,13`; при наличии backbeat — snare/clap steps `5,13`. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `0…±1` тик; velocity `±3…8`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Repeat → Variation → Build → Fill → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: в 5-м меняется вторичная перкуссия или optional bass onset; в 6-м повышается hat/perc activity; в 7-м допустим END_FILL; 8-й возвращает anchor hierarchy. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 3/5/6; `REMOVE_LOW_PRIORITY` в 4/8; `SHIFT_OPTIONAL_EVENT` в 3/5; `KICK_DISPLACEMENT` только в 5/7 и не для всех quarter anchors; `OPEN_HAT_SWAP` в 3/6; `END_FILL` в 7; `ACCENT_CHANGE` в 3/5/7. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `MIXED`, с преобладанием `FOLLOW` на section anchors и `ANSWER` между ними. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance по тактам 1–8: `0 / 0–4 / 4–10 / 0–6 / 8–16 / 6–14 / 12–24 / 10–20%` изменённых шагов относительно первого такта. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 35, age 20, space 35, width 45, instability 25, aggression 65, darkness 55`. `[РЕДАКТОРСКОЕ]`
- Передаёт сухую, функциональную и футуристичную поверхность без обязательной жёсткости.

---

## 2. Dub Techno

### A. GENRE

- Atlas-якорь: recipes Deep Chord и Minimal Space имеют `120/116 BPM` и `54/51% swing`. `[КОРПУС C1]`
- Предлагаемый BPM-коридор: `110–128`, типичная зона `116–124`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, straight/light swing; высокая доля нейтральных и пустых позиций. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`, причём 8 — текущий системный максимум. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...X...X...X...  [РЕДАКТОРСКОЕ]
snare/clap ....o.......o...  [РЕДАКТОРСКОЕ]
closed hat ..o.......o...o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       ...o.......o....  [РЕДАКТОРСКОЕ]
bass       X.......o.......  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `4–5`; snare/clap `0–2`; closed hat `2–8`; open hat `0–2`; perc `0–4`; bass `1–4`; lead/chord `0–3`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Dorian, Aeolian, suspended/modal one-chord fields; chromatic passing tones редки. `[РЕДАКТОРСКОЕ]`
- Связь bass/kick определяется только в GENERATION.

### B. FEEL

- Тип: straight или shallow swing; swing `50–56%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick anchors `0`; hats `0…+1`; perc `-1…+1`; bass `0…+1`; chord/lead `+1…+2` тика для ощущения пространства после pulse. `[РЕДАКТОРСКОЕ]`
- Protected anchors: kick `1,5,9,13`; первый chord/bass event секции; остальные stabs movable. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика только для secondary events; velocity `±6…12`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Breakdown → Variation → Repeat → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й меняет один stab или perc pickup; 6-й удерживает новую пустоту; 7-й возвращает часть hats/perc; 8-й восстанавливает исходный downbeat и сокращает optional events. `[РЕДАКТОРСКОЕ]`
- Операторы: `REMOVE_LOW_PRIORITY` в 4/6/8; `SHIFT_OPTIONAL_EVENT` в 3/5; `ADD_GHOST` только для тихой perc в 3/7; `OPEN_HAT_SWAP` в 5/7; `ACCENT_CHANGE` в 3/5; `END_FILL` очень редко в 7; `KICK_DISPLACEMENT` запрещён для required quarter anchors. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `AVOID` или `MIXED`; bass чаще занимает пространство после kick, но section root может совпасть с step 1. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–3 / 2–7 / 8–18 / 4–10 / 0–5 / 5–12 / 8–16%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 28, age 45, space 110, width 85, instability 55, aggression 35, darkness 75`. `[РЕДАКТОРСКОЕ]`
- Передаёт глубину, эрозию повторов и ощущение, что пространство является активной частью исполнения.

---

## 3. Industrial Techno

### A. GENRE

- BPM-коридор: `125–150`, типичная зона `135–145`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, преимущественно straight; `32 шага` допустимы для ratchet-like secondary percussion, но базовые anchors остаются 16-step. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `2`, `4` или `8` тактов; характерная — `4`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...X...X...X...  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.o.o.o.o.o.o.o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       .o.o.o.o.o.o.o.o  [РЕДАКТОРСКОЕ]
bass       X...o...X...o...  [РЕДАКТОРСКОЕ]
lead       ..o...o...o...o.  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `4–8`; snare/clap `2–4`; closed hat `8–16`; open hat `0–4`; perc `4–12`; bass `4–10`; lead `2–8`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Aeolian, Phrygian, chromatic cells, tritone-bearing cells; major pentatonic excluded from default vocabulary. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: straight; swing `50–53%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick/snare `0`; hats `-1…0`; perc `-1…+1`; bass `-1…0` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: kick `1,5,9,13`; snare/clap `5,13`; первый удар каждого build segment. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `0…±1` тик; velocity `±3…8`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 4 тактов: `Statement → Repeat → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 2/3; `REMOVE_LOW_PRIORITY` в 4; `SHIFT_OPTIONAL_EVENT` в 2/3; `KICK_DISPLACEMENT` только для дополнительных kick, не required anchors; `OPEN_HAT_SWAP` в 3; `END_FILL` в 3; `ACCENT_CHANGE` в 2/3. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `FOLLOW` или `MIXED`; repeated bass attacks поддерживают механический pulse. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 2–8 / 10–22 / 12–26%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 85, age 30, space 55, width 70, instability 50, aggression 115, darkness 90`. `[РЕДАКТОРСКОЕ]`
- Передаёт абразивность и давление, но ритмический каркас остаётся отдельным от этой поверхности.

---

## 4. Chicago House

### A. GENRE

- Публикационный диапазон house: `120–135 BPM`. `[ИСТОЧНИК S2]`
- Предлагаемый коридор Chicago House: `118–130`, типичная зона `122–126`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, four-on-the-floor. `[ИСТОЧНИК S2]`
- Допустимые длины: `2`, `4` или `8` тактов; характерная — `4`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...X...X...X...  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat ..o...o...o...o.  [РЕДАКТОРСКОЕ]
open hat   ..o...o...o...o.  [РЕДАКТОРСКОЕ]
perc       ...o...o...o...o  [РЕДАКТОРСКОЕ]
bass       X..o....X..o....  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `4–6`; snare/clap `2–4`; closed hat `4–12`; open hat `2–4`; perc `2–8`; bass `3–8`; lead `0–4`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: minor pentatonic, Dorian, Mixolydian, diatonic seventh-chord tones. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: straight/light swing; swing `50–58%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick `0`; clap `0…+1`; hats `0…+1`; perc `-1…+1`; bass `-1…0` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: kick `1,5,9,13`; clap `5,13`. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `0…±1` тик; velocity `±4…10`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 4 тактов: `Statement → Repeat → MicroVariation → Fill`. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 3; `REMOVE_LOW_PRIORITY` в 2/4; `SHIFT_OPTIONAL_EVENT` в 3; `KICK_DISPLACEMENT` только для extra kick в 3/4; `OPEN_HAT_SWAP` в 3; `END_FILL` в 4; `ACCENT_CHANGE` в 3/4. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `ANSWER` или `MIXED`; quarter-note kick остаётся независимым anchor, bass заполняет промежутки. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–4 / 4–12 / 10–22%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 40, age 35, space 45, width 55, instability 30, aggression 55, darkness 45`. `[РЕДАКТОРСКОЕ]`
- Передаёт прямой клубный drive и умеренную историческую шероховатость без привязки к конкретной машине.

---

## 5. UK Garage / 2-Step

### A. GENRE

- Atlas-якорь: Classic 2-Step и Dark Skippy имеют `134/136 BPM` и `66/68% swing`. `[КОРПУС C1]`
- Предлагаемый BPM-коридор: `130–138`, типичная зона `132–136`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`; `32 шага` допустимы для ghost percussion. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X..o....o..X....  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat ..o.o.o...o.o.o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       .o...o.o.o...o.o  [РЕДАКТОРСКОЕ]
bass       X.o...o.X.o...o.  [РЕДАКТОРСКОЕ]
lead       ...o...o...o...o  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `2–5`; snare/clap `2–4`; closed hat `6–14`; open hat `0–3`; perc `3–10`; bass `3–9`; lead `0–5`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Aeolian, Dorian, minor pentatonic; diatonic seventh-chord tones допустимы для chord/lead roles. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: swung/broken; swing `58–68%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: required snare `0`; kick pickups `-1…+1`; offbeat hats `+1…+2`; perc `-1…+2`; bass `-1…+1` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: snare/clap `5,13`; первый kick section anchor; остальные kick events movable. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика; velocity `±6…14`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → MicroVariation → Repeat → Variation → Breakdown → Variation → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й убирает часть hats/kick pickups; 6-й меняет bass answer и perc syncopation; 7-й возвращает плотность и open hat; 8-й содержит ending fill и возвращает required snare hierarchy. `[РЕДАКТОРСКОЕ]`
- Операторы: все семь разрешены; `KICK_DISPLACEMENT` особенно уместен в 3/4/6/7, `OPEN_HAT_SWAP` в 3/6/7, `END_FILL` в 8, `REMOVE_LOW_PRIORITY` в 5. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `ANSWER` или `AVOID`; совпадение разрешено только на section anchors. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 4–10 / 0–6 / 8–18 / 14–30 / 10–22 / 12–26 / 14–28%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 45, age 25, space 60, width 75, instability 45, aggression 70, darkness 60`. `[РЕДАКТОРСКОЕ]`
- Передаёт упругий, широкий и немного шероховатый club sound, не определяя сам 2-step рисунок.

---

## 6. Boom Bap

### A. GENRE

- BPM-коридор: `78–100`, типичная зона `86–94`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`; базовый backbeat остаётся читаемым даже при off-grid FEEL. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `2`, `4` или `8` тактов; характерная — `4`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X..o....o.o.....  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.o.o.o.o.o.o.o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       ...o.......o....  [РЕДАКТОРСКОЕ]
bass       X...o...o...o...  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `2–5`; snare/clap `2–4`; closed hat `6–12`; open hat `0–2`; perc `0–4`; bass `2–6`; lead `0–4`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: minor pentatonic, blues vocabulary, Dorian, Mixolydian; chromatic sample tones допустимы как контекст, но не как генеративная цель. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: swung или broken; swing `54–64%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick `-1…0`; snare `+1…+2`; hats `0…+1`; bass `-1…0` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: snare `5,13`; primary kick step `1`; secondary kicks movable. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика; velocity `±8…18`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 4 тактов: `Statement → Repeat → MicroVariation → Fill`. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 3/4; `REMOVE_LOW_PRIORITY` в 2/4; `SHIFT_OPTIONAL_EVENT` в 3; `KICK_DISPLACEMENT` в 3/4; `OPEN_HAT_SWAP` редко в 3; `END_FILL` в 4; `ACCENT_CHANGE` в 2/3/4. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `FOLLOW` или `MIXED`; primary bass attacks часто поддерживают kick, secondary bass notes отвечают snare gaps. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–4 / 3–10 / 10–22%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 60, age 70, space 35, width 40, instability 55, aggression 50, darkness 70`. `[РЕДАКТОРСКОЕ]`
- Передаёт sample-derived weight и сухую midrange-плотность, не подменяя backbeat и off-grid FEEL.

---

## 7. Trip-Hop

### A. GENRE

- Публикационный ориентир: slow beats, обычно не выше `90 BPM`. `[ИСТОЧНИК S7]`
- Предлагаемый BPM-коридор: `68–96`, типичная зона `76–88`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, broken/backbeat; `32 шага` допустимы для тихих ghost events. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...o...o..o....  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.o...o.o.o...o.  [РЕДАКТОРСКОЕ]
open hat   ......o.........  [РЕДАКТОРСКОЕ]
perc       ...o...o...o....  [РЕДАКТОРСКОЕ]
bass       X.....o.X.....o.  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `2–5`; snare/clap `2–4`; closed hat `2–8`; open hat `0–2`; perc `1–6`; bass `2–6`; lead `0–4`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Aeolian, Dorian, Phrygian, minor pentatonic; harmonic-minor fragments допустимы для tension. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: laid-back broken; swing `54–66%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick `0…+1`; snare `+1…+2`; hats `-1…+1`; perc `-1…+2`; bass `0…+1` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: snare `5,13`; первый kick/bass event секции; все ghost events movable. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика; velocity `±8…18`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Breakdown → Variation → Repeat → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й меняет kick/bass dialogue; 6-й сохраняет новую пустоту; 7-й вводит тихие ghosts или hat layer; 8-й возвращает backbeat и удаляет low-priority decoration. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 3/5/7; `REMOVE_LOW_PRIORITY` в 4/6/8; `SHIFT_OPTIONAL_EVENT` в 3/5; `KICK_DISPLACEMENT` в 5/7; `OPEN_HAT_SWAP` редко в 7; `END_FILL` сдержанно в 8; `ACCENT_CHANGE` в 3/5/7. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `MIXED`, с большим числом `ANSWER`-событий, чем в boom bap. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–3 / 3–9 / 10–20 / 8–18 / 0–6 / 6–14 / 10–20%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 75, age 80, space 85, width 70, instability 65, aggression 55, darkness 100`. `[РЕДАКТОРСКОЕ]`
- Передаёт кинематографическую тяжесть и тёмную sample-based поверхность, не определяя slow broken rhythm.

---

## 8. Lo-Fi Hip-Hop

### A. GENRE

- BPM-коридор: `60–90`, типичная зона `70–82`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, backbeat; off-grid характер находится в FEEL, а не в GENRE. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X..o....o...o...  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.o.o.o.o.o.o.o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       ...o.......o...o  [РЕДАКТОРСКОЕ]
bass       X...o...o...o...  [РЕДАКТОРСКОЕ]
lead       ....o...o.......  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `2–5`; snare/clap `2–4`; closed hat `4–12`; open hat `0–2`; perc `0–5`; bass `2–6`; lead `0–5`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: major/minor pentatonic, Dorian, Mixolydian, diatonic seventh-chord tones. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: swung/laid-back; swing `54–67%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick `-1…+1`; snare `+1…+2`; hats `-1…+2`; perc `-1…+2`; bass `0…+1` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: backbeat steps `5,13` защищены от удаления, но их timing может быть поздним; первый kick step защищён. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика; velocity `±10…20`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Repeat → Variation → Breakdown → Repeat → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й меняет один kick/bass onset; 6-й убирает hats или lead event; 7-й возвращает исходный loop с новым ghost; 8-й разрешает фразу без обязательного большого fill. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST` в 3/5/7; `REMOVE_LOW_PRIORITY` в 4/6/8; `SHIFT_OPTIONAL_EVENT` в 3/5; `KICK_DISPLACEMENT` в 5; `OPEN_HAT_SWAP` редко; `END_FILL` очень сдержанно в 8; `ACCENT_CHANGE` в 3/5/7. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `FOLLOW` или `MIXED`. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–3 / 2–8 / 0–4 / 4–10 / 8–18 / 0–5 / 5–12%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 85, age 110, space 55, width 60, instability 90, aggression 35, darkness 95`. `[РЕДАКТОРСКОЕ]`
- Передаёт намеренно состаренную и нестабильную поверхность; именно эта часть чаще всего отличает lo-fi от соседнего slow boom bap.

---

## 9. Jungle

### A. GENRE

- Исследовательское описание: fast-paced electronic dance music с resequenced breakbeats и классическими breakbeat models. `[ИСТОЧНИК S5]`
- Предлагаемый BPM-коридор: `155–175`, типичная зона `160–170`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт` для corridor masks; `32 шага` предпочтительны для secondary break slices. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X..o..o...o.....  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.ooooooooooooo.  [РЕДАКТОРСКОЕ]
open hat   ..o...o...o...o.  [РЕДАКТОРСКОЕ]
perc       .o.o.o.o.o.o.o.o  [РЕДАКТОРСКОЕ]
bass       X.......o..o....  [РЕДАКТОРСКОЕ]
lead       ....o...o...o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `3–8`; snare `2–6`; closed hat `8–16`; open hat `2–6`; perc/break ghosts `6–16`; bass `1–5`; lead `0–5`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Aeolian, Dorian, minor pentatonic; bass может удерживать one-note/root vocabulary в пределах section. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: broken; swing `50–58%`, причём ощущение движения создаётся прежде всего перестановкой событий, а не большим swing ratio. `[РЕДАКТОРСКОЕ]`
- Push/pull: main kick/snare anchors `0`; break ghosts `-1…+1`; hats `-1…+1`; bass `0…+1` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: primary snare `5,13`; первый kick section anchor; остальные break events movable. `[РЕДАКТОРСКОЕ]`
- Humanize: anchors `0…±1` тик, secondary slices `±1`; velocity `±8…16`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → MicroVariation → Variation → Fill → Breakdown → Variation → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й сокращает break до skeleton; 6-й вводит новый kick/snare ordering; 7-й наращивает ghosts/open hats; 8-й делает ending fill и возвращает исходный downbeat. `[РЕДАКТОРСКОЕ]`
- Операторы: все семь; особенно `SHIFT_OPTIONAL_EVENT`, `KICK_DISPLACEMENT`, `ADD_GHOST`, `END_FILL`; required snare anchors не сдвигаются. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `AVOID` или `MIXED`; длинный bass event не должен механически дублировать каждый kick slice. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 6–14 / 12–26 / 18–36 / 24–45 / 14–30 / 20–40 / 24–45%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 55, age 30, space 65, width 90, instability 50, aggression 100, darkness 70`. `[РЕДАКТОРСКОЕ]`
- Передаёт яркость и давление breakbeat layers, не заменяя высокую структурную вариативность.

---

## 10. Acid House

### A. GENRE

- Atlas-якорь: Chicago Jack и Rolling Acid имеют `124/128 BPM` и `52/54% swing`. `[КОРПУС C1]`
- Предлагаемый BPM-коридор: `118–138`, типичная зона `124–130`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, four-on-the-floor с плотной syncopated bass line. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `2`, `4` или `8` тактов; характерная — `8`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X...X...X...X...  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat ..o...o...o...o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       ...o...o...o...o  [РЕДАКТОРСКОЕ]
bass       Xoo.o.ooXoo.o.oo  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `4–6`; snare/clap `2–4`; closed hat `4–12`; open hat `1–4`; perc `1–6`; bass `8–16`; lead `0–3`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: Aeolian, Phrygian, minor pentatonic; chromatic passing tones и repeated-note cells допустимы. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: straight/light swing; swing `50–58%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick/clap `0`; hats `0…+1`; bass optional events `-1…+1`, bass anchors `0` тиков. `[РЕДАКТОРСКОЕ]`
- Protected anchors: kick `1,5,9,13`; clap `5,13`; первый bass root event секции. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `0…±1` тик; velocity `±4…10`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Variation → Breakdown → Variation → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й редуцирует bass phrase; 6-й меняет optional onset и accent pattern; 7-й восстанавливает плотность и ending tension; 8-й возвращает root/anchor hierarchy. `[РЕДАКТОРСКОЕ]`
- Операторы: `ADD_GHOST`, `REMOVE_LOW_PRIORITY`, `SHIFT_OPTIONAL_EVENT`, `OPEN_HAT_SWAP`, `END_FILL`, `ACCENT_CHANGE`; `KICK_DISPLACEMENT` только для extra kick и редко. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `MIXED`; root/section anchors могут `FOLLOW`, остальные bass events чаще `ANSWER`. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–4 / 4–10 / 8–18 / 16–30 / 10–22 / 14–28 / 12–24%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 60, age 35, space 45, width 70, instability 55, aggression 95, darkness 60`. `[РЕДАКТОРСКОЕ]`
- Передаёт кислотную интенсивность как поверхность; конкретные cutoff/resonance/decay параметры намеренно не входят в модель.

---

## 11. Electro / 808-Centric

### A. GENRE

- Исторический якорь: TR-808 стала tonal/rhythmic anchor для hip-hop и techno movements и центральным инструментом electro vocabulary. `[ИСТОЧНИК S4]`
- BPM-коридор: `108–132`, типичная зона `116–126`. `[РЕДАКТОРСКОЕ]`
- Сетка: `16 шагов/такт`, broken/mechanical backbeat. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `2`, `4` или `8` тактов; характерная — `4`. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X..o....X...o...  [РЕДАКТОРСКОЕ]
snare/clap ....X.......X...  [РЕДАКТОРСКОЕ]
closed hat o.o.o.o.o.o.o.o.  [РЕДАКТОРСКОЕ]
open hat   ......o.......o.  [РЕДАКТОРСКОЕ]
perc       ..o.o...o.o...o.  [РЕДАКТОРСКОЕ]
bass       X.o...o.X.o...o.  [РЕДАКТОРСКОЕ]
lead       ...o...o...o...o  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `2–6`; snare/clap `2–4`; closed hat `6–16`; open hat `0–3`; perc `4–12`; bass `3–8`; lead `1–6`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: minor pentatonic, Dorian, chromatic cells; короткие symmetrical/whole-tone fragments допустимы для lead, но не обязательны. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: straight/broken; swing `50–55%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: kick/snare `0`; hats `-1…0`; perc `-1…+1`; bass `-1…0` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: snare/clap `5,13`; primary kick `1,9`; syncopated secondary kick movable. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `0…±1` тик; velocity `±3…8`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 4 тактов: `Statement → Repeat → Variation → Fill`. `[РЕДАКТОРСКОЕ]`
- Операторы: все семь; `KICK_DISPLACEMENT`, `SHIFT_OPTIONAL_EVENT` и `ACCENT_CHANGE` несут основную variation load; `END_FILL` уместен в 4-м такте. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `ANSWER` или `MIXED`; syncopated bass не должен полностью удваивать kick. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–5 / 8–18 / 14–28%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 50, age 30, space 35, width 85, instability 35, aggression 90, darkness 55`. `[РЕДАКТОРСКОЕ]`
- Передаёт синтетическую чёткость и wide robotic surface; 808-идентичность не кодируется отдельным параметром тембра.

---

## 12. Ambient Downtempo

### A. GENRE

- BPM-коридор: `50–100`, типичная зона `60–80`. `[РЕДАКТОРСКОЕ]`
- Сетка: `8` или `16 шагов/такт`; разрешена высокая доля пустых тактовых позиций. `[РЕДАКТОРСКОЕ]`
- Допустимые длины: `4` или `8` тактов; характерная — `8`, текущий максимум. `[РЕДАКТОРСКОЕ]`
- Маски:

```text
kick       X.......o.......  [РЕДАКТОРСКОЕ]
snare/clap ....o.......o...  [РЕДАКТОРСКОЕ]
closed hat ..o.......o.....  [РЕДАКТОРСКОЕ]
open hat   ......o.........  [РЕДАКТОРСКОЕ]
perc       ...o.......o....  [РЕДАКТОРСКОЕ]
bass       X.......o.......  [РЕДАКТОРСКОЕ]
lead       ....o.......o...  [РЕДАКТОРСКОЕ]
```

- Плотность, событий/такт: kick `0–3`; snare/clap `0–2`; closed hat `0–6`; open hat `0–2`; perc `0–5`; bass `0–4`; lead `0–4`. `[РЕДАКТОРСКОЕ]`
- Ладовый словарь: pentatonic, Dorian, Aeolian, suspended/modal fields, whole-tone fragments; functional cadence необязательна. `[РЕДАКТОРСКОЕ]`
- Bass/kick relation находится в GENERATION.

### B. FEEL

- Тип: straight, lightly swung или broken; swing `50–60%`. `[РЕДАКТОРСКОЕ]`
- Push/pull: section anchors `0`; non-anchor perc/lead `-2…+2`; bass `0…+1` тика. `[РЕДАКТОРСКОЕ]`
- Protected anchors: первый tonal/root event секции; kick/snare anchors защищены только если они присутствуют, но не required для каждого такта. `[РЕДАКТОРСКОЕ]`
- Humanize: timing `±1…2` тика; velocity `±6…16`. `[РЕДАКТОРСКОЕ]`

### C. GENERATION

- Форма 8 тактов: `Statement → Repeat → MicroVariation → Breakdown → Variation → Repeat → Build → Resolution`. `[РЕДАКТОРСКОЕ]`
- Такты 5–8: 5-й вводит один новый tonal/perc event; 6-й удерживает его без роста плотности; 7-й добавляет максимум один low-priority layer; 8-й возвращает исходный root и удаляет decoration. `[РЕДАКТОРСКОЕ]`
- Операторы: `REMOVE_LOW_PRIORITY`, `SHIFT_OPTIONAL_EVENT`, `ADD_GHOST`, `ACCENT_CHANGE`; `OPEN_HAT_SWAP` редко; `END_FILL` должен быть минимальным; `KICK_DISPLACEMENT` допустим только для optional kick. `[РЕДАКТОРСКОЕ]`
- Bass/kick: `AVOID` или `MIXED`; тишина между событиями важнее полного совпадения. `[РЕДАКТОРСКОЕ]`
- Inter-bar distance: `0 / 0–2 / 1–5 / 6–14 / 3–8 / 0–4 / 3–8 / 5–12%`. `[РЕДАКТОРСКОЕ]`

### D. TEXTURE

- `dirt 20, age 50, space 110, width 100, instability 70, aggression 20, darkness 80`. `[РЕДАКТОРСКОЕ]`
- Передаёт пространственную, медленно меняющуюся поверхность; без неё результат остаётся sparse downtempo, но не обязательно ambient.
