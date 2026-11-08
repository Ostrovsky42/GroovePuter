# Partner Motif Development Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement task-by-task. Steps use checkbox syntax.

**Goal:** Ответ узнаваемо продолжает CALL, оставляет пространство и меняется при повторном обращении.

**Architecture:** Чистое преобразование захваченной фразы перед существующей гармонизацией; компактная память выбора стратегии в listener. ResponsePlayer и MIDI ownership остаются существующими.

**Tech Stack:** C++, fixed storage, существующие CALL/RESPONSE host-тесты.

**Spec:** `docs/superpowers/specs/2026-11-02-musical-smart-play-design.md`, этап B.

## Global Constraints
- Начать после приёмки этапа A; изменения музыкальных правил оценивать отдельно от USB-исправлений.
- Без heap; не увеличивать capacity=16; прирост постоянной памяти A+B <=256 байт.
- Существующие настройки BARS/GENRE/CHANNEL/DELAY сохраняются.
- Один CALL из одной ноты не должен становиться пустым ответом.
- До A/B-прослушивания сохранить ECHO как стандартное поведение; эксперимент включать compile-time флагом `GROOVEPUTER_PARTNER_DEVELOPMENT` (по умолчанию 0).

## Review Focus
- Однотактное окно и последняя шестнадцатая с DELAY=1 — задача 1.
- Повторённый транспонированный CALL в другом такте — задача 2.
- Густые и предельные по capacity фразы — задача 1.
- Человек перебивает уже изменённый ответ — задача 3.
- OFF/новая эпоха/BARS во время диалога — задача 2.

### Task 1: Преобразования мотива
**Files:** create `src/input/call_response_development.h`, `tests/test_cr_development.cpp`; modify `tests/run_smart_play_tests.sh`.
**Interfaces:** `enum class ReplyStrategy : uint8_t { Echo, ShortReply, Develop };` и `bool transformCall(const PhraseCapture::Phrase& input, ReplyStrategy strategy, uint8_t delaySteps, PhraseCapture::Phrase& output)` в GroovePuterCallResponse. Возвращает true, если получилось допустимое изменение; иначе output=input, false. Сохраняет windowStartStep/windowSteps.
- [ ] Написать тесты ECHO parity; SHORT_REPLY оставляет ceil(N/2) первых нот и свободную шестнадцатую в конце; DEVELOP сохраняет первые две атаки, смещает индексы 2,4,6... на +1 при допустимости, обрезает длины до следующей атаки.
- [ ] Проверить n=1/2/16, окна 16/32, delay=0/1, хвост на границе: порядок строгий, уникальные onset, length>=1, результат не выходит за доступное окно. Невозможная трансформация возвращает исходную фразу, которую обрабатывает существующий planner.
- [ ] Добавить тест в runner, запустить `bash tests/run_smart_play_tests.sh`, подтвердить RED.
- [ ] Реализовать преобразования из spec без добавления нот и новых буферов в singleton.
- [ ] Повторить runner, подтвердить PASS. Для ECHO сравнить полные планы со старым planner, а не только число нот.

### Task 2: Выбор ответа и память диалога
**Files:** modify `src/input/call_response_development.h`, `src/input/call_response_listener.h`; test `tests/test_cr_development.cpp`, `tests/test_cr_partner_settings.cpp`.
**Interfaces:** `struct ReplyMemory` хранит lastSignature, repeatCount, lastStrategy, valid; `uint32_t callSignature(const PhraseCapture::Phrase&, const MotifDescriptor&)`; `ReplyStrategy chooseReplyStrategy(const PhraseCapture::Phrase&, const MotifDescriptor&, const ReplyMemory&)`; `void rememberReply(ReplyMemory&, uint32_t signature, ReplyStrategy actualStrategy)`.
- [ ] Тестировать одинаковую сигнатуру при переносе по времени и октавной транспозиции; изменение ритма/контура/длины окна меняет сигнатуру на фиксированных fixtures. Хэш не считать доказательством абсолютной идентичности всех фраз.
- [ ] Зафиксировать выбор: первая плотная фраза (>=8 нот на такт) => ShortReply, первая неплотная N>=3 => Develop, N<3 => Echo. Повтор идентичной сигнатуры перебирает Echo→Develop→ShortReply→Echo, пропуская недопустимые трансформации.
- [ ] Запустить runner: RED, затем реализовать policy и сброс ReplyMemory в stopClock().
- [ ] Обновлять память только после успешного startAnswer, хранить фактически использованную стратегию. Human cancel не сбрасывает память; OFF/эпоха/BARS сбрасывают.
- [ ] Подтвердить PASS; проверить повтор при смене тональности — решение о ритме может сохраниться, гармонизацию всегда вычислять заново.

### Task 3: Подключение и сравнение
**Files:** modify `GroovePuter.ino:serviceCallResponseLog`, `src/input/call_response_listener.h`; tests `tests/test_cr_response_cancel.cpp`, `tests/test_cr_answer_timing.cpp`, `tests/test_cr_answer_channel.cpp`; create `docs/testing/PARTNER_DEVELOPMENT_ACCEPTANCE.md`.
**Interfaces:** listener предоставляет `const ReplyMemory& replyMemory() const` и `void rememberStartedReply(uint32_t signature, ReplyStrategy strategy)`. Основной цикл выбирает стратегию, преобразует CALL, заново вычисляет descriptor изменённой фразы для planner; исходный descriptor остаётся источником идентичности CALL.
- [ ] Добавить интеграционные тесты: меняются атакующие события на повторе подходящего CALL, сохраняется его начало; отмена, каналы и баланс NoteOn/Off остаются правильными.
- [ ] Подтвердить RED, затем подключить transformCall перед planHarmonicEcho под compile-time флагом; fallback=исходная фраза. Добавить имя фактической стратегии в диагностический лог.
- [ ] Прогнать `bash tests/run_smart_play_tests.sh` и `bash tests/run_smf_note_lifetime_panic_tests.sh`: PASS.
- [ ] Собрать CDC и MIDI-only существующими проектными командами, флаг OFF и ON; зафиксировать DRAM, flash и отсутствие динамических аллокаций в новом пути.
- [ ] Подготовить одинаковые MIDI CALL для сравнения Echo/нового policy: одна нота, восходящий мотив, плотная фраза, три повторения, смена аккорда, перебивание. Одинаковые тембр/темп/уровень.
- [ ] Прослушать вместе: узнаваем ли мотив, слышно ли развитие, уместны ли паузы? Если выигрыша нет — пересмотреть правила, не увеличивать случайность.
- [ ] Только после положительной оценки включить новый policy по умолчанию, описать поведение рядом с документами CALL_RESPONSE; выполнить финальный review diff.
