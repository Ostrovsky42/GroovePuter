# GroovePuter: сравнительный анализ GENRE / FEEL / GENERATION / TEXTURE

Статус: музыковедческая спецификация для обсуждения. Этот документ **не меняет архитектуру, структуры данных, поля, генератор или интерфейс**.

Глубокие A–D-профили двенадцати жанров находятся в [`GROOVE_MODEL_GENRE_PROFILES.md`](./GROOVE_MODEL_GENRE_PROFILES.md).

## Зафиксированный контракт

Каждое музыкальное утверждение принадлежит ровно одной оси.

1. **GENRE** — коридор допустимого: BPM, метрическая сетка, required/preferred/forbidden позиции, плотности ролей, допустимые длины, ладовый словарь, роли голосов.
2. **FEEL** — положение события относительно сетки: straight/swing/triplet/broken, swing, push/pull, protected anchors, timing/velocity humanize.
3. **GENERATION** — построение и развитие материала: функции тактов, операторы вариаций, связь bass/kick, inter-bar distance.
4. **TEXTURE** — только семь макропараметров поверхности: dirt, age, space, width, instability, aggression, darkness.

### Неподвижные ограничения этого исследования

- Конструктивный линейный генератор и deterministic repair сохраняются.
- Scoring, перебор кандидатов, retry-циклы и новые операторы не предлагаются.
- Новые структуры данных и поля не предлагаются.
- Конкретные параметры синтезаторов не описываются.
- Маски — коридоры, а не готовые паттерны.
- Выводы о текущих адресах параметров являются **семантическим аудитом**, а не дизайном экранов.

---

# Часть 1. Выбор жанров

Выбраны двенадцать представителей. Критерий — покрытие пространства BPM, плотности, swing/microtiming, длины фразы, kick topology и межтактовой вариативности, а не популярность.

1. **Detroit Techno — семейство techno.** Закрывает прямой four-on-the-floor, умеренную плотность, малый swing и длинное постепенное развитие. Это нейтральная techno-точка между house, dub и industrial.
2. **Dub Techno — семейство techno.** Нужен отдельно от Detroit: низкая плотность и большие паузы проверяют, способен ли GENRE сохранять идентичность без сведения пространства и delay в ритм.
3. **Industrial Techno — семейство techno.** Закрывает верхнюю зону плотности и aggression при почти прямом timing. Показывает, что высокая интенсивность не обязана означать высокий swing.
4. **Chicago House — семейство house.** Даёт эталонный four-on-the-floor house-коридор: устойчивый kick, backbeat clap и offbeat hats. Публикационный ориентир house — `120–135 BPM`. `[ИСТОЧНИК S2]`
5. **UK Garage / 2-Step — семейство house/garage.** Закрывает broken kick topology и высокий swing при сохранённом backbeat. Внутренний Atlas-якорь содержит `134/136 BPM` и `66/68% swing`. `[КОРПУС C1]`
6. **Boom Bap — семейство hip-hop.** Нужен как базовая медленная backbeat-точка со средней вариативностью и выраженным поздним snare.
7. **Trip-Hop — медленная ветвь hip-hop/electronic.** Закрывает низкий BPM, длинную фразу и повышенную роль пустоты. Публикационный ориентир описывает slow beats, обычно не выше `90 BPM`. `[ИСТОЧНИК S7]`
8. **Lo-Fi Hip-Hop — медленная ветвь hip-hop.** Выбран намеренно рядом с boom bap: это стресс-тест, потому что значительная часть бытовой идентичности находится в TEXTURE, а не в ритмической грамматике.
9. **Jungle — breaks/jungle family.** Закрывает максимальную межтактовую вариативность, высокую плотность secondary events и быстрый broken pulse. Исследовательская литература описывает жанр через resequenced breakbeats и сложность downbeat interpretation. `[ИСТОЧНИК S5]`
10. **Acid House — 303-centric family.** Нужен как тест границы между ритмической линией, event articulation и тембром. Внутренний Atlas-якорь содержит `124/128 BPM` и `52/54% swing`. `[КОРПУС C1]`
11. **Electro / 808-Centric — electro family.** Закрывает механический broken rhythm со средним BPM и высокой syncopation при малом swing. TR-808 исторически стала центральным голосом hip-hop и electronic music. `[ИСТОЧНИК S4]`
12. **Ambient Downtempo — контроль минимальной плотности.** Нужен не как ещё один texture preset, а как крайняя точка sparse GENRE и малой inter-bar distance, где тишина является допустимым материалом.

Набор намеренно содержит близкие пары. Если модель не показывает их слипание, значит она скрывает проблему вместо её измерения.

---

# Часть 2. Сравнительная сетка

Обозначения плотности: `VL` very low, `L` low, `M` medium, `H` high, `VH` very high. Это относительные категории по сумме активных ролей; точные per-role corridors приведены в профилях.

| Жанр | BPM | Kick / метрический каркас | Swing | Характерная фраза | Плотность | Inter-bar distance | Bass ↔ kick | Ладовый центр | Зависимость от TEXTURE |
|---|---:|---|---:|---:|---|---:|---|---|---|
| Detroit Techno | `118–138` `[РЕДАКТОРСКОЕ]` | 4OTF, optional extra kicks | `50–56%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | M | `0–24%` `[РЕДАКТОРСКОЕ]` | MIXED | minor/Dorian | средняя |
| Dub Techno | `110–128` `[РЕДАКТОРСКОЕ]` | sparse 4OTF | `50–56%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | L | `0–18%` `[РЕДАКТОРСКОЕ]` | AVOID/MIXED | modal minor | очень высокая |
| Industrial Techno | `125–150` `[РЕДАКТОРСКОЕ]` | hard 4OTF, extra impacts | `50–53%` `[РЕДАКТОРСКОЕ]` | `4B` `[РЕДАКТОРСКОЕ]` | VH | `0–26%` `[РЕДАКТОРСКОЕ]` | FOLLOW/MIXED | Aeolian/Phrygian/chromatic | высокая |
| Chicago House | `118–130` `[РЕДАКТОРСКОЕ]` | 4OTF + backbeat | `50–58%` `[РЕДАКТОРСКОЕ]` | `4B` `[РЕДАКТОРСКОЕ]` | M/H | `0–22%` `[РЕДАКТОРСКОЕ]` | ANSWER/MIXED | pentatonic/Dorian/Mixolydian | средняя |
| UK Garage / 2-Step | `130–138` `[РЕДАКТОРСКОЕ]` | broken kick + fixed backbeat | `58–68%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | H | `0–30%` `[РЕДАКТОРСКОЕ]` | ANSWER/AVOID | minor/Dorian | средняя |
| Boom Bap | `78–100` `[РЕДАКТОРСКОЕ]` | broken kick + backbeat | `54–64%` `[РЕДАКТОРСКОЕ]` | `4B` `[РЕДАКТОРСКОЕ]` | M | `0–22%` `[РЕДАКТОРСКОЕ]` | FOLLOW/MIXED | pentatonic/blues/Dorian | средняя |
| Trip-Hop | `68–96` `[РЕДАКТОРСКОЕ]` | sparse broken backbeat | `54–66%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | L/M | `0–20%` `[РЕДАКТОРСКОЕ]` | MIXED/ANSWER | Aeolian/Dorian/Phrygian | высокая |
| Lo-Fi Hip-Hop | `60–90` `[РЕДАКТОРСКОЕ]` | boom-bap-like backbeat | `54–67%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | M | `0–18%` `[РЕДАКТОРСКОЕ]` | FOLLOW/MIXED | pentatonic/Dorian/Mixolydian | очень высокая |
| Jungle | `155–175` `[РЕДАКТОРСКОЕ]` | resequenced break skeleton | `50–58%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | VH | `0–45%` `[РЕДАКТОРСКОЕ]` | AVOID/MIXED | minor/Dorian/root fields | средняя |
| Acid House | `118–138` `[РЕДАКТОРСКОЕ]` | 4OTF + dense bass cell | `50–58%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | H bass / M drums | `0–30%` `[РЕДАКТОРСКОЕ]` | MIXED | Aeolian/Phrygian/pentatonic | очень высокая |
| Electro / 808 | `108–132` `[РЕДАКТОРСКОЕ]` | mechanical broken backbeat | `50–55%` `[РЕДАКТОРСКОЕ]` | `4B` `[РЕДАКТОРСКОЕ]` | H | `0–28%` `[РЕДАКТОРСКОЕ]` | ANSWER/MIXED | minor/Dorian/chromatic cells | высокая |
| Ambient Downtempo | `50–100` `[РЕДАКТОРСКОЕ]` | optional sparse anchors | `50–60%` `[РЕДАКТОРСКОЕ]` | `8B` `[РЕДАКТОРСКОЕ]` | VL/L | `0–14%` `[РЕДАКТОРСКОЕ]` | AVOID/MIXED | pentatonic/modal fields | очень высокая |

## Atlas C1 как проверочный якорь

Это отдельные корпусные наблюдения, не жанровые нормы:

- Chicago Jack: `124 BPM`, `52% swing`, `102` runtime events. `[КОРПУС C1]`
- Rolling Acid: `128 BPM`, `54% swing`, `106` runtime events. `[КОРПУС C1]`
- Classic 2-Step: `134 BPM`, `66% swing`, `93` runtime events. `[КОРПУС C1]`
- Dark Skippy: `136 BPM`, `68% swing`, `91` runtime events. `[КОРПУС C1]`
- Deep Chord: `120 BPM`, `54% swing`, `73` runtime events. `[КОРПУС C1]`
- Minimal Space: `116 BPM`, `51% swing`, `70` runtime events. `[КОРПУС C1]`

C1 содержит `6` recipes, `18` pattern slots и `535` runtime events. `[КОРПУС C1]` Поэтому он достаточен для проверки компиляции и внутренних различий Atlas slices, но недостаточен для узких статистических жанровых норм.

## Вывод 2.1 — параметры с основной различающей нагрузкой

1. **Kick topology / метрический каркас.** Four-on-the-floor, fixed backbeat, broken kick и resequenced break разделяют семейства раньше, чем тембр.
2. **Swing и role-specific push/pull.** Особенно различают UKG, slow hip-hop и straight techno/electro. Исследование microtiming показывает, что большее отклонение не гарантирует большего groove; поэтому важен ограниченный corridor, а не ручка «человечности» без предела. `[ИСТОЧНИК S12]`
3. **BPM.** Наиболее сильный разделитель крайних кластеров: ambient/slow hip-hop против jungle; внутри house/techno кластеров он слабее.
4. **Inter-bar distance и характерная длина.** Разделяют loop-centric four-bar material от eight-bar broken, acid и sparse forms.
5. **Плотность по ролям, особенно kick/bass/secondary percussion.** Общая note count менее полезна, чем распределение плотности между ролями.

## Вывод 2.2 — почти константы и слабые кандидаты для главного редактирования

- **Reference grid `16 steps/bar`** используется большинством профилей. `[РЕДАКТОРСКОЕ]` Различие чаще возникает в масках и FEEL, а не в самом выборе 1/16. `32 steps` нужны как углубление для secondary events в jungle/UKG, не как центральная жанровая ручка. `[РЕДАКТОРСКОЕ]`
- **Метр `4/4`** является рабочей общей рамкой этого набора. `[РЕДАКТОРСКОЕ]` Сам по себе он почти не различает строки таблицы.
- **Minor/Dorian/pentatonic vocabulary** сильно перекрывается у большинства жанров. Тонкая правка лада имеет низкую различающую силу и не должна занимать основной уровень.
- **Backbeat anchors `5/13` на 16-step grid** повторяются у house, UKG, hip-hop, electro и jungle. `[РЕДАКТОРСКОЕ]` Они важны как защита, но не как постоянная пользовательская ручка.
- **Velocity humanize** полезен, но его допустимые коридоры перекрываются. Он слабее различает жанры, чем swing topology и protected anchors.

## Вывод 2.3 — риск слипания

### Пара, реально различимая преимущественно одной осью

- **Boom Bap ↔ Lo-Fi Hip-Hop:** при пересекающихся BPM, backbeat, swing и плотности надёжно остаётся прежде всего **TEXTURE**. Без age/dirt/instability lo-fi часто становится вариантом slow boom bap. Это не ошибка таблицы, а честная граница модели.

### Почти слипающиеся пары, которым нужны две слабые оси

- **Detroit Techno ↔ Dub Techno:** различаются главным образом GENERATION (плотность, пустота, long-form removal) и TEXTURE (space/age). При нулевой texture и средней плотности граница слабая.
- **Chicago House ↔ Acid House:** pulse, BPM и swing сильно перекрываются; различие несут GENERATION bass articulation/density и TEXTURE.
- **Trip-Hop ↔ Lo-Fi Hip-Hop:** различаются большей пустотой и section-level development в GENERATION плюс более тёмной/пространственной TEXTURE; при короткой фразе могут слиться.
- **Electro ↔ straight Broken/Techno:** различие держится на kick topology/bass answer и articulation; один общий параметр `syncopation` недостаточен.

---

# Часть 3. Глубокие жанровые профили

См. отдельный файл [`GROOVE_MODEL_GENRE_PROFILES.md`](./GROOVE_MODEL_GENRE_PROFILES.md). Он содержит для каждого жанра:

- GENRE masks и per-role density corridors;
- FEEL ranges в тиках `96 PPQN`;
- GENERATION BarFunction sequence, допустимые операторы и inter-bar distance;
- TEXTURE preset из семи макропараметров;
- отдельную provenance-маркировку каждого числового диапазона и каждой маски.

---

# Часть 4. Тест на прочность инварианта

## 4.1 Acid House

**(а) Без TEXTURE.** Остаются four-on-the-floor, плотная syncopated monophonic bass cell, repeated-note vocabulary, long phrase и accent/slide event semantics. Если убрать также slide/accent events, ритмический остаток легко слипается с Chicago House.

**(б) В TEXTURE.** Корректны рекомендованные dirt/instability/aggression/darkness/width/space/age. Исторический TB-303 связан с характерным изменяемым acid sound. `[ИСТОЧНИК S3]` Но конкретные cutoff/resonance/decay не входят в эту модель.

**(в) Граница.** Наличие accent event и его изменение — `GENERATION / ACCENT_CHANGE`. Связь соседних нот через slide — event articulation в GENERATION. Реальное положение onset и velocity deviation — FEEL. Спектральный результат slide/accent — renderer/timbre, не новое поле GENRE.

## 4.2 Dub Techno

**(а) Без TEXTURE.** Остаётся sparse four-on-the-floor, малое число stabs, длинная 8-bar форма и removal-based development. При средней плотности это часто неотличимо от minimal/deep techno.

**(б) В TEXTURE.** Высокие space/width, средние age/instability/darkness корректно задают поверхность. История Basic Channel/Rhythm & Sound подчёркивает связь minimal techno и dub practice. `[ИСТОЧНИК S10]`

**(в) Граница.** Удаление/возврат stab — GENERATION. Поздний onset stab — FEEL. Длина хвоста, spatial spread и erosion повторов выражаются только семью TEXTURE macros.

## 4.3 Industrial Techno

**(а) Без TEXTURE.** Остаются высокая role density, straight timing, repeated impacts, short build-resolution cycle и жёсткая protected-anchor hierarchy. Этого достаточно для отличия от sparse Detroit/dub techno, но граница с hard techno остаётся условной.

**(б) В TEXTURE.** Dirt/aggression/darkness/instability передают абразивность; space/width не обязаны быть малыми.

**(в) Граница.** Дополнительный impact или ghost — GENERATION. Его микросмещение и velocity spread — FEEL. Искажение/жёсткость поверхности — TEXTURE.

## 4.4 Lo-Fi Hip-Hop

**(а) Без TEXTURE.** Остаётся slow backbeat, late snare, modest variation и loop-based phrase. Этого часто недостаточно: профиль становится неотличим от slow boom bap.

**(б) В TEXTURE.** Age, dirt, instability и darkness корректно выражают фильтрацию, noise/dust и деградацию. Современный lo-fi анализ прямо описывает high-frequency roll-off и добавленный dust/noise как средства реконструкции lo-fi surface. `[ИСТОЧНИК S9]`

**(в) Граница.** Ghost-note existence — GENERATION. Late snare и timing variance — FEEL. Dust, filtering impression и instability — TEXTURE. Они не должны добавлять или удалять события.

## 4.5 Trip-Hop

**(а) Без TEXTURE.** Остаются slow broken beat, sparse hats, длинная форма, breakdown и bass/kick answer. Это отличает его от стандартного boom bap, но может пересекаться с lo-fi/downtempo.

**(б) В TEXTURE.** Darkness, space, dirt и age корректно передают cinematic/sample-derived surface. Публикационные описания подчёркивают slow, dirty beats. `[ИСТОЧНИК S7]`

**(в) Граница.** Breakdown и удаление low-priority events — GENERATION. Laid-back snare — FEEL. Filth/space/darkness — TEXTURE.

## 4.6 Electro / 808-Centric

**(а) Без TEXTURE.** Остаются mechanical broken backbeat, syncopated kick/bass answer, straight timing и активная secondary percussion. Это достаточно сильный ритмический отпечаток.

**(б) В TEXTURE.** Width/aggression/dirt могут рекомендовать синтетическую поверхность. Историческая роль TR-808 относится к тембровому происхождению, но семь macros не должны кодировать конкретную drum machine. `[ИСТОЧНИК S4]`

**(в) Граница.** Kick displacement и accent pattern — GENERATION. Tight onset corridor — FEEL. Синтетическая резкость и ширина — TEXTURE.

## 4.7 Ambient Downtempo

**(а) Без TEXTURE.** Остаются минимальная плотность, optional anchors, длинная форма и малая inter-bar distance. Это честно называется sparse downtempo; слово ambient без пространственной поверхности становится менее определённым.

**(б) В TEXTURE.** Space/width/instability/darkness задают ambient surface, не меняя количество событий.

**(в) Граница.** Решение оставить такт почти пустым — GENRE corridor и GENERATION removal. Небольшой push/pull существующего события — FEEL. Пространственность — TEXTURE.

## Общая граница артикуляции

- **Ghost**: существование события — `GENERATION / ADD_GHOST`; его onset/velocity deviation — FEEL.
- **Accent**: изменение event emphasis — `GENERATION / ACCENT_CHANGE`; точная velocity variation — FEEL; spectral response — renderer/TEXTURE surface.
- **Open-hat replacement**: `GENERATION / OPEN_HAT_SWAP`; timing offset — FEEL.
- **Slide**: связь двух pitch events — GENERATION articulation. Она не является timing FEEL и не должна превращаться в synth-parameter TEXTURE.
- **TEXTURE** никогда не добавляет, не удаляет и не перемещает ноты.

---

# Часть 5. Гранулярность редактирования

## L1 — жанр и несколько макро-ручек

**Что реально позволяет:**

- выбрать corridor family;
- менять главные различители: swing/feel amount, role-density tendency, phrase/variation amount;
- выбирать рекомендованную TEXTURE surface и её amount;
- получать заметно разные результаты без редактирования внутренней грамматики.

**Чего не позволяет:**

- менять отдельные required/preferred/forbidden positions;
- настраивать per-role min/max;
- менять protected-anchor set;
- задавать eligibility каждого variation operator по тактам.

**Где ломается идентичность:**

- слишком широкий density macro может стереть sparse/ dense contrast;
- unrestricted swing может превратить straight techno/electro в чужой feel;
- texture amount может создать бытовой ярлык, не исправив ритмический corridor.

**Защита:**

- macro работает только внутри genre corridors;
- protected anchors не удаляются и не двигаются;
- texture не меняет events;
- variation amount выбирает разрешённые операторы, а не расширяет их список.

**Оценка:** основной уровень.

## L2 — числовые коридоры

**Что реально позволяет:**

- править per-role density min/max;
- сужать swing, push/pull, timing/velocity humanize;
- выбирать допустимые phrase lengths и inter-bar distance;
- менять вероятность уже разрешённых accent/slide/ghost operators.

**Чего не позволяет:**

- перестраивать метрическую грамматику по отдельным шагам;
- отменять required anchors;
- вводить новые BarFunction или variation operator;
- переносить свойства между осями.

**Где ломается идентичность:**

- `min > max` или чрезмерный corridor;
- kick density несовместима с kick topology;
- humanize шире шага разрушает метрическую читаемость;
- inter-bar distance превращает Repeat в Variation;
- accent/slide probability ошибочно воспринимается как FEEL.

**Защита:**

- `min <= max`, bounded role maxima;
- genre-specific hard bounds для swing/humanize;
- protected anchors immutable;
- BarFunction задаёт допустимый range inter-bar distance;
- operator eligibility остаётся фиксированной;
- deterministic repair возвращает только контрактные anchors, без retry.

**Оценка:** опциональное углубление для advanced mode и разработки presets.

## L3 — пошаговые masks

**Что реально позволяет:**

- переопределить required/preferred/forbidden topology каждого голоса;
- создать новый dialect внутри семейства;
- диагностировать, почему генератор выбирает или отвергает позицию.

**Чего не позволяет само по себе:**

- создать правильный feel;
- создать long-form development;
- создать texture;
- гарантировать музыкальную идентичность без согласования семи role masks.

**Где ломается идентичность:**

- удаление required kick/backbeat anchors;
- одновременное разрешение всех шагов делает genre corridor бессодержательным;
- чрезмерные forbidden masks делают density corridor невыполнимым;
- пользователь получает формально валидный, но стилистически пустой набор.

**Защита:**

- required anchors нельзя стереть в обычном режиме;
- mask feasibility проверяется против density min/max;
- forbidden не может перекрыть все позиции роли;
- изменения должны быть отдельным expert/offline preset-authoring действием;
- стандартный firmware editor не обязан предоставлять L3.

**Оценка:** не нужен как обычный пользовательский уровень. Полезен как read-only inspector или инструмент автора presets/tests.

## Итоговая рекомендация

- **Основной: L1.** Он должен показывать только параметры с высокой различающей силой: genre, swing/feel, role-density tendency, phrase/variation amount и texture preset/amount.
- **Опциональный: L2.** Для точной настройки corridors и разработки жанровых профилей.
- **Не выводить в стандартное редактирование: L3.** Маски остаются внутренним контрактом и объектом проверки.

Ладовый словарь и raw grid resolution имеют слишком малую различающую нагрузку, чтобы занимать основной уровень. Exact masks важны для защиты идентичности, но их ценность как постоянной пользовательской ручки ниже риска разрушения corridor.

---

# Семантический аудит текущих адресов

Ниже не предлагается новая компоновка. Таблица отвечает только на вопросы: какой оси принадлежит фактическое значение, на каком уровне оно полезно и какой дублирующий адрес должен считаться основным.

| Текущее обозначение | Фактический смысл на `dev` | Ось по контракту | Уровень | Решение об адресе |
|---|---|---|---|---|
| `G>` genre list | `GenerativeMode` | GENRE | L1 | оставить основным выбором genre family |
| `MODE` | `GrooveboxMode`, legacy генеративный macro mode | GENERATION/legacy composite | status, не L1 genre | не считать вторым адресом GENRE; до декомпозиции — read-only status или убрать editable duplicate |
| `FLAVOR` | меняет `PatternCorridors`, может применять sound macros | composite | не классифицируется чисто | не основной параметр; нарушает one-axis rule при одновременном pattern+sound effect |
| `N min..max` | notes corridor | GENRE | L2 | оставить как advanced corridor/readout |
| `A` | `accentProbability` | GENERATION | L2 | не маркировать как FEEL; это вероятность/интенсивность разрешённой articulation operation |
| `S` | `slideProbability` | GENERATION | L2 | не называть syncopation; это event articulation |
| `SW` | `swingAmount` | FEEL | L1/L2 | сильный кандидат для основного feel macro с genre bounds |
| `T>` texture list | `TextureMode` | TEXTURE | L1 | оставить основным texture preset address |
| `TX %` | texture amount | TEXTURE | L1 | оставить основным amount |
| `P SPACE/NORM/WIDE/GRIT` | composite feel/drum-FX presets | mixed/legacy | alias, не independent state | не создавать второй texture address; либо alias операции, либо удалить из основной модели |
| `GRID` | `FeelSettings.gridSteps` | FEEL только как reference resolution | L2 | почти константа; не основной discriminator |
| `TB HALF/NORM/DBL` | временное масштабирование/метрическая интерпретация | не FEEL в строгом контракте | unresolved | семантически реклассифицировать до реализации; скрыть из main axis model |
| `LEN` | `FeelSettings.patternBars` сейчас | выбранная длина — GENERATION; allowed lengths — GENRE | L1/L2 | текущая принадлежность FEEL нарушает контракт; один основной адрес у phrase generation |
| `GROOVE:ACD` | legacy `GrooveboxMode` readout | GENERATION/legacy | status | не ещё один genre selector |
| `LINK:GEN` | соответствие genre recipe и groove mode | межосевой status | status | только readout, не музыкальная ось |
| `M:%` | recipe morph amount | GENERATION | L1 | может быть variation/development macro при сохранении operator bounds |
| `R:BASE` | recipe selection | GENERATION | L2 | advanced recipe address |
| `APPLY:SND` | операция применения | нет оси | action | не хранить как музыкальное свойство |
| `MACROS OFF/ON` | разрешает Flavor → Sound coupling | межосевая операция | safe default OFF | не считать параметром жанра; ON явно нарушает независимость осей |
| `BUDGET FREE/DUCK` | инженерная диагностика | нет оси | diagnostic | не участвует в musical edit depth |

## Критическая поправка к визуальной интерпретации строки

Текущий код формирует:

```text
N notesMin..notesMax  A accentProbability  S slideProbability  SW swingAmount
```

Следовательно:

- `N` — GENRE;
- `A` — GENERATION;
- `S` — GENERATION;
- `SW` — FEEL.

Строка смешивает три уровня модели, а не два. Особенно опасно читать `S` как syncopation: фактически это slide probability. `[РЕПО R3]`

## Основной вывод по текущему состоянию

Проблема не в отсутствии глубины, а в отсутствии единственного семантического адреса:

- genre family дублируется legacy mode;
- texture preset конкурирует с composite preset row;
- выбранная phrase length хранится и показывается как FEEL;
- accent и slide визуально соседствуют со swing без указания разных осей;
- legacy Flavor может менять одновременно generation corridors и sound macros.

До добавления новых редактируемых параметров следует сначала признать один source-of-truth address на ось и перевести остальные элементы в readout, alias-action или advanced layer.

---

# Провенанс

## Категории

- `[ИСТОЧНИК S#]` — внешняя публикация, официальный материал, интервью или исследование.
- `[КОРПУС C1]` — статистика конкретного внутреннего корпуса; размер указан ниже.
- `[РЕДАКТОРСКОЕ]` — экспертный corridor, выбранный по метрической функции, слуховой практике и теоретической совместимости с ограниченной 16-step/96-PPQN моделью.
- `[РЕПО R#]` — факт текущей реализации; не музыковедческий источник.

Широкие редакторские диапазоны используются намеренно. Внешние источники редко задают per-role masks, PPQN offsets и exact density corridors; выдумывать для них ложную точность было бы хуже, чем явно маркировать экспертное решение.

## Внешние источники

- **S1.** Mark J. Butler, *Unlocking the Groove: Rhythm, Meter, and Musical Design in Electronic Dance Music*. https://books.google.com/books/about/Unlocking_the_Groove.html?id=0_9dMXf2susC
- **S2.** Carnegie Hall, *History of House*. https://timeline.carnegiehall.org/genres/house
- **S3.** Roland, *TB-303 Software Synthesizer / history of the TB-303 sound*. https://www.roland.com/global/products/rc_tb-303/
- **S4.** Roland, *TR-808: the history, the sound and the present*. https://experience.roland.com/musicconnects/global/180305-01.html
- **S5.** Hockman, Davies, Fujinaga, *One in the Jungle: Downbeat Detection in Hardcore, Jungle, and Drum and Bass*. https://zenodo.org/records/1417054
- **S6.** R. Christodoulou, *Bring the Break-Beat Back! Authenticity and the Politics of Rhythm in Drum & Bass*. https://dj.dancecult.net/index.php/dancecult/article/view/1153
- **S7.** MusicRadar, *The core thing to remember is that your beats are slow — 90 BPM maximum — and they are filthy: unpacking the sound of trip-hop*. https://www.musicradar.com/tutorials/the-core-thing-to-remember-is-that-your-beats-are-slow-90-bpm-maximum-and-they-are-filthy-unpacking-the-dark-sample-based-sound-of-trip-hop
- **S8.** Dan Charnas, *Dilla Time*. https://us.macmillan.com/books/9781250862976/dillatime/
- **S9.** Adam Scott Neal, *Lo-fi Today*, Organised Sound 27(1). https://www.cambridge.org/core/journals/organised-sound/article/lofi-today/73B4DDB240C2B2DC0A0E64249AB44325
- **S10.** Moritz von Oswald, Red Bull Music Academy lecture. https://www.redbullmusicacademy.com/lectures/moritz-von-oswald-early-morning-freestyles/
- **S11.** The Quietus, *The Strange and Frightening World of Basic Channel*. https://thequietus.com/interviews/strange-world-of/the-strange-and-frightening-world-of-basic-channel/
- **S12.** Senn et al., *The Effect of Expert Performance Microtiming on Listeners' Experience of Groove in Swing or Funk Music*. https://pmc.ncbi.nlm.nih.gov/articles/PMC5050221/

## Корпус

- **C1. GroovePuter Atlas v2.6 runtime slice.** `tools/atlas/atlas_v2_6_runtime_manifest.json`: `6` recipes, `18` pattern slots, `535` runtime events; recipes Chicago Jack, Rolling Acid, Classic 2-Step, Dark Skippy, Deep Chord, Minimal Space. `[КОРПУС C1]`

## Текущее состояние репозитория

- **R1.** `src/dsp/genre_manager.h` — текущие GenerativeMode, TextureMode, GenerativeParams, GrooveRecipe и GenreBehavior.
- **R2.** `src/ui/pages/genre_page.cpp` и `src/ui/pages/feel_texture_page.cpp` — текущие addresses genre/texture/grid/timebase/length/presets.
- **R3.** `src/ui/pages/mode_page.cpp` — фактический формат `N/A/S/SW`, GrooveboxMode/Flavor/Phrase/Macros/Budget.

---

# Acceptance checklist исследования

- [x] Представлены все требуемые семейства.
- [x] Набор покрывает крайние зоны BPM, density, swing, phrase length и inter-bar variation.
- [x] Есть одна сравнительная таблица до глубоких профилей.
- [x] Выделены сильные discriminators, почти константы и collision pairs.
- [x] Каждая маска и каждый числовой corridor в жанровых профилях имеют provenance tag.
- [x] Texture-defined genres проверены при нулевой TEXTURE.
- [x] Accent, slide, ghost и timing разведены между GENERATION и FEEL.
- [x] Сравнены L1/L2/L3 и дана итоговая рекомендация.
- [x] Текущие addresses классифицированы без предложения новой архитектуры или UI layout.
- [x] Не добавлены новые поля, структуры, operators, scoring или retry loops.
