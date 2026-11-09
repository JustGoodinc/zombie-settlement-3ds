#pragma once
// Tile maps: handcrafted ASCII layouts (maps.cpp) parsed into tiles, barriers,
// props and spawns. Collision, line of sight and BFS flow fields live here.
#include <stdint.h>

enum Tile : uint8_t {
    T_GRASS, T_ASPHALT, T_LINE, T_DIRT, T_TILE, T_WOOD, T_CARPET, T_CONCRETE,
    T_WALL, T_WINDOW, T_BARS, T_FENCE, T_EXTRACT, T_GARDEN, T_PALISADE, T_DOOR, T_COUNT
};

enum BarrierKind : uint8_t { BK_DOOR, BK_KEYDOOR, BK_CELLDOOR, BK_WINDOW, BK_GATE };
enum BarrierState : uint8_t { BS_OPEN, BS_CLOSED, BS_LOCKED, BS_BROKEN };

struct Barrier {
    uint8_t x, y, kind, state;
    int16_t hp, maxHp;
    float anim;      // 0..1 opening animation (render only)
    float shake;     // hit wobble (render only)
};

enum PropType : uint8_t {
    PR_DESK, PR_FILING, PR_LOCKER, PR_CRATE, PR_MEDCAB, PR_SHELF, PR_CAR, PR_WRECK, PR_TREE, PR_BENCH, PR_COT,
    PR_CHAIR, PR_PLANT, PR_COOLER, PR_FIRE, PR_RADIO, PR_GENERATOR, PR_WORKBENCH, PR_LAMP, PR_DECOR_CRATE,
    PR_TENT, PR_BARREL, PR_SANDBAG, PR_LOGS, PR_TORCH, PR_CONE,
    PR_PLOT, PR_COUNT
};

struct Prop {
    uint8_t type, tx, ty, tw, th;
    int8_t container;   // index into the level's container list, -1 = decoration
    uint8_t special;    // forced loot / plot building id
    bool solid;
};

struct Spawn { char kind; uint8_t x, y; };

struct Map {
    enum { MAXW = 64, MAXH = 56, MAXBAR = 64, MAXPROP = 160, MAXSPAWN = 64 };
    int w = 0, h = 0, id = 0;
    uint8_t t[MAXW * MAXH];
    uint8_t flags[MAXW * MAXH];   // F_* bits (recomputed by map_refresh)
    int8_t barAt[MAXW * MAXH];
    Barrier bar[MAXBAR]; int nbar = 0;
    Prop prop[MAXPROP]; int nprop = 0;
    int8_t propAt[MAXW * MAXH];
    Spawn spawn[MAXSPAWN]; int nspawn = 0;
    int ncontainers = 0;
    int spawnX = 0, spawnY = 0;   // player spawn tile
    int exitX = 0, exitY = 0;     // centre of extraction / gate

    int idx(int x, int y) const { return y * w + x; }
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
};

enum { F_BLOCK = 1, F_OPAQUE = 2, F_INDOOR = 4, F_BARRIER = 8 };
enum { MAP_POLICE = 0, MAP_SETTLEMENT = 1, MAP_COUNT = 2 };

void map_load(Map& m, int which);
void map_refresh(Map& m);
bool map_blocked(const Map& m, int tx, int ty);    // movement
bool map_opaque(const Map& m, int tx, int ty);     // sight
bool map_los(const Map& m, float x0, float y0, float x1, float y1);
// Moves a circle with wall sliding. Returns index of a barrier that blocked it, or -1.
int map_move(const Map& m, float& x, float& y, float dx, float dy, float r);
// BFS distance field from (tx,ty). throughBarriers: closed doors/windows count as passable (zombies attack them).
void map_flow(const Map& m, int tx, int ty, uint16_t* dist, bool throughBarriers);
// Steps one tile downhill in a flow field; returns false if no better neighbour.
bool map_flowStep(const Map& m, const uint16_t* dist, int tx, int ty, int& nx, int& ny);
const char* map_name(int which);
