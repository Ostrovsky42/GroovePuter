from pathlib import Path

path = Path("tests/test_rhythm_stage2_atlas_realization.cpp")
text = path.read_text(encoding="utf-8")

replacements = [
    (
        "AtlasGrammarFixture rollingAcidGrammar() {\n  AtlasGrammarFixture fixture{};\n",
        "void configureRollingAcidGrammar(AtlasGrammarFixture& fixture) {\n",
    ),
    (
        "  finishGrammar(fixture);\n  return fixture;\n}\n\nAtlasGrammarFixture classicTwoStepGrammar() {\n  AtlasGrammarFixture fixture{};\n",
        "  finishGrammar(fixture);\n}\n\nvoid configureClassicTwoStepGrammar(AtlasGrammarFixture& fixture) {\n",
    ),
    (
        "  finishGrammar(fixture);\n  return fixture;\n}\n\nAtlasGrammarFixture deepChordGrammar() {\n  AtlasGrammarFixture fixture{};\n",
        "  finishGrammar(fixture);\n}\n\nvoid configureDeepChordGrammar(AtlasGrammarFixture& fixture) {\n",
    ),
    (
        "  finishGrammar(fixture);\n  return fixture;\n}\n\nuint64_t structuralSignature",
        "  finishGrammar(fixture);\n}\n\nuint64_t structuralSignature",
    ),
    (
        "int main() {\n  const AtlasGrammarFixture acid = rollingAcidGrammar();\n  const AtlasGrammarFixture ukg = classicTwoStepGrammar();\n  const AtlasGrammarFixture dub = deepChordGrammar();\n\n  assertRealizerAcceptsGrammar(acid, false);\n  assertRealizerAcceptsGrammar(ukg, true);\n  assertRealizerAcceptsGrammar(dub, true);\n",
        "int main() {\n  AtlasGrammarFixture acid{};\n  AtlasGrammarFixture ukg{};\n  AtlasGrammarFixture dub{};\n  configureRollingAcidGrammar(acid);\n  configureClassicTwoStepGrammar(ukg);\n  configureDeepChordGrammar(dub);\n\n  assertRealizerAcceptsGrammar(acid, false);\n  assertRealizerAcceptsGrammar(ukg, true);\n  assertRealizerAcceptsGrammar(dub, true);\n",
    ),
]

for old, new in replacements:
    if text.count(old) != 1:
        raise SystemExit(f"expected one Atlas fixture patch target, found {text.count(old)}")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
