# 0.9.16 — проверка синтовой генерации

База: `31018bfe`; ветка `feature/20261001-0914-15-candidate`, изолированный worktree `miniacid-wt-0914-15`. Код сохранён в `f31124db65378ba2b6470d248fc824be1cd4316b`. Финальный полный core прошёл на этом SHA с чистым отслеживаемым деревом. Это кандидат для аппаратной приёмки, а не принятый на слух релиз.

## Что изменено

Удалён верхний STOP-handler, перехватывавший также модифицированную G. Простая G на SYNTH A/B → NOTES → STEPS теперь проходит через общий жанр/рецепт/STYLE/гармонию; STOP активирует сразу, PLAY — на следующем BAR_START. На этой вкладке вне NOTE ENTRY клавиша P циклически переключает общий FAITHFUL / VARIANT / REWORK. Alt+G сохраняет наследуемый генератор выбранного синта и не зависит от P.

Undo/Redo синтовой Generation обновляет runtime-bank. Alt+G также обновляет его после коммита; запись защищена существующим AudioGuard. Весь обмен B1 Undo защищён от пересечения с BAR_START. Контекстный SONG WAIT/FAIL footer сохраняется. Новая справка соблюдает лимит 38 символов; модификаторный HUD и аудио-пробы не перенесены.

## Регрессии и хост

Красные прогоны зафиксировали неверный тип Undo у STOP G, отсутствие P и устаревшие runtime-события у Undo/Alt+G. Логи находятся в `build/evidence/0.9.16-synth-generation/`.

Новый runtime gate проверяет реальные страницы и движок, оба синта, 16 жанров, P1–P3, три исходные формы ударных и STOP/PLAY. Ожидаемые события строятся в отдельном банке: matcher не исправляет проверяемый engine. Проверены сохранность другой партии, немедленные события STOP, старые события до границы PLAY, повторная G при занятом owner, отмена до границы, отказ Redo в PLAY и Redo в STOP на ненулевом bank/slot; Alt+G на одинаковом исходнике/seed не зависит от P. P не меняет паттерн и Undo; selector общий с DRUMS/MATERIAL. G/P со скан-кодами в NOTE ENTRY остаются нотами.

Полный `scripts/ci/run_core.sh`: **PASS**, 44 группы, exit 0. Прогон: 2026-10-03, 14:18–15:16 (Europe/Belgrade). Прерванные прогоны с кодом 143 не засчитаны. Итоговые логи: `build/evidence/0.9.16-synth-generation/{core,sdl,adv,seqtrak,fs1b}-final.log`; состояние и точный SHA — `verify-status.json` в том же каталоге.

## SDL

Команда сборки: `make -C platform_sdl -j8`; запуск только из временного `runtime/`, с `SDL_AUDIODRIVER=dummy`. Использован отдельный Xephyr `DISPLAY=:0`: основной рабочий стол отправлял кириллицу вместо G/P. Окно найдено через PID `2981990`, сверено через `getwindowpid`, WID `2097160`. Кадры сняты `import -window`, каждый приведённый кадр просмотрен.

| Шаг | Ожидали | Увидели | Результат / кадр |
|---|---|---|---|
| SYNTH A: P | Следующий общий STYLE | REWORK | OK / `03-a-style.png` |
| SYNTH A: G STOP | Генерация выбранного синта | GENERATED, новая сетка | OK / `04-a-g.png` |
| SYNTH A: Undo | Generation Undo | UNDO: GENERATION, прежняя сетка | OK / `05-a-undo.png` |
| SYNTH A: Redo | Generation Redo | REDO: GENERATION | OK / `06-a-redo.png` |
| SYNTH A: Alt+G | Наследуемая генерация | Изменилась выбранная сетка | OK визуально / `07-a-alt-g.png`; алгоритм проверяет host |
| SYNTH B: P, P | Продолжение общего цикла | FAITHFUL, VARIANT | OK / `09-b-faithful.png`, `10-b-variant.png` |
| SYNTH B: G STOP | Генерация | GENERATED | OK / `11-b-g-stop.png` |
| NOTE ENTRY: P / G | Ввод нот | A4 / E3 | OK / `13-b-note-p.png`, `14-b-note-g.png` |
| SYNTH B: G PLAY | Отложенная активация | GEN -> NEXT BAR | OK визуально / `17-b-g-play.png`; bank activation проверяет host |
| Undo до границы | Отмена pending | UNDO: GENERATION, возвращена сетка | OK / `18-b-undo-pending.png` |
| STOP, Redo | Восстановление генерации | REDO: GENERATION | OK / `20-b-redo-stop.png` |

Финальный бинарник дополнительно проверен в отдельном Xephyr (`DISPLAY=:0`, PID `4065339`, WID `2097160`, сверены через PID). Полная транспортная матрица G/Alt+G обоих синтов подтверждена кадрами:

| Сценарий | Увидели | Кадры |
|---|---|---|
| A: G / Alt+G STOP | GENERATED / изменённая сетка | `23-a-stop-g.png`, `24-a-stop-alt-g.png` |
| A: G PLAY → Undo до границы | GEN -> NEXT BAR → UNDO: GENERATION | `26-a-play-g.png`, `27-a-cancel-g.png` |
| A: Alt+G PLAY → Undo до границы | GEN -> NEXT BAR → UNDO: GENERATION | `28-a-play-alt-g.png`, `29-a-cancel-alt-g.png` |
| B: Alt+G STOP | Изменённая сетка выбранного синта | `32-b-stop-alt-g.png` |
| B: G PLAY → Undo до границы | GEN -> NEXT BAR → UNDO: GENERATION | `34-b-play-g.png`, `35-b-cancel-g.png` |
| B: Alt+G PLAY → Undo до границы | GEN -> NEXT BAR → UNDO: GENERATION | `36-b-play-alt-g.png`, `37-b-cancel-alt-g.png` |

Все строки этой дополнительной визуальной матрицы — OK. Исходные финальные кадры: `/tmp/grooveputer-synth-0916-final-y2suhgkf/`. Временный процесс эмулятора завершён по своему PID, каталог репозитория не использовался для runtime-файлов.

Пути к кадрам: `build/evidence/0.9.16-synth-generation/`; исходные файлы — `/tmp/grooveputer-synth-0916-xephyr-tc2n2amm/`. Журналы SDL: `sdl-console.log` и `sdl-final-console.log` в каталоге evidence. Эта проверка относится к синтовой теме; полный интерактивный Phrase-capacity сценарий отдельно не повторялся.

### BLOCKERS FOR HARDWARE

Полный core и SDL/ADV/SEQTRAK/FS1B сборки завершились с exit 0. Программные гейты закрыты; аппаратная приёмка этого кандидата не выполнена. Образ не прошит. Повтор Alt+↑/↓ и усиленный указатель проигрывания из следующей UI-задачи в этот кандидат не входят.

### SDL UX FINDINGS

Все перечисленные тосты читаются, пересечения с body/HUD не обнаружены. В текущем MINIMAL shell футер показывает `[H] HELP`, а уровень виден в тосте P; новый контекстный footer не объявляется видимым в этом shell. Кириллический ввод рабочего стола был причиной непринятых букв в первом запуске, а не доказанным дефектом маршрутизации G/P.

### NOT OBSERVABLE IN SDL

Ровность физического звука, Cardputer-клавиши и модификаторы, аппаратные DRAM/heap/stack пики и холодный старт с карты, Save/Load на SD; USB Host/Device, VBUS, внешняя MIDI-клавиатура и MIDI OUT. Dummy audio и runtime-assertions не заменяют эти проверки.

Предсуществующий риск по ревью: plain G в PLAY обновляет resident-bank после публикации overlay без ожидания ранее начатого аудиочтения. Его наличие в общем пути уже на базе не доказано однопоточными тестами как устранённое; дальнейшая синхронизация публикации требует отдельной работы без удержания gate на scratch-подготовке. На устройстве обычный keyboard-dispatch уже имеет внешнюю AudioMutationScope; вывод о незакрытой ссылке прежде всего касается SDL.

## Финальные сборки и происхождение

Финальные сборки (exit 0): SDL, ADV CDC-on, SEQTRAK CDC-off, продуктовый FS1B CDC-off. У FS1B доказан link только candidate FatFs archive; статический DRAM **183220 / 191488 B** (data 37900, bss 145320). Это статический бюджет, не аппаратный минимум heap/stack.

Артефакты: `build/synth-0916-fs1b-cdcoff/GroovePuter.ino.bin` и `.elf`.

- BIN SHA256: `e0e825e03611657029243f02e3c7099c02501d2296888106ac2f66ac4d93f163`
- ELF SHA256: `fe9a1c09d8dfc082850a9c5776fd821d9641c01fc4acc55069d799732c9fb29d`

Удалённая документационная фасада объединена merge-коммитом `47249f5b`; изменены только README и docs/README. Её старый source-test требовал прежнюю маркетинговую фразу; теперь проверяет навигацию и правдивый статус аппаратной приёмки. Phrase Core после исправления проходит. Старые R6 и P2 guards, требовавшие удалённый STOP-handler, обновлены под владение NOTES; P2 executor contract сохранён и проходит. Эти адаптации не меняют код устройства.

Манифест проверяемых файлов: `build/evidence/0.9.16-synth-generation/source-manifest.json`, SHA256 `4fa84483563e994a243cb52e122081858ab51b504e42584d62e4b7d2885f091c` (1356 файлов, включая новые тесты; документация и runtime-файлы исключены). Финальный полный core и четыре финальные сборки запущены на чистом отслеживаемом коде `f31124db`; четыре сборки завершились успешно. Предыдущий полный прогон остановился на source-test, использующем исходный комментарий как якорь: комментарий восстановлен, соответствующие source-tests и оставшиеся interaction/scale/host-гейты прошли отдельно. Этот предыдущий прогон не засчитан как полный PASS; его лог сохранён как `core-comment-anchor-failed.log`.

Локальное состояние: реализация — коммит `f31124db`; этот отчёт фиксируется отдельным документационным коммитом. Push и тегирование в рамках локального отчёта не выполнялись. Общий грязный checkout не изменён; существующий untracked `platform_sdl/patterns/` в изолированном worktree не добавлялся и не удалялся.
