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

**Файлы, затронутые в переходе**
- `src/ui/pages/feel_texture_page.h`
- `src/ui/pages/feel_texture_page.cpp`
- `src/ui/pages/settings_page.h`
- `src/ui/pages/settings_page.cpp`
- `src/ui/pages/genre_page.h`
- `src/ui/pages/genre_page.cpp`
- `src/ui/ui_common.h`
- `src/ui/ui_common.cpp`
- `src/ui/miniacid_display.cpp`
- `src/dsp/miniacid_engine.h`
- `src/dsp/miniacid_engine.cpp`
- `src/dsp/genre_manager.h`
- `src/dsp/genre_manager.cpp`
- `src/dsp/pattern_generator.cpp`
- `scenes.h`
- `scenes.cpp`
- `keys_sheet.md`
- `docs/FEEL_RECIPES.md`
