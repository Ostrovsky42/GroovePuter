# GroovePuter Genre Lab

Офлайн-конвейер: реальное аудио → проверяемые профили вариантов жанра → `constexpr`-таблицы для прошивки. И обратно: дубли GroovePuter → те же метрики → PASS/FAIL. Уши нужны только в конце, на слепом A/B.

Метод: сначала одно семейство (пилот — House), потом то же самое на остальные 16 жанров.

## Быстрый старт (2 минуты, без ML)

```bash
pip install -r requirements-core.txt
make test            # 17 тестов, включая транскрибер на синтетическом аудио
make demo            # синтетический корпус → профили → C++ заголовок → 3 проверки
make calibrate-demo  # калибровка порогов holdout'ом на 10 сидах
```

`make demo` доказывает, что валидатор:
- **пропускает** хороший генератор;
- **ловит** баг из аудита 2026-11-10 (реролл меняет ноты, но не ритм) — diversity 1/8;
- **ловит** генератор, который выдаёт чужой вариант — accuracy 0.0.

## Как устроено

```
MTG-Jamendo TSV ─┐
                 ├─ candidates ─ styles (Discogs-400) ─ select ──► manifest.csv  [GO/NO-GO]
теги авторов ────┘                                                   │
                                                                     ▼
              analyze: beats → stems → drums/bass → 16-step grid ─► bars/*.jsonl
                                                                     │
              profile ──► profiles.json ──► codegen ──► genre_lab_profiles.h
                              │
GroovePuter gen_probe ─► takes.jsonl ─► validate ─► PASS/FAIL ─► слепой A/B на SEQTRAK
```

Ключевая идея: корпус и дубли пишутся в **одну схему** (`docs/CONTRACT.md`) и меряются **одним** экстрактором признаков.

## Документы

| файл | о чём |
|---|---|
| `docs/METHOD.md` | метод, метрики, известные ограничения |
| `docs/CONTRACT.md` | схема bars/takes JSONL, профиль, заголовок |
| `docs/PLAN.md` | этапы с GO/NO-GO, расширение на 16 жанров |
| `docs/AB_PROTOCOL.md` | слепой ABX и тест предпочтения на SEQTRAK |
| `docs/AGENT_TASKS.md` | готовые задачи агенту, по одному PR |

## Полный прогон

```bash
pip install -r requirements-ml.txt     # тяжёлое: torch, demucs, beat_this, essentia
scripts/fetch_models.sh models/
scripts/run_pilot.sh                   # см. переменные внутри
```

## Лицензии (проверь сам перед использованием)

- MTG-Jamendo: аудио под Creative Commons, разные лицензии у треков. Аудио **не коммитить**; в репо идут только агрегированные профили.
- Модели Discogs (Essentia): CC BY-NC-SA 4.0, некоммерческое использование. В прошивку модель не попадает.
- beat_this — MIT. Demucs, basic-pitch, ADTOF — смотри их репозитории.

Если GroovePuter станет коммерческим, вопрос с моделью классификатора нужно решить заранее: коммерческая лицензия у MTG или другой классификатор.
