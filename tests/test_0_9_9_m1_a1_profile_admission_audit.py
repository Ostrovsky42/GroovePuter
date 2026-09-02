from pathlib import Path

root = Path(__file__).resolve().parents[1]
profile = (root / "src/generation/composition/generation_profile.cpp").read_text()
ui = (root / "src/ui/pages/genre_page.cpp").read_text()
table = profile[profile.index("constexpr ProfileDefinition kProfiles[]"):
                profile.index("const ProfileDefinition* definitionFor")]
direct = table.count("profile(")
assert direct == 33, direct
first = profile.index("if (p.generativeMode == settings.generativeMode && p.recipe == settings.recipe)")
fallback = profile.index("if (p.generativeMode == settings.generativeMode && p.recipe == kBaseRecipeId)")
assert first < fallback
assert "recipeChoicesForGenre" in ui and "normalizeRecipeForGenre" in ui
print("A1_DIRECT_PROFILE_OWNER src/generation/composition/generation_profile.cpp:kProfiles")
print("A1_DIRECT_ADMISSION_PAIRS 33")
print("A1_FALLBACK_OBSERVABILITY YES exact_lookup_before_base_same_mode_fallback")
print("A1_UI_POLICY genre_page.cpp:recipeChoicesForGenre/normalizeRecipeForGenre")
print("A1_198_PROVENANCE NOT_REPRODUCED_FROM_EXISTING_OWNER")
