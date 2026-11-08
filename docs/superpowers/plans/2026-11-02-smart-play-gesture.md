# Smart Play Gesture Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement task-by-task. Steps use checkbox syntax.

**Goal:** VoiceLead сохраняет направление входной мелодии вместо залипания на предыдущем результате.

**Architecture:** Чистый выбор высоты на основе интервала ввода; компактная история принятых жестов в PerformanceKeyboard. Существующие NoteOn/NoteOff ownership и остальные режимы сохраняются.

**Tech Stack:** C++, существующие host-тесты, ESP32/Cardputer.

**Spec:** `docs/superpowers/specs/2026-11-02-musical-smart-play-design.md`, этап A.

## Global Constraints
- Рабочий репозиторий `genre-cell-source`, ветка `genre/cell-source`; перед работой проверить HEAD и dirty diff.
- Без heap в обработке нот; PerformanceContext <=24 байт; совокупный прирост постоянной памяти этапов A+B <=256 байт.
- Не коммитить чужие изменения; не прошивать плату в рамках выполнения только host-проверок.
- Алгоритмические параметры — предложения из spec; музыкальная приёмка требует прослушивания.

## Review Focus
- Граница диапазона: не обращать движение произвольным скачком — задача 1.
- Смена аккорда при повторе клавиши: допустимая нота без залипания вне гармонии — задача 1.
- Отвергнутый ввод: не меняет историю — задача 2.
- Внешний MIDI и физическая клавиатура: одинаковая последовательная семантика — задача 2.
- Смена цели/panic при удержанной ноте: корректный NoteOff и сброс истории — задача 2.

### Task 1: Чистый выбор ноты с учётом жеста
**Files:** modify `src/input/performance_smart_play.h`; test `tests/test_performance_smart_play.cpp`.
**Interface:** новая `bool gestureVoiceLeadNote(uint8_t input, uint8_t previousInput, uint8_t previousOutput, uint16_t allowed, uint8_t minimum, uint8_t maximum, uint8_t& result)`; без состояния и аллокаций. Существующая voiceLeadNote остаётся доступной другим потребителям.
- [ ] Добавить тест: allowed={C,E,G}, previousInput=60, previousOutput=60, input=62 => result=64; следующий input=64, previousInput=62, previousOutput=64 => result=67.
- [ ] Добавить зеркальный спуск; повтор input=previousInput сохраняет разрешённый previousOutput; смена маски пересчитывает результат; пустая маска возвращает false; результат всегда внутри диапазона.
- [ ] Запустить `bash tests/run_smart_play_tests.sh`, подтвердить падение новых тестов.
- [ ] Реализовать правила этапа A из spec, используя signed-арифметику для разности и ограничения диапазона.
- [ ] Повторить тесты; PASS. Проверить, что RAW/SCALE/CHORD/BASS не изменились.

### Task 2: Подключение истории к живому вводу
**Files:** modify `src/input/performance_keyboard.h`, `src/input/performance_keyboard.cpp`; tests `tests/test_performance_keyboard.cpp`, `tests/test_performance_keyboard_external.cpp`, `tests/test_cr_keyboard_tap.cpp`, `tests/test_smart_play_input_ownership.cpp`.
**Interfaces:** хранить `previousLiveLeadInput_` рядом с существующим `previousLiveLeadNote_`; валидность истории привязать к существующему флагу. Новую функцию вызывать только для Lead/VoiceLead с полной историей, сохраняя вычисление allowed из interpretNote в едином helper, без дублирования гармонических правил.
- [ ] Добавить тесты реальных последовательностей через keyDown/keyUp и externalNoteOn/externalNoteOff: движение, повтор, отказ ввода, panic, смена режима/цели.
- [ ] Подтвердить RED новым тестом движения: разные восходящие клавиши не дают вечный унисон на неизменном аккорде.
- [ ] Подключить чистый selector к interpretNote через `const LeadGestureHistory* history = nullptr`, где `LeadGestureHistory` содержит `uint8_t previousInput` и `uint8_t previousOutput`; сохранить совместимость вызовов без него и размер PerformanceContext.
- [ ] Обновлять историю только для принятого жеста; определить и протестировать порядок для нескольких одновременных внешних NoteOn: порядок поступления, без обещания полифонического voice leading.
- [ ] Подтвердить PASS всего `bash tests/run_smart_play_tests.sh`; observer CALL продолжает получать исходные ноты, а NoteOff освобождает фактически сыгранную высоту.

### Task 3: Приёмка на слух и плате
**Files:** create `docs/testing/SMART_PLAY_GESTURE_ACCEPTANCE.md`.
- [ ] Зафиксировать идентичный набор MIDI-входов для старой/новой версии и их выходные события: подъём, спуск, повтор, скачок, смена гармонии, предел диапазона.
- [ ] Записать в отчёт ожидаемое и фактическое поведение, включая fallback без гармонии; новые тесты должны отличать старый дефект от нового поведения.
- [ ] Найти существующие команды сборки для CDC `build/cardputer-adv-dynbuffers` и MIDI-only `build/cardputer-adv-seqtrak-dynbuffers`; собрать обе и записать DRAM/flash до/после. Не восстанавливать команды по памяти.
- [ ] Провести согласованное прослушивание на одинаковом тембре и темпе. Критерий: движение исполнителя слышимо, намеренный повтор сохранён, MIDI-ноты сбалансированы.
- [ ] Завершить этап A отдельным review diff; переходить к этапу B после оценки этого результата.
