#pragma once
#include "gfx.h"
#include "anim.h"

enum TileArt {
    TA_GRASS, TA_ASPHALT, TA_LINE, TA_DIRT, TA_TILE, TA_WOOD, TA_CARPET, TA_CONCRETE,
    TA_WALL_TOP, TA_WALL_FRONT, TA_WINDOW, TA_WINDOW_BROKEN, TA_BARS, TA_FENCE, TA_EXTRACT,
    TA_GARDEN, TA_PALISADE, TA_DOOR_CLOSED, TA_DOOR_OPEN, TA_DOOR_LOCKED, TA_DOOR_BROKEN,
    TA_CELLDOOR, TA_GATE_CLOSED, TA_GATE_OPEN, TA_CRACK1, TA_CRACK2, TA_RAIL, TA_WATER, TA_COUNT
};

enum SprId {
    S_DESK, S_FILING, S_LOCKER, S_CRATE, S_MEDCAB, S_SHELF, S_CAR, S_TREE, S_TREE2, S_BENCH, S_COT,
    S_CHAIR, S_PLANT, S_COOLER, S_FIRE0, S_FIRE1, S_FIRE2, S_RADIO, S_GENPROP0, S_GENPROP1,
    S_WORKBENCH, S_LAMP, S_BLOOD0, S_BLOOD1, S_BLOOD2, S_BAG, S_BEDROLL, S_WRECK,
    S_ICON_FOOD, S_ICON_MED, S_ICON_AMMO, S_ICON_MAT, S_ICON_FUEL, S_ICON_SHIELD, S_ICON_SKULL,
    S_ICON_BIO, S_ICON_PEOPLE, S_ICON_CLOCK, S_LOGO_ZOMBIE,
    S_TENT, S_BARREL, S_SANDBAG, S_LOGS, S_TORCH0, S_TORCH1, S_CONE,
    S_DEC_TUFT, S_DEC_FLOWER, S_DEC_ROCK, S_DEC_PAPER, S_DEC_DEBRIS, S_DEC_OIL, S_DEC_CRACK, S_DEC_SMEAR, S_DEC_LEAVES,
    S_ICON_HOUSE, S_ICON_TREE, S_ICON_TRAIN,
    S_SHIPBOX0, S_SHIPBOX1, S_SHIPBOX2, S_SHIPBOX3, S_BOXCAR, S_BED, S_SOFA, S_FRIDGE,
    S_COUNT
};

// Zombie looks: who each one used to be (art.cpp draws them, zombie_look() in content.cpp decides who turns up where).
// They differ in appearance only.
enum ZombLookId {
    ZL_OFFICE, ZL_PLAID, ZL_LONG, ZL_HOODIE, ZL_SUIT, ZL_COP, ZL_RIOT, ZL_PRISONER, ZL_DOCTOR, ZL_NURSE, ZL_PATIENT,
    ZL_CLERK, ZL_JOGGER, ZL_HIVIS, ZL_MECHANIC, ZL_ROBE,
    ZL_SURVIVOR,   // + survivor variant: someone from the settlement who turned keeps their own hair and clothes
};
// A zombie's variant is look + ZOMB_LOOKS * version: each look comes in ZOMB_ALTS versions (other skin, wound, hair, top).
enum { SURV_VARIANTS = 8, ZOMB_LOOKS = ZL_SURVIVOR + SURV_VARIANTS, ZOMB_ALTS = 4, ZOMB_VARIANTS = ZOMB_LOOKS * ZOMB_ALTS,
       PORTRAITS = 8, BLD_TYPES = 6, BLD_LEVELS = 4 };

extern Sprite g_tile[TA_COUNT][4];
extern Sprite g_spr[S_COUNT];
enum { ITEM_UNIT = 12 };                  // item icons are drawn at 12 px per inventory cell
extern Sprite g_item[40];                 // item icons (itemart.cpp)
extern Sprite g_held[40];                 // weapons as held in the hand (side view, muzzle right)
extern int8_t g_heldGrip[40][2], g_heldMuzzle[40][2];
extern Sprite g_portrait[PORTRAITS];      // 24x24, drawn at 2x
extern Sprite g_building[BLD_TYPES][BLD_LEVELS];  // 48x48 menu thumbnails, level 0 = empty plot
extern Sprite g_bldWorld[BLD_TYPES][BLD_LEVELS];  // in-world art: own silhouette per type, drawn bottom-centred on the 3x3 plot
struct BldFx { int8_t smokeX, smokeY, flagX, flagY; int8_t nwin; int8_t win[4][4]; };   // chimney, flag pole, lit windows (night glow)
extern BldFx g_bldFx[BLD_TYPES][BLD_LEVELS];

void art_init();
// Character pose lookup: zombie=false -> survivor variants. dir is dir8() index.
// Zombie frames are drawn on first use, so the reference is good until the next art_zombie_flush().
const Sprite& art_char(bool zombie, int variant, int dir, Pose p, bool& flip);
void art_zombie_flush();   // drop the zombie frames drawn so far (called when a level loads, to bound memory)
