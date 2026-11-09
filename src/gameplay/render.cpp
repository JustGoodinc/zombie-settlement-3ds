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
    flashDX += (p.fx - flashDX) * 0.35f; flashDY += (p.fy - flashDY) * 0.35f;
    float fl = sqrtf(flashDX * flashDX + flashDY * flashDY) + 1e-4f;
    float fdx = flashDX / fl, fdy = flashDY / fl;
    int tx0 = camX / 16 - 1, ty0 = camY / 16 - 1, tx1 = (camX + TOP_W) / 16 + 1, ty1 = (camY + TOP_H) / 16 + 1;
    float t = G.uiTime;
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int i = m.idx(tx, ty);
        bool indoor = (m.flags[i] & F_INDOOR) != 0;
        float L = indoor ? 70 + day * 110 : 28 + day * 227;
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
            if (pr.type != PR_LAMP && pr.type != PR_FIRE) continue;
            float lx = pr.tx * 16 + 8.0f, ly = pr.ty * 16 + 8.0f, ld = sqrtf((cx - lx) * (cx - lx) + (cy - ly) * (cy - ly));
            float r = pr.type == PR_FIRE ? 88.0f : 72.0f;
            if (ld > r) continue;
            float flick = pr.type == PR_FIRE ? 0.85f + 0.15f * sinf(t * 13 + k) * sinf(t * 7.3f) : ((int)(t * 6 + k * 3) % 23 == 0 ? 0.4f : 1.0f);
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

static void drawChar(Canvas& c, bool zombie, int variant, float x, float y, uint8_t dir, const Animator& a, float fx, float fy, float flash, int weaponId, float muzzle, bool shadow = true) {
    const AnimFrame& f = a.cur();
    bool flip;
    const Sprite& s = art_char(zombie, variant, dir, (Pose)f.pose, flip);
    int sx = (int)(x + fx * f.kick) - camX - 8, sy = (int)(y + fy * f.kick * 0.5f + f.bob) - camY - 14;
    if (shadow && f.pose != P_DEAD) gfx_fillA(c, sx + 3, sy + 14, 10, 3, rgb(0, 0, 0), 110);
    // weapon: drawn behind the body when facing away from the camera
    auto weapon = [&]() {
        if (weaponId <= 0 || f.pose == P_DEAD || f.pose == P_FALL) return;
        const ItemDef& d = idef(weaponId);
        int hx = sx + 8 + (int)(fx * 3), hy = sy + 9 + (int)(fy * 2);
        float len = d.cat == IC_FIREARM ? (d.w >= 4 ? 10.0f : (d.w == 3 ? 8.0f : 6.0f)) : (d.w >= 3 ? 9.0f : 6.0f);
        float ax = fx, ay = fy;
        if (a.state == AnimState::Melee) {   // swing arc: anticipation -> strike
            float ang = atan2f(fy, fx) + (a.frame == 0 ? -1.2f : (a.frame == 1 ? 0.0f : 0.7f));
            ax = cosf(ang); ay = sinf(ang);
        }
        if (a.state == AnimState::Reload) { ax = fx * 0.5f; ay = 0.8f; }
        px col = d.cat == IC_FIREARM ? rgb(40, 40, 46) : (weaponId == IT_BAT ? rgb(150, 120, 80) : rgb(150, 150, 156));
        gfx_line(c, hx, hy, hx + (int)(ax * len), hy + (int)(ay * len), col);
        gfx_line(c, hx + 1, hy, hx + 1 + (int)(ax * len), hy + (int)(ay * len), gfx_scale(col, 160));
        if (muzzle > 0 && d.cat == IC_FIREARM) {
            int mx = hx + (int)(ax * (len + 3)), my = hy + (int)(ay * (len + 3));
            gfx_fill(c, mx - 1, my - 1, 3, 3, rgb(255, 230, 140));
            gfx_pset(c, mx + (int)(ax * 3), my + (int)(ay * 3), rgb(255, 200, 80));
            gfx_pset(c, mx - (int)(ay * 2), my + (int)(ax * 2), rgb(255, 200, 80));
            gfx_pset(c, mx + (int)(ay * 2), my - (int)(ax * 2), rgb(255, 200, 80));
        }
    };
    if (fy < -0.3f) weapon();
    gfx_blitEx(c, s, sx, sy, flip, 256, rgb(255, 250, 240), flash > 0 ? 170 : 0);
    if (fy >= -0.3f) weapon();
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
    int tx0 = camX / 16, ty0 = camY / 16, tx1 = (camX + TOP_W) / 16 + 1, ty1 = (camY + TOP_H) / 16 + 1;
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
    }
    // decals (blood) and corpses lie under everything
    for (int k = 0; k < G.ndecal; k++) gfx_blit(c, g_spr[S_BLOOD0 + G.decal[k].kind % 3], (int)G.decal[k].x - camX, (int)G.decal[k].y - camY);
    for (int k = 0; k < G.nz; k++) {
        Zombie& z = G.z[k];
        if (z.state != ZS_DEAD || z.anim.state != AnimState::Dead) continue;
        if (!visibleAt(z.x, z.y)) continue;
        drawChar(c, true, z.variant, z.x, z.y, z.dir, z.anim, z.hx, z.hy, 0, 0, 0, false);
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
            case PR_COT: gfx_blit(c, g_spr[S_COT], x, y); break;
            case PR_CHAIR: gfx_blit(c, g_spr[S_CHAIR], x, y); break;
            case PR_PLANT: gfx_blit(c, g_spr[S_PLANT], x, y); break;
            case PR_COOLER: gfx_blit(c, g_spr[S_COOLER], x, y); break;
            case PR_FIRE: gfx_blit(c, g_spr[S_FIRE0 + (int)(G.uiTime * 8) % 3], x, y); break;
            case PR_RADIO: gfx_blit(c, g_spr[S_RADIO], x, y); if ((int)(G.uiTime * 2) & 1) gfx_pset(c, x + 12, y + 4, rgb(255, 80, 60)); break;
            case PR_WORKBENCH: gfx_blit(c, g_spr[S_WORKBENCH], x, y); break;
            case PR_LAMP: gfx_blit(c, g_spr[S_LAMP], x, y - 4); break;
            case PR_GENERATOR: if (G.st.generator) gfx_blit(c, g_spr[S_GENPROP0 + ((int)(G.uiTime * 14) & 1)], x, y); break;
            case PR_PLOT: {
                int b = p.special, lv = G.st.bLevel[b];
                gfx_blit(c, g_building[b][lv > 3 ? 3 : lv], x, y);
                if (lv > 0 && G.st.bDamage[b]) {
                    for (int s = 0; s < G.st.bDamage[b] * 3; s++) gfx_fill(c, x + 8 + (hash32(b * 17 + s) % 30), y + 10 + (hash32(b * 7 + s) % 24), 3, 2, rgb(30, 26, 22));
                    int sy = (int)fmodf(G.uiTime * 10 + b * 9, 20);   // drifting smoke
                    gfx_fillA(c, x + 30, y + 4 - sy, 4, 4, rgb(90, 90, 96), 140 - sy * 6);
                }
                if (G.st.bBuildFx[b] > 0) {   // construction dust + scaffold flicker
                    for (int s = 0; s < 6; s++) gfx_pset(c, x + (hash32(G.tick + s) % 48), y + 30 + (hash32(G.tick * 3 + s) % 16), rgb(170, 150, 120));
                    if ((G.tick / 4) & 1) gfx_rect(c, x + 2, y + 2, 44, 44, pal::gold);
                }
                if (b == B_QUARTERS && lv && is_night()) { gfx_fill(c, x + 9, y + 32, 6, 4, rgb(240, 200, 100)); gfx_fill(c, x + 33, y + 32, 6, 4, rgb(240, 200, 100)); }
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
            if (u.escort && u.order == ORD_HOLD && ((int)(G.uiTime * 2) & 1)) gfx_textC(c, (int)x - camX, (int)y - camY - 26, "HELP!", pal::cream);
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
    px tint = day > 0.6f ? rgb(0, 0, 0) : (day > 0.15f ? rgb(40, 20, 16) : rgb(6, 10, 26));
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!m.in(tx, ty)) continue;
        int L = G.light[m.idx(tx, ty)];
        for (int q = 0; q < 4; q++) {
            int ox = q & 1, oy = q >> 1;
            int nx = tx + (ox ? 1 : -1), ny = ty + (oy ? 1 : -1);
            int Lx = m.in(nx, ty) ? G.light[m.idx(nx, ty)] : L, Ly = m.in(tx, ny) ? G.light[m.idx(tx, ny)] : L;
            int v = (L * 2 + Lx + Ly) / 4;
            if (v >= 250) continue;
            int x0 = tx * 16 + ox * 8 - camX, y0 = ty * 16 + oy * 8 - camY;
            for (int y = y0; y < y0 + 8; y++) {
                if (y < 0 || y >= c.h) continue;
                px* row = c.p + y * c.w;
                for (int x = x0; x < x0 + 8; x++) if (x >= 0 && x < c.w) row[x] = gfx_mix(tint, row[x], v + 1);
            }
        }
    }
    // ---- post-light overlays (stay readable at night)
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
    const Unit& pl = G.u[G.ctrl];
    if (pl.searchT > 0) {
        int x = (int)pl.x - camX - 12, y = (int)pl.y - camY - 26;
        ui_bar(c, x, y, 24, 4, 1 - pl.searchT / 1.4f, pal::gold);
    }
    if (pl.reloadT > 0) gfx_textC(c, (int)pl.x - camX, (int)pl.y - camY - 28, "RELOADING", pal::cream);
    for (auto& f : G.ft) if (f.life > 0) gfx_textC(c, (int)f.x - camX, (int)f.y - camY, f.txt, f.col);
    if (g_prompt && !game_paused()) {
        int x = (int)g_promptX - camX, y = (int)g_promptY - camY + 12;
        int w = gfx_textW(g_prompt) + 18;
        if (x - w / 2 < 2) x = w / 2 + 2; if (x + w / 2 > TOP_W - 2) x = TOP_W - 2 - w / 2;
        if (y > TOP_H - 40) y = TOP_H - 40;
        gfx_fillA(c, x - w / 2, y, w, 13, pal::ink, 200);
        gfx_rect(c, x - w / 2, y, w, 13, pal::border);
        ui_prompt(c, x - w / 2 + 3, y + 2, 'A', g_prompt);
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
        gfx_fillA(c, 200 - tw / 2, 216, tw, 14, pal::ink, a); gfx_rect(c, 200 - tw / 2, 216, tw, 14, pal::goldDk);
        gfx_textC(c, 200, 219, G.toast, pal::cream);
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
    gfx_text(c, 52 + 3, 40 + 3, "ZOMBIE", rgb(80, 10, 10), 5, false);
    gfx_text(c, 52, 40, "ZOMBIE", pal::cream, 5, false);
    gfx_text(c, 34 + 2, 88 + 2, "SETTLEMENT", rgb(80, 10, 10), 3, false);
    gfx_text(c, 34, 88, "SETTLEMENT", pal::red, 3, false);
    gfx_textC(c, 200, 124, "Scavenge. Survive. Rebuild.", pal::dim);
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
    static px zoomBuf[16 * 16]; Canvas z; z.init(zoomBuf, 16, 16);
    for (auto& p : zoomBuf) p = rgb(40, 40, 44);
    int keepX = camX, keepY = camY; camX = 0; camY = 0;
    drawChar(z, g_animTestZombie, g_animTestVariant, 8, 14, 2, a[8], 0, 1, 0, 0, 0, false);
    camX = keepX; camY = keepY;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) gfx_fill(c, 152 + i * 6, 96 + j * 6, 6, 6, zoomBuf[j * 16 + i]);
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
        render_world(top, alpha);
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
