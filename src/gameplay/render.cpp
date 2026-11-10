#include "game.h"
#include "../core/art.h"
#include "../ui/ui.h"
#include <math.h>
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
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int i = m.idx(tx, ty);
        bool indoor = (m.flags[i] & F_INDOOR) != 0;
        float L = indoor ? 70 + day * 104 : (G.scene == SC_SETTLEMENT ? 70 + day * 176 : 28 + day * 210);   // the settlement keeps a lantern-lit floor
        float cx = tx * 16 + 8.0f, cy = ty * 16 + 8.0f, dx = cx - p.x, dy = cy - (p.y - 4), d = sqrtf(dx * dx + dy * dy);
        bool los = d < 12 || map_los(m, p.x, p.y - 4, cx, cy);
        if (los) {
            if (d < 36) L = fmaxf(L, 150 - d * 1.5f);                         // personal ambient light
            if (G.flashAnim > 0.02f && d < 160 && d > 1) {
                float dot = (dx * fdx + dy * fdy) / d;
                if (dot > 0.86f) L = fmaxf(L, (235 - d * 0.7f) * G.flashAnim * fminf(1, (dot - 0.86f) * 25));
            }
        }
        // light sources: lamps, campfire (flicker), generator lamp
        for (int k = 0; k < m.nprop; k++) {
            const Prop& pr = m.prop[k];
            if (pr.type != PR_LAMP && pr.type != PR_FIRE && pr.type != PR_TORCH) continue;
            float lx = pr.tx * 16 + 8.0f, ly = pr.ty * 16 + 8.0f, ld = sqrtf((cx - lx) * (cx - lx) + (cy - ly) * (cy - ly));
            float r = pr.type == PR_FIRE ? 88.0f : (pr.type == PR_TORCH ? 56.0f : 72.0f);
            if (ld > r) continue;
            float flick = pr.type != PR_LAMP ? 0.85f + 0.15f * sinf(t * 13 + k) * sinf(t * 7.3f) : ((int)(t * 6 + k * 3) % 23 == 0 ? 0.4f : 1.0f);
            if (map_los(m, lx, ly, cx, cy)) L = fmaxf(L, 200 * (1 - ld / r) * flick + 30);
        }
        if (!los) L = fminf(L * 0.3f, 48);
        G.light[i] = (uint8_t)(L > 255 ? 255 : L);
        if (los && L >= 60) G.explored[i] = 1;
    }
}

static bool visibleAt(float x, float y) {
    int tx = (int)(x / 16), ty = (int)(y / 16);
    return G.map.in(tx, ty) && G.light[G.map.idx(tx, ty)] >= 60;
}

static const Sprite& tileSprite(int tx, int ty) {
    Map& m = G.map;
    int i = m.idx(tx, ty), v = hash32(tx * 73 + ty * 911) & 3;
    switch (m.t[i]) {
    case T_GRASS: return g_tile[TA_GRASS][v];
    case T_ASPHALT: return g_tile[TA_ASPHALT][v];
    case T_LINE: return g_tile[TA_LINE][v];
    case T_DIRT: return g_tile[TA_DIRT][v];
    case T_TILE: return g_tile[TA_TILE][v];
    case T_WOOD: return g_tile[TA_WOOD][v];
    case T_CARPET: return g_tile[TA_CARPET][v];
    case T_CONCRETE: return g_tile[TA_CONCRETE][v];
    case T_BARS: return g_tile[TA_BARS][v];
    case T_FENCE: return g_tile[TA_FENCE][v];
    case T_EXTRACT: return g_tile[TA_EXTRACT][v];
    case T_GARDEN: return g_tile[TA_GARDEN][v];
    case T_PALISADE: return g_tile[TA_PALISADE][v];
    case T_WALL: {
        bool below = ty + 1 < m.h && m.t[m.idx(tx, ty + 1)] == T_WALL;
        return below ? g_tile[TA_WALL_TOP][v] : g_tile[TA_WALL_FRONT][v];
    }
    case T_DOOR: {
        const Barrier& b = m.bar[m.barAt[i]];
        if (b.kind == BK_WINDOW) return b.state == BS_BROKEN ? g_tile[TA_WINDOW_BROKEN][v] : g_tile[TA_WINDOW][v];
        if (b.kind == BK_GATE) return b.anim > 0.5f ? g_tile[TA_GATE_OPEN][v] : g_tile[TA_GATE_CLOSED][v];
        if (b.kind == BK_CELLDOOR) return b.anim > 0.5f ? g_tile[TA_TILE][v] : g_tile[TA_CELLDOOR][v];
        if (b.state == BS_BROKEN) return g_tile[TA_DOOR_BROKEN][v];
        if (b.anim > 0.5f) return g_tile[TA_DOOR_OPEN][v];
        return b.state == BS_LOCKED ? g_tile[TA_DOOR_LOCKED][v] : g_tile[TA_DOOR_CLOSED][v];
    }
    }
    return g_tile[TA_DIRT][0];
}

// three flickering flame tongues (fire pit); drawn again after lighting so they stay the brightest thing on screen
static void drawFlames(Canvas& c, int cx, int cy) {
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
            for (int k = 1; k <= 9; k++) { px col = k < 4 ? core : (k < 7 ? mid : outer); gfx_pset(c, mx + fxd * k, my + fyd * k, col); if (k < 6) { gfx_pset(c, mx + fxd * k + fyd, my + fyd * k + fxd, k < 3 ? core : mid); gfx_pset(c, mx + fxd * k - fyd, my + fyd * k - fxd, k < 3 ? core : mid); } }   // forward cone
            for (int k = 2; k <= 4; k++) {
                gfx_pset(c, mx + fyd * k, my + fxd * k, k == 1 ? mid : outer); gfx_pset(c, mx - fyd * k, my - fxd * k, k == 1 ? mid : outer);   // side rays
                gfx_pset(c, mx + (fxd + fyd) * k, my + (fyd + fxd) * k, outer); gfx_pset(c, mx + (fxd - fyd) * k, my + (fyd - fxd) * k, outer);   // diagonals
            }
            gfx_fill(c, mx - 1, my - 2, 3, 5, mid); gfx_fill(c, mx - 2, my - 1, 5, 3, mid); gfx_fill(c, mx - 1, my - 1, 3, 3, core);
        }
    };
    if (behind) weapon();
    gfx_blitEx(c, s, sx, sy, flip, 256, rgb(255, 250, 240), flash > 0 ? 170 : 0);
    if (!behind) weapon();
}

struct DrawItem { int16_t y; uint8_t kind; uint8_t idx; };
enum { DK_PROP, DK_ZOMB, DK_UNIT, DK_NPC, DK_BAG };

void render_world(Canvas& c, float alpha) {
    Map& m = G.map;
    float sh = G.shake > 0 && G.set.shake ? G.shake : 0;
    camX = (int)G.camX + (sh > 0 ? (int)((hash32(G.tick * 7) % 3) - 1) * (int)ceilf(sh) : 0);
    camY = (int)G.camY + (sh > 0 ? (int)((hash32(G.tick * 13) % 3) - 1) * (int)ceilf(sh) : 0);
    render_compute_light();
    gfx_clear(c, pal::black);
    int tx0 = camX / 16, ty0 = camY / 16, tx1 = (camX + view_w()) / 16 + 1, ty1 = (camY + view_h()) / 16 + 1;
    int palX0 = 1 << 30, palX1 = -1, palY0 = 1 << 30, palY1 = -1;   // the compound's fence bounds (settlement only)
    if (G.scene == SC_SETTLEMENT) for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) if (m.t[m.idx(x, y)] == T_PALISADE) { if (x < palX0) palX0 = x; if (x > palX1) palX1 = x; if (y < palY0) palY0 = y; if (y > palY1) palY1 = y; }
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int bx = tx * 16 - camX, by = ty * 16 - camY;
        int i = m.idx(tx, ty);
        if (m.barAt[i] >= 0) {
            const Barrier& b = m.bar[m.barAt[i]];
            int jx = b.shake > 0 ? ((G.tick & 2) ? 1 : -1) : 0;
            gfx_blit(c, tileSprite(tx, ty), bx + jx, by);
            float hp = b.maxHp ? (float)b.hp / b.maxHp : 1;
            if (b.state != BS_BROKEN && b.state != BS_OPEN && hp < 0.66f) gfx_blit(c, g_tile[hp < 0.33f ? TA_CRACK2 : TA_CRACK1][0], bx + jx, by);
        } else gfx_blit(c, tileSprite(tx, ty), bx, by);
        if (m.t[i] == T_EXTRACT && ((int)(G.uiTime * 2) & 1)) gfx_rect(c, bx + 2, by + 2, 12, 12, rgb(154, 228, 64));
        if (G.scene == SC_SETTLEMENT && m.t[i] == T_GRASS && (tx < palX0 || tx > palX1 || ty < palY0 || ty > palY1)) gfx_fillA(c, bx, by, 16, 16, rgb(10, 24, 14), 130);   // dark forest floor beyond the fence
        uint8_t t = m.t[i];
        if (t == T_WALL || t == T_PALISADE || t == T_DOOR || t == T_BARS || t == T_FENCE) continue;
        // ground detail scattered deterministically (no storage, identical every frame)
        uint32_t h = hash32((uint32_t)(tx * 92837 + ty * 689287 + m.id * 7919));
        int r = h % 100, dx = (h >> 8) % 9, dy = (h >> 12) % 9;
        int dec = -1;
        if (m.propAt[i] < 0) {
            if (t == T_GRASS) dec = r < 9 ? S_DEC_TUFT : (r < 13 ? S_DEC_FLOWER : (r < 16 ? S_DEC_ROCK : (r < 18 ? S_DEC_LEAVES : -1)));
            else if (t == T_DIRT) dec = r < 6 ? S_DEC_ROCK : (r < 11 ? S_DEC_TUFT : (r < 13 ? S_DEC_LEAVES : -1));
            else if (t == T_ASPHALT || t == T_LINE) dec = r < 2 ? S_DEC_OIL : (r < 6 ? S_DEC_CRACK : (r < 12 ? S_DEC_PAPER : (r < 14 ? S_DEC_DEBRIS : -1)));
            else if (t == T_TILE || t == T_WOOD || t == T_CARPET || t == T_CONCRETE) dec = r < 6 ? S_DEC_PAPER : (r < 10 ? S_DEC_DEBRIS : (r < 12 ? S_DEC_CRACK : ((r < 14 && m.id == MAP_POLICE) ? S_DEC_SMEAR : -1)));
        }
        if (dec >= 0) gfx_blit(c, g_spr[dec], bx + dx, by + dy);
        // ambient occlusion: floors darken where they meet a wall (north) or wall side (west/east)
        auto solidWall = [&](int x, int y) { if (!m.in(x, y)) return false; uint8_t w = m.t[m.idx(x, y)]; return w == T_WALL || w == T_PALISADE || (w == T_DOOR && m.bar[m.barAt[m.idx(x, y)]].kind == BK_WINDOW); };
        if (solidWall(tx, ty - 1)) { static const int AO[5] = {130, 160, 190, 215, 238}; for (int k = 0; k < 5; k++) gfx_shade(c, bx, by + k, 16, 1, AO[k]); }
        if (solidWall(tx - 1, ty)) { gfx_shade(c, bx, by, 2, 16, 170); gfx_shade(c, bx + 2, by, 2, 16, 215); }
        if (solidWall(tx + 1, ty)) gfx_shade(c, bx + 14, by, 2, 16, 200);
    }
    // police station signage above the front doors (Try 2 look)
    if (m.id == MAP_POLICE) {
        int sx = 19 * 16 + 14 - camX, sy = 32 * 16 + 3 - camY;   // facade wall left of the double doors
        gfx_fill(c, sx - 1, sy - 1, 74, 13, pal::ink);
        gfx_fill(c, sx, sy, 72, 11, rgb(34, 46, 84)); gfx_fill(c, sx, sy, 72, 1, rgb(70, 90, 140));
        gfx_text(c, sx + 9, sy + 2, "POLICE", pal::cream, 1, true); gfx_text(c, sx + 10, sy + 2, "POLICE", pal::cream, 1, false);
        bool on = ((int)(G.uiTime * 3)) & 1;   // emergency strobes either side
        gfx_fill(c, sx - 8, sy + 2, 4, 4, on ? rgb(240, 60, 50) : rgb(90, 30, 30));
        gfx_fill(c, sx + 76, sy + 2, 4, 4, on ? rgb(60, 90, 240) : rgb(30, 40, 90));
    }
    // decals (blood) and corpses lie under everything
    for (int k = 0; k < G.ndecal; k++) gfx_blit(c, g_spr[S_BLOOD0 + G.decal[k].kind % 3], (int)G.decal[k].x - camX, (int)G.decal[k].y - camY);
    for (int k = 0; k < G.nz; k++) {
        Zombie& z = G.z[k];
        if (z.state != ZS_DEAD || z.anim.state != AnimState::Dead) continue;
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
    for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k];
        if (p.tx + p.tw < tx0 - 3 || p.tx > tx1 + 1 || p.ty + p.th < ty0 - 3 || p.ty > ty1 + 1) continue;
        int x = p.tx * 16 - camX, y = p.ty * 16 - camY;
        switch (p.type) {
        case PR_TREE: gfx_fillA(c, x - 6, y + 6, 32, 12, rgb(0, 0, 0), 90); gfx_fillA(c, x - 2, y + 4, 24, 16, rgb(0, 0, 0), 50); break;
        case PR_PLOT: if (G.st.bLevel[p.special]) { const Sprite& bs = g_bldWorld[p.special][G.st.bLevel[p.special] > 3 ? 3 : G.st.bLevel[p.special]]; shadow(bs, x + 24 - bs.w / 2, y + 48 - bs.h, 4, 3, 150); } break;
        case PR_CAR: shadow(g_spr[S_CAR], x, y, 2, 3, 140); break;
        case PR_WRECK: shadow(g_spr[S_WRECK], x, y, 2, 3, 140); break;
        case PR_TENT: shadow(g_spr[S_TENT], x - 7, y - 24, 3, 3, 150); break;
        case PR_CHAIR: case PR_CONE: case PR_GENERATOR: break;
        default: if (p.type <= PR_LOGS && p.type != PR_PLOT) {
            static const int SPR[PR_COUNT] = {S_DESK, S_FILING, S_LOCKER, S_CRATE, S_MEDCAB, S_SHELF, S_CAR, S_WRECK, S_TREE, S_BENCH, S_COT, S_CHAIR, S_PLANT, S_COOLER,
                                              S_FIRE0, S_RADIO, S_GENPROP0, S_WORKBENCH, S_LAMP, S_CRATE, S_TENT, S_BARREL, S_SANDBAG, S_LOGS, S_TORCH0, S_CONE, S_CRATE};
            shadow(g_spr[SPR[p.type]], x, y, 2, 2, 160);
        }
        }
    }
    // y-sorted scene
    static DrawItem items[Map::MAXPROP + MAX_ZOMB + MAX_UNITS + MAX_SURV + MAX_BAGS];
    int n = 0;
    for (int k = 0; k < m.nprop; k++) {
        const Prop& p = m.prop[k];
        if (p.tx + p.tw < tx0 - 2 || p.tx > tx1 + 2 || p.ty + p.th < ty0 - 2 || p.ty > ty1 + 2) continue;
        items[n++] = DrawItem{(int16_t)((p.ty + p.th) * 16 - 1), DK_PROP, (uint8_t)k};
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
            switch (p.type) {
            case PR_DESK: gfx_blit(c, g_spr[S_DESK], x, y); break;
            case PR_FILING: gfx_blit(c, g_spr[S_FILING], x, y); break;
            case PR_LOCKER: gfx_blit(c, g_spr[S_LOCKER], x, y); break;
            case PR_CRATE: gfx_blit(c, g_spr[S_CRATE], x, y); break;
            case PR_DECOR_CRATE: gfx_blit(c, g_spr[S_CRATE], x, y); break;
            case PR_MEDCAB: gfx_blit(c, g_spr[S_MEDCAB], x, y); break;
            case PR_SHELF: gfx_blit(c, g_spr[S_SHELF], x, y); break;
            case PR_CAR: gfx_blit(c, g_spr[S_CAR], x, y - 2); break;
            case PR_WRECK: gfx_blit(c, g_spr[S_WRECK], x, y - 2); break;
            case PR_TREE: {
                // gentle sway: canopy offset by a slow sine, trunk static
                int sway = (int)(sinf(G.uiTime * 0.9f + p.tx * 0.7f) * 1.2f);
                gfx_fill(c, x + 6, y + 6, 4, 10, rgb(60, 44, 30));
                gfx_blit(c, g_spr[S_TREE + (p.tx + p.ty) % 2], x - 8 + sway, y - 20);
                break;
            }
            case PR_BENCH: gfx_blit(c, g_spr[S_BENCH], x, y); break;
            case PR_TENT: gfx_blit(c, g_spr[S_TENT], x - 7, y - 24); break;
            case PR_BARREL: gfx_blit(c, g_spr[S_BARREL], x, y); break;
            case PR_SANDBAG: gfx_blit(c, g_spr[S_SANDBAG], x, y); break;
            case PR_LOGS: gfx_blit(c, g_spr[S_LOGS], x, y); break;
            case PR_TORCH: gfx_blit(c, g_spr[S_TORCH0 + (((int)(G.uiTime * 7) + p.tx) & 1)], x, y - 4); break;
            case PR_CONE: gfx_blit(c, g_spr[S_CONE], x, y); break;
            case PR_COT: gfx_blit(c, g_spr[S_COT], x, y); break;
            case PR_CHAIR: gfx_blit(c, g_spr[S_CHAIR], x, y); break;
            case PR_PLANT: gfx_blit(c, g_spr[S_PLANT], x, y); break;
            case PR_COOLER: gfx_blit(c, g_spr[S_COOLER], x, y); break;
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
            case PR_RADIO: gfx_blit(c, g_spr[S_RADIO], x, y); if ((int)(G.uiTime * 2) & 1) gfx_pset(c, x + 12, y + 4, rgb(255, 80, 60)); break;
            case PR_WORKBENCH: gfx_blit(c, g_spr[S_WORKBENCH], x, y); break;
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
    // projectiles (tracers) and particles
    for (auto& p : G.proj) if (p.active) {
        int x = (int)p.x - camX, y = (int)p.y - camY;
        float l = sqrtf(p.vx * p.vx + p.vy * p.vy) + 1e-3f;
        gfx_line(c, x, y, x - (int)(p.vx / l * 5), y - (int)(p.vy / l * 5), rgb(250, 230, 150));
    }
    for (auto& p : G.part) {
        if (p.life <= 0) continue;
        int x = (int)p.x - camX, y = (int)(p.y - p.z) - camY;
        switch (p.kind) {
        case PK_MUZZLE: gfx_fill(c, x - 2, y - 2, 5, 5, rgb(255, 240, 170)); break;
        case PK_SMOKE: case PK_DUST: gfx_fillA(c, x - 1, y - 1, 3, 3, p.col, (int)(150 * p.life / p.max)); break;
        case PK_CASING: gfx_pset(c, x, y, p.col); gfx_pset(c, x + 1, y, gfx_scale(p.col, 150)); break;
        case PK_LEAF: gfx_pset(c, x, y, p.col); gfx_pset(c, x + 1, y + ((int)(p.life * 4) & 1), gfx_scale(p.col, 170)); break;
        default: gfx_pset(c, x, y, p.col); if (p.kind == PK_BLOOD) gfx_pset(c, x + 1, y, p.col); break;
        }
    }

    // ---- lighting: 8x8 cells, each blended toward its neighbours to soften tile edges
    float day = daylight();
    // warm dusty grade by day, amber at dusk, deep blue at night
    px tint = G.scene == SC_SETTLEMENT ? (day > 0.15f ? rgb(46, 30, 44) : rgb(14, 16, 40))   // settlement: violet dusk shadows keep the warm lights popping
                                       : (day > 0.6f ? rgb(30, 22, 14) : (day > 0.15f ? rgb(44, 22, 14) : rgb(6, 10, 26)));
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int L = G.light[m.idx(tx, ty)];
        for (int q = 0; q < 4; q++) {
            int ox = q & 1, oy = q >> 1;
            int nx = tx + (ox ? 1 : -1), ny = ty + (oy ? 1 : -1);
            int Lx = m.in(nx, ty) ? G.light[m.idx(nx, ty)] : L, Ly = m.in(tx, ny) ? G.light[m.idx(tx, ny)] : L;
            int v = (L * 2 + Lx + Ly) / 4;
            int x0 = tx * 16 + ox * 8 - camX, y0 = ty * 16 + oy * 8 - camY;
            // vignette: darken toward the screen edges (folded into the same pass)
            int VW = view_w(), VH = view_h(); float vx = (x0 + 4 - VW / 2) / (float)(VW / 2), vy = (y0 + 4 - VH / 2) / (float)(VH / 2);
            v = v * (256 - (int)((G.scene == SC_SETTLEMENT ? 110 : 70) * (vx * vx + vy * vy))) >> 8;
            if (v >= 250) continue;
            for (int y = y0; y < y0 + 8; y++) {
                if (y < 0 || y >= c.h) continue;
                px* row = c.p + y * c.w;
                for (int x = x0; x < x0 + 8; x++) if (x >= 0 && x < c.w) row[x] = gfx_mix(tint, row[x], v + 1);
            }
        }
    }
    // golden hour over the settlement (Try 2 evening): warm, saturated, a touch brighter
    if (G.scene == SC_SETTLEMENT) {
        int amt = 0;
        for (int i = 0; i < c.w * c.h; i++) {   // saturation boost (+30%) then a light warm wash
            px v = c.p[i]; int r = ((v >> 11) & 31) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3, l = (r * 3 + g * 6 + b) / 10;
            r = l + (r - l) * 23 / 20; g = l + (g - l) * 23 / 20; b = l + (b - l) * 23 / 20;
            r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
            c.p[i] = amt > 0 ? gfx_mix(rgb(r, g, b), rgb(255, 172, 92), amt) : rgb(r, g, b);
        }
    }
    // ---- post-light overlays (stay readable at night)
    if (day < 0.97f) {
        // warm light pools around fires, lamps and torches (Try 2 dusk): per-pixel radial blend
        float dk = fminf(1.0f, (1.0f - day) / 0.45f);   // lights are at full strength well before full dark
        auto pool = [&](int cx, int cy, int rx, int ry, px col, int amax) {
            for (int dy = -ry; dy <= ry; dy++) {
                int yy = cy + dy; if (yy < 0 || yy >= c.h) continue;
                for (int dx = -rx; dx <= rx; dx++) {
                    int xx = cx + dx; if (xx < 0 || xx >= c.w) continue;
                    float d = (float)(dx * dx) / (rx * rx) + (float)(dy * dy) / (ry * ry); if (d >= 1) continue;
                    px& p = c.p[yy * c.w + xx]; p = gfx_mix(p, col, (int)(amax * (1 - d) * (1 - d)));
                }
            }
        };
        float flick = 0.9f + 0.1f * sinf(G.uiTime * 13) * sinf(G.uiTime * 7.3f);
        for (int k = 0; k < m.nprop; k++) {
            const Prop& p = m.prop[k];
            int x = p.tx * 16 - camX, y = p.ty * 16 - camY;
            if (x < -60 || y < -60 || x > c.w + 60 || y > c.h + 60) continue;
            if (p.type == PR_FIRE) {
                pool(x + 8, y + 10, 70, 40, rgb(255, 140, 50), (int)(170 * dk * flick));
                drawFlames(c, x + 8, y + 12);   // flames stay the brightest thing on screen
            } else if (p.type == PR_LAMP) {
                pool(x + 7, y + 4, 36, 20, rgb(255, 180, 80), (int)(140 * dk));
                pool(x + 7, y - 17, 9, 9, rgb(255, 210, 120), (int)(160 * dk));
                gfx_fill(c, x + 4, y - 21, 3, 3, rgb(255, 230, 150)); gfx_fill(c, x + 8, y - 21, 3, 3, rgb(255, 214, 110)); gfx_fill(c, x + 4, y - 16, 3, 3, rgb(255, 214, 110)); gfx_fill(c, x + 8, y - 16, 3, 3, rgb(255, 200, 100));
            } else if (p.type == PR_TENT) {
                pool(x + 16, y + 6, 26, 12, rgb(255, 160, 70), (int)(90 * dk));
                gfx_fillA(c, x - 7 + 17, y - 24 + 26, 12, 11, rgb(240, 172, 76), (int)(110 * dk)); gfx_fillA(c, x - 7 + 19, y - 24 + 27, 8, 4, rgb(255, 220, 130), (int)(160 * dk));
            } else if (p.type == PR_TORCH) {
                pool(x + 8, y + 6, 22, 13, rgb(255, 150, 60), (int)(120 * dk * flick));
            }
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
    if (g_highlightProp >= 0) {
        const Prop& p = m.prop[g_highlightProp];
        bool searched = G.curLoc >= 0 && G.loc[G.curLoc].generated[p.container];
        bool searching = G.u[G.ctrl].searchProp == g_highlightProp;
        const Sprite* s = &g_spr[S_DESK];
        switch (p.type) { case PR_FILING: s = &g_spr[S_FILING]; break; case PR_LOCKER: s = &g_spr[S_LOCKER]; break; case PR_CRATE: s = &g_spr[S_CRATE]; break;
            case PR_MEDCAB: s = &g_spr[S_MEDCAB]; break; case PR_SHELF: s = &g_spr[S_SHELF]; break; case PR_CAR: s = &g_spr[S_CAR]; break; case PR_WRECK: s = &g_spr[S_WRECK]; break; }
        int oy = (p.type == PR_CAR || p.type == PR_WRECK) ? -2 : 0;
        px col = searching ? pal::gold : (searched ? pal::grey : pal::lime);
        gfx_outline(c, *s, p.tx * 16 - camX, p.ty * 16 - camY + oy, col);
        if (!searched && !searching) gfx_outline(c, *s, p.tx * 16 - camX, p.ty * 16 - camY + oy - 0, (G.tick / 8) & 1 ? pal::lime : rgb(110, 170, 40));
    }
}

// Text and prompts are drawn after the zoom scale-up, at native resolution, so they stay crisp.
static void render_overlay(Canvas& c, float z) {
    auto SX = [&](float wx) { return (int)((wx - camX) * z); };
    auto SY = [&](float wy) { return (int)((wy - camY) * z); };
    const Unit& pl = G.u[G.ctrl];
    if (pl.searchT > 0) ui_bar(c, SX(pl.x) - 14, SY(pl.y - 34), 28, 5, 1 - pl.searchT / 1.4f, pal::gold);
    if (pl.reloadT > 0) gfx_textC(c, SX(pl.x), SY(pl.y - 38), "RELOADING", pal::cream);
    for (auto& f : G.ft) if (f.life > 0) gfx_textC(c, SX(f.x), SY(f.y), f.txt, f.col);
    if (g_prompt && !game_paused()) {
        int w = ui_promptW('A', g_prompt) + 8;
        int x = SX(g_promptX), y = SY(g_promptY + 12);
        if (x - w / 2 < 2) x = w / 2 + 2; if (x + w / 2 > TOP_W - 2) x = TOP_W - 2 - w / 2;
        if (y > TOP_H - 40) y = TOP_H - 40;
        gfx_fillA(c, x - w / 2, y, w, 16, pal::ink, 210);
        gfx_rect(c, x - w / 2, y, w, 16, pal::gold);
        ui_prompt(c, x - w / 2 + 4, y + 2, 'A', g_prompt);
    }
}

static void hud(Canvas& c) {
    const Unit& u = G.u[G.ctrl];
    if (u.sv < 0) return;
    const Survivor& s = G.st.surv[u.sv];
    // survivor plate (top-left)
    gfx_fillA(c, 3, 3, 118, 30, pal::ink, 190); gfx_rect(c, 3, 3, 118, 30, pal::border);
    gfx_blit(c, g_portrait[s.variant % PORTRAITS], 6, 6);
    gfx_text(c, 34, 6, s.name, pal::cream);
    ui_bar(c, 34, 16, 82, 6, s.hp / s.maxHp, s.hp < 35 ? pal::red : pal::green);
    ui_bar(c, 34, 24, 82, 5, u.stam / 100, pal::cyan);
    if (s.infection && s.infKnown) gfx_blit(c, g_spr[S_ICON_BIO], 110, 5);
    // squad mini bars
    int yy = 37;
    for (int i = 0; i < G.nu; i++) {
        if (i == G.ctrl || !G.u[i].alive) continue;
        const Unit& o = G.u[i];
        gfx_fillA(c, 3, yy, 80, 11, pal::ink, 170);
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
    gfx_fillA(c, 200 - w / 2, 3, w, 13, pal::ink, 190); gfx_rect(c, 200 - w / 2, 3, w, 13, pal::border);
    gfx_textC(c, 200, 6, up, pal::cream);
    // weapon + ammo (top-right)
    gfx_fillA(c, 300, 3, 97, 22, pal::ink, 190); gfx_rect(c, 300, 3, 97, 22, pal::border);
    if (s.equip >= 0 && s.equip < s.pack.n) {
        const Item& it = s.pack.it[s.equip]; const ItemDef& d = idef(it.id);
        const Sprite& ic = g_item[it.id];
        gfx_blit(c, ic, 304, 4 + (8 - ic.h / 2 > 0 ? 8 - ic.h / 2 : 0));
        char b[24];
        if (d.cat == IC_FIREARM) snprintf(b, sizeof b, "%d/%d", it.mag, s.pack.countOf(d.wpn.ammo));
        else snprintf(b, sizeof b, d.wpn.dur ? "%d%%" : "--", d.wpn.dur ? it.dur * 100 / d.wpn.dur : 0);
        gfx_text(c, 394 - gfx_textW(b, 1), 8, b, it.dur == 0 && d.wpn.dur ? pal::red : pal::cream);
        if (d.cat == IC_FIREARM && it.mag == 0) gfx_text(c, 345, 16, "RELOAD X", pal::red);
    } else gfx_text(c, 306, 9, "Unarmed", pal::dim);
    // clock
    char cl[32]; snprintf(cl, sizeof cl, "Day %u %s", G.day, clock_str());
    gfx_fillA(c, 300, 27, 97, 12, pal::ink, 160);
    gfx_blit(c, g_spr[S_ICON_CLOCK], 302, 29);
    gfx_text(c, 312, 29, cl, is_night() ? pal::cyan : pal::cream);
    if (!G.flashlight && is_night()) gfx_text(c, 304, 41, "R: Light off", pal::dim);
    if (G.toastT > 0) {
        int tw = gfx_textW(G.toast) + 12; if (tw > 396) tw = 396;
        int a = G.toastT < 0.4f ? (int)(G.toastT / 0.4f * 200) : 200;
        int ty = tut_active() ? 174 : 216;   // keep clear of the tutorial banner
        gfx_fillA(c, 200 - tw / 2, ty, tw, 14, pal::ink, a); gfx_rect(c, 200 - tw / 2, ty, tw, 14, pal::goldDk);
        gfx_textC(c, 200, ty + 3, G.toast, pal::cream);
    }
    if (G.set.showFps) { char f[24]; snprintf(f, sizeof f, "%.0ffps %.1fms", G.fps, G.frameMs); gfx_text(c, 4, 228, f, pal::lime); }
}

void render_title(Canvas& c) {
    // night street backdrop with a slow shambling horde
    for (int y = 0; y < TOP_H; y++) gfx_fill(c, 0, y, TOP_W, 1, gfx_mix(rgb(14, 16, 26), rgb(40, 34, 36), y * 256 / TOP_H));
    for (int x = 0; x < TOP_W; x += 16) gfx_blit(c, g_tile[TA_ASPHALT][(x / 16) & 3], x, 192), gfx_blit(c, g_tile[TA_ASPHALT][(x / 16 + 1) & 3], x, 208), gfx_blit(c, g_tile[TA_ASPHALT][(x / 16 + 2) & 3], x, 224);
    for (int x = 0; x < TOP_W; x += 16) gfx_blit(c, g_tile[TA_WALL_FRONT][(x / 16) & 3], x, 176);
    for (int x = 40; x < TOP_W; x += 96) gfx_blit(c, g_tile[TA_WINDOW][1], x, 176);
    for (int i = 0; i < 6; i++) gfx_blit(c, g_spr[S_TREE + (i & 1)], -10 + i * 76, 150 - (i & 1) * 6);
    for (int i = 0; i < 7; i++) {
        Animator a; a.state = AnimState::ZWalk; a.frame = (uint8_t)((int)(G.uiTime * 3 + i) % 4);
        float x = fmodf(G.uiTime * (7 + i % 3) + i * 61, TOP_W + 40) - 20;
        drawChar(c, true, i % ZOMB_VARIANTS, x + camX, 214 + (i % 3) * 7 + camY, 0, a, 1, 0, 0, 0, 0);
    }
    gfx_fillA(c, 0, 0, TOP_W, TOP_H, rgb(6, 8, 20), 120);
    // flashlight sweep
    float a = sinf(G.uiTime * 0.5f) * 0.6f;
    for (int y = 170; y < 240; y += 2) for (int x = 0; x < TOP_W; x += 2) {
        float dx = x - 200.0f, dy = y - 150.0f, ang = atan2f(dx, dy);
        if (fabsf(ang - a) < 0.25f) gfx_fillA(c, x, y, 2, 2, rgb(255, 240, 200), 40);
    }
    { int w1 = gfx_textW("STAYING", 4), w2 = gfx_textW("ALIVE", 6);
      gfx_text(c, 200 - w1 / 2 + 3, 34 + 3, "STAYING", rgb(80, 10, 10), 4, false); gfx_text(c, 200 - w1 / 2, 34, "STAYING", pal::cream, 4, false);
      gfx_text(c, 200 - w2 / 2 + 3, 72 + 3, "ALIVE", rgb(80, 10, 10), 6, false); gfx_text(c, 200 - w2 / 2, 72, "ALIVE", pal::red, 6, false); }
    gfx_textC(c, 200, 128, "Scavenge. Survive. Rebuild.", pal::dim);
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
            for (int y = 0; y < TOP_H; y++) {
                const px* src = vbuf + (y * 100 / z) * v.w; px* dst = top.p + y * TOP_W;
                for (int x = 0; x < TOP_W; x++) dst[x] = src[xs[x]];
            }
        }
        render_overlay(top, z / 100.0f);
        tut_render(top);
        hud(top);
        if (game_paused()) {
            gfx_fillA(top, 0, 0, TOP_W, TOP_H, pal::ink, 110);
            if (G.panel == PN_INVENTORY || G.panel == PN_CONTAINER || G.panel == PN_PAUSE) {
                gfx_fill(top, 140, 104, 120, 26, pal::redDk); gfx_rect(top, 140, 104, 120, 26, pal::red);
                gfx_textC(top, 200, 108, "PAUSED", pal::cream, 2);
            }
        }
    }
    ui_render(bot);
}
