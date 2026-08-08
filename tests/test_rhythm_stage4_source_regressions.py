#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: str) -> str:
    return (ROOT / path).read_text()


def main() -> None:
    backend = read("src/generation/generation_backend.h")
    materializer_h = read("src/generation/materialization/pattern_materializer.h")
    materializer_cpp = read("src/generation/materialization/pattern_materializer.cpp")
    shadow_cpp = read("src/generation/shadow/pattern_shadow_metrics.cpp")

    require("LegacyAtlas" in backend, "Stage 4 must expose LegacyAtlas backend")
    require("LegacyProcedural" in backend,
            "Stage 4 must expose LegacyProcedural backend")
    require("Vocabulary" in backend, "Stage 4 must expose Vocabulary backend")
    require("scenes.h" not in backend and "Scene" not in backend,
            "generation backend route must not become persisted Scene state")

    require("UnboundRole" in materializer_h,
            "materializer must reject silently dropped realized roles")
    require("ignoredRoles" in materializer_h,
            "deferred roles must be explicitly ignored by the caller")
    require("destination = next;" in materializer_cpp,
            "materializer must build scratch output before transactional commit")
    require("sceneManager" not in materializer_cpp and
            "currentScene" not in materializer_cpp,
            "PatternMaterializer must not own Scene destinations")
    require("target.slide = false;" in materializer_cpp and
            "target.slide = true;" not in materializer_cpp,
            "Stage 4 must not translate rhythm gate intent into synth slide")
    require("groove.swing = 0" not in materializer_cpp and
            "groove.humanize = 0" not in materializer_cpp,
            "PatternMaterializer must not overwrite FEEL timing ownership")

    require("const DrumPatternSet& legacy" in shadow_cpp,
            "shadow comparator must observe legacy patterns by const reference")
    require("const MaterializedPatterns& vocabulary" in shadow_cpp,
            "shadow comparator must observe vocabulary output by const reference")

    print("Groove Vocabulary Stage 4 source ownership regressions: OK")


if __name__ == "__main__":
    main()
