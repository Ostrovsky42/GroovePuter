#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def without_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//.*?$", "", source, flags=re.M)


realizer_h = read("src/generation/rhythm/rhythm_realizer.h")
realizer_cpp = read("src/generation/rhythm/rhythm_realizer.cpp")
resolver_h = read("src/generation/rhythm/relationship_resolver.h")
resolver_cpp = read("src/generation/rhythm/relationship_resolver.cpp")
context_h = read("src/generation/generation_context.h")
context_cpp = read("src/generation/generation_context.cpp")
mode_manager = read("src/dsp/mode_manager.cpp")
miniacid = read("src/dsp/miniacid_engine.cpp")

# Stage 2 must remain a shadow/unwired implementation. Production generation
# must not call it until the later materializer/backend migration stage.
for production in (mode_manager, miniacid):
    assert "rhythm_realizer.h" not in production
    assert "realizeRhythmPhrase(" not in production
    assert "RelationshipResolver" not in production

# No heap-owning or unbounded standard containers in the embedded realization
# path. Strip comments before checking the C++ new-expression so prose such as
# "new shared coordinate" cannot create a false positive.
for source in (realizer_h, realizer_cpp, resolver_h, resolver_cpp, context_h, context_cpp):
    code = without_comments(source)
    assert "std::vector" not in code
    assert "std::string" not in code
    assert re.search(r"\bnew\s+(?:\(|\[|[A-Za-z_:])", code) is None
    assert "malloc(" not in code
    assert "calloc(" not in code
    assert "realloc(" not in code

assert "sizeof(RhythmPhrasePlan) <= 640" in realizer_h
assert "kMaxPhraseBars" in realizer_cpp
assert "kStepsPerBar" in realizer_cpp
assert "GenerationDomain::RhythmIdentity" in realizer_cpp
assert "GenerationDomain::BarEvolution" in realizer_cpp
assert "deriveVariationSeed" in realizer_cpp
assert "hardRelationshipsSatisfied" in realizer_cpp
assert "planRespectsProtectedSpace" in realizer_cpp
assert "planRespectsLaneBounds" in realizer_cpp

# Identity derivation must not include RealizationLevel before the separate
# variation-domain derivation.
identity_function = context_cpp.split("uint32_t deriveGenerationSeed", 1)[1].split(
    "uint32_t deriveVariationSeed", 1
)[0]
assert "RealizationLevel" not in identity_function
assert "variationDomain(level)" in context_cpp

print("Groove Vocabulary Stage 2 source regressions: OK")
