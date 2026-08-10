from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_generation_headers_use_alias_safe_guards() -> None:
    headers = sorted((ROOT / "src/generation").rglob("*.h"))
    require(headers, "generation headers are missing")

    seen_guards: set[str] = set()
    for header in headers:
        lines = header.read_text(encoding="utf-8").splitlines()
        relative = header.relative_to(ROOT)
        require(len(lines) >= 4, f"header is unexpectedly short: {relative}")
        require(lines[0].startswith("#ifndef "),
                f"alias-safe include guard missing: {relative}")
        guard = lines[0].split(maxsplit=1)[1]
        require(lines[1] == f"#define {guard}",
                f"include guard define mismatch: {relative}")
        require(lines[-1] == f"#endif  // {guard}",
                f"include guard terminator mismatch: {relative}")
        require(guard not in seen_guards,
                f"duplicate generation include guard: {guard}")
        seen_guards.add(guard)


def test_desktop_links_phrase_evolution_dependencies() -> None:
    makefile = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")
    require(makefile.count("../src/generation/rhythm/bar_evolution.cpp") == 1,
            "SDL target must link bar_evolution.cpp exactly once")
    require(makefile.count("../src/generation/phrase/phrase_evolution.cpp") == 1,
            "SDL target must link phrase_evolution.cpp exactly once")


def test_scene_event_reader_has_no_dead_byte_counter() -> None:
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    start = scenes.index("bool SceneManager::loadSceneEvented(TReader&& reader)")
    end = scenes.index("#endif // SCENES_H", start)
    block = scenes[start:end]
    require("bytesRead" not in block,
            "evented Scene reader must not retain a write-only byte counter")


def main() -> None:
    test_generation_headers_use_alias_safe_guards()
    test_desktop_links_phrase_evolution_dependencies()
    test_scene_event_reader_has_no_dead_byte_counter()
    print("Pre-flash build source regressions: PASS")


if __name__ == "__main__":
    main()
