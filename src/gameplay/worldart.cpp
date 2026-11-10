// worldart.cpp - the static level layer.
// Every floor, wall, fence, shadow and stain of a map is drawn ONCE when the level loads, into one big
// bitmap (g_bake). Each frame the renderer just copies the visible rows out of it, so detail here costs
// nothing at run time, and permanent marks (blood, casings, bullet holes, bodies) are stamped straight in.
//
// Look: clean two/three-tone pixel clusters instead of per-pixel noise; every room gets its own floor and
// wallpaper colours (hashed from the room), walls are dark cut-away caps with a lit rim and a coloured
// south face, floors darken where they meet a wall, and outdoor ground types blend with ragged edges.
#include "game.h"
#include "../core/art.h"
#include "../core/rng.h"
#include <string.h>
#include <math.h>

px g_bake[Map::MAXW * 16 * Map::MAXH * 16];
int g_bakeW = 0, g_bakeH = 0;

namespace {

const Map* M = nullptr;
uint32_t s_room[Map::MAXW * Map::MAXH];    // hash of the room (same floor type, connected) a tile belongs to
uint32_t s_bld[Map::MAXW * Map::MAXH];     // hash of the building (indoor floor + walls, connected)

inline uint32_t h2(int x, int y, uint32_t s) { return hash32((uint32_t)x * 0x9E3779B1u ^ (uint32_t)y * 0x85EBCA77u ^ s * 0xC2B2AE3Du); }
// smooth value noise 0..255 on a lattice of 2^shift pixels. Integer only, and the lattice values come from a small
// table instead of a hash per corner: this runs about a million times per level, on a 3DS.
uint8_t NT[4096]; bool ntReady = false;
int vnoise(int x, int y, int shift, uint32_t seed) {
    if (!ntReady) { ntReady = true; for (int i = 0; i < 4096; i++) NT[i] = (uint8_t)(hash32((uint32_t)i * 2654435761u + 99u) >> 9); }
    int m = (1 << shift) - 1, fx = x & m, fy = y & m, x0 = (x >> shift) + (int)(seed * 7u), y0 = (y >> shift) + (int)(seed * 13u);
    int a = NT[(x0 & 63) | ((y0 & 63) << 6)], b = NT[((x0 + 1) & 63) | ((y0 & 63) << 6)], c = NT[(x0 & 63) | (((y0 + 1) & 63) << 6)], d = NT[((x0 + 1) & 63) | (((y0 + 1) & 63) << 6)];
    int top = a + (((b - a) * fx) >> shift), bot = c + (((d - c) * fx) >> shift);
    return top + (((bot - top) * fy) >> shift);
}
inline px dk(px c, int lvl) { return gfx_scale(c, lvl); }
inline px lt(px c, int amt) { return gfx_mix(c, rgb(255, 255, 255), amt); }

// ---------------------------------------------------------------- palettes
struct Theme {
    px lino[3][2]; int linoMode;     // 0 = 8 px checker, 1 = 16 px tiles with grout
    px wood[3][4];                   // three plank tones + seam, per room variant
    px carpet[5];
    px faceUp[4], faceLo[4];         // interior wall face: upper colour and wainscot, per room variant
    int faceStripe;                  // 1 = wallpaper stripes (homes)
    px ext[5]; int extStyle;         // exterior wall face per building: 0 brick, 1 panels, 2 stucco, 3 clapboard, 4 corrugated
    px grass[3]; bool mow;
};
const px CAP = rgb(44, 46, 66), CAP2 = rgb(52, 54, 76), CAP_HI = rgb(108, 112, 146), CAP_LO = rgb(24, 24, 38);
const px ASPH[3] = {rgb(78, 80, 96), rgb(66, 68, 84), rgb(92, 94, 110)};
const px CONC[3] = {rgb(166, 164, 156), rgb(140, 138, 134), rgb(186, 184, 174)};
const px CONC_IN[3] = {rgb(136, 138, 140), rgb(116, 118, 122), rgb(152, 154, 154)};
const px DIRT[3] = {rgb(164, 124, 84), rgb(138, 102, 70), rgb(186, 148, 102)};

const Theme THEMES[MAP_COUNT] = {
    {   // police station: civic blue-grey lino, walnut offices, navy carpet, cream walls over slate wainscot, red brick outside
        {{rgb(200, 192, 170), rgb(178, 168, 146)}, {rgb(180, 188, 170), rgb(158, 168, 152)}, {rgb(184, 190, 196), rgb(160, 170, 180)}}, 0,
        {{rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}, {rgb(178, 128, 82), rgb(162, 114, 72), rgb(192, 142, 94), rgb(116, 78, 48)}, {rgb(140, 100, 74), rgb(126, 88, 64), rgb(154, 112, 84), rgb(88, 58, 42)}},
        {rgb(60, 82, 132), rgb(86, 98, 124), rgb(126, 58, 68), rgb(56, 104, 100), rgb(60, 82, 132)},
        {rgb(208, 198, 172), rgb(196, 204, 198), rgb(214, 204, 184), rgb(190, 196, 204)}, {rgb(110, 128, 150), rgb(96, 124, 128), rgb(134, 110, 92), rgb(104, 112, 140)}, 0,
        {rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58)}, 0,
        {rgb(96, 134, 72), rgb(78, 112, 64), rgb(120, 152, 80)}, false,
    },
    {   // settlement: forest clearing (no interiors)
        {{rgb(176, 184, 184), rgb(152, 164, 170)}, {rgb(176, 184, 184), rgb(152, 164, 170)}, {rgb(176, 184, 184), rgb(152, 164, 170)}}, 0,
        {{rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}, {rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}, {rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}},
        {rgb(60, 82, 132), rgb(60, 82, 132), rgb(60, 82, 132), rgb(60, 82, 132), rgb(60, 82, 132)},
        {rgb(208, 198, 172), rgb(208, 198, 172), rgb(208, 198, 172), rgb(208, 198, 172)}, {rgb(110, 128, 150), rgb(110, 128, 150), rgb(110, 128, 150), rgb(110, 128, 150)}, 0,
        {rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58), rgb(150, 74, 58)}, 0,
        {rgb(94, 128, 68), rgb(76, 106, 60), rgb(116, 146, 76)}, false,
    },
    {   // hospital: mint / pale blue / cream floors, white walls over a teal band, white panel exterior
        {{rgb(192, 220, 208), rgb(166, 204, 192)}, {rgb(200, 214, 228), rgb(174, 196, 218)}, {rgb(226, 218, 200), rgb(208, 196, 174)}}, 0,
        {{rgb(186, 150, 110), rgb(172, 136, 98), rgb(198, 164, 122), rgb(128, 98, 70)}, {rgb(186, 150, 110), rgb(172, 136, 98), rgb(198, 164, 122), rgb(128, 98, 70)}, {rgb(186, 150, 110), rgb(172, 136, 98), rgb(198, 164, 122), rgb(128, 98, 70)}},
        {rgb(70, 120, 130), rgb(86, 110, 150), rgb(70, 120, 130), rgb(86, 110, 150), rgb(70, 120, 130)},
        {rgb(228, 232, 228), rgb(224, 230, 234), rgb(232, 228, 216), rgb(226, 232, 226)}, {rgb(92, 170, 160), rgb(110, 158, 200), rgb(196, 110, 100), rgb(128, 180, 140)}, 0,
        {rgb(214, 218, 212), rgb(214, 218, 212), rgb(214, 218, 212), rgb(214, 218, 212), rgb(214, 218, 212)}, 1,
        {rgb(96, 134, 72), rgb(78, 112, 64), rgb(120, 152, 80)}, false,
    },
    {   // supermarket: big cream floor tiles, cream walls over a red brand stripe, tan stucco outside
        {{rgb(224, 212, 182), rgb(196, 182, 152)}, {rgb(214, 216, 204), rgb(184, 188, 178)}, {rgb(224, 212, 182), rgb(196, 182, 152)}}, 1,
        {{rgb(178, 128, 82), rgb(162, 114, 72), rgb(192, 142, 94), rgb(116, 78, 48)}, {rgb(178, 128, 82), rgb(162, 114, 72), rgb(192, 142, 94), rgb(116, 78, 48)}, {rgb(178, 128, 82), rgb(162, 114, 72), rgb(192, 142, 94), rgb(116, 78, 48)}},
        {rgb(150, 52, 52), rgb(150, 52, 52), rgb(150, 52, 52), rgb(150, 52, 52), rgb(150, 52, 52)},
        {rgb(230, 214, 172), rgb(222, 222, 210), rgb(230, 214, 172), rgb(222, 222, 210)}, {rgb(196, 62, 52), rgb(92, 112, 124), rgb(196, 62, 52), rgb(60, 130, 84)}, 0,
        {rgb(206, 176, 126), rgb(206, 176, 126), rgb(206, 176, 126), rgb(206, 176, 126), rgb(206, 176, 126)}, 2,
        {rgb(96, 134, 72), rgb(78, 112, 64), rgb(120, 152, 80)}, false,
    },
    {   // neighbourhood: kitchen checker, warm woods, a different carpet and wallpaper in every room, painted clapboard
        {{rgb(214, 208, 194), rgb(128, 124, 138)}, {rgb(210, 218, 204), rgb(124, 164, 144)}, {rgb(220, 206, 186), rgb(176, 120, 100)}}, 0,
        {{rgb(182, 132, 84), rgb(166, 118, 74), rgb(196, 146, 96), rgb(118, 80, 50)}, {rgb(196, 168, 124), rgb(182, 154, 110), rgb(208, 182, 138), rgb(136, 110, 78)}, {rgb(146, 100, 70), rgb(132, 88, 60), rgb(160, 112, 80), rgb(92, 60, 42)}},
        {rgb(134, 56, 66), rgb(50, 114, 114), rgb(106, 68, 126), rgb(184, 142, 64), rgb(62, 84, 134)},
        {rgb(172, 200, 218), rgb(176, 202, 166), rgb(238, 200, 166), rgb(202, 186, 220)}, {rgb(236, 232, 220), rgb(236, 232, 220), rgb(236, 232, 220), rgb(236, 232, 220)}, 1,
        {rgb(228, 226, 216), rgb(124, 164, 200), rgb(228, 202, 124), rgb(154, 184, 144), rgb(180, 88, 74)}, 3,
        {rgb(104, 150, 74), rgb(86, 128, 66), rgb(128, 168, 84)}, true,
    },
    {   // train yard: grey-green office lino, corrugated steel inside and out, dry grass
        {{rgb(174, 184, 170), rgb(152, 164, 152)}, {rgb(174, 184, 170), rgb(152, 164, 152)}, {rgb(174, 184, 170), rgb(152, 164, 152)}}, 0,
        {{rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}, {rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}, {rgb(152, 106, 70), rgb(138, 94, 60), rgb(166, 118, 80), rgb(96, 62, 40)}},
        {rgb(86, 98, 124), rgb(86, 98, 124), rgb(86, 98, 124), rgb(86, 98, 124), rgb(86, 98, 124)},
        {rgb(150, 168, 160), rgb(206, 196, 168), rgb(150, 168, 160), rgb(206, 196, 168)}, {rgb(98, 116, 114), rgb(132, 104, 80), rgb(98, 116, 114), rgb(132, 104, 80)}, 0,
        {rgb(112, 132, 146), rgb(150, 110, 84), rgb(112, 132, 146), rgb(150, 110, 84), rgb(112, 132, 146)}, 4,
        {rgb(122, 134, 78), rgb(100, 112, 68), rgb(144, 152, 88)}, false,
    },
};
const Theme* TH = &THEMES[0];

// ---------------------------------------------------------------- map queries
inline bool inMap(int x, int y) { return M->in(x, y); }
inline uint8_t typeAt(int x, int y) { return inMap(x, y) ? M->t[M->idx(x, y)] : (uint8_t)T_GRASS; }
inline bool indoorAt(int x, int y) { return inMap(x, y) && (M->flags[M->idx(x, y)] & F_INDOOR); }
int barKind(int x, int y) { if (!inMap(x, y)) return -1; int i = M->idx(x, y); return M->t[i] == T_DOOR && M->barAt[i] >= 0 ? M->bar[M->barAt[i]].kind : -1; }
// a wall face / mass: plain walls and windows (windows are glass set in a wall)
bool solidWall(int x, int y) { if (!inMap(x, y)) return false; uint8_t t = M->t[M->idx(x, y)]; return t == T_WALL || barKind(x, y) == BK_WINDOW; }
// anything that is part of a wall line, including doorways (used so a wall above a door doesn't grow a face)
bool wallish(int x, int y) { int k = barKind(x, y); return solidWall(x, y) || (k >= 0 && k != BK_GATE); }
bool isFloorType(uint8_t t) { return t == T_TILE || t == T_WOOD || t == T_CARPET || t == T_CONCRETE; }
bool isNatural(uint8_t t) { return t == T_GRASS || t == T_DIRT; }
bool isRoad(uint8_t t) { return t == T_ASPHALT || t == T_LINE || t == T_EXTRACT; }

// the floor to draw under a door / fence / bars: borrowed from a neighbour (south first, so doorways continue the room below)
void baseFloor(int tx, int ty, int& fx, int& fy) {
    static const int D[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    for (auto& d : D) {
        int x = tx + d[0], y = ty + d[1]; if (!inMap(x, y)) continue;
        uint8_t t = M->t[M->idx(x, y)];
        if (isFloorType(t) || isNatural(t) || isRoad(t) || t == T_GARDEN) { fx = x; fy = y; return; }
    }
    fx = tx; fy = ty;
}

void analyse() {
    const Map& m = *M; int n = m.w * m.h;
    static uint16_t q[Map::MAXW * Map::MAXH];
    memset(s_room, 0, sizeof s_room); memset(s_bld, 0, sizeof s_bld);
    static uint8_t seen[Map::MAXW * Map::MAXH];
    // rooms: connected indoor tiles of one floor type
    memset(seen, 0, n);
    for (int i = 0; i < n; i++) {
        if (seen[i] || !isFloorType(m.t[i]) || !(m.flags[i] & F_INDOOR)) continue;
        uint32_t hsh = hash32((uint32_t)i * 2654435761u + m.id * 97u + 13u) | 1u; int head = 0, tail = 0;
        seen[i] = 1; q[tail++] = (uint16_t)i;
        while (head < tail) {
            int c = q[head++]; s_room[c] = hsh; int cx = c % m.w, cy = c / m.w;
            static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& d : D) { int x = cx + d[0], y = cy + d[1]; if (!m.in(x, y)) continue; int j = m.idx(x, y); if (!seen[j] && m.t[j] == m.t[i] && (m.flags[j] & F_INDOOR)) { seen[j] = 1; q[tail++] = (uint16_t)j; } }
        }
    }
    // buildings: indoor floor + wall lines, 8-connected
    memset(seen, 0, n);
    auto part = [&](int j) { int x = j % m.w, y = j / m.w; return wallish(x, y) || (isFloorType(m.t[j]) && (m.flags[j] & F_INDOOR)) || m.t[j] == T_BARS; };
    for (int i = 0; i < n; i++) {
        if (seen[i] || !part(i)) continue;
        uint32_t hsh = hash32((uint32_t)i * 40503u + m.id * 131u + 7u) | 1u; int head = 0, tail = 0;
        seen[i] = 1; q[tail++] = (uint16_t)i;
        while (head < tail) {
            int c = q[head++]; s_bld[c] = hsh; int cx = c % m.w, cy = c / m.w;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int x = cx + dx, y = cy + dy; if (!m.in(x, y)) continue; int j = m.idx(x, y); if (!seen[j] && part(j)) { seen[j] = 1; q[tail++] = (uint16_t)j; } }
        }
    }
}
uint32_t roomAt(int x, int y) { return inMap(x, y) ? s_room[M->idx(x, y)] : 0; }

// ---------------------------------------------------------------- materials (world-space, so patterns run across tiles)
px grassPx(int wx, int wy) {
    const px* g = TH->grass;
    int n = (vnoise(wx, wy, 4, 11) * 5 + vnoise(wx, wy, 2, 23) * 3) >> 3;
    px c = n < 100 ? g[1] : (n < 172 ? g[0] : g[2]);
    if (TH->mow && ((wy / 10) & 1)) c = gfx_mix(c, g[2], 70);                 // mowing stripes on suburban lawns
    uint32_t h = h2(wx, wy >> 1, 31);                                          // rows share a hash in pairs: blades are two pixels tall
    if ((h & 31) == 0) c = lt(g[2], 30);
    else if ((h & 63) == 1 && !(wy & 1)) c = dk(g[1], 226);
    return c;
}
px dirtPx(int wx, int wy) {
    int n = vnoise(wx, wy, 4, 41);
    px c = n < 92 ? DIRT[1] : (n < 188 ? DIRT[0] : DIRT[2]);
    uint32_t h = h2(wx, wy, 43);
    if (h % 97 == 0) c = rgb(204, 188, 158); else if (h % 113 == 1) c = rgb(104, 76, 54);
    return c;
}
px asphaltPx(int wx, int wy) {
    int n = vnoise(wx, wy, 5, 51);
    px c = n < 84 ? ASPH[1] : (n < 196 ? ASPH[0] : gfx_mix(ASPH[0], ASPH[2], 110));
    uint32_t h = h2(wx, wy, 53);
    if (h % 43 == 0) c = ASPH[2]; else if (h % 59 == 1) c = dk(ASPH[1], 216);
    return c;
}
px concretePx(int wx, int wy, bool indoor) {
    const px* k = indoor ? CONC_IN : CONC;
    int v = h2(wx >> 4, wy >> 4, 61) & 7;
    px c = v == 0 ? k[2] : (v == 1 ? gfx_mix(k[0], k[1], 110) : k[0]);
    if ((wx & 15) == 0 || (wy & 15) == 0) c = k[1];                            // slab joints
    else if (vnoise(wx, wy, 3, 63) > 204) c = dk(c, 232);                      // stains
    return c;
}
px linoPx(uint32_t room, int wx, int wy) {
    const px* p = TH->lino[room % 3];
    px c;
    if (TH->linoMode == 1) { int gx = wx & 15, gy = wy & 15; c = (gx == 0 || gy == 0) ? p[1] : p[0]; if (gx == 1 || gy == 1) c = lt(p[0], 40); }
    else c = (((wx >> 3) + (wy >> 3)) & 1) ? p[0] : p[1];
    if (vnoise(wx, wy, 4, 71 + (room & 15)) > 204) c = dk(c, 244);            // worn patches, a touch darker
    return c;
}
px woodPx(uint32_t room, int wx, int wy) {
    const px* w = TH->wood[room % 3];
    int row = wy >> 2, off = (int)(h2(row, 0, room) % 28), plank = (wx + off) / 28, inP = (wx + off) % 28;
    px c = w[h2(row, plank, room + 1) % 3];
    if ((wy & 3) == 3 || inP == 0) c = w[3];
    else if (h2(wx >> 1, wy, room + 2) % 41 == 0) c = dk(c, 224);              // grain flecks
    return c;
}
px carpetPx(uint32_t room, int wx, int wy) {
    px c = TH->carpet[room % 5];
    if ((wx + wy) & 1) c = dk(c, 240);
    if (((wx >> 2) + (wy >> 2)) % 5 == 0 && ((wx & 3) == 1) && ((wy & 3) == 1)) c = lt(c, 46);   // small woven motif
    return c;
}
px waterPx(int wx, int wy) {
    int n = vnoise(wx, wy, 4, 81);
    px c = n < 110 ? rgb(26, 70, 98) : (n < 190 ? rgb(32, 82, 110) : rgb(40, 94, 122));
    if (h2(wx >> 2, wy, 83) % 23 == 0) c = rgb(84, 142, 160);                  // wave glints (more are drawn live)
    return c;
}

// one pixel of plain ground for the tile type at (tx,ty)
px groundPx(uint8_t t, int tx, int ty, int wx, int wy) {
    switch (t) {
    case T_GRASS: return grassPx(wx, wy);
    case T_DIRT: return dirtPx(wx, wy);
    case T_ASPHALT: case T_LINE: case T_EXTRACT: return asphaltPx(wx, wy);
    case T_CONCRETE: return concretePx(wx, wy, indoorAt(tx, ty));
    case T_TILE: return linoPx(roomAt(tx, ty), wx, wy);
    case T_WOOD: return woodPx(roomAt(tx, ty), wx, wy);
    case T_CARPET: return carpetPx(roomAt(tx, ty), wx, wy);
    case T_GARDEN: { px c = dirtPx(wx, wy); c = dk(c, 216); int r = wy % 5; if (r == 2) c = dk(c, 190); if (r == 1 && (wx + (wy / 5) * 2) % 3 == 0) c = rgb(104, 168, 70); if (r == 0 && (wx + (wy / 5) * 2) % 3 == 0) c = rgb(132, 190, 84); return c; }
    case T_RAIL: {
        uint32_t h = h2(wx, wy, 91); px c = h % 5 == 0 ? rgb(120, 114, 104) : (h % 7 == 1 ? rgb(84, 80, 76) : rgb(102, 98, 92));   // ballast
        int py = wy & 15;
        if (py >= 2 && py <= 13 && (wx & 3) < 2) c = (wx & 3) == 0 ? rgb(112, 84, 58) : rgb(84, 62, 44);                             // sleepers
        if (py == 4 || py == 10) c = rgb(186, 188, 196); if (py == 5 || py == 11) c = rgb(92, 94, 106);                              // rails
        return c;
    }
    case T_WATER: return waterPx(wx, wy);
    }
    return grassPx(wx, wy);
}

// ---------------------------------------------------------------- tiles
void floorTile(int tx, int ty, int fx, int fy) {
    const Map& m = *M; uint8_t t = m.t[m.idx(fx, fy)];
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16;
    bool natural = isNatural(t) && fx == tx && fy == ty, hard = (isRoad(t) || (t == T_CONCRETE && !indoorAt(fx, fy))) && fx == tx && fy == ty;
    if (natural || hard) {   // ragged borders are only worth computing where two ground types actually meet
        bool meets = false;
        for (int dy = -1; dy <= 1 && !meets; dy++) for (int dx = -1; dx <= 1; dx++) { uint8_t o = typeAt(tx + dx, ty + dy); if (natural ? (isNatural(o) && o != t) : o == T_GRASS) { meets = true; break; } }
        if (!meets) natural = hard = false;
    }
    for (int py = 0; py < 16; py++) for (int px_ = 0; px_ < 16; px_++) {
        int wx = tx * 16 + px_, wy = ty * 16 + py; uint8_t tt = t; int sx = fx, sy = fy;
        if (natural || hard) {
            // ragged borders: grass and dirt wobble into each other by up to 4 px, and grass overhangs hard ground by 2
            bool edge = px_ < 5 || px_ > 10 || py < 5 || py > 10;
            if (edge) {
                int jx = (vnoise(wx, wy, 3, 5) - 128) >> (natural ? 5 : 6), jy = (vnoise(wx, wy, 3, 6) - 128) >> (natural ? 5 : 6);
                int ax = (wx + jx) >> 4, ay = (wy + jy) >> 4;
                if (inMap(ax, ay)) { uint8_t o = m.t[m.idx(ax, ay)]; if (natural ? isNatural(o) : o == T_GRASS) { tt = o; sx = ax; sy = ay; } }
            }
        }
        px c = groundPx(tt, sx, sy, wx, wy);
        dst[py * g_bakeW + px_] = c;
    }
    // per-tile dressing that needs neighbours
    auto P = [&](int x, int y) -> px& { return dst[y * g_bakeW + x]; };
    if (t == T_LINE && fx == tx) for (int y = 0; y < 16; y++) if (h2(tx, ty * 16 + y, 3) % 9) { P(7, y) = rgb(226, 222, 206); P(8, y) = rgb(198, 194, 180); }   // worn stall line
    if (t == T_ASPHALT && fx == tx) {   // roads (narrow strips of asphalt, not car parks) get a worn dashed centre line
        auto road = [&](int x, int y) { return inMap(x, y) && isRoad(M->t[M->idx(x, y)]); };
        int up = 0, down = 0, left = 0, right = 0;
        while (up < 8 && road(tx, ty - up - 1)) up++; while (down < 8 && road(tx, ty + down + 1)) down++;
        while (left < 14 && road(tx - left - 1, ty)) left++; while (right < 14 && road(tx + right + 1, ty)) right++;
        int runH = up + down + 1, runW = left + right + 1;
        const px Y = rgb(226, 196, 92), YD = rgb(180, 152, 76);
        if (runH == 4 && runW >= 12 && up == 1) for (int x = 0; x < 16; x++) if ((((tx * 16 + x) / 10) & 1) && h2(tx * 16 + x, ty, 9) % 7) { P(x, 14) = Y; P(x, 15) = YD; }
        if (runW == 6 && runH >= 9 && left == 2) for (int y = 0; y < 16; y++) if ((((ty * 16 + y) / 10) & 1) && h2(tx, ty * 16 + y, 9) % 7) { P(14, y) = Y; P(15, y) = YD; }
        if (h2(tx, ty, 13 + M->id) % 97 == 0 && runW >= 3 && runH >= 3) {   // manhole cover
            for (int y = 4; y <= 11; y++) for (int x = 3; x <= 12; x++) { int dx = 2 * x - 15, dy = 3 * (2 * y - 15) / 2, d = dx * dx + dy * dy; if (d < 100) P(x, y) = d > 64 ? rgb(46, 46, 60) : (((x + y) & 1) ? rgb(96, 98, 112) : rgb(80, 82, 96)); }
        }
    }
    if (t == T_CONCRETE && !indoorAt(fx, fy) && fx == tx) {   // kerbs where the pavement meets the road, quay lip at the water
        if (isRoad(typeAt(tx, ty + 1))) for (int x = 0; x < 16; x++) { P(x, 14) = CONC[2]; P(x, 15) = rgb(98, 98, 108); }
        if (isRoad(typeAt(tx, ty - 1))) for (int x = 0; x < 16; x++) { P(x, 0) = rgb(98, 98, 108); P(x, 1) = CONC[2]; }
        if (isRoad(typeAt(tx - 1, ty))) for (int y = 0; y < 16; y++) { P(0, y) = rgb(98, 98, 108); P(1, y) = CONC[2]; }
        if (isRoad(typeAt(tx + 1, ty))) for (int y = 0; y < 16; y++) { P(15, y) = rgb(98, 98, 108); P(14, y) = CONC[2]; }
        if (typeAt(tx, ty - 1) == T_WATER) for (int x = 0; x < 16; x++) { P(x, 0) = rgb(60, 62, 70); P(x, 1) = rgb(206, 204, 194); P(x, 2) = rgb(206, 204, 194); P(x, 3) = CONC[1]; if ((x & 7) < 4) P(x, 2) = rgb(226, 190, 60); }
    }
    if (t == T_CARPET && fx == tx) {   // carpet edge: a darker band and a pale piping line where the room ends
        px base = TH->carpet[roomAt(tx, ty) % 5], band = dk(base, 196), pipe = lt(base, 70);
        bool n = typeAt(tx, ty - 1) != T_CARPET, s = typeAt(tx, ty + 1) != T_CARPET, w = typeAt(tx - 1, ty) != T_CARPET, e = typeAt(tx + 1, ty) != T_CARPET;
        for (int k = 0; k < 16; k++) {
            if (n) { P(k, 0) = band; P(k, 1) = band; if (!(w && k < 3) && !(e && k > 12)) P(k, 3) = pipe; }
            if (s) { P(k, 15) = band; P(k, 14) = band; if (!(w && k < 3) && !(e && k > 12)) P(k, 12) = pipe; }
            if (w) { P(0, k) = band; P(1, k) = band; if (!(n && k < 3) && !(s && k > 12)) P(3, k) = pipe; }
            if (e) { P(15, k) = band; P(14, k) = band; if (!(n && k < 3) && !(s && k > 12)) P(12, k) = pipe; }
        }
    }
    if (t == T_EXTRACT && fx == tx) {   // hazard frame round the whole extraction zone + a chevron on every tile
        const px Y = rgb(236, 196, 56), K = rgb(34, 34, 40);
        bool n = typeAt(tx, ty - 1) != T_EXTRACT, s = typeAt(tx, ty + 1) != T_EXTRACT, w = typeAt(tx - 1, ty) != T_EXTRACT, e = typeAt(tx + 1, ty) != T_EXTRACT;
        for (int k = 0; k < 16; k++) {
            px sc = (((k + tx * 16) >> 2) & 1) ? Y : K, sr = (((k + ty * 16) >> 2) & 1) ? Y : K;
            if (n) { P(k, 0) = sc; P(k, 1) = sc; } if (s) { P(k, 15) = sc; P(k, 14) = sc; }
            if (w) { P(0, k) = sr; P(1, k) = sr; } if (e) { P(15, k) = sr; P(14, k) = sr; }
        }
        for (int k = 0; k < 4; k++) { P(4 + k, 6 + k) = Y; P(11 - k, 6 + k) = Y; P(4 + k, 7 + k) = dk(Y, 190); P(11 - k, 7 + k) = dk(Y, 190); }
    }
    if (t == T_WATER && typeAt(tx, ty + 1) != T_WATER) for (int x = 0; x < 16; x++) { P(x, 15) = rgb(18, 44, 64); P(x, 14) = rgb(22, 56, 80); if (h2(tx * 16 + x, ty, 7) % 3 == 0) P(x, 13) = rgb(150, 196, 204); }   // shadow + foam under the quay
}

void wallTile(int tx, int ty) {
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16;
    auto P = [&](int x, int y) -> px& { return dst[y * g_bakeW + x]; };
    bool n = solidWall(tx, ty - 1) || wallish(tx, ty - 1), s = wallish(tx, ty + 1), w = solidWall(tx - 1, ty), e = solidWall(tx + 1, ty);
    bool face = !s && inMap(tx, ty + 1);
    int capH = face ? 5 : 16;
    for (int y = 0; y < capH; y++) for (int x = 0; x < 16; x++) {
        int wx = tx * 16 + x, wy = ty * 16 + y;
        px c = (h2(wx >> 2, wy >> 2, 17) & 7) == 0 ? CAP2 : CAP;
        if (!e && x == 15) c = CAP_LO;
        if (!w && x == 0) c = CAP_HI;
        if (!n && y == 0) c = CAP_HI;
        if (!n && y == 1 && x > 0 && x < 15) c = lt(CAP, 26);
        P(x, y) = c;
    }
    if (!face) { if (!s) for (int x = 0; x < 16; x++) P(x, 15) = CAP_LO; return; }
    bool interior = indoorAt(tx, ty + 1);
    uint32_t room = roomAt(tx, ty + 1), bld = s_bld[M->idx(tx, ty)];
    for (int y = 5; y < 16; y++) for (int x = 0; x < 16; x++) {
        int wx = tx * 16 + x, wy = ty * 16 + y; px c;
        if (interior) {
            px up = TH->faceUp[room % 4], lo = TH->faceLo[room % 4];
            c = y <= 11 ? up : lo;
            if (TH->faceStripe && y <= 11 && (wx % 6) == 0) c = dk(up, 226);          // wallpaper stripes
            if (y == 12) c = TH->faceStripe ? lt(lo, 60) : dk(lo, 200);              // dado rail
            if (y == 15) c = dk(lo, 150);                                            // skirting board
            if (y == 5) c = dk(up, 150);                                             // shadow under the cap
            if (y == 6) c = dk(up, 222);
        } else {
            px b = TH->ext[bld % 5];
            switch (TH->extStyle) {
            case 0: { int row = (y - 6) / 3, bx = (wx + (row & 1) * 4) & 7; bool mortar = (y - 6) % 3 == 2 || bx == 7; uint32_t hh = h2((wx + (row & 1) * 4) >> 3, row + ty * 5, 19) % 3;
                      c = mortar ? dk(b, 150) : (hh == 0 ? b : (hh == 1 ? lt(b, 22) : dk(b, 228))); break; }
            case 1: c = (wx & 15) == 0 ? dk(b, 214) : b; if (y == 9 || y == 10) c = rgb(84, 160, 152); break;
            case 2: c = (h2(wx >> 1, wy >> 1, 21) & 7) == 0 ? dk(b, 236) : b; if (y == 7 || y == 8) c = rgb(196, 62, 52); break;
            case 3: c = ((y - 6) % 3 == 2) ? dk(b, 190) : (((y - 6) % 3 == 0) ? lt(b, 30) : b); break;
            default: c = (wx & 3) < 2 ? b : dk(b, 206); if ((wx & 3) == 0) c = lt(b, 34); break;
            }
            if (y == 5) c = dk(b, 140);
            if (y == 15) c = rgb(96, 96, 104);                                       // footing
        }
        if (!w && x == 0 && y > 5) c = lt(c, 40);
        if (!e && x == 15) c = dk(c, 190);
        P(x, y) = c;
    }
    // lived-in walls: now and then a picture, notice or stain hangs on an interior face
    if (interior) {
        uint32_t h = h2(tx, ty, 29 + M->id);
        if (h % 5 == 0 && w && e) {
            int kind = (h >> 8) % 4; px frame = rgb(60, 44, 34);
            if (kind == 0) { for (int y = 7; y <= 11; y++) for (int x = 5; x <= 10; x++) P(x, y) = (y == 7 || y == 11 || x == 5 || x == 10) ? frame : ((x + y) & 1 ? rgb(120, 160, 190) : rgb(196, 170, 110)); }          // framed picture
            else if (kind == 1) { for (int y = 7; y <= 11; y++) for (int x = 4; x <= 11; x++) P(x, y) = (y == 7 || x == 4 || x == 11 || y == 11) ? rgb(150, 110, 70) : rgb(196, 160, 110); P(6, 9) = rgb(240, 240, 230); P(7, 9) = rgb(240, 240, 230); P(9, 8) = rgb(220, 80, 70); P(9, 10) = rgb(240, 230, 150); }   // cork board
            else if (kind == 2) { for (int y = 7; y <= 11; y++) for (int x = 6; x <= 9; x++) P(x, y) = rgb(232, 228, 214); P(7, 8) = rgb(90, 90, 100); P(8, 8) = rgb(90, 90, 100); P(7, 10) = rgb(170, 60, 56); }                 // paper notice
            else { for (int y = 8; y <= 14; y++) if (h2(tx * 16 + 7, y, 31) % 3) P(7 + (y & 1), y) = dk(P(7 + (y & 1), y), 200); }                                                                                                  // damp streak
        }
    }
}

// overlays that sit on a floor tile
void fenceOver(int tx, int ty) {
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16; auto P = [&](int x, int y) -> px& { return dst[y * g_bakeW + x]; };
    for (int x = 0; x < 16; x++) { P(x, 14) = dk(P(x, 14), 190); P(x, 15) = dk(P(x, 15), 214); }            // shadow under the fence
    for (int y = 3; y < 14; y++) for (int x = 0; x < 16; x++) { int wx = tx * 16 + x; if (((wx + y) & 3) == 0 || ((wx - y) & 3) == 0) P(x, y) = (y & 1) ? rgb(176, 180, 190) : rgb(140, 144, 156); }
    for (int x = 0; x < 16; x++) { P(x, 2) = rgb(196, 200, 208); P(x, 3) = rgb(84, 86, 98); }
    for (int y = 1; y < 15; y++) { P(0, y) = rgb(204, 206, 214); P(1, y) = rgb(88, 90, 102); }
}
void barsOver(int tx, int ty) {
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16; auto P = [&](int x, int y) -> px& { return dst[y * g_bakeW + x]; };
    for (int x = 1; x < 16; x += 3) for (int y = 0; y < 16; y++) { P(x, y) = rgb(164, 168, 180); if (x + 1 < 16) P(x + 1, y) = rgb(70, 72, 88); }
    for (int x = 0; x < 16; x++) { P(x, 0) = rgb(190, 194, 204); P(x, 1) = rgb(96, 98, 112); }
}
void palisadeOver(int tx, int ty) {
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16; auto P = [&](int x, int y) -> px& { return dst[y * g_bakeW + x]; };
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int wx = tx * 16 + x, board = wx >> 2, bx = wx & 3, top = (int)(h2(board, ty, 37) % 3);
        if (y < top || (y == top && (bx == 0 || bx == 3))) continue;                     // pointed tops: ground shows through
        px b = (board & 1) ? rgb(96, 98, 110) : rgb(110, 112, 124);
        if (bx == 0) b = rgb(146, 148, 162);
        if (bx == 3) b = rgb(40, 40, 52);
        if (y == top || (y == top + 1 && bx == 1)) b = rgb(164, 166, 178);
        if ((y == 6 || y == 12) && bx != 3) b = rgb(66, 66, 78);                         // rails
        if ((y == 5 || y == 11) && bx != 3) b = rgb(126, 128, 142);
        P(x, y) = b;
    }
}

// ---------------------------------------------------------------- shading passes
bool castsAO(int x, int y) { return solidWall(x, y) || typeAt(x, y) == T_PALISADE; }
void shadeFloor(int tx, int ty) {
    px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16;
    bool out = !indoorAt(tx, ty);
    bool n = castsAO(tx, ty - 1), w = castsAO(tx - 1, ty), e = castsAO(tx + 1, ty), s = castsAO(tx, ty + 1), nw = castsAO(tx - 1, ty - 1);
    if (!n && !w && !e && !s && !nw) return;
    // light comes from the north-west: deep soft shadow below and to the right of walls, a thin contact line elsewhere
    static const int IN_Y[7] = {150, 172, 194, 212, 228, 240, 249}, IN_X[5] = {178, 202, 224, 240, 250};
    static const int OUT_Y[11] = {136, 144, 154, 166, 178, 192, 206, 220, 232, 242, 250}, OUT_X[8] = {150, 164, 180, 196, 212, 228, 240, 250};
    const int* AY = out ? OUT_Y : IN_Y; const int* AX = out ? OUT_X : IN_X; int ny = out ? 11 : 7, nx = out ? 8 : 5;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        int lv = 256;
        if (n && y < ny && AY[y] < lv) lv = AY[y];
        if (w && x < nx && AX[x] < lv) lv = AX[x];
        if (!n && !w && nw && x < nx && y < ny) { int c = AX[x] > AY[y] ? AX[x] : AY[y]; if (c < lv) lv = c; }
        if (e && x == 15 && lv > 214) lv = 214;
        if (s && y == 15 && lv > 220) lv = 220;
        if (lv < 256) { px& p = dst[y * g_bakeW + x]; p = gfx_mixf(rgb(18, 16, 40), p, lv); }   // shadows lean violet, not grey
    }
}
bool floorPixel(int wx, int wy) {
    if (wx < 0 || wy < 0 || wx >= g_bakeW || wy >= g_bakeH) return false;
    int tx = wx >> 4, ty = wy >> 4; uint8_t t = M->t[M->idx(tx, ty)];
    return !(t == T_WALL || t == T_PALISADE || t == T_WATER || barKind(tx, ty) == BK_WINDOW);
}
void shadowSprite(const Sprite& s, int x, int y, int lvl) {
    for (int j = 0; j < s.h; j++) for (int i = 0; i < s.w; i++) {
        if (s.d[j * s.w + i] == KEYC) continue;
        int wx = x + i, wy = y + j; if (!floorPixel(wx, wy)) continue;
        px& p = g_bake[wy * g_bakeW + wx]; p = gfx_mixf(rgb(18, 16, 40), p, lvl);
    }
}
void shadowEllipse(int cx, int cy, int rx, int ry, int lvl) {
    for (int y = -ry; y <= ry; y++) for (int x = -rx; x <= rx; x++) {
        if (x * x * ry * ry + y * y * rx * rx > rx * rx * ry * ry) continue;
        if (!floorPixel(cx + x, cy + y)) continue;
        px& p = g_bake[(cy + y) * g_bakeW + cx + x]; p = gfx_mixf(rgb(18, 16, 40), p, lvl);
    }
}
void stampSprite(const Sprite& s, int x, int y) {
    for (int j = 0; j < s.h; j++) for (int i = 0; i < s.w; i++) {
        px v = s.d[j * s.w + i]; if (v == KEYC) continue;
        if (floorPixel(x + i, y + j)) g_bake[(y + j) * g_bakeW + x + i] = v;
    }
}


// ---------------------------------------------------------------- lived-in detail (all baked, so it is free at run time)
bool groundHard(int wx, int wy) {   // outdoor road / pavement pixel
    if (wx < 0 || wy < 0 || wx >= g_bakeW || wy >= g_bakeH) return false;
    int tx = wx >> 4, ty = wy >> 4; uint8_t t = M->t[M->idx(tx, ty)];
    return (t == T_ASPHALT || t == T_LINE || t == T_CONCRETE) && !indoorAt(tx, ty) && M->propAt[M->idx(tx, ty)] < 0;
}
void puddle(int cx, int cy, int rx, int ry, uint32_t seed) {
    for (int y = -ry; y <= ry; y++) for (int x = -rx; x <= rx; x++) {
        int e = x * x * ry * ry + y * y * rx * rx, lim = rx * rx * ry * ry; lim = lim * (200 + (int)(h2((cx + x) >> 1, (cy + y) >> 1, seed) & 63)) >> 8;
        if (e > lim || !groundHard(cx + x, cy + y)) continue;
        px& p = g_bake[(cy + y) * g_bakeW + cx + x];
        bool rim = (x + 1) * (x + 1) * ry * ry + (y - 1) * (y - 1) * rx * rx > lim || y == -ry;      // the far edge catches the sky
        p = rim && y < 0 ? gfx_mixf(p, rgb(150, 176, 200), 150) : gfx_mixf(p, rgb(34, 46, 74), 150);
    }
    bake_mark(cx - rx / 3, cy - ry / 3, rgb(190, 214, 230)); bake_mark(cx - rx / 3 + 1, cy - ry / 3, rgb(150, 176, 200));
}
void crack(int x, int y, int len, uint32_t seed) {
    int dx = (seed & 1) ? 1 : -1;
    for (int i = 0; i < len; i++) {
        uint32_t h = hash32(seed + i * 31);
        if (groundHard(x, y)) { px& p = g_bake[y * g_bakeW + x]; p = gfx_mixf(p, rgb(20, 20, 32), 120); if (groundHard(x, y + 1) && (h & 3) == 0) { px& q = g_bake[(y + 1) * g_bakeW + x]; q = gfx_mixf(q, rgb(200, 200, 200), 220); } }
        x += dx; if ((h >> 4) % 3 == 0) y += ((h >> 8) & 1) ? 1 : -1;
        if ((h >> 12) % 11 == 0) dx = -dx;
    }
}
void tyreMarks(int x, int y, int len, uint32_t seed) {
    float a = (seed % 628) / 100.0f, turn = ((int)((seed >> 8) % 9) - 4) * 0.006f, fx = (float)x, fy = (float)y;
    for (int i = 0; i < len; i++) {
        for (int s = -1; s <= 1; s += 2) {
            int px_ = (int)(fx - sinf(a) * 3 * s), py_ = (int)(fy + cosf(a) * 3 * s);
            if (groundHard(px_, py_)) { px& p = g_bake[py_ * g_bakeW + px_]; p = gfx_mixf(p, rgb(24, 24, 34), 196); }
        }
        fx += cosf(a); fy += sinf(a) * 0.6f; a += turn;
    }
}
void detailPass() {
    const Map& m = *M;
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty); uint8_t t = m.t[i]; uint32_t h = h2(tx, ty, 301 + m.id);
        bool hardOut = (t == T_ASPHALT || t == T_CONCRETE) && !indoorAt(tx, ty);
        if (hardOut && m.propAt[i] < 0) {
            int r = h % 1000;
            if (r < 24) puddle(tx * 16 + 8, ty * 16 + 8, 5 + (h >> 10) % 6, 3 + (h >> 14) % 3, h);
            else if (r < 60) crack(tx * 16 + (h >> 10) % 16, ty * 16 + (h >> 14) % 16, 10 + (h >> 18) % 18, h);
            else if (r < 72 && t == T_ASPHALT) tyreMarks(tx * 16 + 8, ty * 16 + 8, 22 + (h >> 10) % 16, h);
        }
        // weeds and flowers pushing up along the foot of outside walls
        if (!indoorAt(tx, ty) && !solidWall(tx, ty) && (t == T_GRASS || t == T_CONCRETE || t == T_ASPHALT || t == T_DIRT) && solidWall(tx, ty - 1) && (h >> 5) % 3 == 0)
            stampSprite(g_spr[(m.id == MAP_HOMES && (h >> 9) % 2) ? S_DEC_FLOWER : S_DEC_TUFT], tx * 16 + (h >> 12) % 9, ty * 16 - 2);
        // a mat just inside every outside door
        int k = barKind(tx, ty);
        if (k == BK_DOOR || k == BK_KEYDOOR) {
            static const int D[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            for (auto& d : D) {
                int ix = tx + d[0], iy = ty + d[1], ox = tx - d[0], oy = ty - d[1];
                if (!inMap(ix, iy) || !inMap(ox, oy) || !indoorAt(ix, iy) || indoorAt(ox, oy) || solidWall(ix, iy) || !isFloorType(m.t[m.idx(ix, iy)])) continue;
                bool horiz = d[1] != 0; int w = horiz ? 12 : 6, hh = horiz ? 6 : 12, x0 = ix * 16 + (16 - w) / 2, y0 = iy * 16 + (16 - hh) / 2;
                px mat = (h >> 3) & 1 ? rgb(122, 86, 62) : rgb(70, 110, 96);
                for (int y = 0; y < hh; y++) for (int x = 0; x < w; x++) if (floorPixel(x0 + x, y0 + y)) g_bake[(y0 + y) * g_bakeW + x0 + x] = (x == 0 || y == 0 || x == w - 1 || y == hh - 1) ? dk(mat, 170) : (((x + y) & 1) ? mat : lt(mat, 30));
            }
        }
    }
    // hospital wayfinding: coloured lines run the length of the main corridor
    if (m.id == MAP_HOSPITAL) for (int tx = 0; tx < m.w; tx++) for (int ty = 14; ty <= 16; ty++) if (m.in(tx, ty) && m.t[m.idx(tx, ty)] == T_TILE && ty == 15)
        for (int x = 0; x < 16; x++) { bake_dot(tx * 16 + x, ty * 16 + 5, rgb(214, 76, 72)); bake_dot(tx * 16 + x, ty * 16 + 6, rgb(170, 52, 54)); bake_dot(tx * 16 + x, ty * 16 + 9, rgb(72, 132, 212)); bake_dot(tx * 16 + x, ty * 16 + 10, rgb(52, 100, 170)); }
    // police lobby: the department badge set into the floor inside the front doors
    if (m.id == MAP_POLICE) {
        int cx = 25 * 16, cy = 29 * 16;
        for (int y = -14; y <= 14; y++) for (int x = -14; x <= 14; x++) {
            int d = x * x + y * y; if (d > 14 * 14 || !floorPixel(cx + x, cy + y) || !indoorAt((cx + x) >> 4, (cy + y) >> 4)) continue;
            px& p = g_bake[(cy + y) * g_bakeW + cx + x];
            if (d > 12 * 12) p = rgb(206, 170, 70); else if (d > 11 * 11) p = rgb(150, 118, 46); else p = gfx_mixf(p, rgb(44, 64, 118), 200);
            // five-pointed star
            float a = atan2f((float)y, (float)x) + 1.5708f, r = sqrtf((float)d), k = cosf(0.6283f) / cosf(fmodf(a + 62.8318f, 1.2566f) - 0.6283f);
            if (r < 9.5f * k * 0.52f + 0.0f || r < 4.0f) p = rgb(236, 226, 196);
        }
    }
}
} // namespace

// ---------------------------------------------------------------- public
void bake_level() {
    const Map& m = G.map; M = &m; TH = &THEMES[m.id >= 0 && m.id < MAP_COUNT ? m.id : 0];
    g_bakeW = m.w * 16; g_bakeH = m.h * 16;
    analyse();
    // doors and overlays borrow a neighbour's room so the floor runs through them
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty); uint8_t t = m.t[i];
        if ((t == T_DOOR && !solidWall(tx, ty)) || t == T_BARS || t == T_FENCE || t == T_PALISADE) { int fx, fy; baseFloor(tx, ty, fx, fy); if (!s_room[i]) s_room[i] = s_room[m.idx(fx, fy)]; }
    }
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty); uint8_t t = m.t[i];
        if (solidWall(tx, ty)) { wallTile(tx, ty); continue; }
        int fx = tx, fy = ty;
        if (t == T_DOOR || t == T_BARS || t == T_FENCE || t == T_PALISADE) baseFloor(tx, ty, fx, fy);
        floorTile(tx, ty, fx, fy);
    }
    // the forest beyond the settlement fence is dark and cool
    if (m.id == MAP_SETTLEMENT) {
        int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1;
        for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) if (m.t[m.idx(x, y)] == T_PALISADE) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) if (m.t[m.idx(tx, ty)] == T_GRASS && (tx < x0 || tx > x1 || ty < y0 || ty > y1)) {
            px* dst = g_bake + (ty * 16) * g_bakeW + tx * 16;
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) dst[y * g_bakeW + x] = gfx_mixf(rgb(14, 30, 30), dst[y * g_bakeW + x], 120);
        }
    }
    // ground clutter, scattered deterministically
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty); uint8_t t = m.t[i];
        if (t == T_WALL || t == T_PALISADE || t == T_DOOR || t == T_BARS || t == T_FENCE || t == T_WATER || t == T_RAIL || m.propAt[i] >= 0) continue;
        uint32_t h = hash32((uint32_t)(tx * 92837 + ty * 689287 + m.id * 7919));
        int r = h % 100, dx = (h >> 8) % 9, dy = (h >> 12) % 9, dec = -1;
        if (t == T_GRASS) dec = r < 9 ? S_DEC_TUFT : (r < 14 ? S_DEC_FLOWER : (r < 17 ? S_DEC_ROCK : (r < 19 ? S_DEC_LEAVES : -1)));
        else if (t == T_DIRT) dec = r < 6 ? S_DEC_ROCK : (r < 11 ? S_DEC_TUFT : (r < 13 ? S_DEC_LEAVES : -1));
        else if (t == T_ASPHALT || t == T_LINE) dec = r < 2 ? S_DEC_OIL : (r < 6 ? S_DEC_CRACK : (r < 11 ? S_DEC_PAPER : (r < 13 ? S_DEC_DEBRIS : -1)));
        else if (isFloorType(t)) dec = r < 5 ? S_DEC_PAPER : (r < 8 ? S_DEC_DEBRIS : (r < 9 ? S_DEC_CRACK : ((r < 11 && m.id != MAP_MARKET) ? S_DEC_SMEAR : -1)));
        if (dec >= 0) stampSprite(g_spr[dec], tx * 16 + dx, ty * 16 + dy);
    }
    detailPass();
    // contact shadows, then the fences that stand on the ground
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        uint8_t t = m.t[m.idx(tx, ty)];
        if (solidWall(tx, ty) || t == T_PALISADE || t == T_WATER) continue;
        shadeFloor(tx, ty);
    }
    // daylight through the windows: a pale slanted patch on the floor inside (the sun sits to the north-west)
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        if (barKind(tx, ty) != BK_WINDOW) continue;
        bool north = solidWall(tx - 1, ty) && solidWall(tx + 1, ty) && indoorAt(tx, ty + 1) && !indoorAt(tx, ty - 1);
        bool west = solidWall(tx, ty - 1) && solidWall(tx, ty + 1) && indoorAt(tx + 1, ty) && !indoorAt(tx - 1, ty);
        if (!north && !west) continue;
        for (int d = 0; d < 30; d++) {
            int a = 78 - d * 2; if (a < 14) break;
            for (int k = 0; k < 12; k++) {
                int wx = north ? tx * 16 + 2 + d / 2 + k : (tx + 1) * 16 + d, wy = north ? (ty + 1) * 16 + d : ty * 16 + 2 + d / 2 + k;
                if (!floorPixel(wx, wy) || !indoorAt(wx >> 4, wy >> 4) || (k == 5 || k == 6)) continue;   // the mullion's shadow splits it
                px& p = g_bake[wy * g_bakeW + wx]; p = gfx_mixf(p, rgb(255, 246, 214), a);
            }
        }
    }
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        uint8_t t = m.t[m.idx(tx, ty)];
        if (t == T_FENCE) fenceOver(tx, ty); else if (t == T_BARS) barsOver(tx, ty); else if (t == T_PALISADE) palisadeOver(tx, ty);
    }
    // furniture never moves: its drop shadow is part of the floor
    for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k]; int x = p.tx * 16, y = p.ty * 16;
        switch (p.type) {
        case PR_TREE: shadowEllipse(x + 12, y + 13, 15, 7, 150); shadowEllipse(x + 11, y + 12, 10, 5, 190); break;
        case PR_LAMP: shadowEllipse(x + 8, y + 14, 5, 2, 150); break;
        case PR_GENERATOR: case PR_PLOT: case PR_FIRE: case PR_TORCH: case PR_RADIO: break;
        default: {
            int ox, oy; const Sprite* ps = prop_sprite(m, p, ox, oy);
            if (!ps) break;
            bool big = ps->w > 24;                                       // vehicles and containers throw a longer shadow
            if (p.type == PR_CHAIR || p.type == PR_CONE) shadowEllipse(x + 8, y + 13, 5, 2, 176);
            else shadowSprite(*ps, x + ox + (big ? 4 : 2), y + oy + (big ? 5 : 3), big ? 132 : 150);
        } break;
        }
    }
}

// ---- permanent marks (floor pixels only: nothing paints over wall tops or water)
void bake_dot(int x, int y, px col) { if (M && floorPixel(x, y)) g_bake[y * g_bakeW + x] = col; }
void bake_blob(float fx, float fy, float r, px col, int alpha, uint32_t seed) {
    if (!M) return;
    int cx = (int)fx, cy = (int)fy, ri = (int)r + 1; float r2 = r * r;
    for (int y = -ri; y <= ri; y++) for (int x = -ri; x <= ri; x++) {
        float d2 = x * x + y * y * 1.7f;                                   // flattened: pools lie on the floor
        float edge = r2 * (0.62f + 0.38f * ((h2((cx + x) >> 1, (cy + y) >> 1, seed) & 255) / 255.0f));   // ragged rim
        if (d2 > edge) continue;
        int wx = cx + x, wy = cy + y; if (!floorPixel(wx, wy)) continue;
        px& p = g_bake[wy * g_bakeW + wx]; p = gfx_mixf(p, col, alpha);
    }
}
void bake_sprite(const Sprite& s, int x, int y, bool flipX, int shade) {
    if (!M) return;
    for (int j = 0; j < s.h; j++) for (int i = 0; i < s.w; i++) {
        px v = s.d[j * s.w + (flipX ? s.w - 1 - i : i)]; if (v == KEYC) continue;
        if (floorPixel(x + i, y + j)) g_bake[(y + j) * g_bakeW + x + i] = shade < 256 ? gfx_scale(v, shade) : v;
    }
}

// ---- doors, windows and gates are the only tiles that change, so they stay sprites drawn over the baked floor.
// Index 0 = set in a horizontal wall (seen from the front), 1 = set in a vertical wall (seen from above).
void worldart_init() {
    const px FR = rgb(38, 32, 34), W0 = rgb(160, 110, 66), W1 = rgb(132, 88, 52), W2 = rgb(186, 136, 88), GOLD = rgb(236, 196, 72);
    const px ST0 = rgb(118, 130, 150), ST1 = rgb(96, 108, 128), ST2 = rgb(150, 162, 180), GL = rgb(112, 186, 216), GLH = rgb(204, 238, 246), WF = rgb(236, 236, 228);
    auto head = [&](Sprite& s) { for (int x = 0; x < 16; x++) { spr_set(s, x, 0, CAP_HI); spr_set(s, x, 1, lt(CAP, 26)); spr_set(s, x, 2, CAP); spr_set(s, x, 3, CAP); spr_set(s, x, 4, CAP_LO); } };
    auto jambsH = [&](Sprite& s) { spr_rect(s, 0, 0, 2, 16, CAP); spr_rect(s, 14, 0, 2, 16, CAP); spr_set(s, 0, 0, CAP_HI); spr_set(s, 1, 0, CAP_HI); spr_set(s, 14, 0, CAP_HI); spr_set(s, 15, 0, CAP_HI); spr_rect(s, 1, 1, 1, 15, CAP_LO); spr_rect(s, 15, 1, 1, 15, CAP_LO); };
    auto jambsV = [&](Sprite& s) { spr_rect(s, 4, 0, 8, 2, CAP); spr_rect(s, 4, 14, 8, 2, CAP); for (int x = 4; x < 12; x++) { spr_set(s, x, 0, CAP_HI); spr_set(s, x, 15, CAP_LO); } };
    // closed
    Sprite d = spr_new(16, 16); head(d);
    spr_rect(d, 0, 5, 16, 11, FR); spr_rect(d, 2, 5, 12, 10, W0); spr_rect(d, 2, 5, 12, 1, W2);
    spr_rect(d, 3, 7, 10, 3, W1); spr_rect(d, 3, 11, 10, 3, W1); spr_rect(d, 4, 8, 8, 1, W0); spr_rect(d, 4, 12, 8, 1, W0);
    spr_set(d, 12, 10, GOLD); spr_set(d, 12, 11, rgb(150, 116, 40));
    Sprite dv = spr_new(16, 16); jambsV(dv);
    spr_rect(dv, 5, 2, 6, 12, FR); spr_rect(dv, 6, 2, 4, 12, W0); spr_rect(dv, 6, 2, 1, 12, W2); spr_rect(dv, 9, 2, 1, 12, W1); spr_set(dv, 8, 8, GOLD);
    g_tile[TA_DOOR_CLOSED][0] = d; g_tile[TA_DOOR_CLOSED][1] = dv;
    // open: jambs, the leaf swung back against the frame
    Sprite o = spr_new(16, 16); jambsH(o); spr_rect(o, 2, 3, 2, 13, W0); spr_rect(o, 2, 3, 2, 1, W2); spr_rect(o, 3, 4, 1, 12, W1); spr_rect(o, 4, 4, 1, 12, FR);
    Sprite ov = spr_new(16, 16); jambsV(ov); spr_rect(ov, 10, 2, 6, 2, W0); spr_rect(ov, 10, 2, 6, 1, W2); spr_rect(ov, 10, 4, 6, 1, FR);
    g_tile[TA_DOOR_OPEN][0] = o; g_tile[TA_DOOR_OPEN][1] = ov;
    // locked: steel security door with a keypad and a red light
    Sprite lk = spr_copy(d);
    spr_rect(lk, 2, 5, 12, 10, ST0); spr_rect(lk, 2, 5, 12, 1, ST2); spr_rect(lk, 3, 7, 5, 7, ST1); spr_rect(lk, 9, 8, 4, 5, rgb(34, 36, 48));
    spr_set(lk, 10, 9, rgb(244, 70, 60)); spr_set(lk, 11, 9, rgb(244, 70, 60)); spr_set(lk, 10, 11, rgb(150, 154, 166)); spr_set(lk, 11, 11, rgb(150, 154, 166));
    Sprite lv = spr_copy(dv); spr_rect(lv, 6, 2, 4, 12, ST0); spr_rect(lv, 6, 2, 1, 12, ST2); spr_rect(lv, 9, 2, 1, 12, ST1); spr_set(lv, 8, 8, rgb(244, 70, 60)); spr_set(lv, 7, 8, rgb(244, 70, 60));
    g_tile[TA_DOOR_LOCKED][0] = lk; g_tile[TA_DOOR_LOCKED][1] = lv;
    // broken: jambs and splinters
    Sprite b = spr_new(16, 16); jambsH(b);
    for (int i = 0; i < 7; i++) spr_rect(b, 3 + (int)(hash32(i * 31) % 9), 7 + (int)(hash32(i * 17) % 8), 2 + (i & 1), 1, i & 1 ? W0 : W1);
    spr_rect(b, 2, 2, 3, 3, W1); spr_rect(b, 11, 1, 3, 2, W0);
    Sprite bv = spr_new(16, 16); jambsV(bv);
    for (int i = 0; i < 6; i++) spr_rect(bv, 2 + (int)(hash32(i * 13) % 11), 3 + (int)(hash32(i * 29) % 10), 1 + (i & 1), 1, i & 1 ? W0 : W1);
    g_tile[TA_DOOR_BROKEN][0] = b; g_tile[TA_DOOR_BROKEN][1] = bv;
    // windows: white frame, sky glass with glints, a sill; from above a strip of glass in the wall top
    Sprite wi = spr_new(16, 16);
    spr_rect(wi, 2, 6, 12, 7, WF); spr_rect(wi, 3, 7, 10, 5, GL); spr_rect(wi, 7, 7, 2, 5, WF); spr_rect(wi, 3, 7, 4, 1, rgb(84, 150, 186)); spr_rect(wi, 9, 7, 4, 1, rgb(84, 150, 186));
    spr_set(wi, 4, 9, GLH); spr_set(wi, 5, 8, GLH); spr_set(wi, 10, 9, GLH); spr_set(wi, 11, 8, GLH);
    spr_rect(wi, 1, 13, 14, 1, rgb(206, 206, 200)); spr_rect(wi, 1, 14, 14, 1, rgb(120, 120, 130));
    Sprite wv = spr_new(16, 16); spr_rect(wv, 6, 1, 4, 14, rgb(28, 28, 42)); spr_rect(wv, 7, 1, 2, 14, GL); spr_set(wv, 7, 3, GLH); spr_set(wv, 7, 4, GLH); spr_set(wv, 8, 10, GLH); spr_rect(wv, 6, 7, 4, 1, rgb(28, 28, 42));
    g_tile[TA_WINDOW][0] = wi; g_tile[TA_WINDOW][1] = wv;
    Sprite wb = spr_copy(wi); spr_rect(wb, 3, 7, 4, 5, rgb(22, 22, 34)); spr_rect(wb, 9, 7, 4, 5, rgb(22, 22, 34));
    spr_set(wb, 3, 7, GL); spr_set(wb, 4, 7, GL); spr_set(wb, 3, 8, GLH); spr_set(wb, 6, 11, GL); spr_set(wb, 12, 7, GL); spr_set(wb, 12, 8, GL); spr_set(wb, 9, 11, GLH); spr_set(wb, 10, 11, GL);
    Sprite wbv = spr_copy(wv); spr_rect(wbv, 7, 3, 2, 9, rgb(22, 22, 34)); spr_set(wbv, 7, 3, GL); spr_set(wbv, 8, 11, GL);
    g_tile[TA_WINDOW_BROKEN][0] = wb; g_tile[TA_WINDOW_BROKEN][1] = wbv;
    // cell door: bars with a lock plate (index 1 = swung open, nothing to draw)
    Sprite cd = spr_new(16, 16);
    for (int x = 1; x < 16; x += 3) { spr_rect(cd, x, 0, 1, 16, rgb(164, 168, 180)); if (x + 1 < 16) spr_rect(cd, x + 1, 0, 1, 16, rgb(70, 72, 88)); }
    spr_rect(cd, 0, 0, 16, 1, rgb(190, 194, 204)); spr_rect(cd, 0, 1, 16, 1, rgb(96, 98, 112));
    spr_rect(cd, 5, 6, 6, 5, rgb(84, 88, 104)); spr_rect(cd, 6, 7, 4, 3, rgb(176, 52, 46)); spr_set(cd, 8, 8, GOLD);
    g_tile[TA_CELLDOOR][0] = cd; g_tile[TA_CELLDOOR][1] = spr_new(16, 16);
    // settlement gate: braced plank leaves, or just the posts when open
    Sprite gc = spr_new(16, 16);
    for (int y = 1; y < 16; y++) for (int x = 0; x < 16; x++) { int bx = x & 3; px c = (x >> 2) & 1 ? rgb(134, 100, 66) : rgb(150, 112, 74); if (bx == 0) c = rgb(176, 136, 92); if (bx == 3) c = rgb(72, 52, 36); spr_set(gc, x, y, c); }
    spr_rect(gc, 0, 4, 16, 2, rgb(84, 60, 40)); spr_rect(gc, 0, 12, 16, 2, rgb(84, 60, 40)); for (int i = 0; i < 16; i++) spr_set(gc, i, 5 + (i * 7) / 16, rgb(96, 70, 46));
    Sprite go = spr_new(16, 16); spr_rect(go, 0, 0, 2, 16, rgb(120, 88, 58)); spr_rect(go, 14, 0, 2, 16, rgb(120, 88, 58)); spr_rect(go, 0, 0, 1, 16, rgb(160, 122, 82)); spr_rect(go, 15, 0, 1, 16, rgb(72, 52, 36));
    g_tile[TA_GATE_CLOSED][0] = gc; g_tile[TA_GATE_OPEN][0] = go;
}
void bake_mark(int x, int y, px col) { if (M && x >= 0 && y >= 0 && x < g_bakeW && y < g_bakeH) g_bake[y * g_bakeW + x] = col; }

// ---- exterior fronts. From the street the renderer stands these 16x40 strips on the camera-facing outer walls, so each
// building shows its own material: red brick police station, white panelled hospital with a teal band, tan stucco
// market under a red fascia, painted clapboard homes (a colour per house), corrugated steel sheds.
// kind 0 = wall, 1 = window, 2 = door.
const Sprite& facade_sprite(int tx, int ty, int kind) {
    static Sprite cache[MAP_COUNT][5][6]; static bool built[MAP_COUNT][5];
    int id = M && M->id >= 0 && M->id < MAP_COUNT ? M->id : 0;
    const Theme& T = THEMES[id];
    int b = M && M->in(tx, ty) ? (int)(s_bld[M->idx(tx, ty)] % 5) : 0;
    if (!built[id][b]) {
        built[id][b] = true;
        px base = T.ext[b];
        for (int v = 0; v < 6; v++) {
            Sprite s = spr_new(16, 40);
            for (int y = 0; y < 40; y++) for (int x = 0; x < 16; x++) {
                px c;
                if (y < 4) c = T.extStyle == 3 ? (y == 0 ? rgb(110, 96, 110) : rgb(74, 62, 78)) : (y == 0 ? lt(CAP, 60) : (y == 3 ? CAP_LO : CAP));       // eave / parapet
                else if (y >= 36) c = y == 36 ? rgb(176, 174, 168) : rgb(120, 120, 128);                                                             // footing
                else switch (T.extStyle) {
                    case 0: { int row = (y - 4) / 4, bx = (x + (row & 1) * 4) & 7; bool mortar = (y - 4) % 4 == 3 || bx == 7; uint32_t hh = hash32(v * 977 + row * 31 + (x + (row & 1) * 4) / 8) % 3;
                              c = mortar ? dk(base, 150) : (hh == 0 ? base : (hh == 1 ? lt(base, 22) : dk(base, 226))); break; }
                    case 1: c = (x == 0 || (y - 4) % 12 == 11) ? dk(base, 210) : base; if (y >= 20 && y <= 23) c = rgb(84, 160, 152); if (y == 19) c = lt(base, 60); break;
                    case 2: c = (hash32(v * 31 + (x >> 1) * 7 + (y >> 1) * 13) & 7) == 0 ? dk(base, 236) : base; if (y >= 4 && y <= 11) c = y == 4 ? rgb(236, 110, 96) : (y == 11 ? rgb(140, 36, 34) : rgb(196, 62, 52)); break;
                    case 3: c = (y - 4) % 4 == 3 ? dk(base, 180) : ((y - 4) % 4 == 0 ? lt(base, 36) : base); if (x == 0 && v == 0) c = lt(base, 90); break;
                    default: c = (x & 3) < 2 ? base : dk(base, 204); if ((x & 3) == 0) c = lt(base, 36); if (y == 20) c = dk(base, 170); break;
                }
                spr_set(s, x, y, c);
            }
            if (v == 4) {   // window: frame, sky-lit glass with glints, sill
                bool shop = T.extStyle == 2 || T.extStyle == 1;
                int y0 = shop ? 13 : 14, h = shop ? 18 : 13, x0 = shop ? 0 : 2, w = shop ? 16 : 12;
                spr_rect(s, x0, y0, w, h, rgb(36, 34, 46)); spr_rect(s, x0 + 1, y0 + 1, w - 2, h - 2, rgb(108, 178, 210)); spr_rect(s, x0 + 1, y0 + 1, w - 2, 3, rgb(150, 208, 230));
                spr_rect(s, 7, y0, 2, h, rgb(36, 34, 46)); if (!shop) spr_rect(s, x0, y0 + h / 2, w, 1, rgb(36, 34, 46));
                for (int k = 0; k < 4; k++) { spr_set(s, x0 + 2 + k, y0 + 6 - k, rgb(214, 240, 248)); spr_set(s, x0 + 9 + k, y0 + 8 - k, rgb(214, 240, 248)); }
                spr_rect(s, x0 > 0 ? x0 - 1 : 0, y0 + h, x0 > 0 ? w + 2 : 16, 2, rgb(206, 206, 200));
            }
            if (v == 5) {   // door: glass double doors on public buildings, a painted front door on a house, a roller door on a shed
                if (T.extStyle == 3) { static const px DOOR[5] = {rgb(180, 60, 56), rgb(236, 232, 220), rgb(60, 96, 150), rgb(120, 76, 50), rgb(232, 200, 110)};
                    spr_rect(s, 1, 9, 14, 27, rgb(236, 232, 220)); spr_rect(s, 3, 11, 10, 25, rgb(36, 34, 46)); spr_rect(s, 4, 12, 8, 24, DOOR[b]); spr_rect(s, 5, 14, 6, 7, dk(DOOR[b], 210)); spr_rect(s, 5, 24, 6, 9, dk(DOOR[b], 210)); spr_set(s, 10, 23, rgb(236, 196, 72)); }
                else if (T.extStyle == 4) { spr_rect(s, 0, 8, 16, 28, rgb(36, 34, 46)); for (int y = 9; y < 36; y++) spr_rect(s, 1, y, 14, 1, (y & 3) == 0 ? rgb(96, 100, 116) : rgb(150, 156, 172)); spr_rect(s, 6, 32, 4, 2, rgb(60, 62, 76)); }
                else { spr_rect(s, 0, 8, 16, 28, rgb(36, 34, 46)); spr_rect(s, 2, 10, 13, 26, rgb(72, 110, 140)); spr_rect(s, 3, 11, 5, 12, rgb(140, 196, 220)); spr_rect(s, 2, 23, 13, 1, rgb(36, 34, 46)); spr_rect(s, 12, 24, 2, 4, rgb(236, 196, 72)); spr_rect(s, 2, 10, 13, 1, rgb(190, 226, 240)); }
            }
            cache[id][b][v] = s;
        }
    }
    return cache[id][b][kind == 0 ? ((tx * 7 + ty) & 3) : (kind == 1 ? 4 : 5)];
}
