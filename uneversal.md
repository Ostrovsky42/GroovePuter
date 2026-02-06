# FEEL/TEXTURE Transition Instructions

**Scope**
Короткое руководство по текущему состоянию и дальнейшим шагам для FEEL/TEXTURE/SETTINGS/GENRE как единой системы.

**Канон (roles)**
- GENRE: что играем (поведение нот, генерация). Не трогает FEEL/TEXTURE автоматически.
- FEEL/TEXTURE: как ощущается сейчас (live timing + окраска).
- SETTINGS: как будет генериться при regen (генераторные параметры).
- REST = событие (note == -1), не ломаем формат. Retrig/Prob — уже в движке.

**Статус (сделано)**
- FEEL/TEXTURE: фокус‑группы (Tab), пресеты SPACE/NORM/WIDE/GRIT, delta‑toast по G/T/L.
- Scene.feel: grid/timebase/length/texture сохранены (streaming + ArduinoJson), опциональные поля с дефолтами.
- Timebase влияет на samplesPerStep, Length управляет циклом (16 stored steps), LEN показывает bar progress.
- Global toast доступен из страниц (UI::showToast/drawToast).
- SETTINGS: группы Timing/Notes/Scale, INFO справа, REGEN‑метки, пресеты TIGHT/HUMAN/LOOSE с delta‑toast.
- Мини‑HUD G/T/L в header GENRE и SETTINGS (read‑only).
- Cycle pulse/HUD на FEEL (wrap подсветка).
- GENRE: добавлены Reggae/TripHop/Broken, список жанров с прокруткой и индикатором scroll.
- GENRE persistence: сохраняются `gen/tex` и режим применения `regen` (SOUND+PATTERN / SOUND ONLY).
- GENRE Texture: добавлен `PSY` и слайдер `Texture Amount (0..100)` на фокусе TEXTURE.

**Канонический рецепт (Dub/Slow/Broken, без новых фич)**
См. `docs/FEEL_RECIPES.md`.

**Осталось (минимум)**
- Хоткеи в футерах без конфликтов: Alt+G (GENRE), Alt+6 (FEEL), Alt+0 (SETTINGS).
- Delta‑toast для GENRE пресетов (опционально, но желательно для прозрачности).
- Чётко зафиксировать LIVE/REGEN для swing/micro, если в будущем появится live‑применение.

**Совместимость**
- SEQ_STEPS остаётся 16 (хранение). Length/Timebase меняют только метрический цикл и samplesPerStep.
- Новые JSON поля — опциональные, с дефолтами на load.
- Формат паттернов не расширяем без отдельного плана миграции.

**Test Checklist**
- FEEL/TEXTURE открывается на Alt+6, навигация/пресеты работают.
- FEEL presets дают delta‑toast по G/T/L.
- SETTINGS presets дают delta‑toast с суффиксом (regen).
- LEN показывает bar progress при L>1.
- G/T/L видны в header GENRE и SETTINGS.
- Scene сохраняется и корректно восстанавливается после перезапуска.

**Performance Baseline (Release Gate)**
- Статус на текущей ветке: средняя CPU обычно в диапазоне ~`25..80%` в зависимости от сцены; краткие пики допустимы, но не должны вызывать лавинообразный рост underrun.
- `underruns`:
- Допустимо: редкий рост на тяжёлых переходах/переключениях страниц.
- Недопустимо: устойчивый рост на каждом окне PERF в спокойном воспроизведении.
- Память:
- `freeInt` и `largInt` должны оставаться стабильными по тренду (без непрерывной деградации).
- Диагностика:
- Детальный DSP breakdown (`v/d/s/f`) выборочный; нули между окнами не трактовать как “DSP не работает”.
- Для сравнения использовать тренд по `cpuAudioPctIdeal`, `cpuAudioPeakPct`, `audioUnderruns`.

**10-Min Stress Test (обязательный перед релизом)**
1. Запустить сцену с 2x303 + drums, проигрывать 10 минут.
2. Каждые 30-60 сек переключать страницы по кругу (`[ ]`), включая `Genre`, `Feel/Texture`, `TB303 Params`, `Project`.
3. В середине теста включить/выключить Tape/LoFi/Drive и сменить жанр с `S+P`, затем с `SND`.
4. Критерий PASS:
- Нет слышимых постоянных заиканий.
- `underruns` не растут лавинообразно.
- Нет падения в “нерабочее” состояние UI/аудио.

**Если снова видим CPU >100%**
- Шаг 1: проверить, что включён релизный билд с decimated profiling.
- Шаг 2: сравнить с отключёнными эффектами (Tape/Looper/Drive/LoFi) и зафиксировать вклад.
- Шаг 3: если перегруз остаётся без FX, приоритетно оптимизировать drums path (retrig density/voice complexity).
- Шаг 4: если перегруз только с FX, держать auto-throttle FX включённым и дополнительно ограничить wet/feedback на пике.

**Файлы, затронутые в переходе**
- `src/ui/pages/feel_texture_page.h`
- `src/ui/pages/feel_texture_page.cpp`
- `src/ui/pages/settings_page.h`
- `src/ui/pages/settings_page.cpp`
- `src/ui/pages/genre_page.h`
- `src/ui/pages/genre_page.cpp`
- `src/ui/ui_common.h`
- `src/ui/ui_common.cpp`
- `src/ui/grooveputer_display.cpp`
- `src/dsp/grooveputer_engine.h`
- `src/dsp/grooveputer_engine.cpp`
- `src/dsp/genre_manager.h`
- `src/dsp/genre_manager.cpp`
- `src/dsp/pattern_generator.cpp`
- `scenes.h`
- `scenes.cpp`
- `keys_sheet.md`
- `docs/FEEL_RECIPES.md`
