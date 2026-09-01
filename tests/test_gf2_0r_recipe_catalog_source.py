#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/composition/generation_profile.h").read_text()
PROFILE = (ROOT / "src/generation/composition/generation_profile.cpp").read_text()
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()

for query in ("availableRecipeCount", "availableRecipeAt", "isRecipeAvailable"):
    assert query in HEADER, f"missing public Recipe catalog query: {query}"
    assert query in PROFILE, f"missing profile-owned Recipe query: {query}"
    assert query in GENRE_PAGE, f"GenrePage does not consume Recipe query: {query}"

for private_storage in ("struct ProfileDefinition", "kProfiles[]"):
    assert private_storage not in HEADER, \
        f"private profile storage leaked through public API: {private_storage}"

for duplicate_owner in (
    "RecipeChoices",
    "kBaseOnlyRecipes",
    "kAcidRecipes",
    "kRaveRecipes",
    "kDubRecipes",
    "kBreakRecipes",
    "kLoFiRecipes",
    "kHipHopRecipes",
    "recipeChoicesForGenre",
):
    assert duplicate_owner not in GENRE_PAGE, \
        f"duplicate UI Recipe membership owner remains: {duplicate_owner}"

for forbidden_runtime_catalog in ("std::vector", "new Recipe", "mutable global"):
    assert forbidden_runtime_catalog not in HEADER + PROFILE, \
        f"unbounded/runtime Recipe catalog introduced: {forbidden_runtime_catalog}"

print("GF2-0R Recipe catalog ownership source gate: PASS")
