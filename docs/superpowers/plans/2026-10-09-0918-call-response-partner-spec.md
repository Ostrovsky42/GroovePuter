# 0.9.18 Call/Response musical partner (owner spec, 2026-10-09)

Verbatim owner-provided task, queued after DnB.

# GroovePuter 0.9.18
# CALL / RESPONSE MUSICAL PARTNER

Работаем в:

```text
Ostrovsky42/GroovePuter
```

Перед любой работой:

```bash
gh repo set-default Ostrovsky42/GroovePuter
gh repo view --json nameWithOwner

git status --short
git branch --show-current
git rev-parse HEAD
```

Expected repository:

```text
Ostrovsky42/GroovePuter
```

Не rebase.

Не предполагать SHA или branch текущей 0.9.18 — сначала установить exact authoritative state.

---

# 0. PRODUCT GOAL

Нужно превратить GroovePuter из:

```text
generator / recorder / sequencer
```

в инструмент, который умеет:

```text
HUMAN CALL
    ↓
LISTEN
    ↓
UNDERSTAND MOTIF STRUCTURE
    ↓
WAIT / LEAVE SPACE
    ↓
MACHINE ANSWER
```

Это НЕ realtime AI melody generation.

Это bounded musical interaction:

```text
short performed motif
→ compact motif descriptor
→ genre-constrained transformation
→ quantized musical answer
```

Главный продуктовый критерий:

> Пользователь играет короткую фразу и ощущает, что инструмент услышал именно эту музыкальную мысль и ответил на неё.

Не:

> Пользователь играет что-то, после чего запускается случайный генератор.

---

# 1. CURRENT MUSICAL FOUNDATION

Считать существующее направление правильным:

```text
paired bass + Synth B template
SEQTRAK-derived genre material
A A′ A B phrase logic
key/mode projection
genre invariants
STYLE / LIVELY
deterministic generation
```

Call/Response должен расширять эту модель, а не создавать второй независимый генератор.

Особенно сохранить принцип:

```text
bass
+
upper voice
=
one compositional decision
```

Если существующий semantic owner уже может представить phrase-level idea, расширить его.

Не создавать parallel generation architecture.

---

# 2. RESEARCH INSPIRATION — DO NOT COPY IMPLEMENTATION

Полезные внешние precedents:

```text
OMax / Somax2
input material
→ descriptors / learned relations
→ stylistically coherent response

Dicy2
incoming stream
→ musically related generated sequence
```

Для GroovePuter НЕ использовать:

```text
neural network
large corpus runtime
factor oracle
dynamic allocations proportional to corpus
Python/Max-like architecture
```

Вместо этого:

```text
MotifDescriptor
+
GenreResponseGrammar
+
DeterministicTransform
```

---

# 3. IMPLEMENT IN STAGES

Не пытаться сразу решить live nanoKEY.

## CR-P0 — GENERATOR CALL/RESPONSE

Сначала доказать саму музыкальную грамматику без live MIDI.

Добавить phrase families, в которых silence и answer являются structural owners.

Baseline:

```text
A | A′ | A | B
```

Новые кандидаты:

```text
CALL_RESPONSE
A | A′ | _ | B

SHORT_CALL
A | _ | A′ | B

DELAYED_RESPONSE
A | A′ | [space + pickup] | B

ANTIPHONY
upper call
bass answer
upper variation
combined B
```

`_` означает protected musical space.

Это НЕ пустой failure pattern.

Это осознанная часть IdeaPlan.

Acceptance CR-P0:

- можно получить phrase, где один голос намеренно молчит;
- B ощущается ответом на A/A′;
- новый topology не разрушает genre identity;
- silence участвует в deterministic signature;
- повторное G даёт разные interaction shapes, а не просто другие pitch values.

Только после musical listening acceptance переходить к live capture.

---

# 4. CR-P1 — FIXED-WINDOW LIVE CALL

Первая live-версия должна быть предельно простой.

НЕ делать arbitrary phrase segmentation.

Контракт:

```text
BAR N
human CALL

BAR N+1
machine ANSWER
```

Первый prototype:

```text
CALL WINDOW   = exactly 1 bar
ANSWER WINDOW = exactly 1 bar
```

При включении LISTEN:

```text
current bar
→ arm

next bar boundary
→ begin capture

1 bar later
→ close capture
→ build descriptor
→ prepare answer

next bar
→ play answer
```

Таким образом пользователь всегда знает:

```text
я играю этот такт
машина отвечает следующий
```

Это важнее «умного» определения окончания phrase.

---

# 5. TURN-TAKING STATE MACHINE

Нужен один authoritative state owner.

Пример:

```text
IDLE

ARMED
  waiting for next musical boundary

CAPTURING_CALL

PREPARING_ANSWER

ANSWER_READY

PLAYING_ANSWER

RETURN_TO_LISTEN
```

Не привязывать state transitions к UI redraw.

Переходы должны идти от authoritative transport clock.

Запрещено:

```text
millis()-based musical boundaries
UI-frame timing
independent MIDI timer
```

Использовать существующий musical transport / bar ordinal.

---

# 6. MOTIF DESCRIPTOR

Не хранить «AI representation».

Нужна компактная структура, которую музыкант мог бы описать словами.

Минимальные dimensions:

```text
noteCount

onset positions
durations

relative scale degrees
interval contour
    UP
    DOWN
    SAME

register center
register span

first degree
last degree

rhythmic density

protected gaps

strong-note positions
```

Опционально, только если текущая архитектура поддерживает дешево:

```text
velocity hierarchy
microtiming residual
accent
```

Не копировать абсолютные MIDI notes как единственное описание.

Главное:

```text
C4 E4 G4
D4 F#4 A4
```

могут быть одной motif relation после tonal projection.

---

# 7. INPUT NORMALIZATION

Human performance НЕ переписывать.

Исходный MIDI должен звучать так, как его сыграл пользователь.

Для анализа строится projection:

```text
absolute MIDI
→ project tonic/mode context
→ scale degree / chromatic relation
→ interval contour
```

Ответ строится уже из descriptor.

Если input содержит chromatic note:

- не исправлять сыгранный CALL;
- response projection решает, разрешён ли chromatic relation текущим genre grammar;
- если нет — выбрать ближайший разрешённый structural equivalent.

---

# 8. QUANTIZATION

Первая версия:

```text
structural quantization
```

а не уничтожение живого исполнения.

Descriptor хранит:

```text
quantized onset
```

и, если текущий sequencer это уже поддерживает:

```text
microtiming offset
```

Response grammar может:

```text
COPY_GROOVE
NORMALIZE_GROOVE
GENRE_GROOVE
```

Но CR-P1 может начать только с:

```text
GENRE_GROOVE
```

чтобы не смешивать две исследовательские задачи.

---

# 9. RESPONSE GRAMMAR

Не порождать новый unrelated melody.

Каждый ответ:

```text
CALL descriptor
+
ONE primary transform
+
ONE cadence / B strategy
```

Максимум.

Минимальный словарь transforms:

## ECHO_VARIANT

Сохраняет rhythm identity.

Меняет:

```text
starting degree
или
landing degree
или
register
```

---

## CONTOUR_REPLY

Сохраняет:

```text
UP / DOWN / SAME contour
```

но строит его от другого разрешённого chord tone.

---

## CONTRARY_REPLY

Когда разрешено genre contract:

```text
CALL rises
ANSWER falls
```

или наоборот.

Не применять автоматически ко всем жанрам.

---

## SHORTEN_REPLY

Использует:

```text
motif head
или
motif tail
```

и оставляет больше space.

Очень важный variant.

---

## REGISTER_REPLY

Сохраняет motif relation, но отвечает:

```text
higher
или
lower
```

из другого register zone.

---

## CADENTIAL_REPLY

Сохраняет часть call identity, но меняет конец на B-turnaround:

```text
b7 → 1
5 → 1
2 → 1
octave → tonic
```

только в соответствии с genre grammar.

---

# 10. ANSWER MUST HAVE B

`B` — не фиксированная последовательность нот.

Это функция:

```text
B =
response-specific closure / turnaround
```

Требования:

```text
recognisably related to CALL
+
points toward next phrase/bar
+
respects current harmony
```

Для Acid допустимы:

```text
octave motion
303-compatible slide
accent landing
```

Для UKG:

```text
late pickup
short bass response
protected silence
```

Для HipHop/LoFi:

```text
shortened answer
b7 / neighbour approach
late landing
```

House:

```text
small chord-tone response
offbeat-compatible ending
```

---

# 11. SILENCE IS FIRST-CLASS DATA

Call/Response провалится, если generator считает:

```text
empty step = unused capacity
```

Нужно различать:

```text
EMPTY
```

и:

```text
PROTECTED_SILENCE
```

Хотя representation может быть компактным и не требовать отдельного event на каждый step.

IdeaPlan должен иметь возможность сказать:

```text
do not place notes here
```

Это genre-aware structural constraint.

Особенно для:

```text
UKG
LoFi
HipHop
darker synth material
```

---

# 12. RESPONSE TARGET

CR-P1 использовать один internal response target.

Preferred:

```text
existing upper melodic voice
```

если это не конфликтует с current voice ownership.

Не предполагать, что это обязательно Synth B.

Сначала построить exact inventory:

```text
nanoKEY MIDI input owner
current Synth A owner
current Synth B owner
external MIDI owner
live monitor owner
```

И выбрать voice, который не разрушает существующий bass+upper pairing.

---

# 13. CR-P2 — SEQTRAK RESPONSE TARGET

Только после internal acceptance.

Абстракция:

```text
ResponseTarget

INTERNAL_VOICE
EXTERNAL_MIDI
```

Обе получают одни и те же semantic response events.

Для SEQTRAK:

- использовать существующий MIDI routing;
- не дублировать Note generation;
- соблюдать project clock;
- response NoteOn/NoteOff должны уходить на выбранный SEQTRAK musical track;
- не hardcode channel numbers, если проект уже имеет authoritative mapping.

Yamaha documentation может использоваться только как validation reference.

---

# 14. EMPTY / BAD CALLS

Не придумывать материал при недостаточном input.

Пример:

```text
0 notes
→ NO_CALL

1 very short note
→ INSUFFICIENT_MOTIF
```

Минимум для melodic transformation определить после audit.

Предпочтительно:

```text
>= 2 structural note events
```

но не хардкодить до проверки existing phrase semantics.

Failure:

```text
CALL TOO SHORT
```

не должен запускать generic generator.

---

# 15. NOTE LIFETIME AT BAR BOUNDARY

Обязательно определить.

Если NoteOn произошёл внутри CALL, а NoteOff после boundary:

первый implementation:

```text
descriptor duration
= clamp to capture boundary
```

Но live monitor NoteOff должен пройти корректно.

Не создавать stuck notes.

Тестировать:

```text
note held across boundary
sustain-like long notes
repeated same pitch
NoteOn immediately before boundary
```

---

# 16. DETERMINISM

Одинаковый:

```text
MotifDescriptor
genre
style
lively
key
mode
harmonic context
response ordinal
seed
```

→ одинаковый response.

Не использовать uncontrolled RNG.

---

# 17. MEMORY / REALTIME

Никаких allocations в realtime input path.

Предпочтительно:

```text
fixed-capacity captured event array
fixed MotifDescriptor
fixed response scratch
```

Никаких:

```text
std::vector growth
heap corpus
runtime graph allocation
```

Не увеличивать accepted realtime memory requirements без измерения.

---

# 18. UI

Первая live UX должна быть простой:

```text
PARTNER OFF
PARTNER ARM
LISTENING
ANSWER
```

Минимум состояний.

Не показывать:

```text
TRANSFORM 7
TRAJECTORY 0.63
```

Если позже нужен musical control:

```text
ANSWER:
ECHO
CONTRAST
SPARSE
AUTO
```

Но CR-P1 может иметь только AUTO.

---

# 19. TESTS

## Structural

Для нескольких synthetic calls:

```text
ascending motif
descending motif
repeated-note motif
syncopated motif
sparse two-note motif
long-note motif
```

проверить:

```text
answer shares structural relation
answer remains in allowed pitch context
answer has valid B
answer leaves declared protected space
```

## Determinism

Same call → same answer.

## Boundary

```text
capture starts exactly on bar
capture ends exactly after one bar
answer starts only at next intended boundary
```

## No stuck notes

Все active notes получают valid lifecycle.

## Genre

Минимум:

```text
Acid
House
UKG
HipHop
LoFi
```

## Realtime

На hardware:

```text
no reboot
no underrun
no heap ratchet
no increasing largest-block fragmentation
```

---

# 20. MANUAL MUSICAL ACCEPTANCE

Для каждого жанра:

1. сыграть 10 CALL phrases;
2. услышать 10 ANSWERS;
3. записать verdict:

```text
RELATED
UNRELATED
TRIVIAL COPY
OVERACTIVE
GOOD RESPONSE
```

Acceptance:

```text
ответ узнаваемо связан с вопросом
но не является буквальным replay
```

Особое внимание:

```text
умеет ли машина молчать?
умеет ли ждать?
умеет ли закончить?
```

Это важнее количества different notes.

---

# 21. FIREWALL

На этом track НЕ менять без доказанной необходимости:

```text
Song architecture
MaterialSlot storage
project persistence model
audio engine
genre dataset extraction
phrase storage architecture
Undo model
Performance Scenes
```

Performance Scenes — отдельный checkpoint.

---

# 22. DELIVERABLE

В конце:

```text
BASE_SHA=
HEAD_SHA=

CURRENT_VOICE_OWNERSHIP=
TRANSPORT_OWNER=

CR_P0_GENERATOR=
CR_P1_LIVE_CAPTURE=
CR_P2_EXTERNAL_TARGET=

MOTIF_DESCRIPTOR=
RESPONSE_TRANSFORMS=
PROTECTED_SILENCE_MODEL=

DETERMINISM=
MEMORY_DELTA=
TESTS=
HARDWARE=

MUSICAL_ACCEPTANCE=
```

И отдельно ответить:

> Почему generated answer является ответом именно на сыгранную phrase, а не просто новым genre-correct pattern?
