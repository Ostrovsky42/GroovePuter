#!/usr/bin/env python3
"""Make Genre page Apply recipe-aware and BPM-consistent."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/ui/pages/genre_page.cpp"


def replace_once(text: str, old: str, new: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one match, found {count}: {old[:140]}")
    return text.replace(old, new, 1)


def main() -> None:
    text = PATH.read_text(encoding="utf-8")

    text = replace_once(
        text,
        """const char* linkStateShort(const MiniAcid& mini) {
    const GrooveboxMode expected =
        GenreManager::grooveboxModeForGenerative(mini.genreManager().generativeMode());
    return (mini.grooveboxMode() == expected) ? "GEN" : "OVR";
}
""",
        """const char* linkStateShort(const MiniAcid& mini) {
    const GrooveboxMode expected =
        GenreManager::grooveboxModeForRecipe(
            mini.genreManager().recipe(), mini.genreManager().generativeMode());
    return (mini.grooveboxMode() == expected) ? "GEN" : "OVR";
}
""",
    )

    text = replace_once(
        text,
        """        mini_acid_.setGrooveboxMode(
            GenreManager::grooveboxModeForGenerative(static_cast<GenerativeMode>(genreIndex_)));
        
        // Apply base timbre, reset bias tracking, then apply texture as delta from 0
""",
        """        mini_acid_.setGrooveboxMode(
            GenreManager::grooveboxModeForRecipe(
                static_cast<GenreRecipeId>(recipeIndex_),
                static_cast<GenerativeMode>(genreIndex_)));

        // Set the requested tempo before generation so BPM-dependent density
        // and articulation use the final corridor. Atlas recipes may refine the
        // generic genre hint to their own reviewed BPM during regeneration.
        if (doApplyTempo) mini_acid_.setBpm(static_cast<float>(targetBpm));

        // Apply base timbre, reset bias tracking, then apply texture as delta from 0
""",
    )

    text = replace_once(
        text,
        """        if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
        if (doApplyTempo) mini_acid_.setBpm(static_cast<float>(targetBpm));
""",
        """        if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
""",
    )

    PATH.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
