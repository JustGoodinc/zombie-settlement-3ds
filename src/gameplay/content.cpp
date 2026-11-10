#include "content.h"
#include "../world/world.h"

#define NOW {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0}
//                         dmg acc pel  cd  mag ammo           dur noise rng cone st reach auto shake reload
#define GUN(d, a, p, c, m, am, du, n, r, co, au, sh, rl) {d, a, p, c, m, am, du, n, r, co, 0, 0, au, sh, rl}
#define MEL(d, a, c, du, st, re) {d, a, 1, c, 0, 0, du, 0, re, 60, st, re, false, 0, 0}

const ItemDef ITEMS[IT_COUNT] = {
    {"", "", IC_NONE, 1, 1, 1, NOW, 0, 0, 0},
    {"Pistol", "9mm handgun. Reliable, lightweight.", IC_FIREARM, 2, 1, 1, GUN(40, 0, 1, 350, 12, IT_AMMO_9MM, 400, 260, 200, 12, false, 1, 1200), RES_AMMO, 0, 0},
    {"Revolver", ".38 six-shooter. Hits hard, slow reload.", IC_FIREARM, 2, 1, 1, GUN(60, 5, 1, 600, 6, IT_AMMO_38, 500, 300, 220, 11, false, 2, 1800), RES_AMMO, 0, 0},
    {"Shotgun", "Pump action. Devastating up close.", IC_FIREARM, 4, 1, 1, GUN(22, -10, 6, 900, 5, IT_AMMO_SHELL, 300, 380, 130, 18, false, 3, 2200), RES_AMMO, 0, 0},
    {"Hunting Rifle", "Bolt action with scope. Long range.", IC_FIREARM, 5, 1, 1, GUN(95, 15, 1, 1200, 5, IT_AMMO_RIFLE, 350, 420, 320, 9, false, 3, 2400), RES_AMMO, 0, 0},
    {"SMG", "Automatic 9mm. Hungry for ammo.", IC_FIREARM, 3, 1, 1, GUN(28, -8, 1, 110, 30, IT_AMMO_9MM, 600, 300, 180, 13, true, 1, 1600), RES_AMMO, 0, 0},
    {"Knife", "Never wears out. Quiet.", IC_MELEE, 2, 1, 1, MEL(34, 10, 380, 0, 8, 18), RES_MAT, 0, 0},
    {"Baseball Bat", "Heavy swings. Wears out.", IC_MELEE, 4, 1, 1, MEL(48, 0, 650, 60, 14, 22), RES_MAT, 1, 0},
    {"Crowbar", "Sturdy steel. Wears slowly.", IC_MELEE, 3, 1, 1, MEL(44, 5, 550, 120, 12, 20), RES_MAT, 2, 0},
    {"Machete", "Long blade. Brutal.", IC_MELEE, 3, 1, 1, MEL(60, 0, 600, 90, 13, 22), RES_MAT, 1, 0},
    {"9mm Rounds", "Pistol and SMG ammunition.", IC_AMMO, 1, 1, 60, NOW, RES_AMMO, 1, 0},
    {".38 Rounds", "Revolver ammunition.", IC_AMMO, 1, 1, 36, NOW, RES_AMMO, 1, 0},
    {"Shells", "12 gauge shotgun shells.", IC_AMMO, 1, 1, 24, NOW, RES_AMMO, 1, 0},
    {"Rifle Rounds", "7.62 hunting rounds.", IC_AMMO, 1, 1, 20, NOW, RES_AMMO, 1, 0},
    {"Bandage", "Stops bleeding. +20 health.", IC_MEDICAL, 1, 1, 3, NOW, RES_MED, 1, 20},
    {"Painkillers", "Dulls injuries. +12 health.", IC_MEDICAL, 1, 1, 3, NOW, RES_MED, 1, 12},
    {"Antibiotics", "Cures early infection in the field.", IC_MEDICAL, 1, 1, 2, NOW, RES_MED, 2, 0},
    {"First Aid Kit", "Full field dressing. +50 health.", IC_MEDICAL, 2, 2, 1, NOW, RES_MED, 4, 50},
    {"Canned Food", "Feeds the settlement.", IC_FOOD, 1, 1, 4, NOW, RES_FOOD, 2, 0},
    {"Ration Pack", "Military meal. Very filling.", IC_FOOD, 1, 2, 2, NOW, RES_FOOD, 4, 0},
    {"Scrap Metal", "Building and repair material.", IC_MATERIAL, 2, 1, 3, NOW, RES_MAT, 3, 0},
    {"Planks", "Lumber for construction.", IC_MATERIAL, 3, 1, 2, NOW, RES_MAT, 5, 0},
    {"Toolbox", "Tools and fittings.", IC_MATERIAL, 2, 1, 1, NOW, RES_MAT, 6, 0},
    {"Fuel Can", "Fuel for generators and vehicles.", IC_FUEL, 2, 2, 1, NOW, RES_FUEL, 5, 0},
    {"Keycard", "Opens a locked door somewhere in this building.", IC_KEY, 1, 1, 1, NOW, 0, 0, 0},
    {"Cell Key", "Opens the holding cells.", IC_KEY, 1, 1, 1, NOW, 0, 0, 0},
    {"Medium Backpack", "Use to upgrade to an 8x5 bag.", IC_BACKPACK, 2, 2, 1, NOW, 0, 0, 0},
    {"Large Backpack", "Use to upgrade to a 10x6 bag.", IC_BACKPACK, 2, 2, 1, NOW, 0, 0, 0},
    {"Generator", "Install at the settlement: powers the kitchen and workshop.", IC_EQUIPMENT, 3, 3, 1, NOW, 0, 0, 0},
    {"Medical Scanner", "Install at the infirmary: better treatment.", IC_EQUIPMENT, 3, 2, 1, NOW, 0, 0, 0},
};

//                                      food med ammo mat fuel
const BuildingDef BUILDINGS[B_COUNT] = {
    {"Infirmary", "Treats injuries and infections. Allows medical screening and quarantine.", 3,
     {{0, 4, 0, 20, 0}, {0, 8, 0, 35, 5}, {0, 12, 0, 50, 10}, {0}},
     {"", "Treat injuries, early infections, quarantine", "Treat advanced infections", "Treat critical infections"}},
    {"Workshop", "Repair weapons, craft equipment, and upgrade gear.", 3,
     {{0, 0, 0, 20, 2}, {0, 0, 0, 35, 6}, {0, 0, 0, 55, 10}, {0}},
     {"", "Repair weapons", "Weapon upgrades (+accuracy)", "Cheaper repairs"}},
    {"Storage", "Store supplies and equipment. Raises stash capacity.", 3,
     {{0, 0, 0, 10, 0}, {0, 0, 0, 25, 0}, {0, 0, 0, 40, 4}, {0}},
     {"", "Stash: 16 items", "Stash: 24 items", "Stash: 32 items"}},
    {"Kitchen/Garden", "Produce food and reduce shortages.", 3,
     {{0, 0, 0, 15, 0}, {5, 0, 0, 30, 4}, {10, 0, 0, 45, 8}, {0}},
     {"", "+2 food per farmer each day", "+3 food per farmer", "+4 food per farmer"}},
    {"Living Quarters", "Beds and shelter. Raises the population cap.", 4,
     {{0, 0, 0, 10, 0}, {0, 0, 0, 30, 0}, {4, 0, 0, 45, 4}, {8, 0, 0, 60, 8}},
     {"", "Room for 8", "Room for 12", "Room for 16"}},
    {"Defenses", "Walls, gate and watchtowers. Guards defend the settlement.", 3,
     {{0, 0, 0, 15, 0}, {0, 0, 4, 35, 0}, {0, 0, 8, 60, 6}, {0}},
     {"", "Palisade walls", "Watchtower (+security)", "Reinforced walls (+security)"}},
};

const LocationDef LOCATIONS[LOC_COUNT] = {
    {"Police Station", "Weapons, Ammo, Medicine", "Medium", MAP_POLICE},
    {"Hospital", "Medicine, Equipment", "High", MAP_HOSPITAL},
    {"Supermarket", "Food, Fuel", "Medium", MAP_MARKET},
    {"Train Yard", "Materials, Fuel, Tools (rail + docks)", "Medium", MAP_YARD},
    {"Neighborhood", "Mixed supplies, Food", "Low", MAP_HOMES},
};

const char* SKILL_NAMES[SK_COUNT] = {"Shooting", "Melee", "Medical", "Mechanical", "Farming", "Guarding"};
const char* JOB_NAMES[JOB_COUNT] = {"Idle", "Guard", "Medic", "Mechanic", "Farmer", "Worker"};
const char* RES_NAMES[RES_COUNT] = {"Food", "Medicine", "Ammo", "Materials", "Fuel"};
