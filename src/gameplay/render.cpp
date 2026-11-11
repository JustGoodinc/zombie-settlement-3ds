#include "game.h"
#include "../core/art.h"
#include "../ui/ui.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern const char* g_prompt;
extern float g_promptX, g_promptY;
extern int g_highlightProp;

static float ilerp(float a, float b, float t) { return a + (b - a) * t; }
static int camX, camY;

static float flashDX = 0, flashDY = 1;   // smoothed flashlight direction

void render_compute_light() {
    Map& m = G.map;
    if (!G.inLevel) return;
    const Unit& p = G.u[G.ctrl];
    float day = daylight();
    // smooth flashlight rotation toward the facing direction
    float fk = 1 - expf(-fmaxf(G.frameMs, 1.0f) / 1000.0f * 14);   // time-based smoothing (same feel at any FPS)
    flashDX += (p.fx - flashDX) * fk; flashDY += (p.fy - flashDY) * fk;
    float fl = sqrtf(flashDX * flashDX + flashDY * flashDY) + 1e-4f;
    float fdx = flashDX / fl, fdy = flashDY / fl;
    int tx0 = camX / 16 - 1, ty0 = camY / 16 - 1, tx1 = (camX + view_w()) / 16 + 1, ty1 = (camY + view_h()) / 16 + 1;
    float t = G.uiTime;
    // Static light sources never move: bake their reach per tile (with line of sight) when the map changes and
    // every 2 s (doors opening). Per frame each tile then costs two lookups instead of a loop over every prop.
    static uint8_t bakeSteady[Map::MAXW * Map::MAXH], bakeFlick[Map::MAXW * Map::MAXH];
    static const Map* bakedMap = nullptr; static int bakedSig = -1; static uint32_t bakedTick = 0;
    int sig = m.id * 7919 + m.nprop * 31 + m.w * 7 + m.h;
    if (bakedMap != &m || bakedSig != sig || G.tick - bakedTick >= 60) {
        bakedMap = &m; bakedSig = sig; bakedTick = G.tick;
        memset(bakeSteady, 0, sizeof bakeSteady); memset(bakeFlick, 0, sizeof bakeFlick);
        for (int k = 0; k < m.nprop; k++) {
            const Prop& pr = m.prop[k];
            if (pr.type != PR_LAMP && pr.type != PR_FIRE && pr.type != PR_TORCH) continue;
            float r = pr.type == PR_FIRE ? 88.0f : (pr.type == PR_TORCH ? 56.0f : 72.0f);
            float lx = pr.tx * 16 + 8.0f, ly = pr.ty * 16 + 8.0f; int rt = (int)(r / 16) + 1;
            uint8_t* dst = pr.type == PR_LAMP ? bakeSteady : bakeFlick;
            for (int y = pr.ty - rt; y <= pr.ty + rt; y++) for (int x = pr.tx - rt; x <= pr.tx + rt; x++) {
                if (!m.in(x, y)) continue;
                float cx = x * 16 + 8.0f, cy = y * 16 + 8.0f, ld = sqrtf((cx - lx) * (cx - lx) + (cy - ly) * (cy - ly));
                if (ld > r || !map_los(m, lx, ly, cx, cy)) continue;
                int v = (int)(200 * (1 - ld / r)); int i = m.idx(x, y);
                if (v > dst[i]) dst[i] = (uint8_t)v;
            }
        }
    }
    // The player's line of sight only depends on which tile they stand on: cache it per player tile
    // (refreshed twice a second for doors). Exact: map_los works on tile coordinates.
    static uint8_t losC[Map::MAXW * Map::MAXH]; static int lpx = -1, lpy = -1, lx0 = 0, ly0 = 0, lx1 = -1, ly1 = -1; static uint32_t losTick = 0; static const Map* losMap = nullptr;
    int ptx = (int)(p.x / 16), pty = (int)((p.y - 4) / 16);
    if (ptx != lpx || pty != lpy || losMap != &m || tx0 < lx0 || ty0 < ly0 || tx1 > lx1 || ty1 > ly1 || G.tick - losTick >= 15) {
        lpx = ptx; lpy = pty; losMap = &m; losTick = G.tick;
        lx0 = tx0 - 4; ly0 = ty0 - 3; lx1 = tx1 + 4; ly1 = ty1 + 3;
        for (int y = ly0; y <= ly1; y++) for (int x = lx0; x <= lx1; x++) if (m.in(x, y)) losC[m.idx(x, y)] = map_los(m, p.x, p.y - 4, x * 16 + 8.0f, y * 16 + 8.0f) ? 1 : 0;
    }
    float flick = 0.85f + 0.15f * sinf(t * 13) * sinf(t * 7.3f);
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int i = m.idx(tx, ty);
        bool indoor = (m.flags[i] & F_INDOOR) != 0;
        float L = indoor ? 70 + day * 104 : (G.scene == SC_SETTLEMENT ? 70 + day * 176 : 28 + day * 210);   // the settlement keeps a lantern-lit floor
        float cx = tx * 16 + 8.0f, cy = ty * 16 + 8.0f, dx = cx - p.x, dy = cy - (p.y - 4), d2 = dx * dx + dy * dy;
        bool los = d2 < 144 || losC[i];
        if (los && d2 < 160 * 160) {
            float d = sqrtf(d2);
            if (d < 36) L = fmaxf(L, 150 - d * 1.5f);                         // personal ambient light
            if (G.flashAnim > 0.02f && d > 1) {
                float dot = (dx * fdx + dy * fdy) / d;
                if (dot > 0.86f) L = fmaxf(L, (235 - d * 0.7f) * G.flashAnim * fminf(1, (dot - 0.86f) * 25));
            }
        }
        // baked light sources: lamps steady, fires and torches flicker
        if (bakeSteady[i]) L = fmaxf(L, bakeSteady[i] + 30.0f);
        if (bakeFlick[i]) L = fmaxf(L, bakeFlick[i] * flick + 30.0f);
        if (!los) L = fminf(L * 0.3f, 48);
        G.light[i] = (uint8_t)(L > 255 ? 255 : L);
        if (los && L >= 60) G.explored[i] = 1;
    }
}

static bool visibleAt(float x, float y) {
    int tx = (int)(x / 16), ty = (int)(y / 16);
    return G.map.in(tx, ty) && G.light[G.map.idx(tx, ty)] >= 60;
}

// Doors, windows and gates are the only tiles that change, so they are sprites drawn over the baked level layer
// (worldart.cpp). Index 1 = the barrier sits in a vertical wall and is seen from above.
static const Sprite& barrierSprite(int tx, int ty) {
    Map& m = G.map;
    int i = m.idx(tx, ty);
    const Barrier& b = m.bar[m.barAt[i]];
    auto wall = [&](int x, int y) { if (!m.in(x, y)) return false; uint8_t t = m.t[m.idx(x, y)]; return t == T_WALL || t == T_DOOR; };
    int v = (wall(tx, ty - 1) || wall(tx, ty + 1)) && !(wall(tx - 1, ty) && wall(tx + 1, ty)) ? 1 : 0;
    if (b.kind == BK_WINDOW) return b.state == BS_BROKEN ? g_tile[TA_WINDOW_BROKEN][v] : g_tile[TA_WINDOW][v];
    if (b.kind == BK_GATE) return b.anim > 0.5f ? g_tile[TA_GATE_OPEN][0] : g_tile[TA_GATE_CLOSED][0];
    if (b.kind == BK_CELLDOOR) return g_tile[TA_CELLDOOR][b.anim > 0.5f ? 1 : 0];
    if (b.state == BS_BROKEN) return g_tile[TA_DOOR_BROKEN][v];
    if (b.anim > 0.5f) return g_tile[TA_DOOR_OPEN][v];
    return b.state == BS_LOCKED ? g_tile[TA_DOOR_LOCKED][v] : g_tile[TA_DOOR_CLOSED][v];
}

// Flames are drawn in the depth-sorted scene, so anyone standing in front covers them. Their pixels are set aside
// before the lighting pass and put back after it: fire stays the brightest thing on screen without painting over people.
enum { FLAME_W = 19, FLAME_H = 23, FLAME_SITES = 8 };
static const px FLAME_COL[3] = {rgb(226, 82, 30), rgb(250, 156, 40), rgb(255, 230, 130)};
static struct FlameSite { int16_t x, y; px keep[FLAME_W * FLAME_H]; } s_flame[FLAME_SITES];
static int s_nflame;
static void flamesKeep(const Canvas& c) {
    for (int k = 0; k < s_nflame; k++) {
        FlameSite& f = s_flame[k];
        for (int j = 0; j < FLAME_H; j++) for (int i = 0; i < FLAME_W; i++) {
            int x = f.x + i, y = f.y + j; px v = KEYC;
            if (x >= 0 && y >= 0 && x < c.w && y < c.h) { px q = c.p[y * c.w + x]; if (q == FLAME_COL[0] || q == FLAME_COL[1] || q == FLAME_COL[2]) v = q; }
            f.keep[j * FLAME_W + i] = v;
        }
    }
}
static void flamesPut(Canvas& c) {
    for (int k = 0; k < s_nflame; k++) {
        const FlameSite& f = s_flame[k];
        for (int j = 0; j < FLAME_H; j++) for (int i = 0; i < FLAME_W; i++) { px v = f.keep[j * FLAME_W + i]; if (v != KEYC) c.p[(f.y + j) * c.w + f.x + i] = v; }
    }
}
// three flickering flame tongues
static void drawFlames(Canvas& c, int cx, int cy) {
    if (s_nflame < FLAME_SITES) { s_flame[s_nflame].x = (int16_t)(cx - FLAME_W / 2); s_flame[s_nflame].y = (int16_t)(cy - FLAME_H + 1); s_nflame++; }
    for (int k = 0; k < 3; k++) {
        int ox = (k - 1) * 4, h = (k == 1 ? 17 : 11) + (int)(sinf(G.uiTime * 9.0f + k * 2.1f) * 3);
        for (int r = 0; r < h; r++) {
            float f = 1.0f - (float)r / h; int sway = (int)(sinf(G.uiTime * 7.0f + r * 0.5f + k) * (1 - f) * 2);
            int w = 1 + (int)(f * (k == 1 ? 6 : 4));
            int fx0 = cx + ox + sway - w / 2, fy = cy - 1 - r;
            gfx_fill(c, fx0, fy, w, 1, rgb(226, 82, 30));
            if (w > 2) gfx_fill(c, fx0 + 1, fy, w - 2, 1, rgb(250, 156, 40));
            if (w > 4 && f > 0.35f) gfx_fill(c, fx0 + 2, fy, w - 4, 1, rgb(255, 230, 130));
        }
    }
}
static void drawChar(Canvas& c, bool zombie, int variant, float x, float y, uint8_t dir, const Animator& a, float fx, float fy, float flash, int weaponId, float muzzle, bool shadow = true) {
    const AnimFrame& f = a.cur();
    bool flip;
    const Sprite& s = art_char(zombie, variant, dir, (Pose)f.pose, flip);
    int sx = (int)(x + fx * f.kick) - camX - s.w / 2, sy = (int)(y + fy * f.kick * 0.5f + f.bob) - camY - s.h + 2;
    if (shadow && f.pose != P_DEAD) { gfx_fillA(c, (int)x - camX - 6, (int)y - camY - 1, 12, 3, rgb(0, 0, 0), 120); gfx_fillA(c, (int)x - camX - 4, (int)y - camY - 2, 8, 5, rgb(0, 0, 0), 60); }
    // weapon: drawn behind the body when facing away from the camera
    // weapon: a pixel-art sprite in the hand. Orientation 0 right, 1 down, 2 left, 3 up.
    static const int VMAP[8] = {2, 1, 0, 1, 2, 3, 4, 3};
    int sd = VMAP[dir & 7];
    bool aiming = f.pose == P_AIM, crouched = f.pose == P_CROUCH || f.pose == P_REACH;
    int hx, hy, orient, behind = 0;
    if (sd == 2) {          // side
        hx = aiming ? 15 : (f.pose == P_WALK_A || f.pose == P_RUN_A || f.pose == P_REACH ? 15 : (f.pose == P_WALK_B || f.pose == P_RUN_B ? 12 : 13)); hy = aiming ? 16 : 18;
        orient = 0;
    } else if (sd <= 1) {   // facing the camera
        if (aiming && sd == 1) { hx = 12; hy = 17; orient = 0; }
        else { hx = aiming ? 8 : 13; hy = aiming ? 18 : (f.pose == P_WALK_A ? 17 : 18); orient = 1; }
    } else {                // facing away
        if (aiming) { hx = sd == 3 ? 12 : 8; hy = 16; orient = sd == 3 ? 0 : 3; behind = 1; }
        else { hx = 13; hy = 18; orient = 1; }
    }
    if (crouched) hy += 2;
    if (a.state == AnimState::Melee) { orient = a.frame == 0 ? 3 : (a.frame == 1 ? (sd == 2 || sd == 1 ? 0 : (sd == 0 ? 1 : 3)) : 1); behind = sd >= 3 && a.frame != 2; }
    if (a.state == AnimState::Reload) orient = 1;
    if (flip) { hx = 15 - hx; if (orient == 0) orient = 2; }
    auto weapon = [&]() {
        if (weaponId <= 0 || weaponId >= 40 || !g_held[weaponId].d || f.pose == P_DEAD || f.pose == P_FALL || zombie) return;
        const Sprite& w = g_held[weaponId];
        int gx = g_heldGrip[weaponId][0], gy = g_heldGrip[weaponId][1], ox = sx + hx, oy = sy + hy;
        auto map = [&](int x, int y, int& X, int& Y) {
            int dx = x - gx, dy = y - gy;
            switch (orient) { case 0: X = dx; Y = dy; break; case 2: X = -dx; Y = dy; break; case 1: X = -dy; Y = dx; break; default: X = dy; Y = -dx; break; }
            X += ox; Y += oy;
        };
        for (int y = 0; y < w.h; y++) for (int x = 0; x < w.w; x++) {
            px v = w.d[y * w.w + x]; if (v == KEYC) continue;
            int X, Y; map(x, y, X, Y); gfx_pset(c, X, Y, v);
        }
        if (muzzle > 0 && idef(weaponId).cat == IC_FIREARM) {   // starburst flash like Try 2
            int mx, my; map(g_heldMuzzle[weaponId][0] + 1, g_heldMuzzle[weaponId][1], mx, my);
            int fxd = orient == 0 ? 1 : orient == 2 ? -1 : 0, fyd = orient == 1 ? 1 : orient == 3 ? -1 : 0;
            mx += fxd * 2; my += fyd * 2;   // flash starts past the barrel tip
            px core = rgb(255, 250, 220), mid = rgb(255, 214, 90), outer = rgb(240, 140, 40);
            for (int k = 1; k <= 12; k++) { px col = k < 5 ? core : (k < 9 ? mid : outer); gfx_pset(c, mx + fxd * k, my + fyd * k, col); if (k < 8) { gfx_pset(c, mx + fxd * k + fyd, my + fyd * k + fxd, k < 4 ? core : mid); gfx_pset(c, mx + fxd * k - fyd, my + fyd * k - fxd, k < 4 ? core : mid); } }   // forward cone
            for (int k = 2; k <= 6; k++) {
                gfx_pset(c, mx + fyd * k, my + fxd * k, k == 1 ? mid : outer); gfx_pset(c, mx - fyd * k, my - fxd * k, k == 1 ? mid : outer);   // side rays
                gfx_pset(c, mx + (fxd + fyd) * k, my + (fyd + fxd) * k, outer); gfx_pset(c, mx + (fxd - fyd) * k, my + (fyd - fxd) * k, outer);   // diagonals
            }
            gfx_fill(c, mx - 2, my - 3, 5, 7, mid); gfx_fill(c, mx - 3, my - 2, 7, 5, mid); gfx_fill(c, mx - 2, my - 2, 5, 5, core); gfx_fillA(c, mx - 6, my - 5, 13, 11, rgb(255, 220, 120), 90);
        }
    };
    if (behind) weapon();
    gfx_blitEx(c, s, sx, sy, flip, 256, rgb(255, 250, 240), flash > 0 ? 170 : 0);
    if (!behind) weapon();
}

struct DrawItem { int16_t y; uint8_t kind; uint16_t idx; };   // idx: prop index can exceed 255
enum { DK_PROP, DK_ZOMB, DK_UNIT, DK_NPC, DK_BAG };

// optional stage profiler (zs_tool --bench sets the clock; zero cost otherwise)
double (*g_profNow)() = nullptr; double g_prof[8];
bool g_lowEnd = false, g_skipBottom = false;
static double profT;
bool g_overview;   // review tool only: whole-map captures are stitched from several views, so they skip the screen-edge vignette
static inline void profMark(int i) { if (!g_profNow) return; double t = g_profNow(); if (i >= 0) g_prof[i] += t - profT; profT = t; }
// Soft light pools: falloff masks ((1-d)^2, 0..255) cached per size, so each pixel is one multiply and a blend.
// additive = the light adds to what is there (glows, muzzle flash); otherwise it tints towards the colour.
static void lightPool(Canvas& c, int cx, int cy, int rx, int ry, px col, int amax, bool additive = false) {
    if (amax <= 0 || rx < 1 || ry < 1) return;
    struct Mask { int rx, ry; uint8_t* w; };
    static Mask masks[16]; static int nmask = 0;
    const uint8_t* w = nullptr;
    for (int k = 0; k < nmask; k++) if (masks[k].rx == rx && masks[k].ry == ry) { w = masks[k].w; break; }
    int W = 2 * rx + 1, H = 2 * ry + 1;
    if (!w) {
        uint8_t* nw = (uint8_t*)malloc(W * H);
        for (int dy = -ry; dy <= ry; dy++) for (int dx = -rx; dx <= rx; dx++) {
            float d = (float)(dx * dx) / (rx * rx) + (float)(dy * dy) / (ry * ry);
            nw[(dy + ry) * W + dx + rx] = d >= 1 ? 0 : (uint8_t)(255 * (1 - d) * (1 - d));
        }
        if (nmask < 16) masks[nmask++] = Mask{rx, ry, nw}; else { free(masks[0].w); masks[0] = Mask{rx, ry, nw}; }
        w = nw;
    }
    int y0 = cy - ry < 0 ? 0 : cy - ry, y1 = cy + ry >= c.h ? c.h - 1 : cy + ry;
    int x0 = cx - rx < 0 ? 0 : cx - rx, x1 = cx + rx >= c.w ? c.w - 1 : cx + rx;
    for (int yy = y0; yy <= y1; yy++) {
        const uint8_t* wr = w + (yy - cy + ry) * W + (x0 - cx + rx); px* row = c.p + yy * c.w;
        if (additive) { for (int xx = x0; xx <= x1; xx++, wr++) { int a = (*wr * amax) >> 8; if (a > 3) row[xx] = gfx_addf(row[xx], col, a); } }
        else for (int xx = x0; xx <= x1; xx++, wr++) { int a = (*wr * amax) >> 8; if (a > 3) row[xx] = gfx_mixf(row[xx], col, a); }
    }
}

// Weather is cosmetic and fixed per day and place (never on day 1, so the first outing is clear): about one day in four it rains.
static bool rainingNow() {
    if (!G.inLevel || G.day <= 1) return false;
    return hash32(G.day * 31u + (uint32_t)(G.curLoc + 2) * 7u) % 4 == 0;
}

void render_world(Canvas& c, float alpha) {
    profMark(-1); s_nflame = 0;
    Map& m = G.map;
    float sh = G.shake > 0 && G.set.shake ? G.shake : 0;
    camX = (int)(G.camX + g_fxKickX) + (sh > 0 ? (int)((hash32(G.tick * 7) % 3) - 1) * (int)ceilf(sh) : 0);   // + recoil kick (render only)
    camY = (int)(G.camY + g_fxKickY) + (sh > 0 ? (int)((hash32(G.tick * 13) % 3) - 1) * (int)ceilf(sh) : 0);
    render_compute_light(); profMark(0);
    // building fronts and their signs only show from the street: indoors you look over the cut-away walls
    bool playerOutside = false;
    { const Unit& pu = G.u[G.ctrl]; int ptx = (int)(pu.x / 16), pty = (int)(pu.y / 16); playerOutside = G.nu > 0 && m.in(ptx, pty) && !(m.flags[m.idx(ptx, pty)] & F_INDOOR); }
    int tx0 = camX / 16, ty0 = camY / 16, tx1 = (camX + view_w()) / 16 + 1, ty1 = (camY + view_h()) / 16 + 1;
    // the static level layer (floors, walls, fences, shadows, stains, blood) was drawn once at level load:
    // copy the visible rows. Anything outside the map is the void.
    {
        const px VOID = rgb(10, 10, 20);
        for (int y = 0; y < c.h; y++) {
            int wy = y + camY; px* dst = c.p + y * c.w; int n = c.w, dx = 0, sx = camX;
            if (wy < 0 || wy >= g_bakeH || g_bakeW != m.w * 16) { for (int x = 0; x < n; x++) dst[x] = VOID; continue; }
            if (sx < 0) { dx = -sx < n ? -sx : n; for (int x = 0; x < dx; x++) dst[x] = VOID; n -= dx; sx = 0; }
            if (sx + n > g_bakeW) { int keep = g_bakeW - sx; if (keep < 0) keep = 0; for (int x = dx + keep; x < c.w; x++) dst[x] = VOID; n = keep; }
            if (n > 0) memcpy(dst + dx, g_bake + wy * g_bakeW + sx, n * sizeof(px));
        }
    }
    // the few things on the ground that change: doors / windows / gates, the pulsing extraction zone, water glints
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int bx = tx * 16 - camX, by = ty * 16 - camY;
        int i = m.idx(tx, ty); uint8_t t = m.t[i];
        if (m.barAt[i] >= 0) {
            const Barrier& b = m.bar[m.barAt[i]];
            int jx = b.shake > 0 ? ((G.tick & 2) ? 1 : -1) : 0;
            gfx_blit(c, barrierSprite(tx, ty), bx + jx, by);
            float hp = b.maxHp ? (float)b.hp / b.maxHp : 1;
            if (b.state != BS_BROKEN && b.state != BS_OPEN && hp < 0.66f) gfx_blit(c, g_tile[hp < 0.33f ? TA_CRACK2 : TA_CRACK1][0], bx + jx, by);
        } else if (t == T_EXTRACT) {
            int ph = ((int)(G.uiTime * 6) + tx + ty) & 7;   // chevrons marching towards the exit
            if (ph < 3) gfx_rect(c, bx + 3, by + 3, 10, 10, ph == 1 ? rgb(190, 250, 90) : rgb(124, 200, 60));
        } else if (t == T_WATER) {
            uint32_t h = hash32(tx * 977 + ty * 131 + (uint32_t)(G.uiTime * 2.5f) * 7919);
            gfx_fill(c, bx + (h & 7) + 2, by + ((h >> 4) & 7) + 3, 3, 1, rgb(120, 180, 196));
            if (h & 0x100) gfx_fill(c, bx + ((h >> 9) & 7) + 4, by + ((h >> 13) & 7) + 5, 2, 1, rgb(84, 142, 160));
        }
    }
    // exterior facades (Try 2): outer walls facing the camera stand 2.5 tiles tall in red brick with a cornice,
    // warm windows and glass double doors. Only while the player is outside, so interiors stay readable.
    {
        const Unit& pu = G.u[G.ctrl];
        int ptx = (int)(pu.x / 16), pty = (int)(pu.y / 16);
        bool outside = G.nu > 0 && m.in(ptx, pty) && !(m.flags[m.idx(ptx, pty)] & F_INDOOR);
        if (outside && G.scene == SC_EXPEDITION)
            for (int ty = ty0; ty <= ty1 + 3; ty++) for (int tx = tx0; tx <= tx1; tx++) {
                if (!m.in(tx, ty) || !m.in(tx, ty + 1) || !m.in(tx, ty - 1)) continue;
                int i = m.idx(tx, ty); uint8_t t = m.t[i];
                if (t != T_WALL && t != T_DOOR) continue;
                if ((m.flags[m.idx(tx, ty + 1)] & F_INDOOR) || !(m.flags[m.idx(tx, ty - 1)] & F_INDOOR)) continue;
                int kind = 0;
                if (t == T_DOOR && m.barAt[i] >= 0) kind = m.bar[m.barAt[i]].kind == BK_WINDOW ? 1 : 2;
                gfx_blit(c, facade_sprite(tx, ty, kind), tx * 16 - camX, ty * 16 + 16 - 40 - camY);
            }
    }
    // storefront signs on the other launch maps, on the facade beside the main doors
    static const struct { int map, tx, ty; const char* text; px bg, rim, ink; bool cross; } SIGNS[] = {
        {MAP_HOSPITAL, 16, 33, "HOSPITAL", rgb(200, 202, 204), rgb(244, 244, 240), rgb(150, 30, 30), true},
        {MAP_MARKET, 22, 28, "MARKET", rgb(40, 96, 58), rgb(96, 156, 104), pal::cream, false},
        {MAP_YARD, 15, 33, "RAIL DEPOT", rgb(104, 66, 38), rgb(160, 110, 66), pal::cream, false},
    };
    for (const auto& sg : SIGNS) if (m.id == sg.map && playerOutside) {
        int w = gfx_textW(sg.text) + 16 + (sg.cross ? 12 : 0), sx = sg.tx * 16 + 6 - camX, sy = sg.ty * 16 - 20 - camY, tx = sx + 8;
        gfx_fill(c, sx - 1, sy - 1, w + 2, 13, pal::ink);
        gfx_fill(c, sx, sy, w, 11, sg.bg); gfx_fill(c, sx, sy, w, 1, sg.rim);
        if (sg.cross) { gfx_fill(c, sx + 6, sy + 4, 7, 3, rgb(196, 40, 36)); gfx_fill(c, sx + 8, sy + 2, 3, 7, rgb(196, 40, 36)); tx += 12; }
        gfx_text(c, tx, sy + 2, sg.text, sg.ink, 1, true);
    }
    // police station signage above the front doors (Try 2 look)
    if (m.id == MAP_POLICE && playerOutside) {
        int sx = 19 * 16 + 14 - camX, sy = 32 * 16 - 20 - camY;   // on the brick facade, left of the double doors
        gfx_fill(c, sx - 1, sy - 1, 74, 13, pal::ink);
        gfx_fill(c, sx, sy, 72, 11, rgb(34, 46, 84)); gfx_fill(c, sx, sy, 72, 1, rgb(70, 90, 140));
        gfx_text(c, sx + 9, sy + 2, "POLICE", pal::cream, 1, true); gfx_text(c, sx + 10, sy + 2, "POLICE", pal::cream, 1, false);
        bool on = ((int)(G.uiTime * 3)) & 1;   // emergency strobes either side
        gfx_fill(c, sx - 8, sy + 2, 4, 4, on ? rgb(240, 60, 50) : rgb(90, 30, 30));
        gfx_fill(c, sx + 76, sy + 2, 4, 4, on ? rgb(60, 90, 240) : rgb(30, 40, 90));
    }
    // corpses lie under everything (blood is stamped into the level layer)
    for (int k = 0; k < G.nz; k++) {
        Zombie& z = G.z[k];
        if (z.state != ZS_DEAD || z.anim.state != AnimState::Dead || fx_corpseBaked(k)) continue;   // settled bodies are stamped into the floor
        if (!visibleAt(z.x, z.y)) continue;
        drawChar(c, true, z.variant, z.x, z.y, z.dir, z.anim, z.hx, z.hy, 0, 0, 0, false);
    }
    // soft drop shadows (light from the north-west), drawn before anything stands on the floor
    auto shadow = [&](const Sprite& s, int x, int y, int ox, int oy, int lvl) {
        for (int j = 0; j < s.h; j++) {
            int py = y + j + oy; if (py < 0 || py >= c.h) continue;
            for (int i = 0; i < s.w; i++) {
                int pxx = x + i + ox; if (pxx < 0 || pxx >= c.w || s.d[j * s.w + i] == KEYC) continue;
                c.p[py * c.w + pxx] = gfx_scale(c.p[py * c.w + pxx], lvl);
            }
        }
    };
    // furniture shadows are baked into the level layer; only settlement buildings change (they get built and upgraded)
    if (G.scene == SC_SETTLEMENT) for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k];
        if (p.type != PR_PLOT || !G.st.bLevel[p.special]) continue;
        const Sprite& bs = g_bldWorld[p.special][G.st.bLevel[p.special] > 3 ? 3 : G.st.bLevel[p.special]];
        shadow(bs, p.tx * 16 - camX + 24 - bs.w / 2, p.ty * 16 - camY + 48 - bs.h, 4, 3, 150);
    }
    // y-sorted scene
    static DrawItem items[Map::MAXPROP + MAX_ZOMB + MAX_UNITS + MAX_SURV + MAX_BAGS];
    int n = 0;
    for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k];
        if (p.tx + p.tw < tx0 - 2 || p.tx > tx1 + 2 || p.ty + p.th < ty0 - 2 || p.ty > ty1 + 2) continue;
        items[n++] = DrawItem{(int16_t)((p.ty + p.th) * 16 - 1), DK_PROP, (uint16_t)k};
    }
    for (int k = 0; k < G.nz; k++) {
        Zombie& z = G.z[k];
        if (z.state == ZS_DEAD && z.anim.state == AnimState::Dead) continue;
        items[n++] = DrawItem{(int16_t)ilerp(z.py, z.y, alpha), DK_ZOMB, (uint8_t)k};
    }
    for (int k = 0; k < G.nu; k++) items[n++] = DrawItem{(int16_t)ilerp(G.u[k].py, G.u[k].y, alpha), DK_UNIT, (uint8_t)k};
    if (G.scene == SC_SETTLEMENT) for (int k = 0; k < G.st.nsurv; k++) {
        const Survivor& s = G.st.surv[k];
        if (!s.alive || s.onExpedition || k == G.u[G.ctrl].sv) continue;
        items[n++] = DrawItem{(int16_t)g_npc[k].y, DK_NPC, (uint8_t)k};
    }
    if (G.curLoc >= 0) for (int b = 0; b < MAX_BAGS; b++) if (G.loc[G.curLoc].bags[b].used) items[n++] = DrawItem{(int16_t)G.loc[G.curLoc].bags[b].y, DK_BAG, (uint8_t)b};
    for (int i = 1; i < n; i++) { DrawItem k = items[i]; int j = i - 1; while (j >= 0 && items[j].y > k.y) { items[j + 1] = items[j]; j--; } items[j + 1] = k; }

    for (int k = 0; k < n; k++) {
        const DrawItem& d = items[k];
        if (d.kind == DK_PROP) {
            const Prop& p = m.prop[d.idx];
            int x = p.tx * 16 - camX, y = p.ty * 16 - camY;
            int pox, poy;
            if (const Sprite* ps = prop_sprite(m, p, pox, poy)) {   // furniture and vehicles
                gfx_blit(c, *ps, x + pox, y + poy);
                if (prop_fx(m, p) == 2) drawFlames(c, x - 4 + 32, y - 12 + 14);   // a wreck that is still burning
                continue;
            }
            switch (p.type) {
            case PR_TREE: {
                // gentle sway: canopy offset by a slow sine, trunk static
                int sway = (int)(sinf(G.uiTime * 0.9f + p.tx * 0.7f) * 1.2f);
                gfx_fill(c, x + 5, y + 4, 6, 12, rgb(22, 22, 34)); gfx_fill(c, x + 6, y + 4, 4, 11, rgb(112, 78, 52)); gfx_fill(c, x + 6, y + 4, 1, 11, rgb(150, 108, 72));
                gfx_blit(c, g_spr[S_TREE + (p.tx + p.ty) % 2], x - 8 + sway, y - 20);
                break;
            }
            case PR_TORCH: gfx_blit(c, g_spr[S_TORCH0 + (((int)(G.uiTime * 7) + p.tx) & 1)], x, y - 4); break;
            case PR_FIRE: {   // Try 2 fire pit: wide stone ring, crossed logs, three flame tongues, sparks
                int cx = x + 8, cy = y + 12;
                for (int a = 0; a < 16; a++) {   // back half of the ring first
                    float ang = a * 0.3927f; int sx = cx + (int)(cosf(ang) * 12), sy = cy + (int)(sinf(ang) * 5);
                    if (sinf(ang) > 0) continue;
                    gfx_fill(c, sx - 2, sy - 1, 4, 3, rgb(40, 38, 40)); gfx_fill(c, sx - 1, sy - 1, 3, 2, a & 1 ? rgb(150, 146, 138) : rgb(122, 118, 112)); gfx_pset(c, sx - 1, sy - 1, rgb(186, 182, 172));
                }
                gfx_fill(c, cx - 9, cy - 2, 18, 5, rgb(34, 26, 22));                 // ash bed
                gfx_line(c, cx - 7, cy + 1, cx + 6, cy - 2, rgb(70, 44, 28)); gfx_line(c, cx - 6, cy - 2, cx + 7, cy + 1, rgb(86, 54, 32));   // crossed logs
                gfx_pset(c, cx - 2, cy, rgb(255, 120, 40)); gfx_pset(c, cx + 3, cy - 1, rgb(255, 160, 60));                                   // embers
                drawFlames(c, cx, cy);
                for (int a = 0; a < 16; a++) {   // front half of the ring over the flames' base
                    float ang = a * 0.3927f; int sx = cx + (int)(cosf(ang) * 12), sy = cy + (int)(sinf(ang) * 5);
                    if (sinf(ang) <= 0) continue;
                    gfx_fill(c, sx - 2, sy - 1, 4, 3, rgb(40, 38, 40)); gfx_fill(c, sx - 1, sy - 1, 3, 2, a & 1 ? rgb(150, 146, 138) : rgb(122, 118, 112)); gfx_pset(c, sx - 1, sy - 1, rgb(186, 182, 172));
                }
                for (int k = 0; k < 5; k++) { float t = fmodf(G.uiTime * 0.7f + k * 0.2f, 1.0f); gfx_pset(c, cx + (int)(sinf(t * 9 + k * 1.7f) * 4), cy - 14 - (int)(t * 18), t < 0.5f ? rgb(255, 214, 100) : rgb(236, 120, 40)); }
                break;
            }
            case PR_RADIO: gfx_blit(c, g_spr[S_RADIO], x, y); if ((int)(G.uiTime * 2) & 1) gfx_pset(c, x + 8, y + 5, rgb(255, 150, 130)); break;
            case PR_LAMP: {   // tall iron lamp post with a boxed lantern (Try 2)
                int lx = x + 7;
                gfx_fill(c, lx - 3, y + 12, 7, 3, rgb(30, 30, 34)); gfx_fill(c, lx - 2, y + 11, 5, 1, rgb(60, 60, 66));
                gfx_fill(c, lx - 1, y - 12, 3, 24, rgb(26, 26, 30)); gfx_fill(c, lx, y - 12, 1, 24, rgb(70, 70, 78));
                gfx_fill(c, lx - 4, y - 22, 9, 11, rgb(26, 26, 30)); gfx_fill(c, lx - 3, y - 21, 7, 8, rgb(255, 206, 100)); gfx_fill(c, lx - 2, y - 20, 2, 3, rgb(255, 244, 196));
                gfx_fill(c, lx, y - 21, 1, 8, rgb(40, 36, 34)); gfx_fill(c, lx - 3, y - 17, 7, 1, rgb(40, 36, 34));   // lantern frame + panes
                gfx_fill(c, lx - 5, y - 24, 11, 2, rgb(40, 40, 46)); gfx_pset(c, lx, y - 25, rgb(40, 40, 46));
                break;
            }
            case PR_GENERATOR: if (G.st.generator) gfx_blit(c, g_spr[S_GENPROP0 + ((int)(G.uiTime * 14) & 1)], x, y); break;
            case PR_PLOT: {
                int b = p.special, lv = G.st.bLevel[b] > 3 ? 3 : G.st.bLevel[b];
                const Sprite& bs = g_bldWorld[b][lv]; const BldFx& fx = g_bldFx[b][lv];
                int bx = x + 24 - bs.w / 2, by = y + 48 - bs.h;
                gfx_blit(c, bs, bx, by);
                if (lv > 0) {
                    float dark = 1.0f - daylight();
                    // lit windows: the odd flicker, and a warm halo after dusk
                    for (int k = 0; k < fx.nwin; k++) {
                        const int8_t* r = fx.win[k];
                        if (dark > 0.35f) gfx_fillA(c, bx + r[0] - 2, by + r[1] - 2, r[2] + 4, r[3] + 4, rgb(255, 190, 90), (int)(80 * dark));
                        if ((hash32((uint32_t)(G.uiTime * 6) + b * 31 + k) % 23) == 0) gfx_fillA(c, bx + r[0] + 1, by + r[1] + 1, r[2] - 2, r[3] - 2, rgb(120, 70, 30), 90);
                    }
                    if (fx.smokeX >= 0)   // chimney smoke drifting up and away
                        for (int k = 0; k < 4; k++) {
                            float t = fmodf(G.uiTime * 0.45f + k * 0.25f + b * 0.1f, 1.0f);
                            int sx = bx + fx.smokeX + (int)(sinf(t * 5 + k) * 2 + t * 7), sy = by + fx.smokeY - (int)(t * 22), sz = 2 + (int)(t * 4);
                            gfx_fillA(c, sx - sz / 2, sy - sz / 2, sz, sz, rgb(150, 150, 156), (int)(150 * (1 - t)));
                        }
                    if (fx.flagX >= 0)    // pennant snapping in the wind
                        for (int i = 0; i < 8; i++) {
                            int wob = (int)(sinf(G.uiTime * 7 - i * 0.9f) * 1.4f), fh = 5 - i / 2;
                            for (int j = 0; j < fh; j++) gfx_pset(c, bx + fx.flagX + i, by + fx.flagY + j + wob + i / 3, j == 0 ? rgb(230, 80, 60) : rgb(190, 44, 36));
                        }
                }
                if (lv > 0 && G.st.bDamage[b]) {
                    for (int s = 0; s < G.st.bDamage[b] * 3; s++) gfx_fill(c, bx + 6 + (hash32(b * 17 + s) % (bs.w - 12)), by + bs.h / 3 + (hash32(b * 7 + s) % (bs.h / 2)), 3, 2, rgb(30, 26, 22));
                    int sy = (int)fmodf(G.uiTime * 10 + b * 9, 20);   // drifting smoke
                    gfx_fillA(c, bx + bs.w / 2, by + 4 - sy, 4, 4, rgb(90, 90, 96), 140 - sy * 6);
                }
                if (G.st.bBuildFx[b] > 0) {   // construction dust + scaffold flicker
                    for (int s = 0; s < 6; s++) gfx_pset(c, bx + (hash32(G.tick + s) % bs.w), by + bs.h - 18 + (hash32(G.tick * 3 + s) % 16), rgb(170, 150, 120));
                    if ((G.tick / 4) & 1) gfx_rect(c, bx + 1, by + 1, bs.w - 2, bs.h - 2, pal::gold);
                }
                break;
            }
            }
        } else if (d.kind == DK_ZOMB) {
            Zombie& z = G.z[d.idx];
            float x = ilerp(z.px, z.x, alpha), y = ilerp(z.py, z.y, alpha);
            if (!visibleAt(x, y)) continue;   // hidden in the dark / behind walls
            drawChar(c, true, z.variant, x, y, z.dir, z.anim, z.hx, z.hy, z.flash, 0, 0);
        } else if (d.kind == DK_UNIT) {
            Unit& u = G.u[d.idx];
            float x = ilerp(u.px, u.x, alpha), y = ilerp(u.py, u.y, alpha);
            if (d.idx != G.ctrl && !visibleAt(x, y)) continue;
            int variant = u.sv >= 0 ? G.st.surv[u.sv].variant : u.npcVariant;
            int wid = 0;
            if (u.sv >= 0) { const Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0 && s.equip < s.pack.n) wid = s.pack.it[s.equip].id; }
            drawChar(c, false, variant, x, y, u.dir, u.anim, u.fx, u.fy, u.hurtT > 0.15f ? 1 : 0, wid, u.muzzleT);
            if (u.escort && u.order == ORD_HOLD && ((int)(G.uiTime * 2) & 1)) gfx_textC(c, (int)x - camX, (int)y - camY - 34, "HELP!", pal::cream);
        } else if (d.kind == DK_NPC) {
            Settler& s = g_npc[d.idx];
            if (s.asleep) gfx_blit(c, g_spr[S_BEDROLL], (int)s.x - camX - 8, (int)s.y - camY - 8);
            drawChar(c, false, G.st.surv[d.idx].variant, s.x, s.y, s.dir, s.anim, 0, s.dir == 6 ? -1.0f : 1.0f, 0, G.st.surv[d.idx].job == JOB_GUARD ? IT_PISTOL : 0, 0);
            if (s.working && s.anim.state == AnimState::Work && s.anim.frame == 1) gfx_pset(c, (int)s.x - camX + 4, (int)s.y - camY - 10, rgb(250, 230, 160));
            if (s.asleep && ((int)(G.uiTime * 1.5f + d.idx) % 3 == 0)) gfx_text(c, (int)s.x - camX + 4, (int)s.y - camY - 20, "z", pal::dim);
        } else if (d.kind == DK_BAG) {
            const Bag& b = G.loc[G.curLoc].bags[d.idx];
            gfx_blit(c, g_spr[S_BAG], (int)b.x - camX - 8, (int)b.y - camY - 8);
        }
    }
    // visitor waiting at the gate
    if (G.scene == SC_SETTLEMENT && G.st.visitorPresent) {
        Animator a; a.play(AnimState::Idle, true); a.t = 0; a.frame = (uint8_t)((int)(G.uiTime * 1.5f) % 6);
        drawChar(c, false, G.st.visitor.variant, G.map.exitX * 16 + 8.0f, (G.map.exitY + 1) * 16 + 10.0f, 6, a, 0, -1, 0, 0, 0);
    }
    for (auto& p : G.part) {
        if (p.life <= 0) continue;
        int x = (int)p.x - camX, y = (int)(p.y - p.z) - camY;
        switch (p.kind) {
        case PK_MUZZLE: case PK_CASING: break;   // drawn by the effects layer (fx.cpp) now
        case PK_SMOKE: case PK_DUST: gfx_fillA(c, x - 1, y - 1, 3, 3, p.col, (int)(150 * p.life / p.max)); break;
        case PK_LEAF: gfx_pset(c, x, y, p.col); gfx_pset(c, x + 1, y + ((int)(p.life * 4) & 1), gfx_scale(p.col, 170)); break;
        default: gfx_pset(c, x, y, p.col); if (p.kind == PK_BLOOD) gfx_pset(c, x + 1, y, p.col); break;
        }
    }

    fx_render(c, camX, camY);
    if (rainingNow()) {   // slanting streaks and splashes, only where there is sky overhead
        auto open = [&](int x, int y) { int wx = x + camX, wy = y + camY; if (wx < 0 || wy < 0 || wx >= m.w * 16 || wy >= m.h * 16) return false; int i = m.idx(wx >> 4, wy >> 4); return !(m.flags[i] & F_INDOOR) && m.t[i] != T_WALL; };
        int n = c.w * c.h / 320;
        for (int i = 0; i < n; i++) {
            uint32_t h = hash32(i * 7919u + 17u);
            float ph = fmodf(G.uiTime * (150 + (h % 80)) + (float)((h >> 8) % 400), (float)(c.h + 24));
            int x = (int)((h >> 4) % (uint32_t)(c.w + 40)) - (int)(ph * 0.25f), y = (int)ph - 12;
            if (!open(x, y)) continue;
            gfx_pset(c, x, y, rgb(176, 196, 220)); gfx_pset(c, x + 1, y - 2, rgb(136, 156, 186)); gfx_pset(c, x + 1, y - 3, rgb(112, 132, 164));
        }
        for (int i = 0; i < n / 3; i++) {
            uint32_t h = hash32(i * 31u + (uint32_t)(G.uiTime * 12) * 977u); int x = (int)(h % (uint32_t)c.w), y = (int)((h >> 10) % (uint32_t)c.h);
            if (!open(x, y)) continue;
            gfx_pset(c, x, y, rgb(196, 212, 230)); gfx_pset(c, x - 1, y, rgb(140, 160, 190)); gfx_pset(c, x + 1, y, rgb(140, 160, 190));
        }
    }
    flamesKeep(c);
    profMark(1);   // tiles, props, characters, particles
    // ---- lighting: the per-tile light map is sampled on an 8 px lattice and interpolated across every cell,
    // with ordered dithering between the 32 blend levels, so light falls off smoothly instead of in blocks.
    // Shadows blend towards a cool tint (navy by day, plum at dusk, deep blue at night), never towards grey.
    float day = daylight();
    px tint = G.scene == SC_SETTLEMENT ? (day > 0.15f ? rgb(46, 30, 56) : rgb(12, 14, 42))
                                       : (day > 0.6f ? rgb(24, 26, 56) : (day > 0.15f ? rgb(54, 24, 60) : rgb(8, 12, 40)));
    {
        enum { LW = TOP_W / 8 + 4, LH = TOP_H / 8 + 4 };
        static uint8_t lat[LW * LH];
        const int VW = c.w, VH = c.h;
        int li0 = camX >> 3, lj0 = camY >> 3, lw = (VW >> 3) + 2, lh = (VH >> 3) + 2;
        if (lw > LW) lw = LW; if (lh > LH) lh = LH;
        int dayAdd = (int)(day * 82), vigK = g_overview ? 0 : (G.scene == SC_SETTLEMENT ? 110 : 78); bool rain = rainingNow();   // by day rooms are fully lit, so the torch beam does not show
        // what the eye gets from a tile: the gameplay light, plus daylight filling rooms you can see or remember,
        // plus a moonlit floor outdoors so the night stays readable (gameplay still uses the true light map)
        auto tileV = [&](int tx, int ty) -> int {
            tx = tx < 0 ? 0 : (tx >= m.w ? m.w - 1 : tx); ty = ty < 0 ? 0 : (ty >= m.h ? m.h - 1 : ty);
            int i = m.idx(tx, ty), v = G.light[i];
            if (m.flags[i] & F_INDOOR) { if (v >= 60 || G.explored[i]) v += dayAdd; }
            else { if (G.scene == SC_EXPEDITION && v < 150) v = 150 - (150 - v) / 3; if (rain) v = v * 218 >> 8; }   // overcast
            return v > 255 ? 255 : v;
        };
        int nfl; const FxLight* fls = fx_lights(nfl);
        for (int j = 0; j < lh; j++) for (int i = 0; i < lw; i++) {
            int X = (li0 + i) * 8, Y = (lj0 + j) * 8;
            int v = (tileV((X - 4) >> 4, (Y - 4) >> 4) + tileV((X + 4) >> 4, (Y - 4) >> 4) + tileV((X - 4) >> 4, (Y + 4) >> 4) + tileV((X + 4) >> 4, (Y + 4) >> 4)) >> 2;
            // vignette: darker towards the screen edges (folded into the same pass)
            int sx = X - camX - VW / 2, sy = Y - camY - VH / 2;
            int d2 = (sx * sx * 256) / ((VW / 2) * (VW / 2)) + (sy * sy * 256) / ((VH / 2) * (VH / 2));   // 0 centre .. 512 corner
            v = v * (256 - ((vigK * d2) >> 8)) >> 8;
            // muzzle flashes and impacts throw real light for a few frames (render only: reveals nothing hidden)
            for (int k = 0; k < nfl; k++) {
                const FxLight& fl = fls[k]; if (fl.life <= 0) continue;
                float dx = X - fl.x, dy = Y - fl.y, dd = dx * dx + dy * dy;
                if (dd < fl.r * fl.r) v += (int)(230 * (1 - sqrtf(dd) / fl.r) * (fl.life / fl.max));
            }
            lat[j * LW + i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
        // tint * (32 - level), premultiplied once per frame for the packed 565 blend
        const unsigned MASK = 0x07E0F81Fu, xt = (tint | ((unsigned)tint << 16)) & MASK;
        unsigned tintMul[33]; for (int k = 0; k <= 32; k++) tintMul[k] = xt * (32 - k);
        static const uint8_t BAYER[4][4] = {{0, 4, 1, 5}, {6, 2, 7, 3}, {1, 5, 0, 4}, {7, 3, 6, 2}};
        for (int j = 0; j + 1 < lh; j++) for (int i = 0; i + 1 < lw; i++) {
            int v00 = lat[j * LW + i], v10 = lat[j * LW + i + 1], v01 = lat[(j + 1) * LW + i], v11 = lat[(j + 1) * LW + i + 1];
            if (v00 >= 250 && v10 >= 250 && v01 >= 250 && v11 >= 250) continue;   // fully lit: untouched
            int x0 = (li0 + i) * 8 - camX, y0 = (lj0 + j) * 8 - camY;
            int xa = x0 < 0 ? 0 : x0, xb = x0 + 8 > VW ? VW : x0 + 8;
            if (xa >= xb) continue;
            bool flat = v00 == v10 && v00 == v01 && v00 == v11;
            for (int r = 0; r < 8; r++) {
                int y = y0 + r; if (y < 0 || y >= VH) continue;
                px* row = c.p + y * VW;
                if (flat) {
                    unsigned t5 = (unsigned)(v00 + 4) >> 3, tm = tintMul[t5];
                    for (int x = xa; x < xb; x++) { unsigned p = row[x]; p = (p | (p << 16)) & MASK; p = ((tm + p * t5) >> 5) & MASK; row[x] = (px)(p | (p >> 16)); }
                    continue;
                }
                int a = v00 * (8 - r) + v01 * r, b = v10 * (8 - r) + v11 * r;   // x8
                int v = a * 8 + (b - a) * (xa - x0), dv = b - a;                 // x64 along the row
                const uint8_t* bay = BAYER[y & 3];
                for (int x = xa; x < xb; x++, v += dv) {
                    unsigned t5 = (unsigned)((v >> 6) + bay[x & 3]) >> 3;
                    unsigned p = row[x]; p = (p | (p << 16)) & MASK; p = ((tintMul[t5] + p * t5) >> 5) & MASK; row[x] = (px)(p | (p >> 16));
                }
            }
        }
    }
    profMark(2);
    // settlement colour grade (Try 2 evening): +15% saturation through a 64K lookup table built once,
    // so the per-frame cost is one table read per pixel (it used to be ~20 ALU ops per pixel)
    if (G.scene == SC_SETTLEMENT && !gfx_fast()) {   // a 128 KB table read per pixel: dear on the Old 3DS's small cache
        static px* lut = nullptr;
        if (!lut) {
            lut = (px*)malloc(sizeof(px) * 65536);
            for (int v = 0; v < 65536; v++) {
                int r = ((v >> 11) & 31) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3, l = (r * 3 + g * 6 + b) / 10;
                r = l + (r - l) * 23 / 20; g = l + (g - l) * 23 / 20; b = l + (b - l) * 23 / 20;
                r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
                lut[v] = rgb(r, g, b);
            }
        }
        px* p = c.p; px* end = c.p + c.w * c.h;
        while (p < end) { *p = lut[*p]; p++; }
    }
    profMark(3);
    // ---- post-light overlays (stay readable at night)
    if (day < 0.97f) {
        // warm light pools around fires, lamps and torches (Try 2 dusk): per-pixel radial blend
        float dk = fminf(1.0f, (1.0f - day) / 0.45f);   // lights are at full strength well before full dark

        float flick = 0.9f + 0.1f * sinf(G.uiTime * 13) * sinf(G.uiTime * 7.3f);
        for (int k = 0; k < m.nprop; k++) {
            const Prop& p = m.prop[k];
            int x = p.tx * 16 - camX, y = p.ty * 16 - camY;
            if (x < -60 || y < -60 || x > c.w + 60 || y > c.h + 60) continue;
            if (p.type == PR_FIRE) {
                lightPool(c, x + 8, y + 10, 70, 40, rgb(255, 140, 50), (int)(170 * dk * flick));
            } else if (p.type == PR_LAMP) {
                lightPool(c, x + 7, y + 4, 36, 20, rgb(255, 180, 80), (int)(140 * dk));
                lightPool(c, x + 7, y - 17, 9, 9, rgb(255, 210, 120), (int)(160 * dk));
                gfx_fill(c, x + 4, y - 21, 3, 3, rgb(255, 230, 150)); gfx_fill(c, x + 8, y - 21, 3, 3, rgb(255, 214, 110)); gfx_fill(c, x + 4, y - 16, 3, 3, rgb(255, 214, 110)); gfx_fill(c, x + 8, y - 16, 3, 3, rgb(255, 200, 100));
            } else if (p.type == PR_TENT) {
                lightPool(c, x + 16, y + 6, 26, 12, rgb(255, 160, 70), (int)(90 * dk));
                gfx_fillA(c, x - 7 + 17, y - 24 + 26, 12, 11, rgb(240, 172, 76), (int)(110 * dk)); gfx_fillA(c, x - 7 + 19, y - 24 + 27, 8, 4, rgb(255, 220, 130), (int)(160 * dk));
            } else if (p.type == PR_TORCH) {
                lightPool(c, x + 8, y + 6, 22, 13, rgb(255, 150, 60), (int)(120 * dk * flick));
            }
        }
    }
    if (m.id == MAP_POLICE && day < 0.97f && playerOutside) {   // the lit POLICE sign stays bright after dark (Try 2)
        int sx = 19 * 16 + 14 - camX, sy = 32 * 16 - 20 - camY;
        if (sx > -90 && sx < c.w + 10 && sy > -20 && sy < c.h + 10) {
            gfx_fill(c, sx - 1, sy - 1, 74, 13, pal::ink);
            gfx_fill(c, sx, sy, 72, 11, rgb(30, 40, 74)); gfx_fill(c, sx, sy, 72, 1, rgb(80, 100, 150));
            gfx_text(c, sx + 10, sy + 2, "POLICE", rgb(250, 250, 244), 1, false);
            gfx_fillA(c, sx - 4, sy + 12, 80, 6, rgb(200, 210, 255), 40);   // spill light on the wall
            bool on = ((int)(G.uiTime * 3)) & 1;
            gfx_fill(c, sx - 8, sy + 2, 4, 4, on ? rgb(255, 70, 60) : rgb(90, 30, 30));
            gfx_fill(c, sx + 76, sy + 2, 4, 4, on ? rgb(70, 110, 255) : rgb(30, 40, 90));
        }
    }
    if (G.scene == SC_SETTLEMENT && day < 0.95f) {
        // windows stay lit through the dark: re-light the panes and bloom a warm halo
        float dk = fminf(1.0f, (1.0f - day) / 0.45f);
        for (int k = 0; k < m.nprop; k++) {
            const Prop& p = m.prop[k];
            if (p.type != PR_PLOT || !G.st.bLevel[p.special]) continue;
            int lv = G.st.bLevel[p.special] > 3 ? 3 : G.st.bLevel[p.special];
            const Sprite& bs = g_bldWorld[p.special][lv]; const BldFx& fx = g_bldFx[p.special][lv];
            int bx = p.tx * 16 + 24 - bs.w / 2 - camX, by = p.ty * 16 + 48 - bs.h - camY;
            for (int w = 0; w < fx.nwin; w++) {
                const int8_t* r = fx.win[w];
                gfx_fillA(c, bx + r[0] - 3, by + r[1] - 3, r[2] + 6, r[3] + 6, rgb(255, 170, 70), (int)(45 * dk));
                gfx_fillA(c, bx + r[0] + 1, by + r[1] + 1, r[2] - 2, r[3] - 2, rgb(255, 214, 120), (int)(200 * dk));
            }
        }
        // fireflies drifting over the grass
        for (int i = 0; i < 14; i++) {
            float t = G.uiTime * 0.3f + i * 1.7f;
            int fxp = (int)(hash32(i * 977) % (uint32_t)view_w()) + (int)(sinf(t) * 10), fyp = (int)(hash32(i * 613) % (uint32_t)view_h()) + (int)(cosf(t * 1.3f) * 6);
            if (sinf(G.uiTime * 2.2f + i * 2.1f) > 0.2f) { gfx_pset(c, fxp, fyp, rgb(220, 255, 120)); gfx_fillA(c, fxp - 1, fyp - 1, 3, 3, rgb(180, 240, 90), (int)(70 * dk)); }
        }
    }
    // some cruisers still have their lightbars turning, and a few wrecks are still burning
    {
        float dkk = fminf(1.0f, (1.0f - day) / 0.45f);
        for (int k = 0; k < m.nprop; k++) {
            const Prop& p = m.prop[k]; if (p.type != PR_CAR && p.type != PR_WRECK) continue;
            int x = p.tx * 16 - camX, y = p.ty * 16 - camY; if (x < -70 || y < -70 || x > c.w + 70 || y > c.h + 70) continue;
            int kind = prop_fx(m, p); if (!kind) continue;
            if (kind == 1) {
                int ph = ((int)(G.uiTime * 5) + p.tx) & 3, lx = x - 4 + 14, ly = y - 12 + 2;   // red, dark, blue, dark
                if (ph == 0) { gfx_fill(c, lx, ly, 6, 3, rgb(255, 96, 86)); lightPool(c, lx + 3, ly + 1, 11, 8, rgb(255, 60, 50), 190, true); if (dkk > 0.05f) lightPool(c, x + 2, y + 10, 46, 24, rgb(255, 40, 40), (int)(120 * dkk), true); }
                if (ph == 2) { gfx_fill(c, lx + 6, ly, 6, 3, rgb(120, 160, 255)); lightPool(c, lx + 9, ly + 1, 11, 8, rgb(70, 110, 255), 190, true); if (dkk > 0.05f) lightPool(c, x + 30, y + 10, 46, 24, rgb(50, 80, 255), (int)(130 * dkk), true); }
            } else {
                int fx0 = x - 4 + 32, fy0 = y - 12 + 14;
                for (int q = 0; q < 4; q++) { float t = fmodf(G.uiTime * 0.5f + q * 0.25f + p.tx * 0.13f, 1.0f); int sz = 2 + (int)(t * 5); gfx_fillA(c, fx0 + (int)(sinf(t * 5 + q) * 3 + t * 8) - sz / 2, fy0 - 10 - (int)(t * 30) - sz / 2, sz, sz, rgb(56, 56, 68), (int)(150 * (1 - t))); }
                lightPool(c, fx0, fy0 + 4, 44, 24, rgb(255, 130, 40), (int)(36 + 110 * dkk), true);
            }
        }
    }
    flamesPut(c);
    // unsearched containers in view catch the light now and then, so loot reads at a glance
    if (G.curLoc >= 0) for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k]; if (p.container < 0 || G.loc[G.curLoc].generated[p.container]) continue;
        int x = p.tx * 16 - camX, y = p.ty * 16 - camY; if (x < -70 || y < -30 || x > c.w + 8 || y > c.h + 30) continue;
        if (G.light[m.idx(p.tx, p.ty)] < 60) continue;
        float ph = fmodf(G.uiTime * 0.45f + (hash32(k * 977u) % 100) / 100.0f, 1.0f); if (ph > 0.14f) continue;
        int gx = x + p.tw * 16 - 5, gy = y - 4, r = ph < 0.05f || ph > 0.10f ? 1 : 2;
        gfx_fill(c, gx - r, gy, 2 * r + 1, 1, rgb(236, 255, 190)); gfx_fill(c, gx, gy - r, 1, 2 * r + 1, rgb(236, 255, 190)); gfx_pset(c, gx, gy, rgb(255, 255, 255));
    }
    // muzzle flashes and impact sparks glow on top of everything (additive, so they bloom instead of tinting)
    {
        int nfl; const FxLight* fls = fx_lights(nfl);
        for (int k = 0; k < nfl; k++) {
            const FxLight& fl = fls[k]; if (fl.life <= 0) continue;
            int r = (int)(fl.r * 0.42f); if (r > 40) r = 40;
            lightPool(c, (int)fl.x - camX, (int)fl.y - camY, r, r * 3 / 4, fl.col, (int)(150 * fl.life / fl.max), true);
        }
        // tracers: a hot core with a longer orange tail, bright even in the dark
        for (auto& p : G.proj) if (p.active) {
            int x = (int)p.x - camX, y = (int)p.y - camY;
            float l = sqrtf(p.vx * p.vx + p.vy * p.vy) + 1e-3f; int tx = (int)(p.vx / l * 18), ty = (int)(p.vy / l * 18);
            gfx_line(c, x, y, x - tx, y - ty, rgb(240, 150, 50));
            gfx_line(c, x, y, x - tx * 5 / 9, y - ty * 5 / 9, rgb(255, 226, 130));
            gfx_line(c, x, y, x - tx / 5, y - ty / 5, rgb(255, 252, 228));
            lightPool(c, x, y, 7, 6, rgb(255, 200, 110), 110, true);
        }
        fx_renderGlow(c, camX, camY);
    }
    if (g_highlightProp >= 0) {
        const Prop& p = m.prop[g_highlightProp];
        bool searched = G.curLoc >= 0 && G.loc[G.curLoc].generated[p.container];
        bool searching = G.u[G.ctrl].searchProp == g_highlightProp;
        int ox = 0, oy = 0; const Sprite* s = prop_sprite(m, p, ox, oy); if (!s) { s = &g_spr[S_DESK]; ox = oy = 0; }
        px col = searching ? pal::gold : (searched ? pal::grey : pal::lime);
        gfx_outline(c, *s, p.tx * 16 - camX + ox, p.ty * 16 - camY + oy, col);
        if (!searched && !searching) gfx_outline(c, *s, p.tx * 16 - camX + ox, p.ty * 16 - camY + oy, (G.tick / 8) & 1 ? pal::lime : rgb(110, 170, 40));
    }
    profMark(4);   // post-light overlays
}

// Text and prompts are drawn after the zoom scale-up, at native resolution, so they stay crisp.
static void render_overlay(Canvas& c, float z) {
    auto SX = [&](float wx) { return (int)((wx - camX) * z); };
    auto SY = [&](float wy) { return (int)((wy - camY) * z); };
    const Unit& pl = G.u[G.ctrl];
    if (pl.searchT > 0) ui_bar(c, SX(pl.x) - 14, SY(pl.y - 34), 28, 5, 1 - pl.searchT / 1.4f, pal::gold);
    if (pl.reloadT > 0) gfx_textC(c, SX(pl.x), SY(pl.y - 38), "RELOADING", pal::cream);
    for (auto& f : G.ft) if (f.life > 0) gfx_textC(c, SX(f.x), SY(f.y), f.txt, f.col);
    if (g_prompt && !game_paused() && !tut_scene()) {
        int w = ui_promptW('A', g_prompt) + 8;
        int x = SX(g_promptX), y = SY(g_promptY + 12);
        if (x - w / 2 < 2) x = w / 2 + 2; if (x + w / 2 > TOP_W - 2) x = TOP_W - 2 - w / 2;
        if (y > TOP_H - 40) y = TOP_H - 40;
        gfx_fillA(c, x - w / 2, y, w, 16, pal::ink, 210);
        gfx_rect(c, x - w / 2, y, w, 16, pal::gold);
        ui_prompt(c, x - w / 2 + 4, y + 2, 'A', g_prompt);
    }
}

// HUD plate: the same sunk navy well as the menu panels, see-through enough to keep the world visible
static void plate(Canvas& c, int x, int y, int w, int h, int alpha = 215) {
    gfx_fillA(c, x, y, w, h, rgb(24, 26, 42), alpha);
    gfx_rect(c, x, y, w, h, rgb(22, 22, 34));
    gfx_fillA(c, x + 1, y + 1, w - 2, 1, rgb(108, 112, 146), 150);
}
// coloured frame closing in from the screen edges (hurt flash, low-health pulse); cheap: a few nested translucent bands
static void edgeFlash(Canvas& c, px col, int strength, int bands) {
    for (int k = 0; k < bands; k++) {
        int a = strength * (bands - k) / bands, t = 5, o = k * t;
        gfx_fillA(c, o, o, c.w - 2 * o, t, col, a); gfx_fillA(c, o, c.h - o - t, c.w - 2 * o, t, col, a);
        gfx_fillA(c, o, o + t, t, c.h - 2 * o - 2 * t, col, a); gfx_fillA(c, c.w - o - t, o + t, t, c.h - 2 * o - 2 * t, col, a);
    }
}
static void hud(Canvas& c) {
    const Unit& u = G.u[G.ctrl];
    if (u.sv < 0) return;
    const Survivor& s = G.st.surv[u.sv];
    // survivor plate (top-left, Try 2): framed portrait + two thick bars (health green, stamina yellow)
    // pain reads at the screen edge: a red flash when bitten, a slow pulse while badly hurt
    if (u.hurtT > 0) edgeFlash(c, rgb(214, 40, 44), (int)(150 * fminf(1.0f, u.hurtT / 0.25f)), 3);
    else if (s.hp < 35) edgeFlash(c, rgb(190, 30, 40), 60 + (int)(40 * sinf(G.uiTime * 5)), 1);
    plate(c, 3, 3, 110, 32);
    gfx_fill(c, 5, 5, 28, 28, rgb(22, 22, 34)); gfx_rect(c, 5, 5, 28, 28, rgb(108, 112, 146));
    gfx_blit(c, g_portrait[s.variant % PORTRAITS], 7, 7);
    gfx_fill(c, 36, 8, 74, 10, pal::ink); ui_bar(c, 37, 9, 72, 8, s.hp / s.maxHp, s.hp < 35 ? pal::red : rgb(96, 200, 64));
    gfx_fill(c, 36, 21, 74, 9, pal::ink); ui_bar(c, 37, 22, 72, 7, u.stam / 100, rgb(226, 206, 60));
    if (s.infection && s.infKnown) gfx_blit(c, g_spr[S_ICON_BIO], 100, 24);
    // squad mini bars
    int yy = 37;
    for (int i = 0; i < G.nu; i++) {
        if (i == G.ctrl || !G.u[i].alive) continue;
        const Unit& o = G.u[i];
        plate(c, 3, yy, 80, 11, 190);
        if (o.sv >= 0) {
            const Survivor& os = G.st.surv[o.sv];
            static const char* ORD[] = {"F", "H", "R"};
            gfx_text(c, 5, yy + 2, os.name, pal::dim);
            ui_bar(c, 50, yy + 3, 22, 5, os.hp / os.maxHp, pal::green);
            gfx_text(c, 75, yy + 2, ORD[o.order], pal::gold);
        } else gfx_text(c, 5, yy + 2, o.order == ORD_HOLD ? "Trapped" : "Escort", pal::cream);
        yy += 12;
    }
    // location plate (top centre)
    const char* loc = G.scene == SC_SETTLEMENT ? "SETTLEMENT" : (G.curLoc >= 0 ? LOCATIONS[G.curLoc].name : "");
    char up[32]; int k = 0; for (; loc[k] && k < 31; k++) up[k] = (loc[k] >= 'a' && loc[k] <= 'z') ? loc[k] - 32 : loc[k]; up[k] = 0;
    int w = gfx_textW(up) + 14;
    plate(c, 200 - w / 2, 3, w, 13);
    gfx_textC(c, 200, 6, up, pal::cream);
    // weapon + ammo (top-right)
    // Try 2: a light weapon silhouette and big bold ammo digits, on a soft dark backing
    plate(c, 262, 3, 135, 22, 190);
    if (s.equip >= 0 && s.equip < s.pack.n) {
        const Item& it = s.pack.it[s.equip]; const ItemDef& d = idef(it.id);
        const Sprite& ic = g_item[it.id];
        int sc = 1, iw = ic.w * sc, ih = ic.h * sc, ix = 266, iy = 14 - ih / 2;
        for (int y = 0; y < ih; y++) for (int x = 0; x < iw; x++) {   // silhouette: icon brightened toward light grey
            px v = ic.d[(y / sc) * ic.w + x / sc]; if (v == KEYC) continue;
            gfx_pset(c, ix + x, iy + y, v);
        }
        char b[24];
        if (d.cat == IC_FIREARM) snprintf(b, sizeof b, "%d/%d", it.mag, s.pack.countOf(d.wpn.ammo));
        else snprintf(b, sizeof b, d.wpn.dur ? "%d%%" : "--", d.wpn.dur ? it.dur * 100 / d.wpn.dur : 0);
        px col = (it.dur == 0 && d.wpn.dur) || (d.cat == IC_FIREARM && it.mag == 0) ? pal::red : rgb(250, 248, 240);
        int tw = gfx_textW(b, 2);
        gfx_text(c, 394 - tw + 1, 7, b, pal::ink, 2); gfx_text(c, 394 - tw, 6, b, col, 2);
        if (d.cat == IC_FIREARM && it.mag == 0) gfx_text(c, 345, 26, "RELOAD X", pal::red);
    } else gfx_text(c, 306, 9, "Unarmed", pal::dim);
    // clock
    char cl[32]; snprintf(cl, sizeof cl, "Day %u %s", G.day, clock_str());
    plate(c, 300, 26, 97, 13, 190);
    gfx_blit(c, g_spr[S_ICON_CLOCK], 302, 29);
    gfx_text(c, 312, 29, cl, is_night() ? pal::cyan : pal::cream);
    if (!G.flashlight && is_night()) gfx_text(c, 304, 41, "R: Light off", pal::dim);
    if (G.toastT > 0) {
        int tw = gfx_textW(G.toast) + 12; if (tw > 396) tw = 396;
        int a = G.toastT < 0.4f ? (int)(G.toastT / 0.4f * 200) : 200;
        int ty = tut_active() ? 148 : 216;   // keep clear of the tutorial banner (up to three lines tall, top or bottom)
        gfx_fillA(c, 200 - tw / 2, ty, tw, 14, rgb(24, 26, 42), a); gfx_rect(c, 200 - tw / 2, ty, tw, 14, pal::gold);
        gfx_textC(c, 200, ty + 3, G.toast, pal::cream);
    }
    if (G.set.showFps) { char f[24]; snprintf(f, sizeof f, "%.0ffps %.1fms", G.fps, G.frameMs); gfx_text(c, 4, 228, f, pal::lime); }
}

void render_title(Canvas& c) {
    // Dusk over the dead city: banded sunset sky, a slotted sun, two layers of skyline, the road out front.
    // The backdrop never changes, so it is painted once and copied.
    static px* sky = nullptr;
    const int HZ = 170;
    if (!sky) {
        sky = (px*)malloc(sizeof(px) * TOP_W * TOP_H);
        static const int SY[5] = {0, 56, 112, 150, HZ};
        static const px SC[5] = {rgb(14, 12, 44), rgb(56, 26, 92), rgb(176, 52, 116), rgb(246, 124, 86), rgb(252, 196, 116)};
        static const uint8_t BAY[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        for (int y = 0; y < TOP_H; y++) for (int x = 0; x < TOP_W; x++) {
            px col;
            if (y < HZ) {
                int k = 0; while (k < 3 && y >= SY[k + 1]) k++;
                int t = (y - SY[k]) * 96 / (SY[k + 1] - SY[k]) + BAY[y & 3][x & 3];   // 0..111, dithered into 7 bands
                col = gfx_mix(SC[k], SC[k + 1], (t / 16) * 256 / 6 > 256 ? 256 : (t / 16) * 256 / 6);
                int dx = x - 118, dy = y - (HZ - 6), d2 = dx * dx + dy * dy;           // the sun, sliced by widening slots
                if (d2 < 44 * 44) { bool slot = y > 136 && ((HZ - y) % 8) < 1 + (y - 136) / 10; if (!slot) col = gfx_mix(rgb(255, 244, 150), rgb(252, 100, 90), (y - 120) * 256 / 50 < 0 ? 0 : ((y - 120) * 256 / 50 > 256 ? 256 : (y - 120) * 256 / 50)); }
            } else {
                col = gfx_mix(rgb(26, 20, 46), rgb(14, 12, 28), (y - HZ) * 256 / (TOP_H - HZ));
                if (y >= 196) col = gfx_mix(rgb(44, 42, 62), rgb(26, 24, 42), (y - 196) * 256 / 44);                       // the road
                if (y == 195) col = rgb(96, 92, 118);
                if (y >= 216 && y <= 217 && ((x / 14) & 1)) col = rgb(214, 196, 120);                                       // centre line
            }
            sky[y * TOP_W + x] = col;
        }
        Canvas s; s.init(sky, TOP_W, TOP_H);
        for (int layer = 0; layer < 2; layer++) {   // far (plum) then near (ink) skyline
            px body = layer ? rgb(18, 16, 38) : rgb(70, 34, 96), lit = layer ? rgb(252, 214, 112) : rgb(240, 140, 120);
            for (int x = -6, i = 0; x < TOP_W; i++) {
                uint32_t h = hash32(i * 977 + layer * 131 + 7);
                int w = 16 + (int)(h % 22), hgt = (layer ? 10 : 18) + (int)((h >> 8) % (layer ? 24 : 30));
                if (!layer && x > 60 && x < 170) hgt = hgt * 2 / 5;                                                           // keep the far blocks low in front of the sun
                gfx_fill(s, x, HZ - hgt, w, hgt + (layer ? 26 : 0), body);
                if ((h >> 20) % 3 == 0) gfx_fill(s, x + w / 2 - 1, HZ - hgt - 8, 2, 8, body);                                // aerials
                for (int wy = HZ - hgt + 4; wy < HZ - 3; wy += 6) for (int wx = x + 3; wx < x + w - 3; wx += 5)
                    if (hash32(wx * 31 + wy * 17 + layer) % (layer ? 9 : 6) == 0) gfx_fill(s, wx, wy, 2, 3, lit);
                x += w + (int)((h >> 14) % 5);
            }
        }
    }
    memcpy(c.p, sky, sizeof(px) * TOP_W * TOP_H);
    // street lamp throwing a warm pool across the road
    gfx_fill(c, 322, 150, 2, 48, rgb(18, 16, 38)); gfx_fill(c, 316, 146, 14, 5, rgb(18, 16, 38)); gfx_fill(c, 318, 151, 10, 2, rgb(255, 226, 150));
    lightPool(c, 323, 204, 58, 18, rgb(255, 190, 100), 90, true); lightPool(c, 323, 152, 14, 12, rgb(255, 210, 130), 120, true);
    // the horde shuffling out of town: dark shapes, lit only where they pass the lamp
    for (int i = 0; i < 9; i++) {
        float x = fmodf(G.uiTime * (6 + i % 3) + i * 53, TOP_W + 48) - 24; int y = 208 + (i % 3) * 9;
        bool flip; const Sprite& s = art_char(true, i % ZOMB_VARIANTS, 0, ((int)(G.uiTime * 3 + i) & 1) ? P_WALK_A : P_WALK_B, flip);
        float d = fabsf(x - 323); int shade = d < 60 ? 110 + (int)((60 - d) * 2.2f) : 110;
        gfx_blitEx(c, s, (int)x - s.w / 2, y - s.h, flip, shade, rgb(255, 190, 100), 0);
    }
    // logo: cream and red block letters with a pink / cyan offset and a hard drop shadow, each letter bobbing
    auto word = [&](const char* w, int scale, int y, px col) {
        int n = (int)strlen(w), x0 = 200 - n * 6 * scale / 2;
        for (int i = 0; i < n; i++) {
            char ch[2] = {w[i], 0}; int x = x0 + i * 6 * scale, yy = y + (int)(sinf(G.uiTime * 2.2f + i * 0.7f) * 2);
            gfx_text(c, x + scale, yy + scale, ch, rgb(14, 12, 30), scale, false);
            gfx_text(c, x - 2, yy, ch, rgb(240, 70, 150), scale, false); gfx_text(c, x + 2, yy, ch, rgb(70, 220, 236), scale, false);
            gfx_text(c, x, yy, ch, col, scale, false);
        }
    };
    word("STAYING", 5, 20, rgb(250, 242, 214));
    word("ALIVE", 7, 64, rgb(242, 62, 70));
    for (int y = 1; y < TOP_H; y += 2) gfx_shade(c, 0, y, TOP_W, 1, 228);   // scanlines
}


extern int g_animTestState, g_animTestVariant; extern bool g_animTestZombie; extern float g_animTestSpeed;
static void render_animtest(Canvas& c) {
    // 8 directions + a 3x zoom, all driven by the same data-defined animation on real time
    static Animator a[9]; static int lastState = -1; static float evFlash = 0; static uint8_t lastEv = 0;
    AnimState st = (AnimState)g_animTestState;
    float dt = G.frameMs / 1000.0f * g_animTestSpeed;
    if (lastState != g_animTestState) { for (auto& x : a) { x = Animator(); x.play(st, true); } lastState = g_animTestState; }
    for (auto& x : a) { uint8_t ev = x.update(dt); if (ev) { lastEv = ev; evFlash = 0.25f; } if (x.finished || (x.state != st && !anim_def(st).loop)) { x = Animator(); x.play(st, true); } }
    gfx_clear(c, pal::black);
    for (int y = 0; y < TOP_H; y += 16) for (int x = 0; x < TOP_W; x += 16) gfx_blit(c, g_tile[TA_TILE][((x + y) / 16) & 3], x, y);
    static const float FX[8] = {1, 0.707f, 0, -0.707f, -1, -0.707f, 0, 0.707f}, FY[8] = {0, 0.707f, 1, 0.707f, 0, -0.707f, -1, -0.707f};
    camX = camY = 0;
    for (int d = 0; d < 8; d++) {
        float x = 40.0f + d * 46, y = 60;
        drawChar(c, g_animTestZombie, g_animTestVariant, x, y, (uint8_t)d, a[d], FX[d], FY[d], 0, g_animTestZombie ? 0 : IT_PISTOL, a[d].state == AnimState::Shoot && a[d].frame == 0 ? 1.0f : 0);
        static const char* DN[8] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
        gfx_textC(c, (int)x, 70, DN[d], pal::dim);
    }
    // 3x zoom of the south-facing variant
    static px zoomBuf[24 * 24]; Canvas z; z.init(zoomBuf, 24, 24);
    for (auto& p : zoomBuf) p = rgb(40, 40, 44);
    int keepX = camX, keepY = camY; camX = 0; camY = 0;
    drawChar(z, g_animTestZombie, g_animTestVariant, 12, 22, 2, a[8], 0, 1, 0, 0, 0, false);
    camX = keepX; camY = keepY;
    for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) gfx_fill(c, 152 + i * 4, 96 + j * 4, 4, 4, zoomBuf[j * 24 + i]);
    gfx_rect(c, 151, 95, 98, 98, pal::border);
    char b[48]; snprintf(b, sizeof b, "%s  frame %d/%d", anim_name(a[8].state), a[8].frame + 1, anim_def(a[8].state).n);
    gfx_textC(c, 200, 200, b, pal::cream);
    if (evFlash > 0) { evFlash -= G.frameMs / 1000.0f; gfx_textC(c, 200, 214, (lastEv & EV_FOOTSTEP) ? "EVENT: FOOTSTEP" : (lastEv & EV_MUZZLE) ? "EVENT: MUZZLE" : (lastEv & EV_IMPACT) ? "EVENT: IMPACT" : (lastEv & EV_RELOAD) ? "EVENT: RELOAD" : (lastEv & EV_BODYFALL) ? "EVENT: BODYFALL" : (lastEv & EV_GROAN) ? "EVENT: GROAN" : "EVENT", pal::gold); }
    gfx_textC(c, 200, 8, "ANIMATION TEST ROOM", pal::gold);
}

void game_render(Canvas& top, Canvas& bot, float alpha) {
    if (G.panel == PN_ANIMTEST) render_animtest(top);
    else if (G.scene == SC_TITLE || !G.inLevel) { camX = camY = 0; render_title(top); }
    else {
        int z = view_zoom();
        if (z == 100) render_world(top, alpha);
        else {
            // render the smaller world view, then nearest-neighbour scale it to the full top screen
            static px vbuf[TOP_W * TOP_H]; Canvas v; v.init(vbuf, view_w(), view_h());
            render_world(v, alpha);
            static int xs[TOP_W]; for (int x = 0; x < TOP_W; x++) xs[x] = x * 100 / z;
            int last = -1;
            for (int y = 0; y < TOP_H; y++) {
                const int sy = y * 100 / z; px* dst = top.p + y * TOP_W;
                if (sy == last) { memcpy(dst, dst - TOP_W, TOP_W * sizeof(px)); continue; }   // same source row as the row above
                last = sy;
                const px* src = vbuf + sy * v.w;
                if (z == 200) {   // exactly 2x: each pixel twice, two at a time
                    uint32_t* d = (uint32_t*)dst;
                    for (int x = 0; x < TOP_W / 2; x++) d[x] = src[x] * 0x10001u;
                } else for (int x = 0; x < TOP_W; x++) dst[x] = src[xs[x]];
            }
        }
        profMark(5);   // zoom scale-up
        render_overlay(top, z / 100.0f);
        if (!tut_scene()) hud(top);   // a tutorial scene has the whole screen
        if (game_paused()) {
            gfx_fillA(top, 0, 0, TOP_W, TOP_H, pal::ink, 110);
            if (G.panel == PN_INVENTORY || G.panel == PN_CONTAINER || G.panel == PN_PAUSE) {
                gfx_fill(top, 140, 104, 120, 26, pal::redDk); gfx_rect(top, 140, 104, 120, 26, pal::red);
                gfx_textC(top, 200, 108, "PAUSED", pal::cream, 2);
            }
        }
        tut_render(top);              // last, so a lesson stays readable over a paused menu
        profMark(6);   // overlay, HUD, pause dim, tutorial
    }
    // the touch screen: skipped on frames the platform says to (Fast graphics); timers in the menus still get the time
    static float carry = 0;
    if (g_skipBottom) carry += G.frameMs;
    else { const float keep = G.frameMs; G.frameMs += carry; carry = 0; ui_render(bot); G.frameMs = keep; }
    profMark(7);   // bottom screen
}
