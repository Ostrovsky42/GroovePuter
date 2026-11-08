#ifndef GROOVEPUTER_GENERATION_RHYTHM_BROWSER_PRESETS_H
#define GROOVEPUTER_GENERATION_RHYTHM_BROWSER_PRESETS_H

// Hand-authored drum presets for the RHYTHMS browser (first iteration).
// Data reuses the Atlas Event/Pattern/Recipe layout but is NOT part of the
// generated Atlas table: these ids never reach AtlasRuntime, genre recipes or
// song generation. All tables are constexpr, so they live in flash.
//
// Drum targets: 0 KICK, 1 SNARE, 2 CLOSED HAT, 3 OPEN HAT, 4 MID TOM (perc 1),
// 5 HIGH TOM (perc 2 / conga), 6 RIM, 7 CLAP. Steps are 16ths, 0..15.
// Velocity ~110-122 = normal hit, ~44-60 = ghost.

#include "rhythm_browser_catalog.h"
#include "../../generated/atlas_runtime_types.generated.h"

namespace RhythmBrowser {
namespace Presets {

using AtlasGenerated::Event;
using AtlasGenerated::Pattern;
using AtlasGenerated::Recipe;

// {target, step, note, velocity, timing, probability, flags}
#define GP_HIT(target, step, velocity) {target, step, -1, velocity, 0, 100, AtlasGenerated::kActive}
#define GP_LATE(target, step, velocity, timing) {target, step, -1, velocity, timing, 100, AtlasGenerated::kActive}

// --- Chicago Jack 909 (HOUSE, 124) ------------------------------------------
// KICK X... X... X... X... / CLAP ....X... ....X... / OHAT ..X. ..X. ..X. ..X.
// CHAT X... X... X... X...
inline constexpr Event kChicagoJack909A[] = {
    GP_HIT(0, 0, 120), GP_HIT(0, 4, 116), GP_HIT(0, 8, 120), GP_HIT(0, 12, 116),
    GP_HIT(7, 4, 108), GP_HIT(7, 12, 112),
    GP_HIT(3, 2, 88), GP_HIT(3, 6, 84), GP_HIT(3, 10, 88), GP_HIT(3, 14, 92),
    GP_HIT(2, 0, 72), GP_HIT(2, 4, 68), GP_HIT(2, 8, 72), GP_HIT(2, 12, 68),
};
inline constexpr Event kChicagoJack909B[] = {
    GP_HIT(0, 0, 120), GP_HIT(0, 4, 116), GP_HIT(0, 8, 120), GP_HIT(0, 12, 116),
    GP_HIT(7, 4, 108), GP_HIT(7, 12, 112), GP_HIT(7, 15, 54),
    GP_HIT(3, 2, 88), GP_HIT(3, 6, 84), GP_HIT(3, 10, 88), GP_HIT(3, 14, 92),
    GP_HIT(2, 0, 72), GP_HIT(2, 3, 46), GP_HIT(2, 4, 68), GP_HIT(2, 7, 46),
    GP_HIT(2, 8, 72), GP_HIT(2, 11, 46), GP_HIT(2, 12, 68), GP_HIT(2, 15, 50),
    GP_HIT(6, 10, 60),
};

// --- Dirty Tech House (HOUSE, 126) ------------------------------------------
// Dense kick with a ghost push, 16th shuffle hats, syncopated rim/tom perc.
inline constexpr Event kDirtyTechHouseA[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 4, 118), GP_HIT(0, 8, 122), GP_HIT(0, 11, 60), GP_HIT(0, 12, 118),
    GP_HIT(7, 4, 104), GP_HIT(7, 12, 108),
    GP_HIT(3, 2, 80), GP_HIT(3, 6, 76), GP_HIT(3, 10, 80), GP_HIT(3, 14, 84),
    GP_HIT(2, 1, 46), GP_HIT(2, 3, 54), GP_HIT(2, 5, 46), GP_HIT(2, 7, 54),
    GP_HIT(2, 9, 46), GP_HIT(2, 11, 54), GP_HIT(2, 13, 46), GP_HIT(2, 15, 58),
    GP_HIT(6, 3, 72), GP_HIT(6, 6, 58), GP_HIT(6, 11, 74),
    GP_HIT(4, 7, 64), GP_HIT(5, 10, 58), GP_HIT(5, 14, 66),
};
inline constexpr Event kDirtyTechHouseB[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 4, 118), GP_HIT(0, 7, 56), GP_HIT(0, 8, 122),
    GP_HIT(0, 11, 62), GP_HIT(0, 12, 118),
    GP_HIT(7, 4, 104), GP_HIT(7, 12, 108), GP_HIT(1, 15, 52),
    GP_HIT(3, 2, 80), GP_HIT(3, 6, 76), GP_HIT(3, 10, 80), GP_HIT(3, 14, 84),
    GP_HIT(2, 1, 46), GP_HIT(2, 3, 54), GP_HIT(2, 5, 46), GP_HIT(2, 7, 54),
    GP_HIT(2, 9, 46), GP_HIT(2, 11, 54), GP_HIT(2, 13, 46), GP_HIT(2, 15, 58),
    GP_HIT(6, 3, 72), GP_HIT(6, 6, 58), GP_HIT(6, 9, 56), GP_HIT(6, 11, 74), GP_HIT(6, 13, 60),
    GP_HIT(4, 7, 64), GP_HIT(5, 10, 58), GP_HIT(4, 14, 66),
};

// --- Detroit 909 Roller (TECHNO, 136) ---------------------------------------
// KICK X... X... X... X... / RIDE ..X. ..X. ..X. ..X. (open hat stands in:
// the drum kit has no ride lane) / CHAT X.X. X.X. X.X. X.X. / PERC ...x .... ...x ....
inline constexpr Event kDetroit909RollerA[] = {
    GP_HIT(0, 0, 120), GP_HIT(0, 4, 118), GP_HIT(0, 8, 120), GP_HIT(0, 12, 118),
    GP_HIT(3, 2, 82), GP_HIT(3, 6, 78), GP_HIT(3, 10, 82), GP_HIT(3, 14, 86),
    GP_HIT(2, 0, 74), GP_HIT(2, 2, 58), GP_HIT(2, 4, 74), GP_HIT(2, 6, 58),
    GP_HIT(2, 8, 74), GP_HIT(2, 10, 58), GP_HIT(2, 12, 74), GP_HIT(2, 14, 58),
    GP_HIT(4, 3, 52), GP_HIT(4, 11, 56),
};
inline constexpr Event kDetroit909RollerB[] = {
    GP_HIT(0, 0, 120), GP_HIT(0, 4, 118), GP_HIT(0, 8, 120), GP_HIT(0, 12, 118),
    GP_HIT(7, 4, 96), GP_HIT(7, 12, 100),
    GP_HIT(3, 2, 82), GP_HIT(3, 6, 78), GP_HIT(3, 10, 82), GP_HIT(3, 14, 86),
    GP_HIT(2, 0, 74), GP_HIT(2, 2, 58), GP_HIT(2, 4, 74), GP_HIT(2, 6, 58),
    GP_HIT(2, 8, 74), GP_HIT(2, 10, 58), GP_HIT(2, 12, 74), GP_HIT(2, 14, 58),
    GP_HIT(4, 3, 52), GP_HIT(4, 7, 48), GP_HIT(4, 11, 56), GP_HIT(5, 15, 50),
};

// --- Hardgroove Tribal (TECHNO, 138) ----------------------------------------
// KICK X... X... X... X... / OHAT ..X. ..X. ..X. ..X.
// CONGA ...x ..X. .x.. ..X. (high tom) / RIM .... .x.. .... .x..
inline constexpr Event kHardgrooveTribalA[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 4, 120), GP_HIT(0, 8, 122), GP_HIT(0, 12, 120),
    GP_HIT(3, 2, 86), GP_HIT(3, 6, 82), GP_HIT(3, 10, 86), GP_HIT(3, 14, 90),
    GP_HIT(5, 3, 54), GP_HIT(5, 6, 104), GP_HIT(5, 9, 56), GP_HIT(5, 14, 108),
    GP_HIT(6, 5, 56), GP_HIT(6, 13, 58),
};
inline constexpr Event kHardgrooveTribalB[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 4, 120), GP_HIT(0, 8, 122), GP_HIT(0, 12, 120),
    GP_HIT(7, 12, 90),
    GP_HIT(3, 2, 86), GP_HIT(3, 6, 82), GP_HIT(3, 10, 86), GP_HIT(3, 14, 90),
    GP_HIT(5, 3, 54), GP_HIT(5, 6, 104), GP_HIT(5, 9, 56), GP_HIT(5, 14, 108),
    GP_HIT(4, 7, 62), GP_HIT(4, 11, 78), GP_HIT(4, 15, 58),
    GP_HIT(6, 5, 56), GP_HIT(6, 13, 58),
};

// --- Boom Bap Headnod (HIP-HOP, 92) -----------------------------------------
// KICK X... ...x ..X. .... / SNARE ....X... ....X... / CHAT eighths, offbeats
// lighter and a touch late for the nod.
inline constexpr Event kBoomBapHeadnodA[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 7, 72), GP_HIT(0, 10, 112),
    GP_HIT(1, 4, 118), GP_HIT(1, 12, 120), GP_HIT(1, 15, 46),
    GP_HIT(2, 0, 82), GP_LATE(2, 2, 56, 2), GP_HIT(2, 4, 80), GP_LATE(2, 6, 56, 2),
    GP_HIT(2, 8, 82), GP_LATE(2, 10, 56, 2), GP_HIT(2, 12, 80), GP_LATE(2, 14, 58, 2),
};
inline constexpr Event kBoomBapHeadnodB[] = {
    GP_HIT(0, 0, 122), GP_HIT(0, 3, 64), GP_HIT(0, 10, 112), GP_HIT(0, 11, 82),
    GP_HIT(1, 4, 118), GP_HIT(1, 9, 44), GP_HIT(1, 12, 120),
    GP_HIT(2, 0, 82), GP_LATE(2, 2, 56, 2), GP_HIT(2, 4, 80), GP_LATE(2, 6, 56, 2),
    GP_HIT(2, 8, 82), GP_LATE(2, 10, 56, 2), GP_HIT(2, 12, 80),
    GP_LATE(3, 14, 70, 2),
};

#undef GP_HIT
#undef GP_LATE

#define GP_PATTERN(id, slot, function, events) \
  {id, slot, function, events, static_cast<uint16_t>(sizeof(events) / sizeof(events[0]))}

inline constexpr Pattern kChicagoJack909[] = {
    GP_PATTERN("PRESET_CHICAGO_JACK_909_V1", "V1", "BASE", kChicagoJack909A),
    GP_PATTERN("PRESET_CHICAGO_JACK_909_V2", "V2", "DEVELOPMENT", kChicagoJack909B),
};
inline constexpr Pattern kDirtyTechHouse[] = {
    GP_PATTERN("PRESET_DIRTY_TECH_HOUSE_V1", "V1", "BASE", kDirtyTechHouseA),
    GP_PATTERN("PRESET_DIRTY_TECH_HOUSE_V2", "V2", "DEVELOPMENT", kDirtyTechHouseB),
};
inline constexpr Pattern kDetroit909Roller[] = {
    GP_PATTERN("PRESET_DETROIT_909_ROLLER_V1", "V1", "BASE", kDetroit909RollerA),
    GP_PATTERN("PRESET_DETROIT_909_ROLLER_V2", "V2", "DEVELOPMENT", kDetroit909RollerB),
};
inline constexpr Pattern kHardgrooveTribal[] = {
    GP_PATTERN("PRESET_HARDGROOVE_TRIBAL_V1", "V1", "BASE", kHardgrooveTribalA),
    GP_PATTERN("PRESET_HARDGROOVE_TRIBAL_V2", "V2", "DEVELOPMENT", kHardgrooveTribalB),
};
inline constexpr Pattern kBoomBapHeadnod[] = {
    GP_PATTERN("PRESET_BOOM_BAP_HEADNOD_V1", "V1", "BASE", kBoomBapHeadnodA),
    GP_PATTERN("PRESET_BOOM_BAP_HEADNOD_V2", "V2", "DEVELOPMENT", kBoomBapHeadnodB),
};

#undef GP_PATTERN

#define GP_RECIPE(id, key, name, bpm, swing, patterns) \
  {id, key, name, bpm, swing, patterns, static_cast<uint8_t>(sizeof(patterns) / sizeof(patterns[0]))}

struct Entry {
  Recipe recipe;
  Group group;
};

// Stable ids are private to Source::Preset (1xx), so they cannot collide with
// vocabulary archetype ids or Atlas runtime recipe ids. swingPercent is
// informational only: the browser does not apply swing (same as Atlas drums).
inline constexpr Entry kEntries[] = {
    {GP_RECIPE(101, "PRESET_CHICAGO_JACK_909", "Chicago Jack 909", 124, 50, kChicagoJack909), Group::House},
    {GP_RECIPE(102, "PRESET_DIRTY_TECH_HOUSE", "Dirty Tech House", 126, 54, kDirtyTechHouse), Group::House},
    {GP_RECIPE(103, "PRESET_DETROIT_909_ROLLER", "Detroit 909 Roller", 136, 50, kDetroit909Roller), Group::Techno},
    {GP_RECIPE(104, "PRESET_HARDGROOVE_TRIBAL", "Hardgroove Tribal", 138, 50, kHardgrooveTribal), Group::Techno},
    {GP_RECIPE(105, "PRESET_BOOM_BAP_HEADNOD", "Boom Bap Headnod", 92, 57, kBoomBapHeadnod), Group::HipHop},
};

#undef GP_RECIPE

inline constexpr unsigned kEntryCount = sizeof(kEntries) / sizeof(kEntries[0]);

}  // namespace Presets
}  // namespace RhythmBrowser

#endif  // GROOVEPUTER_GENERATION_RHYTHM_BROWSER_PRESETS_H
