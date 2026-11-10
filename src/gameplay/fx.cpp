// fx.cpp - cosmetic combat effects. Nothing here touches gameplay state or the gameplay RNG, so the rules,
// timings and tests are exactly as before; it only makes hits, shots and kills read on screen:
//   shots   : a burst of warm light, a puff of smoke, a brass casing that bounces and stays on the floor, camera recoil
//   impacts : sparks, dust and chips off walls, a bullet hole stamped into the wall
//   wounds  : blood thrown along the bullet's path, spattering the floor and any wall behind the target
//   kills   : a burst, a pool that spreads for a couple of seconds, and the body becomes part of the floor
// Everything permanent is stamped into the baked level layer, so a long firefight costs nothing to draw afterwards.
#include "game.h"
#include "../core/art.h"
#include "../core/rng.h"
#include <math.h>
#include <string.h>

namespace {
enum { FX_SPARK, FX_CASING, FX_BLOOD, FX_SMOKE, FX_DUST, FX_CHIP };
struct Part { float x, y, z, vx, vy, vz, life, max; px col; uint8_t kind, bounces; };
struct Pool { float x, y, t; uint32_t seed; bool on; };
enum { MAXP = 220, MAXL = 6, MAXPOOL = 10 };
Part parts[MAXP];
FxLight lights[MAXL];
Pool pools[MAXPOOL];
Rng frng(0x51ED5EED);
uint8_t corpseDone[MAX_ZOMB];

Part* alloc() {
    for (auto& p : parts) if (p.life <= 0) return &p;
    return &parts[frng.range(MAXP)];
}
float fr() { return frng.f(); }
float frs() { return frng.f() * 2 - 1; }
bool wallAt(float x, float y) {
    int tx = (int)(x / 16), ty = (int)(y / 16);
    return G.map.in(tx, ty) && G.map.t[G.map.idx(tx, ty)] == T_WALL;
}
void addLight(float x, float y, float r, px col, float life) {
    FxLight* l = &lights[0];
    for (auto& q : lights) { if (q.life <= 0) { l = &q; break; } if (q.life < l->life) l = &q; }
    l->x = x; l->y = y; l->r = r; l->col = col; l->life = l->max = life;
}
} // namespace

float g_fxKickX = 0, g_fxKickY = 0;

void fx_reset() {
    memset(parts, 0, sizeof parts); memset(lights, 0, sizeof lights); memset(pools, 0, sizeof pools); memset(corpseDone, 0, sizeof corpseDone);
    g_fxKickX = g_fxKickY = 0;
}

const FxLight* fx_lights(int& n) { n = MAXL; return lights; }

// a firearm went off at the muzzle (x,y) pointing (dx,dy); power ~ the weapon's shake value
void fx_shot(float x, float y, float dx, float dy, float power, bool player) {
    addLight(x + dx * 6, y + dy * 6, 64 + power * 10, rgb(255, 214, 130), 0.085f);
    for (int k = 0; k < 2; k++) {   // smoke drifting off the muzzle
        Part* p = alloc(); p->kind = FX_SMOKE; p->x = x + dx * (4 + k * 4); p->y = y + dy * (4 + k * 4); p->z = 6;
        p->vx = dx * (22 + fr() * 16) + frs() * 6; p->vy = dy * (22 + fr() * 16) + frs() * 6; p->vz = 10 + fr() * 8;
        p->life = p->max = 0.45f + fr() * 0.3f; p->col = rgb(206, 204, 210);
    }
    {   // brass: thrown out to the shooter's right, bounces, then lies there for good
        Part* p = alloc(); p->kind = FX_CASING; p->x = x - dx * 6; p->y = y - dy * 6; p->z = 7; p->bounces = 0;
        p->vx = -dy * (34 + fr() * 22) - dx * 12 + frs() * 8; p->vy = dx * (34 + fr() * 22) - dy * 12 + frs() * 8; p->vz = 70 + fr() * 30;
        p->life = p->max = 1.4f; p->col = rgb(236, 196, 72);
    }
    if (player) { float k = 2.2f + power * 0.8f; g_fxKickX -= dx * k; g_fxKickY -= dy * k; }
}

// a bullet struck something at (x,y) travelling (dx,dy) (any length). flesh: zombie body, else wall / furniture
void fx_bulletHit(float x, float y, float dx, float dy, bool flesh) {
    float l = sqrtf(dx * dx + dy * dy) + 1e-3f; dx /= l; dy /= l;
    if (!flesh) {
        for (int k = 0; k < 6; k++) {   // sparks kick back off the surface
            Part* p = alloc(); p->kind = FX_SPARK; p->x = x; p->y = y; p->z = 5 + fr() * 4;
            float a = atan2f(-dy, -dx) + frs() * 1.1f, s = 60 + fr() * 110;
            p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->vz = 20 + fr() * 50; p->life = p->max = 0.12f + fr() * 0.2f; p->col = rgb(255, 236, 160);
        }
        for (int k = 0; k < 2; k++) {
            Part* p = alloc(); p->kind = FX_DUST; p->x = x + frs() * 2; p->y = y + frs() * 2; p->z = 5;
            p->vx = -dx * (10 + fr() * 14) + frs() * 8; p->vy = -dy * (10 + fr() * 14) + frs() * 8; p->vz = 6; p->life = p->max = 0.35f + fr() * 0.25f; p->col = rgb(206, 200, 188);
        }
        for (int k = 0; k < 2; k++) {
            Part* p = alloc(); p->kind = FX_CHIP; p->x = x; p->y = y; p->z = 6; float a = atan2f(-dy, -dx) + frs() * 0.9f, s = 30 + fr() * 50;
            p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->vz = 40 + fr() * 40; p->life = p->max = 0.5f; p->col = rgb(64, 64, 80);
        }
        addLight(x, y, 26, rgb(255, 220, 150), 0.05f);
        // the hole: stamped on the wall just beyond the impact point
        float hx = x + dx * 3, hy = y + dy * 3;
        if (wallAt(hx, hy)) { bake_mark((int)hx, (int)hy, rgb(26, 24, 34)); bake_mark((int)hx + 1, (int)hy, rgb(26, 24, 34)); bake_mark((int)hx, (int)hy - 1, rgb(150, 146, 150)); }
        return;
    }
    for (int k = 0; k < 5; k++) {   // blood carried on along the bullet's path
        Part* p = alloc(); p->kind = FX_BLOOD; p->x = x; p->y = y; p->z = 7 + fr() * 4; p->bounces = 0;
        float a = atan2f(dy, dx) + frs() * 0.45f, s = 26 + fr() * fr() * 110;   // mostly close, a few long throws
        p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->vz = 20 + fr() * 50; p->life = p->max = 0.5f + fr() * 0.3f;
        p->col = k & 1 ? rgb(196, 26, 34) : rgb(150, 18, 28);
    }
    // spatter on a wall close behind the target
    for (float d = 6; d <= 26; d += 4) {
        float wx = x + dx * d, wy = y + dy * d;
        if (!wallAt(wx, wy)) continue;
        for (int k = 0; k < 9; k++) { int ox = (int)(frs() * 5), oy = (int)(frs() * 4); if (wallAt(wx + ox, wy + oy)) bake_mark((int)wx + ox, (int)wy + oy, k % 3 ? rgb(170, 22, 30) : rgb(120, 14, 24)); }
        break;
    }
}

// a zombie died at (x,y): a burst outward and a pool that keeps spreading
void fx_kill(float x, float y) {
    for (int k = 0; k < 12; k++) {
        Part* p = alloc(); p->kind = FX_BLOOD; p->x = x; p->y = y - 3; p->z = 8; p->bounces = 0;
        float a = fr() * 6.283f, s = 16 + fr() * fr() * 80;
        p->vx = cosf(a) * s; p->vy = sinf(a) * s * 0.7f; p->vz = 40 + fr() * 60; p->life = p->max = 0.6f + fr() * 0.3f;
        p->col = k & 1 ? rgb(196, 26, 34) : rgb(150, 18, 28);
    }
    for (auto& pl : pools) if (!pl.on) { pl.on = true; pl.x = x; pl.y = y + 2; pl.t = 0; pl.seed = frng.next(); break; }
}

void fx_update(float dt) {
    // camera recoil springs back
    float k = expf(-dt * 16); g_fxKickX *= k; g_fxKickY *= k;
    for (auto& l : lights) if (l.life > 0) l.life -= dt;
    for (auto& p : parts) {
        if (p.life <= 0) continue;
        p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.kind == FX_SMOKE || p.kind == FX_DUST) { p.z += p.vz * dt; p.vx *= 0.94f; p.vy *= 0.94f; continue; }
        p.vz -= 260 * dt; p.z += p.vz * dt;
        if (p.z > 0) continue;
        p.z = 0;
        if (p.kind == FX_CASING) {
            if (p.bounces < 2 && p.vz < -30) { p.vz *= -0.42f; p.vx *= 0.55f; p.vy *= 0.55f; p.bounces++; }
            else { bake_dot((int)p.x, (int)p.y, rgb(236, 196, 72)); bake_dot((int)p.x + 1, (int)p.y, rgb(156, 122, 40)); p.life = 0; }   // comes to rest for good
        } else if (p.kind == FX_BLOOD) {
            { float r = 1.1f + fr() * fr() * 2.6f; bake_blob(p.x, p.y, r, p.col, 236, frng.next()); if (r > 2.2f) bake_blob(p.x - p.vx * 0.02f, p.y - p.vy * 0.02f, r * 0.6f, p.col, 236, frng.next()); }   // big drops leave a short streak
            p.life = 0;
        } else if (p.kind == FX_CHIP) { bake_dot((int)p.x, (int)p.y, p.col); p.life = 0; }
        else p.life = 0;
    }
    for (auto& pl : pools) {
        if (!pl.on) continue;
        float before = pl.t; pl.t += dt;
        if ((int)(pl.t * 6) != (int)(before * 6)) bake_blob(pl.x, pl.y, 7.5f + pl.t * 2.4f, rgb(140, 14, 24), 150, pl.seed);   // same ragged outline, a little wider each time
        if (pl.t > 1.8f) pl.on = false;
    }
    // a body that has finished falling becomes part of the floor (and is never drawn per frame again)
    for (int i = 0; i < G.nz && i < MAX_ZOMB; i++) {
        Zombie& z = G.z[i];
        if (z.state != ZS_DEAD) { corpseDone[i] = 0; continue; }
        if (corpseDone[i] || z.anim.state != AnimState::Dead) continue;
        bool flip; const Sprite& s = art_char(true, z.variant, z.dir, (Pose)z.anim.cur().pose, flip);
        bake_sprite(s, (int)z.x - s.w / 2, (int)z.y - s.h + 2, flip, 256);
        corpseDone[i] = 1;
    }
}
bool fx_corpseBaked(int zi) { return zi >= 0 && zi < MAX_ZOMB && corpseDone[zi]; }

// particles that should be lit like the world (drawn before the lighting pass)
void fx_render(Canvas& c, int camX, int camY) {
    for (auto& p : parts) {
        if (p.life <= 0) continue;
        int x = (int)p.x - camX, y = (int)(p.y - p.z) - camY; float f = p.life / p.max;
        switch (p.kind) {
        case FX_SMOKE: { int r = 1 + (int)((1 - f) * 3); gfx_fillA(c, x - r, y - r, r * 2 + 1, r * 2 + 1, p.col, (int)(120 * f)); gfx_fillA(c, x - r + 1, y - r - 1, r * 2 - 1, r * 2 + 3, p.col, (int)(70 * f)); break; }
        case FX_DUST: { int r = 1 + (int)((1 - f) * 2); gfx_fillA(c, x - r, y - r, r * 2 + 1, r * 2, p.col, (int)(140 * f)); break; }
        case FX_CASING: { bool flat = ((int)(p.life * 30)) & 1; gfx_pset(c, x, y, p.col); gfx_pset(c, flat ? x + 1 : x, flat ? y : y + 1, rgb(156, 122, 40)); gfx_pset(c, x, (int)p.y - camY, rgb(20, 18, 36)); break; }
        case FX_BLOOD: gfx_pset(c, x, y, p.col); gfx_pset(c, x + 1, y, p.col); if (f > 0.5f) gfx_pset(c, x, y + 1, p.col); break;
        case FX_CHIP: gfx_pset(c, x, y, p.col); break;
        }
    }
}
// bright things that ignore the darkness (drawn after the lighting pass): sparks with a short tail
void fx_renderGlow(Canvas& c, int camX, int camY) {
    for (auto& p : parts) {
        if (p.life <= 0 || p.kind != FX_SPARK) continue;
        int x = (int)p.x - camX, y = (int)(p.y - p.z) - camY; float f = p.life / p.max;
        px col = f > 0.5f ? rgb(255, 244, 190) : rgb(255, 176, 70);
        gfx_pset(c, x, y, col); gfx_pset(c, x - (int)(p.vx * 0.012f), y - (int)((p.vy - p.vz) * 0.012f), rgb(240, 130, 50));
    }
}
