#include "game.h"
#include "../core/audio.h"
#include "../core/art.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

Game G;
float g_renderAlpha = 0;
Settler g_npc[MAX_SURV];
const char* TRAIT_NAMES[TR_COUNT] = {"None", "Steady Aim", "Tough", "Green Thumb", "Field Medic", "Tinkerer", "Watchful"};
const char* INF_NAMES[4] = {"Healthy", "Infected (Early)", "Infected (Advanced)", "Infected (Critical)"};

static float len2(float x, float y) { return sqrtf(x * x + y * y); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void game_toast(const char* fmt, ...) {
    va_list a; va_start(a, fmt); vsnprintf(G.toast, sizeof(G.toast), fmt, a); va_end(a);
    G.toastT = 3.0f;
}
const char* clock_str() {
    static char b[16]; int m = (int)G.clock;
    snprintf(b, sizeof b, "%02d:%02d", (m / 60) % 24, m % 60); return b;
}
float daylight() {
    float h = (float)G.clock / 60.0f;
    if (h >= 7 && h < 18) return 1;
    if (h >= 5 && h < 7) return (h - 5) / 2;
    if (h >= 18 && h < 20) return 1 - (h - 18) / 2;
    return 0;
}
bool is_night() { return daylight() < 0.35f; }
int unit_ctrl() { return G.ctrl; }
bool game_paused() { return G.panel != PN_HUD || G.scene == SC_TITLE; }

void spawn_particles(int kind, float x, float y, int n, px col) {
    for (int k = 0; k < n; k++) {
        Particle* p = nullptr;
        for (auto& q : G.part) if (q.life <= 0) { p = &q; break; }
        if (!p) { p = &G.part[G.rng.range(MAX_PART)]; }  // capped pool: recycle
        p->x = x; p->y = y; p->z = 4; p->kind = (uint8_t)kind; p->col = col;
        float a = G.rng.f() * 6.283f, s = 20 + G.rng.f() * 50;
        p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->vz = 20 + G.rng.f() * 40;
        p->max = p->life = 0.3f + G.rng.f() * 0.4f;
        if (kind == PK_SMOKE || kind == PK_DUST) { p->vx *= 0.2f; p->vy *= 0.2f; p->vz = 8; p->max = p->life = 1.2f + G.rng.f(); }
        if (kind == PK_LEAF) { p->vx = 8 + G.rng.f() * 10; p->vy = 4 + G.rng.f() * 6; p->vz = 0; p->z = 20; p->max = p->life = 4; }
        if (kind == PK_MUZZLE) { p->vx = p->vy = 0; p->vz = 0; p->max = p->life = 0.05f; }
        if (kind == PK_EMBER) { p->vx *= 0.15f; p->vy = -10 - G.rng.f() * 10; p->vz = 0; p->z = 6; p->max = p->life = 0.8f + G.rng.f() * 0.6f; }
    }
}
void float_text(float x, float y, const char* s, px col) {
    FloatText* f = &G.ft[0];
    for (auto& q : G.ft) if (q.life <= 0) { f = &q; break; }
    f->x = x; f->y = y; f->life = 0.9f; f->col = col;
    snprintf(f->txt, sizeof f->txt, "%s", s);
}
// blood is stamped into the level layer: it stays for the whole visit and costs nothing to draw afterwards.
// (cosmetic only: uses its own counter, never the gameplay RNG)
static uint32_t g_goreSeed = 1;
static void blood_drop(float x, float y) {
    uint32_t s = g_goreSeed++ * 2654435761u;
    bake_blob(x, y, 1.7f + (s >> 28) * 0.12f, rgb(172, 22, 30), 236, s);
}
static void blood_pool(float x, float y) {
    uint32_t s = g_goreSeed++ * 2654435761u;
    bake_blob(x, y, 8.5f + (s >> 29), rgb(146, 16, 26), 232, s);              // the pool
    bake_blob(x - 1, y - 1, 5.0f, rgb(190, 26, 34), 220, s + 1);              // wet, brighter middle
    for (int k = 0; k < 7; k++) { uint32_t h = hash32(s + k * 77); bake_blob(x + (int)(h % 31) - 15, y + (int)((h >> 8) % 19) - 9, 1.4f + ((h >> 16) & 3) * 0.5f, rgb(164, 20, 30), 236, h); }   // spatter
}

// ---------------------------------------------------------------- weapons / items
static Item* equipped(Survivor& s) { return (s.equip >= 0 && s.equip < s.pack.n) ? &s.pack.it[s.equip] : nullptr; }

void survivor_equipBest(Survivor& s) {
    int best = -1, score = -1;
    for (int i = 0; i < s.pack.n; i++) {
        const ItemDef& d = idef(s.pack.it[i].id);
        int sc = -1;
        if (d.cat == IC_FIREARM) sc = 100 + d.wpn.dmg + (s.pack.countOf(d.wpn.ammo) + s.pack.it[i].mag > 0 ? 200 : 0);
        else if (d.cat == IC_MELEE) sc = d.wpn.dmg;
        if (sc > score && (s.pack.it[i].dur > 0 || d.wpn.dur == 0)) { score = sc; best = i; }
    }
    s.equip = (int8_t)best;
}

bool survivor_useItem(int sv, int i, int ui) {
    Survivor& s = G.st.surv[sv];
    if (i < 0 || i >= s.pack.n) return false;
    Item& it = s.pack.it[i];
    const ItemDef& d = idef(it.id);
    if (d.cat == IC_MEDICAL) {
        if (it.id == IT_ANTIBIOTICS) {
            if (s.infection == INF_EARLY) { s.infection = INF_NONE; s.infHours = 0; game_toast("%s's infection is cured.", s.name); }
            else if (s.infection > INF_EARLY) { s.infHours = 0; game_toast("Antibiotics slow the infection."); }
            else { game_toast("No infection to treat."); return false; }
        } else {
            if (s.hp >= s.maxHp) { game_toast("%s is already at full health.", s.name); return false; }
            s.hp = fminf(s.maxHp, s.hp + d.heal + s.skill[SK_MEDIC] * 1.5f);
            skill_xp(s, SK_MEDIC, 2);
        }
        if (ui >= 0) { G.u[ui].anim.play(AnimState::Heal, true); G.u[ui].healT = 0.75f; }
        audio_play(SFX_HEAL);
        bool wasEquipLater = s.equip > i;
        if (--it.count == 0) { s.pack.remove(i); if (s.equip == i) s.equip = -1; else if (wasEquipLater) s.equip--; }
        return true;
    }
    if (d.cat == IC_BACKPACK) {
        int nw = it.id == IT_PACK_M ? 8 : 10, nh = it.id == IT_PACK_M ? 5 : 6;
        if (s.pack.w >= nw) { game_toast("Already using a bag this size."); return false; }
        int8_t eqId = s.equip >= 0 ? (int8_t)s.pack.it[s.equip].id : -1;
        Grid backup = s.pack;
        s.pack.remove(i);
        if (!s.pack.resize(nw, nh)) { s.pack = backup; return false; }
        s.equip = -1;
        if (eqId >= 0) for (int k = 0; k < s.pack.n; k++) if (s.pack.it[k].id == eqId) { s.equip = (int8_t)k; break; }
        game_toast("Backpack upgraded to %dx%d!", nw, nh);
        audio_play(SFX_PICKUP);
        return true;
    }
    if (d.cat == IC_FIREARM || d.cat == IC_MELEE) { s.equip = (int8_t)i; audio_play(SFX_RELOAD, 0.5f); game_toast("Equipped %s.", d.name); return true; }
    return false;
}

static void do_reload(Unit& u, Survivor& s) {
    Item* w = equipped(s);
    if (!w || idef(w->id).cat != IC_FIREARM) return;
    const WeaponStats& ws = idef(w->id).wpn;
    if (w->mag >= ws.mag || u.reloadT > 0) return;
    if (s.pack.countOf(ws.ammo) <= 0) { if (u.sv == G.u[G.ctrl].sv) game_toast("No %s.", idef(ws.ammo).name); return; }
    u.reloadT = ws.reloadMs / 1000.0f;
    if (u.sv == G.u[G.ctrl].sv) G.statReloads++;
    u.anim.play(AnimState::Reload, true);
    audio_playAt(SFX_RELOAD, u.x, u.y);
}
static void finish_reload(Unit& u, Survivor& s) {
    Item* w = equipped(s);
    if (!w || idef(w->id).cat != IC_FIREARM) return;
    const WeaponStats& ws = idef(w->id).wpn;
    int need = ws.mag - w->mag;
    int ammoId = ws.ammo;
    int8_t eqItemIdx = s.equip;
    Item wcopy = *w;
    int got = s.pack.take(ammoId, need);
    // take() may remove stacks and shift indices: re-find the equipped weapon
    s.equip = -1;
    for (int k = 0; k < s.pack.n; k++) if (s.pack.it[k].id == wcopy.id && s.pack.it[k].x == wcopy.x && s.pack.it[k].y == wcopy.y) { s.equip = (int8_t)k; break; }
    (void)eqItemIdx;
    if (s.equip >= 0) s.pack.it[s.equip].mag += (uint8_t)got;
    audio_playAt(SFX_RELOAD, u.x, u.y, 0.8f, 1.2f);
}

// zombie hit resolution: the only place damage is applied
static void zombie_kill(int zi, float fromX, float fromY, bool head);
static void zombie_hit(int zi, int result, int dmg, float fromX, float fromY, int sv) {
    Zombie& z = G.z[zi];
    if (z.state == ZS_DEAD) return;
    float dx = z.x - fromX, dy = z.y - fromY, l = len2(dx, dy) + 0.01f;
    if (result == HR_MISS) return;
    if (sv >= 0) skill_xp(G.st.surv[sv], SK_SHOOT, 1);
    if (result == HR_HEAD) {
        float_text(z.x, z.y - 14, "HEADSHOT", pal::gold);
        spawn_particles(PK_BLOOD, z.x, z.y - 6, 14, pal::blood);
        fx_bulletHit(z.x, z.y - 10, dx, dy, true);
        zombie_kill(zi, fromX, fromY, true);
        if (sv >= 0) G.st.surv[sv].kills++;
        return;
    }
    z.hp -= hit_damage((HitResult)result, dmg);
    z.flash = 0.12f;
    z.kx += dx / l * (result == HR_BODY ? 5.0f : 3.0f); z.ky += dy / l * (result == HR_BODY ? 5.0f : 3.0f);
    spawn_particles(PK_BLOOD, z.x + dx / l * 3, z.y - 4, result == HR_BODY ? 6 : 4, pal::blood);
    fx_bulletHit(z.x, z.y - 6, dx, dy, true);
    audio_playAt(SFX_HIT, z.x, z.y, 0.7f);
    if (result == HR_LEG) { z.slowT = 2.5f; z.anim.play(AnimState::ZStumble, true); float_text(z.x, z.y - 14, "LEG", pal::dim); }
    else { z.anim.play(AnimState::ZHurt); if (result == HR_ARM) float_text(z.x, z.y - 14, "ARM", pal::dim); }
    if (z.hp <= 0) { zombie_kill(zi, fromX, fromY, false); if (sv >= 0) G.st.surv[sv].kills++; return; }
    if (z.state != ZS_CHASE) { z.state = ZS_CHASE; z.tx = fromX; z.ty = fromY; z.lostT = 0; }
}
static void zombie_kill(int zi, float, float, bool head) {
    Zombie& z = G.z[zi];
    z.state = ZS_DEAD; z.hp = 0; z.atkPending = false;
    z.anim.play(AnimState::ZDeath, true);
    audio_playAt(SFX_ZDIE, z.x, z.y, head ? 0.6f : 1.0f);
    (void)G.rng.range(3); blood_pool(z.x, z.y + 2); fx_kill(z.x, z.y);
    if (z.spawnIdx >= 0 && G.curLoc >= 0) G.loc[G.curLoc].deadMask[z.spawnIdx / 32] |= 1u << (z.spawnIdx % 32);
}

static void noise_event(float x, float y, float radius) {
    for (int i = 0; i < G.nz; i++) {
        Zombie& z = G.z[i];
        if (z.state == ZS_DEAD || z.state == ZS_CHASE) continue;
        if (len2(z.x - x, z.y - y) > radius) continue;
        if (z.state == ZS_LYING) { z.state = ZS_RISING; z.anim.play(AnimState::ZRise, true); }
        else z.state = ZS_INVESTIGATE;
        z.tx = x + G.rng.between(-16, 16); z.ty = y + G.rng.between(-16, 16); z.timer = 8;
    }
}

static bool zombie_visibleTo(const Unit& u, const Zombie& z, float range) {
    if (z.state == ZS_DEAD) return false;
    float d = len2(z.x - u.x, z.y - u.y);
    if (d > range) return false;
    if (!map_los(G.map, u.x, u.y, z.x, z.y)) return false;
    int tx = (int)(z.x / 16), ty = (int)(z.y / 16);
    // must be lit enough to see (night limits visibility); companions see what the flashlight / day reveals
    if (G.map.in(tx, ty) && G.light[G.map.idx(tx, ty)] < 60 && d > 28) return false;
    return true;
}

static int aim_target(const Unit& u, float cone, float range) {
    float ca = cosf(cone * 3.14159f / 180.0f), best = 1e9f; int bi = -1;
    for (int i = 0; i < G.nz; i++) {
        const Zombie& z = G.z[i];
        if (z.state == ZS_DEAD) continue;
        float dx = z.x - u.x, dy = (z.y - 4) - u.y, d = len2(dx, dy);
        if (d > range || d < 1) continue;
        float dot = (dx * u.fx + dy * u.fy) / d;
        if (dot < ca) continue;
        if (!map_los(G.map, u.x, u.y, z.x, z.y)) continue;
        float score = d * (2.0f - dot);
        if (score < best) { best = score; bi = i; }
    }
    return bi;
}

static void spawn_proj(float x, float y, float ang, int target, int result, int dmg, int owner, bool untargeted, int item, float speed) {
    for (auto& p : G.proj) if (!p.active) {
        p.active = true; p.untargeted = untargeted; p.x = x; p.y = y;
        p.vx = cosf(ang) * speed; p.vy = sinf(ang) * speed;
        p.life = idef(item).wpn.range / speed; p.target = (int8_t)target; p.result = (uint8_t)result; p.dmg = (uint8_t)dmg; p.owner = (uint8_t)owner; p.item = (uint8_t)item;
        return;
    }
}

static void fire(int ui) {
    Unit& u = G.u[ui];
    if (u.sv < 0 || u.cd > 0 || u.reloadT > 0 || u.dodgeT > 0) return;
    Survivor& s = G.st.surv[u.sv];
    Item* w = equipped(s);
    bool isPlayer = ui == G.ctrl;
    if (!w || idef(w->id).cat == IC_MELEE) {
        // melee (or bare fists): resolved on the animation's impact event
        int cost = w ? idef(w->id).wpn.stamina : 8;
        if (w && w->dur == 0 && idef(w->id).wpn.dur > 0) { if (isPlayer) game_toast("%s is broken. Repair it at the Workshop.", idef(w->id).name); u.cd = 0.4f; return; }
        if (u.stam < cost) { if (isPlayer) float_text(u.x, u.y - 18, "TIRED", pal::dim); u.cd = 0.3f; return; }
        u.stam -= cost;
        u.cd = w ? idef(w->id).wpn.cooldownMs / 1000.0f : 0.45f;
        u.meleePending = true;
        u.anim.play(AnimState::Melee, true);
        if (isPlayer) G.statShots++;
        return;
    }
    const ItemDef& d = idef(w->id); const WeaponStats& ws = d.wpn;
    if (w->dur == 0) { if (isPlayer) game_toast("%s is broken. Repair it at the Workshop.", d.name); audio_play(SFX_EMPTY); u.cd = 0.4f; return; }
    if (w->mag == 0) {
        audio_playAt(SFX_EMPTY, u.x, u.y); u.cd = 0.3f;
        if (isPlayer) float_text(u.x, u.y - 18, "EMPTY", pal::red);
        do_reload(u, s);
        return;
    }
    w->mag--; w->dur--;
    u.cd = ws.cooldownMs / 1000.0f;
    if (isPlayer) G.statShots++;
    u.muzzleT = 0.07f;
    bool moving = len2(u.x - u.px, u.y - u.py) > 0.3f;
    if (!moving) u.anim.play(AnimState::Shoot, true);
    static const Sfx SND[] = {SFX_PISTOL, SFX_PISTOL, SFX_REVOLVER, SFX_SHOTGUN, SFX_RIFLE, SFX_SMG};
    audio_playAt(SND[w->id <= IT_SMG ? w->id : 0], u.x, u.y);
    noise_event(u.x, u.y, ws.noise);
    if (isPlayer && G.set.shake) G.shake = fmaxf(G.shake, ws.shake * 1.2f);
    float mx = u.x + u.fx * 9, my = u.y - 5 + u.fy * 9;
    spawn_particles(PK_MUZZLE, mx, my, 1, pal::gold);
    fx_shot(mx, my, u.fx, u.fy, ws.shake, isPlayer);
    // ejected casing
    Particle* c = nullptr; for (auto& q : G.part) if (q.life <= 0) { c = &q; break; }
    if (c) { c->x = u.x; c->y = u.y - 4; c->z = 6; c->vx = -u.fy * 40 + G.rng.between(-8, 8); c->vy = u.fx * 40; c->vz = 50; c->life = c->max = 0.6f; c->kind = PK_CASING; c->col = pal::gold; }
    audio_playAt(SFX_CASING, u.x, u.y, 0.5f, 0.9f + G.rng.f() * 0.2f);

    float baseAng = atan2f(u.fy, u.fx);
    int skill = s.skill[SK_SHOOT] + (s.trait == TR_STEADY ? 1 : 0);
    for (int p = 0; p < ws.pellets; p++) {
        float spread = ws.pellets > 1 ? (G.rng.f() - 0.5f) * ws.cone * 2 * 3.14159f / 180.0f : 0;
        int ti = aim_target(u, ws.cone, ws.range);
        ShotParams sp; sp.skill = skill; sp.weaponAcc = ws.acc + w->mod * 8 - (ws.pellets > 1 ? 10 : 0);
        sp.range = ws.range; sp.moving = moving; sp.injured = s.hp < 50;
        if (ti >= 0) {
            Zombie& z = G.z[ti];
            sp.dist = len2(z.x - u.x, z.y - u.y);
            sp.targetStill = z.state == ZS_IDLE || z.state == ZS_LYING;
            sp.targetDown = z.state == ZS_LYING;
            HitResult r = hit_roll(hit_table(sp), G.rng);
            float ang = atan2f((z.y - 5) - my, z.x - mx);
            if (r == HR_MISS) {
                // visibly wide of the target so the effect matches the outcome
                float off = (0.08f + G.rng.f() * 0.08f) * (G.rng.chance(0.5f) ? 1 : -1);
                spawn_proj(mx, my, ang + off + spread * 0.3f, -1, HR_MISS, 0, (uint8_t)u.sv, false, w->id, 520);
                if (p == 0 && isPlayer) float_text(z.x, z.y - 14, "MISS", pal::grey);
            } else {
                spawn_proj(mx, my, ang + spread * 0.15f, ti, r, ws.dmg, (uint8_t)u.sv, false, w->id, 520);
            }
        } else {
            spawn_proj(mx, my, baseAng + spread, -1, HR_BODY, ws.dmg, (uint8_t)u.sv, true, w->id, 520);
        }
    }
}

static void resolve_melee(int ui) {
    Unit& u = G.u[ui];
    Survivor& s = G.st.surv[u.sv];
    Item* w = equipped(s);
    const WeaponStats* ws = w && idef(w->id).cat == IC_MELEE ? &idef(w->id).wpn : nullptr;
    float reach = ws ? ws->reach + 6.0f : 18.0f; int dmg = ws ? ws->dmg : 14;
    audio_playAt(SFX_SWING, u.x, u.y);
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < G.nz; i++) {
        Zombie& z = G.z[i]; if (z.state == ZS_DEAD) continue;
        float dx = z.x - u.x, dy = z.y - u.y, d = len2(dx, dy);
        if (d > reach || d < 0.1f) continue;
        if ((dx * u.fx + dy * u.fy) / d < 0.45f) continue;   // ~60 degree arc
        if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) return;
    ShotParams sp; sp.skill = s.skill[SK_MELEE]; sp.weaponAcc = (ws ? ws->acc : 0) + 20; sp.dist = bd; sp.range = reach;
    sp.injured = s.hp < 50; sp.targetDown = G.z[best].state == ZS_LYING;
    HitResult r = hit_roll(hit_table(sp), G.rng);
    if (r == HR_MISS) { float_text(G.z[best].x, G.z[best].y - 14, "MISS", pal::grey); return; }
    if (w && ws && ws->dur > 0 && w->dur > 0) { w->dur--; if (w->dur == 0) game_toast("%s broke!", idef(w->id).name); }
    skill_xp(s, SK_MELEE, 1);
    audio_playAt(SFX_HIT, G.z[best].x, G.z[best].y, 1.0f, 0.8f);
    spawn_particles(PK_SPARK, G.z[best].x, G.z[best].y - 6, 3, pal::cream);
    if (G.set.shake) G.shake = fmaxf(G.shake, 1.0f);
    zombie_hit(best, r, dmg, u.x, u.y, -1);
    if (G.z[best].state != ZS_DEAD) { G.z[best].kx += u.fx * 8; G.z[best].ky += u.fy * 8; }
    else s.kills++;
}

// ---------------------------------------------------------------- level management


static void unit_init(Unit& u, int sv, float x, float y) {
    memset(&u, 0, sizeof(u));
    u.sv = (int8_t)sv; u.alive = true; u.x = u.px = x; u.y = u.py = y; u.fx = 0; u.fy = 1; u.stam = 100; u.searchProp = -1;
    u.dir = 2; u.anim = Animator();
}

static float g_escortHp[MAX_UNITS];   // trapped survivors have no Survivor record yet; full health on every level entry
void level_enter(int mapId, int loc) {
    for (int i = 0; i < MAX_UNITS; i++) g_escortHp[i] = 60;
    map_load(G.map, mapId);
    G.curLoc = loc; G.nz = 0; G.nu = 0; G.ctrl = 0; G.inLevel = true;
    memset(G.proj, 0, sizeof G.proj); memset(G.part, 0, sizeof G.part); memset(G.ft, 0, sizeof G.ft);
    memset(G.explored, 0, sizeof G.explored);
    G.scene = mapId == MAP_SETTLEMENT ? SC_SETTLEMENT : SC_EXPEDITION;
    if (loc >= 0) {
        LocState& L = G.loc[loc];
        if (L.visited) {
            for (int i = 0; i < G.map.nbar; i++) { G.map.bar[i].state = L.barState[i]; G.map.bar[i].hp = L.barHp[i]; G.map.bar[i].anim = L.barState[i] == BS_OPEN ? 1.0f : 0.0f; }
        } else {
            L.visited = true;
            for (int i = 0; i < G.map.nbar; i++) { L.barState[i] = G.map.bar[i].state; L.barHp[i] = G.map.bar[i].hp; }
        }
        map_refresh(G.map);
        int zi = 0;
        for (int s = 0; s < G.map.nspawn; s++) {
            const Spawn& sp = G.map.spawn[s];
            if (sp.kind == 'R') {
                if (!L.rescued && G.nu < MAX_UNITS) {
                    // filled after squad (index chosen below)
                }
                continue;
            }
            if (sp.kind != 'Z' && sp.kind != 'z' && sp.kind != 'q') continue;
            int idx = zi++;
            if (L.deadMask[idx / 32] & (1u << (idx % 32))) continue;
            if (G.nz >= MAX_ZOMB) continue;
            Zombie& z = G.z[G.nz++];
            memset(&z, 0, sizeof z);
            z.x = z.px = sp.x * 16 + 8.0f; z.y = z.py = sp.y * 16 + 10.0f;
            z.hp = 100; z.spawnIdx = (int8_t)idx;
            // who they were (looks only): the place's own people indoors, inmates by the cells, passers-by outside
            z.variant = (uint8_t)zombie_look(mapId, (G.map.flags[G.map.idx(sp.x, sp.y)] & F_INDOOR) != 0, hash32(idx * 7 + L.gen));
            for (int b = 0; b < G.map.nbar; b++) if (G.map.bar[b].kind == BK_CELLDOOR && abs(G.map.bar[b].x - sp.x) + abs(G.map.bar[b].y - sp.y) <= 3) z.variant = (uint8_t)(ZL_PRISONER + z.variant / ZOMB_LOOKS * ZOMB_LOOKS);
            z.speed = 0.85f + (hash32(idx * 13 + 5) % 30) / 100.0f;
            z.state = sp.kind == 'Z' ? ZS_IDLE : (sp.kind == 'z' ? ZS_WANDER : ZS_LYING);
            z.anim = Animator(); z.anim.speed = 0.85f + (hash32(idx * 31) % 30) / 100.0f;
            z.anim.play(z.state == ZS_LYING ? AnimState::ZLying : AnimState::ZIdle, true);
            z.anim.t = (hash32(idx) % 100) / 100.0f;  // desync idle cycles
            z.dir = (uint8_t)(hash32(idx * 3) % 8); z.hx = 0; z.hy = 1; z.target = -1;
        }
        // squad
        float sx = G.map.spawnX * 16 + 8.0f, sy = G.map.spawnY * 16 + 8.0f;
        for (int k = 0; k < 3; k++) {
            if (G.squad[k] < 0 || !G.st.surv[G.squad[k]].alive) continue;
            unit_init(G.u[G.nu], G.squad[k], sx + (G.nu == 0 ? 0 : (G.nu == 1 ? -14 : 14)), sy + (G.nu == 0 ? 0 : 8));
            G.u[G.nu].fy = -1; G.u[G.nu].dir = 6;
            G.nu++;
        }
        if (!L.rescued) for (int s = 0; s < G.map.nspawn; s++) if (G.map.spawn[s].kind == 'R' && G.nu < MAX_UNITS) {
            Unit& e = G.u[G.nu++];
            unit_init(e, -1, G.map.spawn[s].x * 16 + 8.0f, G.map.spawn[s].y * 16 + 8.0f);
            e.escort = true; e.order = ORD_HOLD; e.npcVariant = (uint8_t)(3 + L.gen % 4);
        }
        G.flowT = 0;
    } else {
        // settlement: controlled leader + plots solid when built
        for (int i = 0; i < G.map.nprop; i++) {
            Prop& p = G.map.prop[i];
            if (p.type == PR_PLOT) p.solid = G.st.bLevel[p.special] > 0;
            if (p.type == PR_PLOT) for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) G.map.propAt[G.map.idx(p.tx + k, p.ty + j)] = (int16_t)i;
        }
        for (int i = 0; i < G.map.nbar; i++) if (G.map.bar[i].kind == BK_GATE) G.map.bar[i].state = BS_CLOSED;
        map_refresh(G.map);
        int lead = G.st.leader;
        if (lead < 0 || !G.st.surv[lead].alive) for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive) { lead = i; break; }
        G.st.leader = lead;
        if (lead >= 0) { unit_init(G.u[0], lead, G.map.spawnX * 16 + 8.0f, G.map.spawnY * 16 + 8.0f); G.nu = 1; }
        for (int i = 0; i < G.st.nsurv; i++) {
            Settler& n = g_npc[i];
            n.x = n.px = n.tx = (G.map.spawnX - 6 + (int)(hash32(i) % 12)) * 16 + 8.0f;
            n.y = n.py = n.ty = (G.map.spawnY - 6 + (int)(hash32(i * 5) % 6)) * 16 + 8.0f;
            n.wait = (hash32(i * 9) % 30) / 10.0f; n.dir = 2; n.anim = Animator(); n.anim.speed = 0.9f + (hash32(i) % 20) / 100.0f;
        }
    }
    G.camX = G.u[0].x - view_w() / 2; G.camY = G.u[0].y - view_h() / 2;
    art_zombie_flush();   // zombie frames are drawn on demand: start each level with an empty set
    bake_level();   // draw the static level layer once (floors, walls, shadows); the renderer copies from it
    fx_reset();

}

static void sync_loc_state() {
    if (G.curLoc < 0) return;
    LocState& L = G.loc[G.curLoc];
    for (int i = 0; i < G.map.nbar; i++) { L.barState[i] = G.map.bar[i].state; L.barHp[i] = G.map.bar[i].hp; }
}
void level_leave() { sync_loc_state(); G.inLevel = false; }

// ---------------------------------------------------------------- interaction
enum ItKind { IK_NONE, IK_SEARCH, IK_OPEN_CONT, IK_BAG, IK_DOOR, IK_RESCUE, IK_EXTRACT, IK_PLOT, IK_RADIO, IK_VISITOR, IK_WORKBENCH, IK_SLEEP };
struct Interact { int kind = IK_NONE, idx = -1; const char* label = ""; float x = 0, y = 0; };

static Interact find_interact(const Unit& u) {
    Interact best; float bd = 1e9f;
    auto consider = [&](int kind, int idx, float x, float y, float range, const char* label) {
        float d = len2(x - u.x, y - u.y);
        if (d > range) return;
        // what you're facing wins over what's merely beside you (a door you walk into beats the shelf next to it)
        float score = d;
        if (d > 1 && ((x - u.x) * u.fx + (y - u.y) * u.fy) / d > 0.6f) score *= 0.6f;
        if (score < bd) { bd = score; best.kind = kind; best.idx = idx; best.label = label; best.x = x; best.y = y; }
    };
    for (int i = 0; i < G.map.nprop; i++) {
        const Prop& p = G.map.prop[i];
        float cx = p.tx * 16 + p.tw * 8.0f, cy = p.ty * 16 + p.th * 8.0f;
        if (p.container >= 0 && G.curLoc >= 0) {
            bool searched = G.loc[G.curLoc].generated[p.container];
            // reach is measured to the nearest tile of the footprint (same as the centre for 1x1 props),
            // so long shipping containers and boxcars can be searched from any side
            float nx = fminf(fmaxf(u.x, p.tx * 16 + 8.0f), (p.tx + p.tw) * 16 - 8.0f), ny = fminf(fmaxf(u.y, p.ty * 16 + 8.0f), (p.ty + p.th) * 16 - 8.0f);
            int tx = (int)(nx / 16), ty = (int)(ny / 16);
            if (G.light[G.map.idx(tx, ty)] < 40 && !searched) continue;  // unseen in the dark
            consider(searched ? IK_OPEN_CONT : IK_SEARCH, i, nx, ny, 22, searched ? "Open" : "Search");
            (void)cx; (void)cy;
        }
        if (G.scene == SC_SETTLEMENT) {
            if (p.type == PR_PLOT) consider(IK_PLOT, i, cx, cy + 12, 36, BUILDINGS[p.special].name);
            if (p.type == PR_RADIO) consider(IK_RADIO, i, cx, cy, 22, "World Map");
            if (p.type == PR_WORKBENCH) consider(IK_WORKBENCH, i, cx, cy, 22, "Workshop");
            if (p.type == PR_FIRE) consider(IK_SLEEP, i, cx, cy, 24, "Rest");
        }
    }
    if (G.curLoc >= 0) for (int b = 0; b < MAX_BAGS; b++) if (G.loc[G.curLoc].bags[b].used) consider(IK_BAG, b, G.loc[G.curLoc].bags[b].x, G.loc[G.curLoc].bags[b].y, 18, "Pick through");
    for (int i = 0; i < G.map.nbar; i++) {
        const Barrier& b = G.map.bar[i];
        if (b.kind == BK_WINDOW || b.state == BS_BROKEN) continue;
        // standing in an open doorway: it can't be shut on you, so don't let it hide what's beyond (e.g. Rescue)
        if (b.state == BS_OPEN && fabsf(u.x - (b.x * 16 + 8.0f)) < 11 && fabsf(u.y - (b.y * 16 + 8.0f)) < 11) continue;
        const char* l = b.state == BS_OPEN ? "Close & Lock" : (b.state == BS_LOCKED ? (b.kind == BK_CELLDOOR ? "Unlock Cell" : "Unlock") : "Open");
        if (b.kind == BK_GATE) l = b.state == BS_OPEN ? "Close Gate" : "Open Gate";
        consider(IK_DOOR, i, b.x * 16 + 8.0f, b.y * 16 + 8.0f, 20, l);
    }
    // a trapped survivor in reach always wins over furniture and doors beside them (it's the objective)
    for (int i = 0; i < G.nu; i++) if (G.u[i].escort && G.u[i].alive && G.u[i].order == ORD_HOLD && len2(G.u[i].x - u.x, G.u[i].y - u.y) <= 26) {
        best.kind = IK_RESCUE; best.idx = i; best.label = "Rescue"; best.x = G.u[i].x; best.y = G.u[i].y; bd = 0;
    }
    if (G.scene == SC_SETTLEMENT && G.st.visitorPresent) consider(IK_VISITOR, 0, G.map.exitX * 16 + 8.0f, (G.map.exitY + 1) * 16 + 8.0f, 40, "Talk to stranger");
    int tx = (int)(u.x / 16), ty = (int)(u.y / 16);
    if (G.map.in(tx, ty) && G.map.t[G.map.idx(tx, ty)] == T_EXTRACT) { best.kind = IK_EXTRACT; best.label = "Extract"; best.x = u.x; best.y = u.y; }
    return best;
}
const char* g_prompt = nullptr;
float g_promptX, g_promptY;
int g_highlightProp = -1;

void expedition_finish(bool failed);
static void do_interact(Unit& u, const Interact& it) {
    Survivor& s = G.st.surv[u.sv];
    switch (it.kind) {
    case IK_SEARCH:
        u.searchProp = (int16_t)it.idx; u.searchT = 1.4f; u.anim.play(AnimState::Search, true);
        audio_playAt(SFX_SEARCH, u.x, u.y);
        break;
    case IK_OPEN_CONT: ui_container(it.idx, -1); break;
    case IK_BAG: ui_container(-1, it.idx); break;
    case IK_DOOR: {
        Barrier& b = G.map.bar[it.idx];
        // double doors swing together: the leaf beside this one (same kind, same state) follows it
        auto twin = [&](uint8_t from, uint8_t to) {
            static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& d : D) {
                int nx = b.x + d[0], ny = b.y + d[1]; if (!G.map.in(nx, ny)) continue;
                int bi = G.map.barAt[G.map.idx(nx, ny)]; if (bi < 0) continue;
                Barrier& o = G.map.bar[bi];
                if (o.kind != b.kind || o.state != from) continue;
                if (to == BS_CLOSED) {   // same rule as below: never shut it on someone
                    float cx = o.x * 16 + 8.0f, cy = o.y * 16 + 8.0f; bool occ = false;
                    for (int i = 0; i < G.nu; i++) if (G.u[i].alive && fabsf(G.u[i].x - cx) < 11 && fabsf(G.u[i].y - cy) < 11) occ = true;
                    for (int i = 0; i < G.nz; i++) if (G.z[i].state != ZS_DEAD && fabsf(G.z[i].x - cx) < 11 && fabsf(G.z[i].y - cy) < 11) occ = true;
                    if (occ) continue;
                }
                o.state = to;
            }
        };
        if (b.state == BS_LOCKED) {
            int key = b.kind == BK_CELLDOOR ? IT_CELLKEY : IT_KEYCARD;
            if (s.pack.countOf(key) > 0) { b.state = BS_OPEN; game_toast("Unlocked with the %s.", idef(key).name); audio_playAt(SFX_DOOR, u.x, u.y); }
            else { game_toast("Locked. Requires the %s.", idef(key).name); audio_play(SFX_EMPTY); }
        } else if (b.state == BS_OPEN) {
            // never shut a door on someone standing in it
            bool occ = false; float cx = b.x * 16 + 8.0f, cy = b.y * 16 + 8.0f;
            for (int i = 0; i < G.nu; i++) if (G.u[i].alive && fabsf(G.u[i].x - cx) < 11 && fabsf(G.u[i].y - cy) < 11) occ = true;
            for (int i = 0; i < G.nz; i++) if (G.z[i].state != ZS_DEAD && fabsf(G.z[i].x - cx) < 11 && fabsf(G.z[i].y - cy) < 11) occ = true;
            if (occ) { game_toast("Something is in the way."); break; }
            b.state = BS_CLOSED; twin(BS_OPEN, BS_CLOSED); audio_playAt(SFX_DOOR, u.x, u.y, 1, 0.9f);
        } else { b.state = BS_OPEN; twin(BS_CLOSED, BS_OPEN); audio_playAt(SFX_DOOR, u.x, u.y); }
        map_refresh(G.map);
        sync_loc_state();
        break;
    }
    case IK_RESCUE: {
        Unit& e = G.u[it.idx]; e.order = ORD_FOLLOW;
        game_toast("Survivor rescued! Escort them to the extraction point.");
        audio_play(SFX_MENU_OK);
        break;
    }
    case IK_EXTRACT: expedition_finish(false); break;
    case IK_PLOT: g_buildingSel = G.map.prop[it.idx].special; ui_open(PN_BUILDING); break;
    case IK_RADIO: ui_open(PN_WORLDMAP); break;
    case IK_WORKBENCH: g_buildingSel = B_WORKSHOP; ui_open(PN_BUILDING); break;
    case IK_SLEEP: g_buildingSel = B_QUARTERS; ui_open(PN_BUILDING); break;
    case IK_VISITOR: ui_open(PN_RECRUIT); break;
    }
}

static void finish_search(Unit& u) {
    int pi = u.searchProp; u.searchProp = -1; u.searchT = 0;
    if (pi < 0 || G.curLoc < 0) return;
    const Prop& p = G.map.prop[pi];
    LocState& L = G.loc[G.curLoc];
    if (!L.generated[p.container]) {
        loot_generate(G.curLoc, p.container, p.type, p.special, L.cont[p.container]);
        L.generated[p.container] = true;
    }
    u.anim.play(AnimState::Idle, true);
    if (u.sv == G.u[G.ctrl].sv) ui_container(pi, -1);
}

// ---------------------------------------------------------------- death / extraction
static void drop_bag(float x, float y, Survivor& s) {
    if (G.curLoc < 0 || s.pack.n == 0) return;
    LocState& L = G.loc[G.curLoc];
    int b = 0; while (b < MAX_BAGS && L.bags[b].used) b++;
    if (b == MAX_BAGS) b = 0;
    Bag& bag = L.bags[b]; bag.used = true; bag.x = x; bag.y = y; bag.c.n = 0; bag.c.cap = Container::MAX;
    for (int i = 0; i < s.pack.n; i++) bag.c.push(s.pack.it[i]);
    s.pack.n = 0; s.equip = -1;
}

static float g_deathTimer = 0;
static void unit_die(int ui, const char* cause) {
    Unit& u = G.u[ui];
    if (!u.alive) return;
    u.alive = false;
    u.anim.play(AnimState::Death, true);
    spawn_particles(PK_BLOOD, u.x, u.y - 4, 16, pal::blood);
    blood_pool(u.x, u.y + 4);
    audio_play(SFX_DEATH);
    if (u.escort) { game_toast("The rescued survivor was killed."); return; }
    Survivor& s = G.st.surv[u.sv];
    drop_bag(u.x, u.y + 6, s);
    survivor_kill(u.sv, cause);
    if (ui == G.ctrl) g_deathTimer = 2.0f;
    save_write(G.saveSlot);   // permanent death is saved immediately
}

void expedition_finish(bool failed) {
    Summary& sm = G.sum; memset(&sm, 0, sizeof sm); sm.failed = failed; sm.rescued = -1;
    auto line = [&](const char* fmt, ...) { if (sm.nlines >= 10) return; va_list a; va_start(a, fmt); vsnprintf(sm.lines[sm.nlines++], 48, fmt, a); va_end(a); };
    line(failed ? "EXPEDITION FAILED - %s" : "Extracted from %s", LOCATIONS[G.curLoc].name);
    for (int k = 0; k < G.nu; k++) {
        Unit& u = G.u[k];
        if (u.escort) {
            if (u.alive && u.order == ORD_FOLLOW && !failed) {
                Rng r(G.rng.next());
                G.st.visitor = make_survivor(r, false); G.st.visitor.hp = 55;
                snprintf(G.st.visitor.name, sizeof G.st.visitor.name, "%s", G.st.visitor.name);
                sm.rescued = 1; G.loc[G.curLoc].rescued = true;
                line("Rescued %s (wants to join)", G.st.visitor.name);
            }
            continue;
        }
        Survivor& s = G.st.surv[u.sv];
        s.onExpedition = false;
        if (!s.alive) { line("%s died (%s)", s.name, "permanent"); continue; }
        // deliver food/materials/fuel and install equipment automatically
        for (int i = s.pack.n - 1; i >= 0; i--) {
            const Item& it = s.pack.it[i]; const ItemDef& d = idef(it.id);
            if (d.cat == IC_FOOD || d.cat == IC_MATERIAL || d.cat == IC_FUEL || d.cat == IC_EQUIPMENT) {
                int before[RES_COUNT]; memcpy(before, G.st.res, sizeof before);
                deliver_item(it);
                for (int r = 0; r < RES_COUNT; r++) sm.resGained[r] += G.st.res[r] - before[r];
                if (d.cat == IC_EQUIPMENT) line("Installed: %s", d.name);
                bool eqAfter = s.equip > i;
                s.pack.remove(i);
                if (s.equip == i) s.equip = -1; else if (eqAfter) s.equip--;
            }
        }
        if (G.st.surv[u.sv].pack.countOf(IT_KEYCARD) || G.st.surv[u.sv].pack.countOf(IT_CELLKEY)) {}  // keys kept for next visit
        line("%s returned (HP %d%s)", s.name, (int)s.hp, s.infection ? ", INFECTED" : "");
        skill_xp(s, SK_SHOOT, 1);
    }
    for (int r = 0; r < RES_COUNT; r++) if (sm.resGained[r]) line("+%d %s", sm.resGained[r], RES_NAMES[r]);
    LocState& L = G.loc[G.curLoc];
    if (L.cooldown < 0) L.cooldown = 15 * 60;   // refresh timer starts on first departure
    level_leave();
    G.expLoc = -1;
    for (int k = 0; k < 3; k++) G.squad[k] = -1;
    level_enter(MAP_SETTLEMENT, -1);
    ui_open(PN_SUMMARY);
    save_write(G.saveSlot);
}

// ---------------------------------------------------------------- per-tick updates
static void unit_face(Unit& u, float fx, float fy) {
    float l = len2(fx, fy); if (l < 0.01f) return;
    u.fx = fx / l; u.fy = fy / l; u.dir = (uint8_t)dir8(u.fx, u.fy);
}

static Input g_in;
static uint32_t g_down, g_up;

static void update_player(Unit& u, float dt) {
    Survivor& s = G.st.surv[u.sv];
    float ax = g_in.ax, ay = g_in.ay, mag = len2(ax, ay);
    bool moving = mag > 0.25f;
    if (moving && !(g_in.held & BTN_L)) unit_face(u, ax, ay);   // facing follows the circle pad immediately
    // B: tap = dodge, hold = sprint
    if (g_in.held & BTN_B) u.bHeld += dt;
    bool dodge = false;
    if ((g_in.up & BTN_B) && u.bHeld > 0 && u.bHeld < 0.2f) dodge = true;
    if (!(g_in.held & BTN_B)) { u.bHeld = 0; u.winded = false; }
    if (g_in.up & BTN_B) u.bHeld = 0;
    if (dodge && u.dodgeT <= 0 && u.stam >= 20) {
        float dx = moving ? ax / mag : u.fx, dy = moving ? ay / mag : u.fy;
        u.dodgeT = 0.24f; u.dvx = dx * 190; u.dvy = dy * 190; u.stam -= 20;
        u.reloadT = 0;   // dodging interrupts a reload (no ammo lost: rounds load at the end)
        u.searchT = 0; u.searchProp = -1;
        u.anim.play(AnimState::Dodge, true); G.statDodges++;
        audio_playAt(SFX_STEP, u.x, u.y, 1, 0.7f);
    }
    if (u.sprinting && u.stam <= 1) u.winded = true;   // out of breath: no flicker back into a sprint as stamina trickles in
    u.sprinting = moving && (g_in.held & BTN_B) && u.bHeld >= 0.2f && u.stam > 1 && u.dodgeT <= 0 && !u.winded;
    float speed = u.sprinting ? 86.0f : 54.0f;
    if (s.hp < 35) speed *= 0.8f;
    if (u.reloadT > 0) speed *= 0.75f;
    if (u.dodgeT > 0) map_move(G.map, u.x, u.y, u.dvx * dt, u.dvy * dt, 5);
    else if (moving) {
        float m = mag > 1 ? 1 : mag;
        map_move(G.map, u.x, u.y, ax / mag * speed * m * dt, ay / mag * speed * m * dt, 5);
        if (u.searchT > 0) { u.searchT = 0; u.searchProp = -1; float_text(u.x, u.y - 18, "Stopped", pal::dim); }
    }
    if (u.sprinting) { u.stam -= 22 * dt; G.statSprint += dt; }
    // fire
    Item* w = equipped(s);
    bool automatic = w && idef(w->id).cat == IC_FIREARM && idef(w->id).wpn.automatic;
    if ((g_down & BTN_Y) || (automatic && (g_in.held & BTN_Y))) fire(G.ctrl);
    if (g_down & BTN_X) do_reload(u, s);
    if (g_down & BTN_R) { G.flashlight = !G.flashlight; audio_play(SFX_MENU_MOVE, 0.8f, 0.6f); }
    if (g_down & (BTN_LEFT | BTN_RIGHT)) {
        // cycle through weapons in the pack
        int n = s.pack.n, start = s.equip < 0 ? 0 : s.equip;
        for (int k = 1; k <= n; k++) {
            int i = ((start + ((g_down & BTN_RIGHT) ? k : -k)) % n + n) % n;
            int c = idef(s.pack.it[i].id).cat;
            if (c == IC_FIREARM || c == IC_MELEE) { s.equip = (int8_t)i; u.reloadT = 0; audio_play(SFX_RELOAD, 0.5f); float_text(u.x, u.y - 18, idef(s.pack.it[i].id).name, pal::cream); break; }
        }
    }
    if (g_down & BTN_UP) {
        // quick heal: smallest item that helps
        int best = -1, bestHeal = 999;
        for (int i = 0; i < s.pack.n; i++) { const ItemDef& d = idef(s.pack.it[i].id); if (d.cat == IC_MEDICAL && d.heal > 0 && d.heal < bestHeal) { bestHeal = d.heal; best = i; } }
        if (best >= 0) survivor_useItem(u.sv, best, G.ctrl); else game_toast("No medical supplies.");
    }
    if (g_down & BTN_DOWN) {
        static const char* ON[] = {"FOLLOW", "HOLD", "RETREAT"};
        int ord = -1;
        for (int i = 0; i < G.nu; i++) if (i != G.ctrl && !G.u[i].escort && G.u[i].alive) { G.u[i].order = (uint8_t)((G.u[i].order + 1) % 3); ord = G.u[i].order; }
        if (ord >= 0) game_toast("Squad order: %s", ON[ord]); else game_toast("No squad to command.");
    }
    if (g_down & BTN_A) {
        Interact it = find_interact(u);
        if (it.kind != IK_NONE && u.searchT <= 0) do_interact(u, it);
    }
}

// can a body (radius ~5) walk this straight line? sight alone isn't enough: furniture and windows don't block sight
static bool walkLine(float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0, l = len2(dx, dy);
    if (l < 1) return true;
    float px = -dy / l * 5, py = dx / l * 5;
    for (float s = 0; s <= l; s += 6) {
        float x = x0 + dx * s / l, y = y0 + dy * s / l;
        if (map_blocked(G.map, (int)(x / 16), (int)(y / 16)) || map_blocked(G.map, (int)((x + px) / 16), (int)((y + py) / 16)) ||
            map_blocked(G.map, (int)((x - px) / 16), (int)((y - py) / 16))) return false;
    }
    return true;
}

static void update_companion(Unit& u, int ui, float dt) {
    Unit& p = G.u[G.ctrl];
    float tx = p.x, ty = p.y, speed = 50;
    if (u.escort && u.order == ORD_HOLD) { if (len2(p.x - u.x, p.y - u.y) < 60) unit_face(u, p.x - u.x, p.y - u.y); return; }
    uint16_t* field = G.flowC;
    if (u.order == ORD_RETREAT) { tx = G.map.exitX * 16 + 8.0f; ty = G.map.exitY * 16 + 8.0f; field = G.flowExit; }
    float d = len2(tx - u.x, ty - u.y);
    bool want = (u.order == ORD_FOLLOW && d > 28) || (u.order == ORD_RETREAT && d > 12);
    if (want && u.dodgeT <= 0) {
        float mx = tx - u.x, my = ty - u.y;
        if (!walkLine(u.x, u.y, tx, ty) || d > 120) {
            int nx, ny;
            if (map_flowStep(G.map, field, (int)(u.x / 16), (int)(u.y / 16), nx, ny)) { mx = nx * 16 + 8 - u.x; my = ny * 16 + 8 - u.y; }
        }
        float l = len2(mx, my) + 0.001f;
        if (d > 90) speed = 80;
        float ox = u.x, oy = u.y;
        map_move(G.map, u.x, u.y, mx / l * speed * dt, my / l * speed * dt, 5);
        if (len2(u.x - ox, u.y - oy) < 0.2f) u.stuckT += dt; else u.stuckT = 0;
        unit_face(u, mx, my);
    }
    // never stuck forever: regroup out of sight
    if (u.order == ORD_FOLLOW && (u.stuckT > 5 || d > 260)) {
        int tx2 = (int)(u.x / 16), ty2 = (int)(u.y / 16);
        bool seen = G.map.in(tx2, ty2) && G.light[G.map.idx(tx2, ty2)] > 60;
        if (!seen || u.stuckT > 8) { u.x = u.px = p.x - p.fx * 14; u.y = u.py = p.y - p.fy * 14; map_move(G.map, u.x, u.y, 0.1f, 0.1f, 5); u.stuckT = 0; }
    }
    if (u.escort || u.sv < 0) return;
    // combat: engage the nearest visible zombie
    Survivor& s = G.st.surv[u.sv];
    int best = -1; float bd = 120;
    for (int i = 0; i < G.nz; i++) if (zombie_visibleTo(u, G.z[i], bd)) { bd = len2(G.z[i].x - u.x, G.z[i].y - u.y); best = i; }
    if (best >= 0) {
        unit_face(u, G.z[best].x - u.x, G.z[best].y - u.y);
        Item* w = equipped(s);
        if (w && idef(w->id).cat == IC_FIREARM && w->mag == 0 && u.reloadT <= 0) {
            if (s.pack.countOf(idef(w->id).wpn.ammo) > 0) do_reload(u, s);
            else { survivor_equipBest(s); }
        }
        w = equipped(s);
        bool melee = !w || idef(w->id).cat == IC_MELEE;
        if ((!melee && bd < idef(w->id).wpn.range * 0.8f) || (melee && bd < 22)) {
            if (u.cd <= 0 && u.reloadT <= 0) { u.cd += 0.25f + G.rng.f() * 0.3f; fire(ui); }
        }
    }
    (void)ui;
}

static void update_unit_common(Unit& u, int ui, float dt) {
    if (u.cd > 0) u.cd -= dt;
    if (u.muzzleT > 0) u.muzzleT -= dt;
    if (u.hurtT > 0) u.hurtT -= dt;
    if (u.healT > 0) u.healT -= dt;
    if (u.dodgeT > 0) u.dodgeT -= dt;
    if (!u.sprinting && u.stam < 100) u.stam = fminf(100, u.stam + 16 * dt);
    if (u.sv >= 0 && u.reloadT > 0) { u.reloadT -= dt; if (u.reloadT <= 0) { u.reloadT = 0; finish_reload(u, G.st.surv[u.sv]); } }
    if (u.searchT > 0) { u.searchT -= dt; if (u.searchT <= 0) finish_search(u); }
    // animation state from gameplay state (never blocks input)
    float moved = len2(u.x - u.px, u.y - u.py);
    if (u.dodgeT <= 0 && u.searchT <= 0 && u.reloadT <= 0 && u.healT <= 0) {
        if (moved > 0.3f) u.anim.play(u.sprinting ? AnimState::Run : AnimState::Walk);
        else if (u.anim.state == AnimState::Walk || u.anim.state == AnimState::Run) u.anim.play(AnimState::Idle);
    }
    uint8_t ev = u.anim.update(dt);
    if (ev & EV_FOOTSTEP) { audio_playAt(SFX_STEP, u.x, u.y, u.sprinting ? 0.6f : 0.35f, 0.9f + G.rng.f() * 0.2f); if (u.sprinting) spawn_particles(PK_DUST, u.x, u.y + 6, 1, rgb(110, 100, 86)); }
    if ((ev & EV_IMPACT) && u.meleePending) { u.meleePending = false; resolve_melee(ui); }
    if (u.anim.state != AnimState::Melee) u.meleePending = false;
}

static void zombie_attack_land(Zombie& z) {
    z.atkPending = false;
    if (z.target < 0 || z.target >= G.nu) return;
    Unit& u = G.u[z.target];
    if (!u.alive || len2(u.x - z.x, u.y - z.y) > 18) { return; }
    if (u.dodgeT > 0) { float_text(u.x, u.y - 18, "DODGED", pal::cream); return; }
    int dmg = 8 + G.rng.range(7);
    u.hurtT = 0.3f; u.anim.play(AnimState::Hurt);
    u.searchT = 0; u.searchProp = -1;
    spawn_particles(PK_BLOOD, u.x, u.y - 6, 6, pal::blood);
    audio_playAt(SFX_BITE, u.x, u.y); audio_playAt(SFX_HURT, u.x, u.y, 0.8f);
    if (G.set.shake && z.target == G.ctrl) G.shake = fmaxf(G.shake, 2.0f);
    float dx = u.x - z.x, dy = u.y - z.y, l = len2(dx, dy) + 0.01f;
    map_move(G.map, u.x, u.y, dx / l * 4, dy / l * 4, 5);
    if (u.escort) { g_escortHp[z.target] -= dmg; if (g_escortHp[z.target] <= 0) unit_die(z.target, "zombie"); return; }
    Survivor& s = G.st.surv[u.sv];
    s.hp -= dmg;
    if (s.infection == INF_NONE && G.rng.chance(0.12f)) {
        s.infection = INF_EARLY; s.infHours = 0; s.infKnown = true;
        float_text(u.x, u.y - 22, "BITTEN!", pal::red);
        game_toast("%s was bitten and is infected!", s.name);
    }
    if (s.hp <= 0) { s.hp = 0; unit_die(z.target, "killed by zombies"); }
}

static int bucketHead[16 * 16], bucketNext[MAX_ZOMB];

static void update_zombie(int zi, float dt) {
    Zombie& z = G.z[zi];
    z.px = z.x; z.py = z.y;
    uint8_t ev = z.anim.update(dt);
    if (z.state == ZS_DEAD) return;
    if (z.flash > 0) z.flash -= dt;
    if (z.slowT > 0) z.slowT -= dt;
    if (z.atkCd > 0) z.atkCd -= dt;
    if ((ev & EV_IMPACT) && z.atkPending) zombie_attack_land(z);
    if (z.anim.state != AnimState::ZAttack) z.atkPending = false;
    if ((ev & EV_GROAN) && G.rng.chance(0.35f)) audio_playAt(SFX_GROAN, z.x, z.y, 0.6f, 0.8f + G.rng.f() * 0.4f);
    if (ev & EV_FOOTSTEP) audio_playAt(SFX_ZSTEP, z.x, z.y, 0.25f);
    // knockback decay
    if (fabsf(z.kx) + fabsf(z.ky) > 0.1f) { map_move(G.map, z.x, z.y, z.kx * 0.4f, z.ky * 0.4f, 5); z.kx *= 0.6f; z.ky *= 0.6f; }
    if (z.anim.busy()) return;   // rising / attacking: no movement
    // perception: nearest living unit in sight
    int seen = -1; float sd = 150;
    for (int i = 0; i < G.nu; i++) {
        const Unit& u = G.u[i]; if (!u.alive) continue;
        float dx = u.x - z.x, dy = u.y - z.y, d = len2(dx, dy);
        if (d > sd) continue;
        bool facing = (dx * z.hx + dy * z.hy) > -0.2f * d;
        if (d > 44 && !facing) continue;
        if (!map_los(G.map, z.x, z.y, u.x, u.y)) continue;
        sd = d; seen = i;
    }
    if (z.state == ZS_LYING) {
        if (seen >= 0 && sd < 44) { z.state = ZS_RISING; z.anim.play(AnimState::ZRise, true); }
        return;
    }
    if (z.state == ZS_RISING) { z.state = ZS_CHASE; z.target = (int8_t)(seen >= 0 ? seen : 0); return; }
    if (seen >= 0) {
        if (z.state != ZS_CHASE) { z.anim.play(AnimState::ZDetect, true); }
        z.state = ZS_CHASE; z.target = (int8_t)seen; z.lostT = 0;
        z.tx = G.u[seen].x; z.ty = G.u[seen].y;
    } else if (z.state == ZS_CHASE) {
        z.lostT += dt;
        if (z.lostT > 4) { z.state = ZS_INVESTIGATE; z.timer = 6; }
    }
    float mx = 0, my = 0, speed = 0;
    switch (z.state) {
    case ZS_IDLE:
        z.timer -= dt;
        if (z.timer <= 0) { z.timer = 2 + G.rng.f() * 4; float a = G.rng.f() * 6.283f; z.hx = cosf(a); z.hy = sinf(a); z.dir = (uint8_t)dir8(z.hx, z.hy); }
        break;
    case ZS_WANDER:
        z.timer -= dt;
        if (z.timer <= 0) { z.timer = 3 + G.rng.f() * 4; z.tx = z.x + G.rng.between(-48, 48); z.ty = z.y + G.rng.between(-48, 48); }
        mx = z.tx - z.x; my = z.ty - z.y; speed = len2(mx, my) > 4 ? 11 : 0;
        break;
    case ZS_INVESTIGATE:
        z.timer -= dt;
        mx = z.tx - z.x; my = z.ty - z.y; speed = len2(mx, my) > 6 ? 20 : 0;
        if (z.timer <= 0) { z.state = ZS_WANDER; z.timer = 0; }
        break;
    case ZS_CHASE: {
        mx = z.tx - z.x; my = z.ty - z.y; speed = 25;
        bool direct = seen >= 0;
        if (!direct) {
            int nx, ny;
            if (map_flowStep(G.map, G.flowZ, (int)(z.x / 16), (int)(z.y / 16), nx, ny)) { mx = nx * 16 + 8 - z.x; my = ny * 16 + 8 - z.y; }
        }
        if (seen >= 0 && sd < 13 && z.atkCd <= 0) {
            z.atkCd = 1.3f; z.atkPending = true; z.anim.play(AnimState::ZAttack, true);
            speed = 0;
        }
        break;
    }
    }
    if (z.slowT > 0) speed *= 0.45f;
    speed *= z.speed;
    float l = len2(mx, my);
    if (speed > 0 && l > 0.5f) {
        z.hx = mx / l; z.hy = my / l; z.dir = (uint8_t)dir8(z.hx, z.hy);
        int bi = map_move(G.map, z.x, z.y, z.hx * speed * dt, z.hy * speed * dt, 5);
        // blocked by a door/window/gate while hunting: batter it
        if (bi >= 0 && (z.state == ZS_CHASE || z.state == ZS_INVESTIGATE)) {
            Barrier& b = G.map.bar[bi];
            if (b.state != BS_OPEN && b.state != BS_BROKEN && z.atkCd <= 0) {
                z.atkCd = 1.3f; z.anim.play(AnimState::ZAttack, true);
                if (b.state != BS_LOCKED) b.hp -= 8;   // locked (cell / keycard) doors rattle but hold: whoever is locked in stays safe until you bring the key
                b.shake = 0.25f;
                audio_playAt(b.kind == BK_WINDOW ? SFX_GLASS : SFX_BANG, b.x * 16 + 8.0f, b.y * 16 + 8.0f, b.kind == BK_WINDOW ? 0.4f : 0.8f);
                spawn_particles(b.kind == BK_WINDOW ? PK_GLASS : PK_SPLINTER, b.x * 16 + 8.0f, b.y * 16 + 12.0f, 3, b.kind == BK_WINDOW ? rgb(150, 190, 200) : rgb(112, 80, 50));
                if (b.hp <= 0) {
                    b.hp = 0; b.state = BS_BROKEN; map_refresh(G.map); sync_loc_state();
                    audio_playAt(b.kind == BK_WINDOW ? SFX_GLASS : SFX_BANG, b.x * 16 + 8.0f, b.y * 16 + 8.0f, 1.0f, 0.8f);
                    spawn_particles(b.kind == BK_WINDOW ? PK_GLASS : PK_SPLINTER, b.x * 16 + 8.0f, b.y * 16 + 8.0f, 10, b.kind == BK_WINDOW ? rgb(150, 190, 200) : rgb(112, 80, 50));
                }
            }
        }
        if (!z.anim.busy()) z.anim.play(AnimState::ZWalk);
    } else if (!z.anim.busy() && z.anim.state != AnimState::ZDetect) z.anim.play(AnimState::ZIdle);
}

static void separate_zombies() {
    // spatial hash (64px buckets) so separation stays cheap with many zombies
    for (int i = 0; i < 256; i++) bucketHead[i] = -1;
    for (int i = 0; i < G.nz; i++) {
        if (G.z[i].state == ZS_DEAD) continue;
        int b = ((int)(G.z[i].x / 64) & 15) + ((int)(G.z[i].y / 64) & 15) * 16;
        bucketNext[i] = bucketHead[b]; bucketHead[b] = i;
    }
    for (int i = 0; i < G.nz; i++) {
        Zombie& a = G.z[i]; if (a.state == ZS_DEAD) continue;
        int bx = (int)(a.x / 64), by = (int)(a.y / 64);
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) {
            for (int j = bucketHead[((bx + ox) & 15) + ((by + oy) & 15) * 16]; j >= 0; j = bucketNext[j]) {
                if (j <= i) continue;
                Zombie& b = G.z[j];
                float dx = b.x - a.x, dy = b.y - a.y, d = len2(dx, dy);
                if (d < 9 && d > 0.01f) {
                    float push = (9 - d) * 0.5f / d;
                    map_move(G.map, a.x, a.y, -dx * push, -dy * push, 5);
                    map_move(G.map, b.x, b.y, dx * push, dy * push, 5);
                }
            }
        }
    }
}

static void update_projectiles(float dt) {
    for (auto& p : G.proj) {
        if (!p.active) continue;
        float ox = p.x, oy = p.y;
        if (p.target >= 0 && G.z[p.target].state != ZS_DEAD) {
            // homes onto its predetermined target so the visual always matches the roll
            Zombie& z = G.z[p.target];
            float dx = z.x - p.x, dy = (z.y - 5) - p.y, d = len2(dx, dy), sp = len2(p.vx, p.vy);
            if (d < sp * dt + 4) {
                zombie_hit(p.target, p.result, p.dmg, p.x - p.vx * 0.1f, p.y - p.vy * 0.1f, p.owner);
                p.active = false; continue;
            }
            p.vx = dx / d * sp; p.vy = dy / d * sp;
        } else if (p.target >= 0) { p.target = -1; p.untargeted = false; }  // target died first: bullet flies on harmlessly
        p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
        int tx = (int)(p.x / 16), ty = (int)(p.y / 16);
        if (p.life <= 0) { p.active = false; continue; }
        if (map_opaque(G.map, tx, ty) || (G.map.in(tx, ty) && (G.map.flags[G.map.idx(tx, ty)] & F_BLOCK) && G.map.propAt[G.map.idx(tx, ty)] < 0 && G.map.t[G.map.idx(tx, ty)] != T_BARS && G.map.t[G.map.idx(tx, ty)] != T_FENCE)) {
            spawn_particles(PK_SPARK, ox, oy, 3, pal::cream);
            spawn_particles(PK_DUST, ox, oy, 1, rgb(120, 110, 100));
            fx_bulletHit(ox, oy, p.vx, p.vy, false);
            p.active = false; continue;
        }
        if (p.untargeted) {
            for (int i = 0; i < G.nz; i++) {
                Zombie& z = G.z[i]; if (z.state == ZS_DEAD) continue;
                if (fabsf(z.x - p.x) < 6 && fabsf((z.y - 5) - p.y) < 8) {
                    ShotParams sp; sp.dist = 64; sp.skill = p.owner < MAX_SURV ? G.st.surv[p.owner].skill[SK_SHOOT] : 3;
                    HitResult r = hit_rollNoMiss(hit_table(sp), G.rng);
                    zombie_hit(i, r, p.dmg, ox, oy, p.owner);
                    p.active = false; break;
                }
            }
        }
    }
}

static void update_particles(float dt) {
    for (auto& p : G.part) {
        if (p.life <= 0) continue;
        p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.kind == PK_LEAF) { p.vx += sinf(p.life * 3) * 6 * dt; p.z -= 4 * dt; if (p.z < 0) p.z = 0; continue; }
        if (p.kind == PK_SMOKE || p.kind == PK_DUST || p.kind == PK_EMBER) { p.z += p.vz * dt; continue; }
        p.vz -= 200 * dt; p.z += p.vz * dt;
        if (p.z < 0) { p.z = 0; p.vz *= -0.3f; p.vx *= 0.5f; p.vy *= 0.5f;
            if (p.kind == PK_BLOOD && p.life > 0.1f && G.rng.chance(0.08f)) blood_drop(p.x, p.y); }
    }
    for (auto& f : G.ft) if (f.life > 0) { f.life -= dt; f.y -= 18 * dt; }
}

static void update_settlers(float dt) {
    float day = daylight(); int hour = ((int)G.clock / 60) % 24;
    bool night = hour >= 21 || hour < 6;
    for (int i = 0; i < G.st.nsurv; i++) {
        Survivor& s = G.st.surv[i]; Settler& n = g_npc[i];
        n.px = n.x; n.py = n.y;
        if (!s.alive || s.onExpedition || i == G.u[G.ctrl].sv) continue;
        // job anchor: where this survivor works / sleeps
        float ax = G.map.spawnX * 16.0f, ay = G.map.spawnY * 16.0f;
        int plot = -1; float fireX = -1, fireY = 0;
        switch (s.job) { case JOB_MEDIC: plot = B_INFIRMARY; break; case JOB_MECHANIC: plot = B_WORKSHOP; break; case JOB_FARMER: plot = B_KITCHEN; break; case JOB_WORKER: plot = B_STORAGE; break; case JOB_GUARD: plot = B_DEFENSES; break; }
        int hourNow = ((int)G.clock / 60) % 24;
        bool sleeping = (hourNow >= 22 || hourNow < 6) && s.job != JOB_GUARD;
        bool gather = hourNow >= 17 && hourNow < 22 && s.job != JOB_GUARD && !s.quarantined;   // evenings: everyone off duty gathers at the fire (Try 2)
        (void)night;
        if (s.quarantined) plot = B_INFIRMARY;
        if (sleeping) plot = -2;
        if (gather) plot = -3;
        for (int k = 0; k < G.map.nprop; k++) {
            const Prop& p = G.map.prop[k];
            if (plot >= 0 && p.type == PR_PLOT && p.special == plot) { ax = p.tx * 16 + 24.0f; ay = p.ty * 16 + 56.0f; }
            if (plot == -2 && p.type == PR_FIRE) { ax = p.tx * 16 + 8.0f + ((i % 5) - 2) * 18.0f; ay = p.ty * 16 + 30.0f + (i / 5) * 14.0f; }
            if (plot == -3 && p.type == PR_FIRE) { float a = i * 1.26f + 0.6f; ax = p.tx * 16 + 8.0f + cosf(a) * 34; ay = p.ty * 16 + 10.0f + sinf(a) * 20; fireX = p.tx * 16 + 8.0f; fireY = p.ty * 16 + 10.0f; }
        }
        if (s.job == JOB_GUARD && !sleeping) { ax = G.map.exitX * 16 + 8.0f + ((i & 1) ? -40.0f : 40.0f); ay = G.map.exitY * 16 - 20.0f - (i % 3) * 10.0f; }
        n.asleep = sleeping && len2(n.x - ax, n.y - ay) < 6;
        n.wait -= dt;
        if (n.wait <= 0) {
            n.wait = 2.5f + (hash32(i * 17 + G.tick) % 40) / 10.0f;
            float r = (sleeping || gather) ? 3.0f : (s.job == JOB_GUARD ? 26.0f : 18.0f);
            n.tx = ax + ((int)(hash32(i + G.tick) % 100) - 50) / 50.0f * r;
            n.ty = ay + ((int)(hash32(i * 3 + G.tick) % 100) - 50) / 50.0f * r * 0.5f;
        }
        float dx = n.tx - n.x, dy = n.ty - n.y, d = len2(dx, dy);
        if (d > 2) {
            float sp = 26 * (0.6f + day * 0.4f);
            float ox = n.x, oy = n.y;
            map_move(G.map, n.x, n.y, dx / d * sp * dt, dy / d * sp * dt, 5);
            if (fabsf(n.x - ox) + fabsf(n.y - oy) < sp * dt * 0.3f) {   // blocked: slide around the obstacle, then give up and step through
                n.stuck += dt;
                float side = (hash32(i * 7) & 1) ? 1.0f : -1.0f;
                map_move(G.map, n.x, n.y, -dy / d * sp * dt * side, dx / d * sp * dt * side, 5);
                if (n.stuck > 4) { n.x = n.tx; n.y = n.ty; n.stuck = 0; }
            } else n.stuck = 0;
            n.dir = (uint8_t)dir8(dx, dy); n.anim.play(AnimState::Walk); n.working = false;
        } else if (n.asleep) n.anim.play(AnimState::Sleep);
        else if (gather && fireX >= 0) { n.dir = (uint8_t)dir8(fireX - n.x, fireY - n.y); n.anim.play(AnimState::Idle); n.working = false; }   // warming up, facing the fire
        else if (s.job == JOB_GUARD) { n.anim.play(AnimState::Guard); if (n.anim.state == AnimState::Guard && n.anim.frame == 1) n.dir = (uint8_t)((n.dir + 1) & 7); }
        else if (s.job != JOB_NONE) { n.anim.play(AnimState::Work); n.working = true; n.dir = 6; }
        else n.anim.play(AnimState::Idle);
        n.anim.update(dt);
    }
}

static void update_level(float dt) {
    // flow fields every half second: zombies (through barriers), companions, extraction
    G.flowT -= dt;
    Unit& p = G.u[G.ctrl];
    if (G.flowT <= 0) {
        G.flowT = 0.5f;
        map_flow(G.map, (int)(p.x / 16), (int)(p.y / 16), G.flowZ, true);
        map_flow(G.map, (int)(p.x / 16), (int)(p.y / 16), G.flowC, false);
        map_flow(G.map, G.map.exitX, G.map.exitY, G.flowExit, false);
    }
    for (int i = 0; i < G.nu; i++) { G.u[i].px = G.u[i].x; G.u[i].py = G.u[i].y; }
    if (p.alive && g_deathTimer <= 0) update_player(p, dt);
    for (int i = 0; i < G.nu; i++) {
        Unit& u = G.u[i];
        if (!u.alive) { u.anim.update(dt); continue; }
        if (i != G.ctrl) update_companion(u, i, dt);
        update_unit_common(u, i, dt);
    }
    // throttled AI far from the player keeps the 3DS budget predictable
    for (int i = 0; i < G.nz; i++) {
        Zombie& z = G.z[i];
        bool far = len2(z.x - p.x, z.y - p.y) > 300;
        if (far && ((G.tick + i) & 3)) { z.px = z.x; z.py = z.y; continue; }
        update_zombie(i, far ? dt * 4 : dt);
    }
    separate_zombies();
    update_projectiles(dt);
    update_particles(dt);
    fx_update(dt);
    for (int i = 0; i < G.map.nbar; i++) {
        Barrier& b = G.map.bar[i];
        float target = (b.state == BS_OPEN || b.state == BS_BROKEN) ? 1.0f : 0.0f;
        b.anim += (target - b.anim) * fminf(1, dt * 10);
        if (b.shake > 0) b.shake -= dt;
    }
    if (G.scene == SC_SETTLEMENT) update_settlers(dt);
    // ambient environment particles near the camera
    if (G.rng.chance(0.02f) && daylight() > 0.3f) spawn_particles(PK_LEAF, G.camX + G.rng.range(view_w()), G.camY + G.rng.range(view_h() / 2), 1, rgb(120, 110, 50));
    for (int i = 0; i < G.map.nprop; i++) if (G.map.prop[i].type == PR_FIRE && G.rng.chance(0.25f)) {
        const Prop& f = G.map.prop[i];
        spawn_particles(G.rng.chance(0.5f) ? PK_EMBER : PK_SMOKE, f.tx * 16 + 8.0f, f.ty * 16 + 6.0f, 1, G.rng.chance(0.5f) ? rgb(250, 170, 60) : rgb(90, 90, 96));
    }
    // death handling: pass control or end the expedition
    if (g_deathTimer > 0) {
        g_deathTimer -= dt;
        if (g_deathTimer <= 0) {
            int next = -1;
            for (int i = 0; i < G.nu; i++) if (G.u[i].alive && !G.u[i].escort && G.u[i].sv >= 0) { next = i; break; }
            if (next >= 0 && G.scene == SC_EXPEDITION) { G.ctrl = next; G.u[next].order = ORD_FOLLOW; game_toast("You now control %s.", G.st.surv[G.u[next].sv].name); }
            else if (G.scene == SC_EXPEDITION) expedition_finish(true);
            else ui_open(PN_DEATH);
        }
    }
    // camera: follow with a small look-ahead in the facing direction
    float tx = p.x + p.fx * 24 - view_w() / 2, ty = p.y + p.fy * 18 - view_h() / 2;
    G.camX += (tx - G.camX) * fminf(1, dt * 6); G.camY += (ty - G.camY) * fminf(1, dt * 6);
    G.camX = clampf(G.camX, 0, (float)(G.map.w * 16 - view_w())); G.camY = clampf(G.camY, 0, (float)(G.map.h * 16 - view_h()));
    if (G.map.w * 16 < view_w()) G.camX = (G.map.w * 16 - view_w()) / 2.0f;
    if (G.scene == SC_SETTLEMENT) {   // frame the compound fence to fence (Try 2 overview), towers poking over the top fence
        int y0 = 1 << 30, y1 = -1;
        for (int i = 0; i < G.map.w * G.map.h; i++) if (G.map.t[i] == T_PALISADE) { int y = i / G.map.w; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        if (y1 >= 0) {
            float top = y0 * 16.0f - 20, bot = (y1 + 1) * 16.0f + 4;
            if (bot - top <= view_h()) G.camY = (top + bot - view_h()) / 2.0f;
            else G.camY = clampf(G.camY, top, bot - view_h());
        }
    }
    if (G.shake > 0) G.shake -= dt * 12;
    if (G.toastT > 0) G.toastT -= dt;
    float fa = G.flashlight ? 1.0f : 0.0f; G.flashAnim += (fa - G.flashAnim) * fminf(1, dt * 8);
    audio_listener(p.x, p.y);
    // prompt for the best interaction
    Interact it = find_interact(p);
    g_prompt = it.kind != IK_NONE && p.searchT <= 0 ? it.label : nullptr; g_promptX = it.x; g_promptY = it.y;
    g_highlightProp = (it.kind == IK_SEARCH || it.kind == IK_OPEN_CONT) ? it.idx : -1;
}

static void sim_tick() {
    G.tick++;
    if (G.scene == SC_TITLE || !G.inLevel) return;
    if (game_paused()) return;   // inventory/menus freeze the world and the clock
    G.activeTime += DT;
    float minutesPerSec = 1440.0f / (G.set.dayMinutes * 60.0f);
    G.clock += DT * minutesPerSec;
    G.hourAcc += DT * minutesPerSec;
    while (G.hourAcc >= 60) { G.hourAcc -= 60; settle_hour(); }
    if (G.clock >= 1440) { G.clock -= 1440; }
    loc_tick(DT);
    update_level(DT);
}

void game_advanceHours(float hours) {
    for (int h = 0; h < (int)hours; h++) {
        G.clock += 60; if (G.clock >= 1440) G.clock -= 1440;
        settle_hour();
        loc_tick(0);  // offline time does not refresh loot (active play only)
    }
}

void game_init() {
    art_init();
    worldart_init();
    propart_init();
    itemart_init();
    audio_init();
    G.scene = SC_TITLE; G.panel = PN_MAIN;
    for (auto& L : G.loc) { memset(&L, 0, sizeof L); L.cooldown = -1; }
}

void game_newGame() {
    static Game fresh;
    Settings keep = G.set; int slot = G.saveSlot;
    G = fresh; G.set = keep; G.saveSlot = slot;
    for (auto& L : G.loc) { memset(&L, 0, sizeof L); L.cooldown = -1; for (auto& c : L.cont) c.cap = 8; }
    G.clock = 8 * 60; G.day = 1; G.activeTime = 0; G.hourAcc = 0; G.expLoc = -1;
    G.rng = Rng(0xC0FFEE);
    settle_newGame();
    level_enter(MAP_SETTLEMENT, -1);
    ui_open(PN_HUD);
    journal(JK_INFO, "Day 1. Five of us made it here. We hold this ground.");
}

void game_frame(const Input& in, float realDt) {
    static float acc = 0;
    if (realDt > 0.25f) realDt = 0.25f;
    G.uiTime += realDt; G.panelT += realDt; G.tabT += realDt;
    audio_volumes(G.set.master, G.set.sfx, G.set.music);
    audio_pause(game_paused());
    ui_update(in);
    g_in = in;
    g_down |= in.down; g_up |= in.up;
    Panel before = G.panel;
    acc += realDt;
    while (acc >= DT) {
        g_in.up = g_up;
        sim_tick();
        acc -= DT;
        g_down = 0; g_up = 0;
    }
    g_renderAlpha = acc / DT;
    tut_update();
    if (G.panel != before) ui_dropInput();
    G.frameMs = realDt * 1000;
    bool indoor = false;
    if (G.inLevel) { int tx = (int)(G.u[G.ctrl].x / 16), ty = (int)(G.u[G.ctrl].y / 16); indoor = G.map.in(tx, ty) && (G.map.flags[G.map.idx(tx, ty)] & F_INDOOR); }
    audio_ambience(is_night(), indoor, G.scene == SC_SETTLEMENT, G.scene == SC_TITLE);
}


// An infected survivor died and rose again.
void spawn_turned(int sv) {
    Survivor& s = G.st.surv[sv];
    float x = -1, y = -1;
    if (G.inLevel) {
        for (int i = 0; i < G.nu; i++) if (G.u[i].sv == sv && !G.u[i].escort && G.u[i].alive) {
            x = G.u[i].x; y = G.u[i].y; G.u[i].alive = false; G.u[i].anim.play(AnimState::Death, true);
            if (i == G.ctrl) g_deathTimer = 2.0f;
        }
        if (x < 0 && G.scene == SC_SETTLEMENT && !s.onExpedition) { x = g_npc[sv].x; y = g_npc[sv].y; }
    }
    if (x >= 0 && G.nz < MAX_ZOMB) {
        Zombie& z = G.z[G.nz++];
        memset(&z, 0, sizeof z);
        z.x = z.px = x; z.y = z.py = y; z.hp = 100; z.spawnIdx = -1; z.variant = (uint8_t)(ZL_SURVIVOR + s.variant % SURV_VARIANTS);   // still in their own clothes
        z.speed = 1.0f; z.state = ZS_CHASE; z.target = (int8_t)G.ctrl; z.hx = 0; z.hy = 1;
        z.anim = Animator(); z.anim.play(AnimState::ZRise, true);
        journal(JK_DEATH, "%s turned into a zombie!", s.name);
        game_toast("%s has turned!", s.name);
        audio_play(SFX_ALARM, 0.8f);
        return;
    }
    // resolved off-screen: guards deal with it, but someone may be bitten
    journal(JK_DEATH, "%s turned while out of sight.", s.name);
    if (settle_security() < 45) {
        for (int i = 0; i < G.st.nsurv; i++) {
            Survivor& o = G.st.surv[i];
            if (i != sv && o.alive && !o.onExpedition && !o.infection && G.rng.chance(0.5f)) { o.infection = INF_EARLY; o.infKnown = false; o.infHours = 0; break; }
        }
    }
}

void dbg_zombieHit(int zi, int result, int dmg) { zombie_hit(zi, result, dmg, G.z[zi].x - 10, G.z[zi].y, -1); }
void dbg_fire(int ui) { fire(ui); }
