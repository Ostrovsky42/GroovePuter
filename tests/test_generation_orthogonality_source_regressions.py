import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATION = ROOT / "src/generation"

# Composition and migration may map Genre to generic vocabulary IDs. The
# vocabulary/realizer layers below must remain unaware of GenreSettings and
# GenerativeMode, which mechanically prevents Genre dispatch in those layers.
GENRE_BLIND_LAYERS = (
    "feel",
    "materialization",
    "phrase",
    "rhythm",
    "roles",
    "shadow",
)

GENRE_TERMS = (
    "acid",
    "breaks",
    "chill",
    "darksynth",
    "dnb",
    "drumandbass",
    "dub",
    "electro",
    "funk",
    "garage",
    "hiphop",
    "house",
    "jungle",
    "latin",
    "lofi",
    "outrun",
    "rave",
    "reggae",
    "synthwave",
    "techno",
    "triphop",
)


def compact(identifier: str) -> str:
    return re.sub(r"[^a-z0-9]", "", identifier.lower())


def contains_genre_name(identifier: str) -> bool:
    normalized = compact(identifier)
    return any(term in normalized for term in GENRE_TERMS)


sources = sorted(GENERATION.rglob("*.h")) + sorted(GENERATION.rglob("*.cpp"))
assert sources, "src/generation source set is empty"

for source in sources:
    assert not contains_genre_name(source.stem), (
        f"genre-named generation file is forbidden: {source.relative_to(ROOT)}"
    )

    text = source.read_text()
    type_names = re.findall(
        r"\b(?:class|struct|enum(?:\s+class)?)\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)[^;{]*\{",
        text,
    )
    type_names += re.findall(
        r"\busing\s+([A-Za-z_][A-Za-z0-9_]*)\s*=",
        text,
    )
    function_names = re.findall(
        r"(?:^|\n)\s*(?:constexpr\s+|inline\s+|static\s+)*"
        r"[A-Za-z_][A-Za-z0-9_:<>,*&\s]*\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*\(",
        text,
    )
    for identifier in type_names + function_names:
        assert not contains_genre_name(identifier), (
            "genre-named type/function is forbidden in generation layers: "
            f"{source.relative_to(ROOT)}::{identifier}"
        )

for layer in GENRE_BLIND_LAYERS:
    for source in sorted((GENERATION / layer).rglob("*.h")) + sorted(
        (GENERATION / layer).rglob("*.cpp")
    ):
        text = source.read_text()
        code = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        code = re.sub(r"//.*", "", code)
        for forbidden in ("GenreSettings", "GenerativeMode"):
            assert forbidden not in code, (
                f"{source.relative_to(ROOT)} leaked {forbidden}; "
                "Genre dispatch belongs only in composition/migration"
            )
        assert not re.search(
            r"\bswitch\s*\([^)]*\b(?:genre|recipe)\b",
            code,
            flags=re.IGNORECASE,
        ), (
            f"{source.relative_to(ROOT)} contains Genre/recipe dispatch; "
            "dispatch belongs only in composition/migration"
        )
