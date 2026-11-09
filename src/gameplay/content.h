#pragma once
// Shared content database (items, weapons, buildings, locations). Identical on PC and 3DS.
#include <stdint.h>

enum ItemId : uint8_t {
    IT_NONE, IT_PISTOL, IT_REVOLVER, IT_SHOTGUN, IT_RIFLE, IT_SMG, IT_KNIFE, IT_BAT, IT_CROWBAR, IT_MACHETE,
    IT_AMMO_9MM, IT_AMMO_38, IT_AMMO_SHELL, IT_AMMO_RIFLE, IT_BANDAGE, IT_PILLS, IT_ANTIBIOTICS, IT_MEDKIT,
    IT_CANNED, IT_MRE, IT_SCRAP, IT_PLANKS, IT_TOOLS, IT_FUEL, IT_KEYCARD, IT_CELLKEY, IT_PACK_M, IT_PACK_L,
    IT_GENERATOR, IT_MEDMACHINE, IT_COUNT
};

enum ItemCat : uint8_t { IC_NONE, IC_FIREARM, IC_MELEE, IC_AMMO, IC_MEDICAL, IC_FOOD, IC_MATERIAL, IC_FUEL, IC_KEY, IC_BACKPACK, IC_EQUIPMENT };
enum Res : uint8_t { RES_FOOD, RES_MED, RES_AMMO, RES_MAT, RES_FUEL, RES_COUNT };

struct WeaponStats {
    uint8_t dmg;        // body damage per hit (per pellet for shotguns)
    int8_t acc;         // accuracy bonus in percent (reduces miss weight)
    uint8_t pellets;    // projectiles per shot (1 = single)
    uint16_t cooldownMs;
    uint8_t mag;        // magazine size (0 = melee)
    uint8_t ammo;       // ItemId of ammunition
    uint16_t dur;       // max durability, 0 = infinite (knife)
    uint16_t noise;     // gunshot hearing radius (px)
    uint16_t range;     // px
    uint8_t cone;       // aim-assist half angle (degrees)
    uint8_t stamina;    // melee stamina cost
    uint8_t reach;      // melee reach (px)
    bool automatic;     // hold to fire
    uint8_t shake;      // camera shake strength
    uint16_t reloadMs;
};

struct ItemDef {
    const char* name;
    const char* desc;
    ItemCat cat;
    uint8_t w, h;          // grid cells
    uint16_t maxStack;
    WeaponStats wpn;
    uint8_t res;           // resource this converts to at the settlement
    uint8_t resAmt;        // per item (ammo: per 6 rounds)
    uint8_t heal;          // hp restored when used
};

extern const ItemDef ITEMS[IT_COUNT];
inline const ItemDef& idef(int id) { return ITEMS[id]; }

enum BuildingId : uint8_t { B_INFIRMARY, B_WORKSHOP, B_STORAGE, B_KITCHEN, B_QUARTERS, B_DEFENSES, B_COUNT };
struct BuildingDef {
    const char* name;
    const char* desc;
    uint8_t maxLevel;
    uint8_t cost[4][RES_COUNT];   // cost to reach level 1..maxLevel (index level-1)
    const char* perk[4];          // benefit description per level
};
extern const BuildingDef BUILDINGS[B_COUNT];

enum { LOC_COUNT = 7 };
struct LocationDef {
    const char* name;
    const char* loot;
    const char* threat;
    int8_t map;        // index into world maps, -1 = not yet surveyed
};
extern const LocationDef LOCATIONS[LOC_COUNT];

enum Skill : uint8_t { SK_SHOOT, SK_MELEE, SK_MEDIC, SK_MECH, SK_FARM, SK_GUARD, SK_COUNT };
extern const char* SKILL_NAMES[SK_COUNT];
enum Job : uint8_t { JOB_NONE, JOB_GUARD, JOB_MEDIC, JOB_MECHANIC, JOB_FARMER, JOB_WORKER, JOB_COUNT };
extern const char* JOB_NAMES[JOB_COUNT];
extern const char* RES_NAMES[RES_COUNT];
