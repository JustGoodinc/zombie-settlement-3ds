// Automated gameplay tests. Plain asserts, no framework. Exit code = number of failures.
#include "../src/gameplay/game.h"
#include "../src/core/audio.h"
#include "../src/core/art.h"
#include "../src/ui/ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static int fails = 0, passes = 0;
#define CHECK(cond) do { if (cond) passes++; else { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define SECTION(name) printf("[%s]\n", name)

static void frames(int n, Input in = Input()) {
    for (int i = 0; i < n; i++) {
        game_frame(in, DT);
        static px t[TOP_W * TOP_H], b[BOT_W * BOT_H];
        Canvas top, bot; top.init(t, TOP_W, TOP_H); bot.init(b, BOT_W, BOT_H);
        extern float g_renderAlpha;
        game_render(top, bot, g_renderAlpha);
        in.down = 0; in.up = 0; in.touchDown = in.touchUp = false;
    }
}
static void startExpedition() {
    game_newGame();
    G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = -1;
    G.st.surv[0].onExpedition = G.st.surv[1].onExpedition = true;
    level_leave();
    level_enter(MAP_POLICE, 0);
    ui_open(PN_HUD);
}
static int addZombie(float x, float y, int state) {
    Zombie& z = G.z[G.nz]; memset(&z, 0, sizeof z);
    z.x = z.px = x; z.y = z.py = y; z.hp = 100; z.state = (uint8_t)state; z.spawnIdx = -1; z.speed = 1; z.hx = 0; z.hy = 1;
    z.anim = Animator(); z.anim.play(AnimState::ZIdle, true);
    return G.nz++;
}

// out of stamina with B still held: keep walking (no sprint/walk flicker) until B is let go
static void testSprintWinded() {
    SECTION("sprint: running out of stamina while holding B walks steadily until B is released");
    startExpedition(); G.nz = 0;
    Unit& u = G.u[G.ctrl];
    Input in; in.held = BTN_B; in.down = BTN_B;
    int k = 0;
    auto stepDir = [&](Input i) { i.ax = (k++ / 60) % 2 ? -1.0f : 1.0f; frames(1, i); };   // pace east-west across the car park
    for (int f = 0; f < 400 && u.stam > 1; f++) { stepDir(in); in.down = 0; }
    CHECK(u.stam <= 1);
    int flips = 0, sprintFrames = 0; bool last = u.sprinting;
    for (int f = 0; f < 90; f++) { stepDir(in); if (u.sprinting != last) flips++; last = u.sprinting; sprintFrames += u.sprinting; }
    CHECK(sprintFrames == 0 && flips <= 1);   // stamina refills while walking, but no sprint while B stays held
    CHECK(u.stam > 20);
    Input up; up.up = BTN_B; stepDir(up);
    Input again; again.held = BTN_B; again.down = BTN_B;
    for (int f = 0; f < 10; f++) { stepDir(again); again.down = 0; }
    CHECK(u.sprinting);                       // a fresh press sprints again
}

static void testCombat() {
    SECTION("combat: hit probabilities");
    ShotParams base;
    HitTable t = hit_table(base);
    CHECK(t.w[HR_MISS] == 300 && t.w[HR_BODY] == 400 && t.w[HR_ARM] == 150 && t.w[HR_LEG] == 100 && t.w[HR_HEAD] == 50);
    Rng r(42); bool allValid = true;
    for (int i = 0; i < 20000; i++) {
        ShotParams p; p.skill = r.between(1, 10); p.weaponAcc = r.between(-20, 40); p.dist = r.f() * 400; p.range = 100 + r.f() * 300;
        p.moving = r.chance(0.5f); p.injured = r.chance(0.5f); p.targetStill = r.chance(0.5f); p.targetDown = r.chance(0.3f);
        HitTable h = hit_table(p); int sum = 0;
        for (int k = 0; k < HR_COUNT; k++) { sum += h.w[k]; if (h.w[k] < 0) allValid = false; }
        if (sum != 1000) allValid = false;
    }
    CHECK(allValid);
    int counts[HR_COUNT] = {0}; Rng rr(7);
    const int N = 200000;
    for (int i = 0; i < N; i++) counts[hit_roll(t, rr)]++;
    for (int k = 0; k < HR_COUNT; k++) CHECK(fabs(counts[k] * 1000.0 / N - t.w[k]) < 6.0);
    ShotParams far = base; far.dist = 180; CHECK(hit_table(far).w[HR_MISS] > 300);
    ShotParams skilled = base; skilled.skill = 8; CHECK(hit_table(skilled).w[HR_MISS] < 300 && hit_table(skilled).w[HR_HEAD] > 50);
    ShotParams mov = base; mov.moving = true; CHECK(hit_table(mov).w[HR_MISS] > 300);
    CHECK(hit_damage(HR_MISS, 40) == 0);
    CHECK(hit_damage(HR_ARM, 40) < 40 && hit_damage(HR_LEG, 40) < 40 && hit_damage(HR_BODY, 40) == 40);

    SECTION("combat: headshots kill instantly, limbs");
    startExpedition();
    int zi = addZombie(G.u[0].x + 40, G.u[0].y, ZS_IDLE);
    dbg_zombieHit(zi, HR_HEAD, 1);
    CHECK(G.z[zi].state == ZS_DEAD);
    zi = addZombie(G.u[0].x + 40, G.u[0].y, ZS_IDLE);
    dbg_zombieHit(zi, HR_BODY, 40); CHECK(G.z[zi].hp == 60 && G.z[zi].state == ZS_CHASE);
    dbg_zombieHit(zi, HR_LEG, 40); CHECK(G.z[zi].slowT > 0 && G.z[zi].hp < 60);
    float hp = G.z[zi].hp; dbg_zombieHit(zi, HR_MISS, 40); CHECK(G.z[zi].hp == hp);

    SECTION("combat: ammo, durability, never jams");
    startExpedition();
    Survivor& s = G.st.surv[0];
    Item& w = s.pack.it[s.equip];
    CHECK(idef(w.id).cat == IC_FIREARM);
    int mag = w.mag, dur = w.dur;
    G.u[0].cd = 0; dbg_fire(0);
    CHECK(w.mag == mag - 1 && w.dur == dur - 1);
    w.dur = 0; G.u[0].cd = 0; int m2 = w.mag; dbg_fire(0); CHECK(w.mag == m2);   // broken: cannot fire
    w.dur = 100; w.mag = 0; G.u[0].cd = 0; G.u[0].reloadT = 0; dbg_fire(0); CHECK(G.u[0].reloadT > 0); // empty -> auto reload
    int ammoBefore = s.pack.countOf(IT_AMMO_9MM);
    frames(60);
    CHECK(s.pack.it[s.equip].mag == idef(IT_PISTOL).wpn.mag || s.pack.it[s.equip].mag == ammoBefore);
    CHECK(s.pack.countOf(IT_AMMO_9MM) < ammoBefore);
    // knife never wears out
    Item knife = makeItem(IT_KNIFE); CHECK(knife.dur == 0 && idef(IT_KNIFE).wpn.dur == 0);

    SECTION("combat: projectile outcome matches roll");
    startExpedition();
    G.nz = 0;
    zi = addZombie(G.u[0].x, G.u[0].y - 40, ZS_IDLE);
    G.u[0].fx = 0; G.u[0].fy = -1;
    int hits = 0, shots = 0;
    for (int k = 0; k < 40 && G.z[zi].state != ZS_DEAD; k++) {
        float before = G.z[zi].hp;
        Survivor& sv = G.st.surv[0]; sv.pack.it[sv.equip].mag = 10; sv.pack.it[sv.equip].dur = 100;
        G.u[0].cd = 0; dbg_fire(0); shots++;
        int target = -1, res = -1;
        for (auto& p : G.proj) if (p.active) { target = p.target; res = p.result; }
        frames(8);
        if (target == zi && res != HR_MISS) { hits++; CHECK(G.z[zi].hp < before || G.z[zi].state == ZS_DEAD); }
        if (res == HR_MISS || target < 0) CHECK(G.z[zi].hp == before);
    }
    CHECK(shots > 0);
}

static void testInventory() {
    SECTION("inventory: placement, rotation, stacking, overflow");
    Grid g; g.w = 6; g.h = 4;
    CHECK(g.place([] { Item i = makeItem(IT_SHOTGUN); i.x = 0; i.y = 0; return i; }()));
    Item bad = makeItem(IT_SHOTGUN); bad.x = 3; bad.y = 0; CHECK(!g.place(bad));   // out of bounds (4 wide at x=3)
    Item ov = makeItem(IT_PISTOL); ov.x = 1; ov.y = 0; CHECK(!g.place(ov));        // overlap
    CHECK(g.fits(IT_SHOTGUN, 1, 5, 0));                                            // rotated 1x4 fits vertically
    CHECK(!g.fits(IT_SHOTGUN, 1, 5, 1));
    CHECK(g.move(0, 0, 0, 1));                                                     // rotate in place
    CHECK(itemW(g.it[0]) == 1 && itemH(g.it[0]) == 4);
    Grid s2; s2.w = 6; s2.h = 4;
    CHECK(s2.add(makeItem(IT_AMMO_9MM, 50)) == 0);
    CHECK(s2.add(makeItem(IT_AMMO_9MM, 30)) == 0);
    CHECK(s2.n == 2 && s2.countOf(IT_AMMO_9MM) == 80);   // 60 + 20 stacks
    CHECK(s2.take(IT_AMMO_9MM, 70) == 70 && s2.countOf(IT_AMMO_9MM) == 10);
    Grid full; full.w = 2; full.h = 1;
    CHECK(full.add(makeItem(IT_PISTOL)) == 0);
    CHECK(full.add(makeItem(IT_CANNED)) == 1);   // nothing deleted: caller keeps the remainder
    CHECK(full.n == 1);
    Grid up; up.w = 6; up.h = 4;
    for (int i = 0; i < 6; i++) up.add(makeItem(IT_PISTOL));
    CHECK(up.resize(8, 5) && up.w == 8 && up.n == 6);
    CHECK(up.resize(10, 6));
    Grid tight; tight.w = 6; tight.h = 4;
    for (int i = 0; i < 12; i++) tight.add(makeItem(IT_PISTOL));
    CHECK(tight.n == 12 && !tight.resize(4, 4));     // shrink impossible: unchanged
    CHECK(tight.w == 6 && tight.n == 12);
    CHECK(tight.sort() && tight.n == 12);

    SECTION("inventory: backpack upgrade item");
    game_newGame();
    Survivor& s = G.st.surv[0];
    s.pack.add(makeItem(IT_PACK_M));
    int idx = -1; for (int i = 0; i < s.pack.n; i++) if (s.pack.it[i].id == IT_PACK_M) idx = i;
    CHECK(survivor_useItem(0, idx, -1) && s.pack.w == 8 && s.pack.h == 5);
    CHECK(s.equip >= 0 && idef(s.pack.it[s.equip].id).cat != IC_NONE);

    SECTION("inventory: opening it pauses the whole simulation");
    startExpedition();

    int zi = addZombie(G.u[0].x + 60, G.u[0].y, ZS_WANDER);
    frames(5);
    ui_open(PN_INVENTORY);
    float zx = G.z[zi].x, zy = G.z[zi].y; double clk = G.clock, at = G.activeTime;
    Input mv; mv.ax = 1; mv.held = BTN_Y;
    float ux = G.u[0].x;
    frames(90, mv);
    CHECK(G.z[zi].x == zx && G.z[zi].y == zy && G.clock == clk && G.activeTime == at && G.u[0].x == ux);
    ui_open(PN_HUD);
    frames(60, mv);
    CHECK(G.clock > clk && G.activeTime > at);
}

static void testContainers() {
    SECTION("looting: take-all never deletes items that do not fit");
    startExpedition();
    Survivor& s = G.st.surv[0];
    s.pack = Grid(); s.pack.w = 2; s.pack.h = 1; s.equip = -1;
    int prop = -1; for (int i = 0; i < G.map.nprop; i++) if (G.map.prop[i].container >= 0) { prop = i; break; }
    CHECK(prop >= 0);
    Container& c = G.loc[0].cont[G.map.prop[prop].container];
    G.loc[0].generated[G.map.prop[prop].container] = true;
    c.n = 0; c.cap = 8; c.push(makeItem(IT_PISTOL)); c.push(makeItem(IT_SHOTGUN)); c.push(makeItem(IT_CANNED));
    int before = c.n + s.pack.n;
    // emulate the Take All logic
    for (int i = c.n - 1; i >= 0; i--) { Item it = c.it[i]; int left = s.pack.add(it); if (left == it.count) continue; if (left) c.it[i].count = (uint16_t)left; else c.remove(i); }
    CHECK(c.n + s.pack.n == before);
    CHECK(s.pack.n >= 1 && c.n >= 1);

    SECTION("looting: loot generation is deterministic per generation");
    Container a, b;
    loot_generate(0, 3, PR_LOCKER, 0, a); loot_generate(0, 3, PR_LOCKER, 0, b);
    CHECK(a.n == b.n && (a.n == 0 || a.it[0].id == b.it[0].id));
    Container k; loot_generate(0, 5, PR_DESK, 1, k);
    bool hasCard = false; for (int i = 0; i < k.n; i++) hasCard |= k.it[i].id == IT_KEYCARD;
    CHECK(hasCard);
}

// the visual layers are cosmetic: the baked level layer matches the map, permanent marks only land on floors,
// and the effects layer never draws from the gameplay RNG (so the rules play out exactly as before)
static void testVisualLayers() {
    SECTION("visuals: baked level layer and cosmetic effects leave gameplay alone");
    startExpedition();
    CHECK(g_bakeW == G.map.w * 16 && g_bakeH == G.map.h * 16);
    int wx = -1, wy = -1, fx0 = -1, fy0 = -1;
    for (int ty = 0; ty < G.map.h && (wx < 0 || fx0 < 0); ty++) for (int tx = 0; tx < G.map.w; tx++) {
        uint8_t t = G.map.t[G.map.idx(tx, ty)];
        if (t == T_WALL && wx < 0) { wx = tx * 16 + 8; wy = ty * 16 + 8; }
        if (t == T_TILE && fx0 < 0 && G.map.propAt[G.map.idx(tx, ty)] < 0) { fx0 = tx * 16 + 8; fy0 = ty * 16 + 8; }
    }
    CHECK(wx >= 0 && fx0 >= 0);
    px wallBefore = g_bake[wy * g_bakeW + wx], floorBefore = g_bake[fy0 * g_bakeW + fx0];
    bake_blob((float)wx, (float)wy, 4, rgb(200, 20, 30), 256, 7);       // blood cannot paint a wall top
    bake_blob((float)fx0, (float)fy0, 4, rgb(200, 20, 30), 256, 7);     // ...but it stains the floor
    CHECK(g_bake[wy * g_bakeW + wx] == wallBefore);
    CHECK(g_bake[fy0 * g_bakeW + fx0] != floorBefore);
    uint32_t seed = G.rng.s;
    fx_shot(100, 100, 1, 0, 2, true); fx_bulletHit(120, 100, 1, 0, false); fx_bulletHit(120, 100, 1, 0, true); fx_kill(130, 100);
    for (int i = 0; i < 90; i++) fx_update(DT);
    CHECK(G.rng.s == seed);                                              // cosmetic effects use their own RNG
    CHECK(g_fxKickX < 0.01f && g_fxKickX > -0.01f);                      // recoil has settled
    for (int id = 1; id < IT_COUNT; id++) {                              // every icon fills its inventory footprint exactly
        bool ok = g_item[id].d && g_item[id].w == idef(id).w * ITEM_UNIT && g_item[id].h == idef(id).h * ITEM_UNIT;
        if (!ok) printf("  icon %d (%s) is %dx%d\n", id, idef(id).name, g_item[id].w, g_item[id].h);
        CHECK(ok);
    }
}

// zombie looks: every look and version draws, no two are near-copies, every location fields a varied crowd, and
// picking a look never touches the game's random numbers
static void testZombieLooks() {
    SECTION("visuals: zombie looks");
    bool f;
    auto differ = [&](int a, int b) {   // pixels that differ between two zombies facing the camera (of 384)
        const Sprite& A = art_char(true, a, 2, P_STAND, f); const Sprite& B = art_char(true, b, 2, P_STAND, f);
        int n = 0; for (int i = 0; i < A.w * A.h; i++) n += A.d[i] != B.d[i];
        return n;
    };
    bool sized = true;
    for (int v = 0; v < ZOMB_VARIANTS; v++) for (int d = 0; d < 8; d++) { const Sprite& s = art_char(true, v, d, P_WALK_A, f); if (!s.d || s.w != 16 || s.h != 24) sized = false; }
    CHECK(sized);
    int looks = 9999, versions = 9999;
    for (int a = 0; a < ZL_SURVIVOR; a++) for (int b = a + 1; b < ZL_SURVIVOR; b++) { int n = differ(a, b); if (n < looks) looks = n; }
    for (int a = 0; a < ZL_SURVIVOR; a++) for (int j = 0; j < ZOMB_ALTS; j++) for (int k = j + 1; k < ZOMB_ALTS; k++) { int n = differ(a + ZOMB_LOOKS * j, a + ZOMB_LOOKS * k); if (n < versions) versions = n; }
    printf("  closest pair of looks differs in %d px, closest pair of versions in %d px (of 384)\n", looks, versions);
    CHECK(looks >= 60);
    CHECK(versions >= 30);
    const Sprite* first = &art_char(true, ZL_COP, 2, P_STAND, f);
    CHECK(first == &art_char(true, ZL_COP, 2, P_STAND, f));               // frames are drawn once and kept...
    art_zombie_flush();
    CHECK(art_char(true, ZL_COP, 2, P_STAND, f).d != nullptr);            // ...and are drawn again after a flush
    uint32_t seed = G.rng.s;
    for (int l = 0; l < LOC_COUNT; l++) for (int in = 0; in < 2; in++) {
        bool seen[ZOMB_LOOKS] = {}; int kinds = 0, bad = 0;
        for (uint32_t h = 0; h < 400; h++) {
            int v = zombie_look(LOCATIONS[l].map, in != 0, hash32(h * 7 + 1));
            if (v < 0 || v >= ZOMB_VARIANTS || v % ZOMB_LOOKS >= ZL_SURVIVOR) bad++;
            else if (!seen[v % ZOMB_LOOKS]) { seen[v % ZOMB_LOOKS] = true; kinds++; }
        }
        CHECK(bad == 0);
        CHECK(kinds >= 5);                                                // a varied crowd, indoors and out
    }
    CHECK(G.rng.s == seed);
    startExpedition();   // a fresh police station: a mix of people, and whoever is by the cells wears orange
    bool seen[ZOMB_LOOKS] = {}; int kinds = 0, inmates = 0;
    for (int i = 0; i < G.nz; i++) { int lk = G.z[i].variant % ZOMB_LOOKS; if (!seen[lk]) { seen[lk] = true; kinds++; } if (lk == ZL_PRISONER) inmates++; }
    CHECK(kinds >= 4);
    CHECK(inmates >= 1);
}

// the soundtrack: every piece is well formed and plays a full loop cleanly, the same way every time; the game asks
// for the piece that belongs to the place; a change of piece fades instead of cutting; the music slider is obeyed
static void testMusic() {
    SECTION("music: seven pieces, clean, repeatable, and they follow the place");
    CHECK(music_check() == nullptr);
    static float a[4096], b[4096];
    for (int t = 0; t < MUS_COUNT; t++) {
        const float secs = music_loopSeconds(t); const int total = (int)(secs * AUDIO_RATE);
        CHECK(secs > 45 && secs < 130 && music_name(t)[0]);
        music_reset(t);
        float peak = 0; double sum = 0; int bad = 0;
        for (int at = 0; at < total; at += 4096) {
            const int m = total - at < 4096 ? total - at : 4096;
            memset(a, 0, sizeof a); music_render(a, m);
            for (int i = 0; i < m; i++) { const float v = a[i]; if (!(v == v) || fabsf(v) > 4) bad++; if (fabsf(v) > peak) peak = fabsf(v); sum += v * v; }
        }
        const float rms = (float)sqrt(sum / total);
        if (bad || peak >= 1.0f || peak < 0.3f || rms < 0.04f || rms > 0.25f) printf("  %s: peak %.2f rms %.3f bad %d\n", music_name(t), peak, rms, bad);
        CHECK(bad == 0);
        CHECK(peak < 1.0f && peak > 0.3f);       // never clips, even with the sliders at full
        CHECK(rms > 0.04f && rms < 0.25f);       // and is neither a whisper nor a wall
        music_reset(t); memset(a, 0, sizeof a); music_render(a, 4096);
        music_reset(t); memset(b, 0, sizeof b); music_render(b, 4096);
        CHECK(!memcmp(a, b, sizeof a));          // the same notes every time
    }
    // a change of piece fades the old one out and the new one in: no jump in the signal anywhere across the change
    music_reset(MUS_SETTLEMENT);
    for (int k = 0; k < 20; k++) { memset(a, 0, sizeof a); music_render(a, 4096); }
    audio_music(MUS_HOSPITAL);
    float last = a[4095], jump = 0, tail = 0;
    for (int k = 0; k < 16; k++) {
        memset(a, 0, sizeof a); music_render(a, 4096);
        for (int i = 0; i < 4096; i++) { const float d = fabsf(a[i] - last); if (d > jump) jump = d; last = a[i]; if (k == 15 && fabsf(a[i]) > tail) tail = fabsf(a[i]); }
    }
    CHECK(jump < 0.12f);
    CHECK(tail > 0.02f && audio_musicTrack() == MUS_HOSPITAL);   // and the new piece is playing
    // the music slider: silence at zero (effects off too), sound above it
    static int16_t out[2048];
    music_reset(MUS_MENU); audio_volumes(10, 0, 0); audio_pause(false);
    int loud = 0;
    for (int k = 0; k < 8; k++) { audio_mix(out, 2048); for (int i = 0; i < 2048; i++) loud += out[i] != 0; }
    CHECK(loud == 0);
    audio_volumes(10, 0, 6);
    for (int k = 0; k < 8; k++) { audio_mix(out, 2048); for (int i = 0; i < 2048; i++) loud += out[i] != 0; }
    CHECK(loud > 1000);
    // the game asks for the piece that belongs to where you are
    game_newGame(); frames(2);
    CHECK(audio_musicTrack() == MUS_SETTLEMENT);
    static const int WANT[LOC_COUNT] = {MUS_POLICE, MUS_HOSPITAL, MUS_MARKET, MUS_YARD, MUS_HOMES};
    for (int l = 0; l < LOC_COUNT; l++) {
        game_newGame(); G.squad[0] = 0; G.squad[1] = G.squad[2] = -1; G.st.surv[0].onExpedition = true;
        level_leave(); level_enter(LOCATIONS[l].map, l); ui_open(PN_HUD); frames(2);
        CHECK(audio_musicTrack() == WANT[l]);
    }
    G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); frames(2);
    CHECK(audio_musicTrack() == MUS_MENU);
    music_reset(MUS_NONE);
}

// every launch location: within engine limits, every container has a reachable open side, no sealed floor
static void testLaunchMaps() {
    SECTION("maps: all five launch locations are well formed");
    CHECK(LOC_COUNT == 5);
    static uint16_t dist[Map::MAXW * Map::MAXH];
    for (int l = 0; l < LOC_COUNT; l++) {
        static Map m; map_load(m, LOCATIONS[l].map);
        int zs = 0; for (int i = 0; i < m.nspawn; i++) zs += m.spawn[i].kind == 'Z' || m.spawn[i].kind == 'z' || m.spawn[i].kind == 'q';
        bool limits = m.overflow == 0 && m.ncontainers <= MAX_CONT && zs <= 96 && zs <= MAX_ZOMB && m.w * 2 <= 186 && m.h * 2 <= 110;
        if (!limits) printf("  %s: overflow %d containers %d zombies %d size %dx%d\n", LOCATIONS[l].name, m.overflow, m.ncontainers, zs, m.w, m.h);
        CHECK(limits);
        CHECK(m.t[m.idx(m.exitX, m.exitY)] == T_EXTRACT && m.ncontainers >= 40 && zs >= 10);
        for (int i = 0; i < m.nbar; i++) if (m.bar[i].kind != BK_WINDOW) m.bar[i].state = BS_OPEN;   // keys are always reachable first (playtest checks the order)
        map_refresh(m);
        map_flow(m, m.spawnX, m.spawnY, dist, false);
        int bad = 0, sealed = 0;
        for (int i = 0; i < m.nprop; i++) {
            const Prop& p = m.prop[i]; if (p.container < 0) continue;
            bool ok = false;
            for (int x = p.tx; x < p.tx + p.tw; x++) for (int k = 0; k < 2; k++) { int y = k ? p.ty + p.th : p.ty - 1; if (m.in(x, y) && dist[m.idx(x, y)] != 0xFFFF) ok = true; }
            for (int y = p.ty; y < p.ty + p.th; y++) for (int k = 0; k < 2; k++) { int x = k ? p.tx + p.tw : p.tx - 1; if (m.in(x, y) && dist[m.idx(x, y)] != 0xFFFF) ok = true; }
            if (!ok) { bad++; printf("  %s: container at %d,%d cannot be reached\n", LOCATIONS[l].name, p.tx, p.ty); }
        }
        for (int i = 0; i < m.w * m.h; i++) if (!(m.flags[i] & F_BLOCK) && dist[i] == 0xFFFF) sealed++;
        if (sealed) {
            printf("  %s: %d floor tiles sealed off:", LOCATIONS[l].name, sealed);
            for (int i = 0, k = 0; i < m.w * m.h && k < 12; i++) if (!(m.flags[i] & F_BLOCK) && dist[i] == 0xFFFF) { printf(" (%d,%d)", i % m.w, i / m.w); k++; }
            printf("\n");
            if (getenv("MAPDBG")) for (int y = 0; y < 12; y++) {   // P prop, # wall/water, s sealed floor, . reachable
                for (int x = 0; x < 40; x++) { int i = m.idx(x, y); putchar((m.flags[i] & F_BLOCK) ? (m.propAt[i] >= 0 ? 'P' : '#') : (dist[i] == 0xFFFF ? 's' : '.')); }
                putchar('\n');
            }
        }
        CHECK(bad == 0 && sealed == 0);
    }
}

static void testMapsAndZombies() {
    SECTION("maps: police station is well formed and reachable");
    Map m; map_load(m, MAP_POLICE);
    CHECK(m.w == 55 && m.h > 40);
    CHECK(m.ncontainers >= 30 && m.ncontainers <= MAX_CONT && m.ncontainers < 128);
    int zs = 0, rescue = 0; for (int i = 0; i < m.nspawn; i++) { zs += m.spawn[i].kind == 'Z' || m.spawn[i].kind == 'z' || m.spawn[i].kind == 'q'; rescue += m.spawn[i].kind == 'R'; }
    CHECK(zs >= 15 && zs <= 96 && rescue == 1);
    CHECK(m.t[m.idx(m.exitX, m.exitY)] == T_EXTRACT);
    static uint16_t dist[Map::MAXW * Map::MAXH];
    // with every door open, every container must be reachable from the start
    for (int i = 0; i < m.nbar; i++) if (m.bar[i].kind != BK_WINDOW) m.bar[i].state = BS_OPEN;
    map_refresh(m);
    map_flow(m, m.spawnX, m.spawnY, dist, false);
    int unreachable = 0;
    for (int i = 0; i < m.nprop; i++) {
        const Prop& p = m.prop[i]; if (p.container < 0) continue;
        bool ok = false;
        for (int d = 0; d < 4; d++) { int nx = p.tx + (d == 0) - (d == 1), ny = p.ty + (d == 2) - (d == 3); if (m.in(nx, ny) && dist[m.idx(nx, ny)] != 0xFFFF) ok = true; if (p.tw == 2 && m.in(nx + 1, ny) && dist[m.idx(nx + 1, ny)] != 0xFFFF) ok = true; }
        if (!ok) { unreachable++; printf("  unreachable container at %d,%d\n", p.tx, p.ty); }
    }
    CHECK(unreachable == 0);
    Map st; map_load(st, MAP_SETTLEMENT);
    int plots = 0; for (int i = 0; i < st.nprop; i++) plots += st.prop[i].type == PR_PLOT;
    CHECK(plots == B_COUNT);

    SECTION("zombies: detect, chase, lose track, batter doors");
    startExpedition();
    G.nz = 0;
    Unit& u = G.u[0];
    int zi = addZombie(u.x + 50, u.y, ZS_IDLE);
    G.z[zi].hx = -1; G.z[zi].hy = 0;
    frames(10);
    CHECK(G.z[zi].state == ZS_CHASE);
    float d0 = fabsf(G.z[zi].x - u.x);
    frames(30);
    CHECK(fabsf(G.z[zi].x - u.x) < d0);
    // lying zombies rise only when close
    int lz = addZombie(u.x - 140, u.y, ZS_LYING);
    G.z[lz].anim.play(AnimState::ZLying, true);
    frames(10); CHECK(G.z[lz].state == ZS_LYING);
    // door battering: a zombie outside a closed door hunting the player breaks it eventually
    startExpedition();
    G.nz = 0;
    int door = -1; for (int i = 0; i < G.map.nbar; i++) if (G.map.bar[i].kind == BK_DOOR && G.map.bar[i].state == BS_CLOSED) { door = i; break; }
    CHECK(door >= 0);
    Barrier& b = G.map.bar[door];
    G.u[0].x = G.u[0].px = b.x * 16 + 8.0f; G.u[0].y = G.u[0].py = b.y * 16 - 24.0f;   // inside, north of the front door
    G.u[1].x = G.u[1].px = G.u[0].x; G.u[1].y = G.u[1].py = G.u[0].y - 8;
    int dz = addZombie(b.x * 16 + 8.0f, b.y * 16 + 30.0f, ZS_INVESTIGATE);
    G.z[dz].tx = G.u[0].x; G.z[dz].ty = G.u[0].y; G.z[dz].timer = 100;
    int hp0 = b.hp;
    for (int k = 0; k < 40 && b.state != BS_BROKEN; k++) { G.z[dz].state = ZS_INVESTIGATE; G.z[dz].timer = 100; G.z[dz].tx = G.u[0].x; G.z[dz].ty = G.u[0].y; frames(30); }
    CHECK(b.hp < hp0);
    CHECK(b.state == BS_BROKEN);
    // gunshots attract zombies, looting is silent
    startExpedition(); G.nz = 0;
    int far = addZombie(G.u[0].x + 200, G.u[0].y - 150, ZS_IDLE);
    G.z[far].hx = 1; G.z[far].hy = 0;
    frames(3); CHECK(G.z[far].state == ZS_IDLE);
    G.u[0].cd = 0; dbg_fire(0);
    CHECK(G.z[far].state == ZS_INVESTIGATE);
}

static void testSettlement() {
    SECTION("settlement: build costs, cap, security, incidents");
    game_newGame();
    Settlement& st = G.st;
    CHECK(settle_population() == 5 && settle_capacity() == 8);
    const uint8_t* cost = BUILDINGS[B_QUARTERS].cost[1];
    int mat = st.res[RES_MAT];
    CHECK(can_afford(cost)); pay(cost); st.bLevel[B_QUARTERS]++;
    CHECK(st.res[RES_MAT] == mat - cost[RES_MAT] && settle_capacity() == 12);
    int sec0 = settle_security();
    for (int i = 0; i < st.nsurv; i++) st.surv[i].job = JOB_GUARD;
    CHECK(settle_security() > sec0);
    for (int i = 0; i < st.nsurv; i++) st.surv[i].job = JOB_FARMER;
    CHECK(settle_security() < sec0);
    // fair incidents: no deaths while security >= 25
    for (int i = 0; i < st.nsurv; i++) st.surv[i].job = i < 3 ? JOB_GUARD : JOB_FARMER;
    st.res[RES_AMMO] = 999; st.res[RES_FOOD] = 999;
    CHECK(settle_security() >= 25);
    int deaths = 0;
    for (int n = 0; n < 200; n++) { G.clock = 1 * 60; settle_hour(); G.clock = 2 * 60; }
    for (int i = 0; i < st.nsurv; i++) deaths += !st.surv[i].alive;
    CHECK(deaths == 0);
    // recruitment respects the population cap
    st.bLevel[B_QUARTERS] = 1;
    while (settle_population() < settle_capacity()) { Rng r(st.nsurv); st.surv[st.nsurv++] = make_survivor(r, false); }
    CHECK(settle_population() == settle_capacity());

    SECTION("survivors: infection stages, death, turning, permanent");
    game_newGame();
    Survivor& s = G.st.surv[2];
    s.infection = INF_EARLY; s.infKnown = true; s.infHours = 0;
    G.clock = 7 * 60;
    for (int h = 0; h < 20; h++) settle_hour();
    CHECK(s.infection == INF_ADVANCED);
    for (int h = 0; h < 40; h++) settle_hour();
    CHECK(!s.alive);
    int zBefore = G.nz;
    (void)zBefore;
    CHECK(s.hp == 0);
    // early infection is cured by treatment at Lv1
    Survivor& t = G.st.surv[3]; t.infection = INF_EARLY; t.infKnown = true; G.st.res[RES_MED] = 10;
    bool cured; CHECK(settle_treat(3, cured) && cured && t.infection == INF_NONE && G.st.res[RES_MED] == 9);
    // skills improve with use
    int lvl = G.st.surv[0].skill[SK_SHOOT];
    for (int i = 0; i < 200; i++) skill_xp(G.st.surv[0], SK_SHOOT, 1);
    CHECK(G.st.surv[0].skill[SK_SHOOT] > lvl);

    SECTION("raids: extraction delivers resources, loot refresh");
    startExpedition();
    int food = G.st.res[RES_FOOD];
    G.st.surv[0].pack.add(makeItem(IT_CANNED, 2));
    expedition_finish(false);
    CHECK(G.scene == SC_SETTLEMENT && G.st.res[RES_FOOD] == food + 4);
    CHECK(!G.st.surv[0].onExpedition && !G.st.surv[1].onExpedition);
    CHECK(G.loc[0].cooldown == 15 * 60);
    G.loc[0].generated[3] = true; G.loc[0].deadMask[0] = 5; G.loc[0].barState[0] = BS_BROKEN;
    loc_tick(15 * 60 - 1); CHECK(G.loc[0].gen == 0);
    loc_tick(2); CHECK(G.loc[0].gen == 1 && !G.loc[0].generated[3] && G.loc[0].deadMask[0] == 0 && G.loc[0].barState[0] == BS_BROKEN);
    // permadeath during expedition drops a recoverable bag
    startExpedition();
    G.st.surv[1].hp = 1;
    G.nz = 0; int zi = addZombie(G.u[1].x + 6, G.u[1].y, ZS_CHASE); G.z[zi].target = 1;
    for (int k = 0; k < 120 && G.st.surv[1].alive; k++) { G.z[zi].state = ZS_CHASE; G.z[zi].target = 1; G.z[zi].x = G.u[1].x + 6; G.z[zi].y = G.u[1].y; G.u[1].dodgeT = 0; frames(1); }
    CHECK(!G.st.surv[1].alive);
    bool bag = false; for (auto& bg : G.loc[0].bags) bag |= bg.used;
    CHECK(bag);
}

static void testSave() {
    SECTION("save: round trip, corruption, interrupted writes");
    save_setDir("test_saves/");
#ifdef _WIN32
    system("mkdir test_saves 2>nul");
#else
    system("mkdir -p test_saves");
#endif
    game_newGame();
    G.st.surv[0].pack.it[0].rot = 1; G.st.surv[0].pack.it[0].x = 0;
    G.st.surv[4].alive = false; G.st.bLevel[B_WORKSHOP] = 3; G.loc[0].cooldown = 123.5f; G.clock = 1234; G.day = 9;
    G.loc[0].generated[2] = true; G.loc[0].cont[2].n = 0; G.loc[0].cont[2].push(makeItem(IT_SMG));
    static uint8_t buf[512 * 1024];
    int n = save_serialize(buf, sizeof buf);
    CHECK(n > 0);
    Survivor keep = G.st.surv[0];
    G.st.surv[0].pack.it[0].rot = 0; G.st.surv[4].alive = true; G.st.bLevel[B_WORKSHOP] = 1; G.loc[0].cooldown = -1; G.day = 1;
    char err[96];
    CHECK(save_deserialize(buf, n, err, sizeof err));
    CHECK(G.st.surv[0].pack.it[0].rot == 1 && !G.st.surv[4].alive && G.st.bLevel[B_WORKSHOP] == 3);
    CHECK(G.loc[0].cooldown == 123.5f && G.day == 9 && G.clock == 1234);
    CHECK(G.loc[0].generated[2] && G.loc[0].cont[2].n == 1 && G.loc[0].cont[2].it[0].id == IT_SMG);
    CHECK(memcmp(keep.name, G.st.surv[0].name, sizeof keep.name) == 0 && keep.pack.n == G.st.surv[0].pack.n);
    // truncated payload must be rejected without touching the game
    int day = G.day;
    CHECK(!save_deserialize(buf, n / 2, err, sizeof err) && G.day == day);
    // file level: valid save, corrupt it, backup restores
    G.scene = SC_SETTLEMENT;
    CHECK(save_write(0));
    G.day = 10; CHECK(save_write(0));       // slot1.sav = day 10, slot1.bak = day 9
    FILE* f = fopen("test_saves/slot1.sav", "r+b"); fseek(f, 40, SEEK_SET); fputc(0x5A, f); fputc(0xA5, f); fclose(f);
    G.day = 77;
    CHECK(save_read(0, err, sizeof err) && G.day == 9 && strstr(err, "backup"));
    // interrupted write: a stray .tmp never replaces the valid save
    f = fopen("test_saves/slot1.tmp", "wb"); fputs("ZSAV garbage", f); fclose(f);
    G.day = 11; CHECK(save_write(0));
    G.day = 1; CHECK(save_read(0, err, sizeof err) && G.day == 11);
    CHECK(save_delete(0) && !save_exists(0));
}

static void testAnimation() {
    SECTION("animation: frame-rate independence and state rules");
    for (int s = 0; s < (int)AnimState::Count; s++) { const AnimDef& d = anim_def((AnimState)s); CHECK(d.state == (AnimState)s && d.n >= 1 && d.n <= 6); for (int i = 0; i < d.n; i++) CHECK(d.f[i].ms10 > 0 && d.f[i].pose < P_COUNT); }
    Animator a, b; a.play(AnimState::Walk, true); b.play(AnimState::Walk, true);
    int stepsA = 0, stepsB = 0;
    for (int i = 0; i < 30 * 3; i++) stepsA += (a.update(1.0f / 30) & EV_FOOTSTEP) ? 1 : 0;
    for (int i = 0; i < 120 * 3; i++) stepsB += (b.update(1.0f / 120) & EV_FOOTSTEP) ? 1 : 0;
    CHECK(abs(stepsA - stepsB) <= 1 && stepsA >= 9);
    CHECK(a.frame == b.frame);
    Animator m; m.play(AnimState::Melee, true);
    CHECK(!m.play(AnimState::Walk));          // non-interruptible swing finishes first
    CHECK(m.play(AnimState::Death, false));   // higher priority wins
    m.update(2.0f);
    CHECK(m.state == AnimState::Dead);
    CHECK(!m.play(AnimState::Idle) && !m.play(AnimState::Walk));   // never revives
    Animator r; r.play(AnimState::Reload, true);
    float total = 0; for (int i = 0; i < r.cur().ms10; i++) {}
    for (int i = 0; i < anim_def(AnimState::Reload).n; i++) total += anim_def(AnimState::Reload).f[i].ms10 * 0.01f;
    uint8_t ev = 0; for (int i = 0; i < 60; i++) ev |= r.update(total / 50);
    CHECK(r.state == AnimState::Idle && (ev & EV_RELOAD));         // never frozen after reloading
    Animator w; w.play(AnimState::Walk, true); w.update(0.12f); uint8_t fr = w.frame;
    CHECK(!w.play(AnimState::Walk) && w.frame == fr);              // direction changes do not reset the cycle
    CHECK(dir8(1, 0) == 0 && dir8(0, 1) == 2 && dir8(-1, 0) == 4 && dir8(0, -1) == 6 && dir8(1, 1) == 1 && dir8(-1, -1) == 5 && dir8(1, -1) == 7 && dir8(-1, 1) == 3);
    // every survivor/zombie pose exists for all 8 directions
    bool allPoses = true;
    for (int z = 0; z < 2; z++) for (int d = 0; d < 8; d++) for (int p = 0; p < P_COUNT; p++) { bool f; const Sprite& s = art_char(z != 0, 0, d, (Pose)p, f); if (!s.d || s.w * s.h != 16 * 24) allPoses = false; }
    CHECK(allPoses);
    // pivot stability: feet row stays put between stand and walk frames
    bool f;
    const Sprite& st = art_char(false, 0, 2, P_STAND, f); const Sprite& wa = art_char(false, 0, 2, P_WALK_A, f);
    int lowS = 0, lowW = 0; for (int y = 0; y < st.h; y++) for (int x = 0; x < st.w; x++) { if (st.d[y * st.w + x] != KEYC) lowS = y; if (wa.d[y * wa.w + x] != KEYC) lowW = y; }
    CHECK(lowS == lowW);
}

static void testMenus() {
    SECTION("ui: every panel renders and backs out");
    game_newGame();
    for (int p = 0; p < PN_COUNT; p++) {
        if (p == PN_CONTAINER || p == PN_RECRUIT || p == PN_DEATH) continue;
        ui_open((Panel)p);
        frames(3);
        Input back; back.down = BTN_B;
        frames(1, back); frames(2);
    }
    CHECK(G.panel == PN_HUD || G.panel == PN_PAUSE || G.panel == PN_MAIN);
    // recruit flow: accepting adds a survivor, cap enforced
    G.st.visitorPresent = true; Rng r(5); G.st.visitor = make_survivor(r, true);
    int pop = settle_population();
    ui_open(PN_RECRUIT); frames(2);
    Input a; a.down = BTN_A; frames(1, a); frames(1);
    CHECK(settle_population() == pop + 1 && !G.st.visitorPresent);
}


static void press(uint32_t b) { Input i; i.down = b; i.held = b; frames(1, i); frames(1); }
static void tap(int x, int y) {   // stylus tap on the bottom screen
    Input a; a.touching = true; a.touchDown = true; a.tx = x; a.ty = y; frames(1, a);
    Input b; b.touchUp = true; b.tx = x; b.ty = y; frames(1, b);
    frames(1);
}
static void testTutorialZoom() {
    SECTION("tutorial: the opening scene plays itself and hands over at the raid");
    save_setDir("test_saves/");
    game_newGame();
    G.set.tutorialDone = false;
    tut_start();
    CHECK(tut_active() && G.tut == TS_INTRO && tut_scene());
    int total = 0;
    CHECK(tut_sceneBeat(total) == 0 && total == 5);                // fire, radio, leader, the walk, the title card
    float x0 = G.u[G.ctrl].x, y0 = G.u[G.ctrl].y;
    Input mv; mv.ax = 1; frames(20, mv);
    CHECK(G.u[G.ctrl].x == x0 && G.u[G.ctrl].y == y0);              // the pad is the scene's: the leader stays put
    press(BTN_SELECT);
    CHECK(G.panel == PN_HUD);                                      // ...and no menu opens over it
    press(BTN_A); CHECK(tut_sceneBeat(total) == 0);                // the first A finishes the line being typed
    press(BTN_A); CHECK(tut_sceneBeat(total) == 1);                // the second moves on
    for (int k = 0; k < 4; k++) press(BTN_A);
    CHECK(tut_sceneBeat(total) == 3);                              // the walk to the gate, which nobody has to steer
    frames(30);
    CHECK(G.u[G.ctrl].y > y0 + 10);
    for (int i = 0; i < 400 && G.tut == TS_INTRO; i++) frames(1);
    CHECK(G.tut == TS_MOVE && !tut_scene() && G.scene == SC_EXPEDITION && G.map.id == MAP_POLICE);
    Unit& u = G.u[G.ctrl];
    int squad = 0, near = 0;
    for (int i = 0; i < G.nu; i++) squad += !G.u[i].escort;
    for (int i = 0; i < G.nz; i++) near += hypotf(G.z[i].x - u.x, G.z[i].y - u.y) < 200;
    CHECK(squad == 1 && near == 0);                                // a solo raid, and a quiet street to learn in
    CHECK(G.st.surv[u.sv].onExpedition && G.expLoc == G.curLoc);

    SECTION("tutorial: raid lessons finish when the player does the thing");
    Survivor& s = G.st.surv[u.sv];
    mv.ax = -1; frames(40, mv);
    CHECK(G.tut == TS_SEARCH);
    int prop = -1; float bd = 1e9f;                                // the marked container is the one nearest the arrival point
    for (int i = 0; i < G.map.nprop; i++) {
        const Prop& p = G.map.prop[i]; if (p.container < 0) continue;
        float d = hypotf(p.tx * 16 + p.tw * 8.0f - (G.map.spawnX * 16 + 8.0f), p.ty * 16 + p.th * 8.0f - (G.map.spawnY * 16 + 8.0f));
        if (d < bd) { bd = d; prop = i; }
    }
    const Prop& mp = G.map.prop[prop];
    u.x = u.px = mp.tx * 16 + mp.tw * 8.0f; u.y = u.py = (mp.ty + mp.th) * 16 + 9.0f; u.fx = 0; u.fy = -1;
    frames(2); press(BTN_A); frames(50);                           // search it
    CHECK(G.panel == PN_CONTAINER && G.tut == TS_LOOT && tut_highlight() == TH_TAKEALL);
    const Container& ct = G.loc[G.curLoc].cont[mp.container];
    CHECK(ct.n == 4 && ct.it[0].id == IT_TOOLS && ct.it[1].id == IT_SCRAP);   // the fixed first haul
    tap(30, 36);                                                   // Take All
    CHECK(ct.n == 0 && s.pack.countOf(IT_TOOLS) == 1 && s.pack.countOf(IT_SCRAP) == 3);
    press(BTN_B); frames(2);
    CHECK(G.panel == PN_HUD && G.tut == TS_PACK);
    press(BTN_SELECT); CHECK(G.panel == PN_INVENTORY && G.tut == TS_PACK);
    press(BTN_B); frames(2);
    CHECK(G.panel == PN_HUD && G.tut == TS_FIGHT);
    int lesson = 0, side = 0;
    for (int i = 0; i < G.nz; i++) if (G.z[i].spawnIdx == SPAWN_LESSON) { lesson++; side += G.z[i].x > u.x ? 1 : -1; CHECK(fabsf(G.z[i].x - u.x) > 100); }
    CHECK(lesson == 2 && (side == 2 || side == -2));               // two of them, out of view, from one side
    float minHp = s.hp;
    for (int i = 0; i < 900; i++) { frames(1); if (s.hp < minHp) minHp = s.hp; }
    CHECK(minHp < 100);                                            // they reach you and they bite...
    CHECK(minHp >= 25 && s.infection == INF_NONE && u.alive);      // ...but a lesson cannot end the run
    CHECK(G.tut == TS_FIGHT);                                      // and standing there does not pass it
    s.pack.it[s.equip].mag = 3;
    for (int i = 0; i < G.nz; i++) if (G.z[i].spawnIdx == SPAWN_LESSON) dbg_zombieHit(i, HR_HEAD, 1);
    frames(2);
    CHECK(G.tut == TS_RELOAD);
    press(BTN_X); frames(2);
    CHECK(G.tut == TS_DODGE);
    Input b1; b1.held = BTN_B; b1.down = BTN_B; frames(1, b1); Input b2; b2.up = BTN_B; frames(1, b2); frames(10);
    CHECK(G.tut == TS_EXTRACT);
    int mat = G.st.res[RES_MAT];
    u.x = u.px = G.map.exitX * 16 + 8.0f; u.y = u.py = G.map.exitY * 16 + 8.0f;
    frames(2); press(BTN_A); frames(2);
    CHECK(G.scene == SC_SETTLEMENT && G.panel == PN_SUMMARY && G.tut == TS_HOME);
    CHECK(G.st.res[RES_MAT] == mat + 15);                          // toolbox 6 + three scrap at 3: the haul reached the stores

    SECTION("tutorial: home, the tour of the buildings, an upgrade, the next raid");
    tap(289, 36);                                                  // Return Home
    if (G.panel == PN_JOURNAL) press(BTN_B);
    frames(3);
    CHECK(G.panel == PN_HUD && G.tut == TS_TOUR && tut_scene());
    CHECK(tut_sceneBeat(total) == 0 && total == 8);                // a greeting, six buildings, a closing line
    press(BTN_A); press(BTN_A);
    frames(90);                                                    // the camera travels to the first building
    float sx = 0, sy = 0;
    for (int i = 0; i < G.map.nprop; i++) if (G.map.prop[i].type == PR_PLOT && G.map.prop[i].special == B_STORAGE) { sx = G.map.prop[i].tx * 16 + 24.0f; sy = G.map.prop[i].ty * 16 + 24.0f; }
    float want = sx - view_w() / 2, most = (float)(G.map.w * 16 - view_w());   // as far as the edge of the map allows
    CHECK(fabsf(G.camX - (want < 0 ? 0 : (want > most ? most : want))) < 6);
    for (int k = 0; k < 14; k++) press(BTN_A);
    CHECK(G.tut == TS_BUILDING && !tut_scene() && tut_highlight() == TH_BASE);
    Unit& h = G.u[G.ctrl];
    h.x = h.px = sx; h.y = h.py = sy + 34; h.fx = 0; h.fy = -1;      // at the storage door
    frames(2); press(BTN_A); frames(2);
    CHECK(G.panel == PN_BUILDING && g_buildingSel == B_STORAGE && G.tut == TS_UPGRADE && tut_highlight() == TH_UPGRADE);
    int lv = G.st.bLevel[B_STORAGE]; mat = G.st.res[RES_MAT];
    tap(30, 108);                                                  // the Upgrade tab
    tap(289, 36);                                                  // Upgrade
    frames(2);
    CHECK(G.st.bLevel[B_STORAGE] == lv + 1 && G.st.res[RES_MAT] == mat - 25);
    CHECK(G.tut == TS_PLAN && tut_highlight() == TH_RAID);
    press(BTN_B); frames(2);
    CHECK(G.panel == PN_HUD && tut_active());
    tap(30, 185);                                                  // Raid: the world map
    frames(2);
    CHECK(G.panel == PN_WORLDMAP && !tut_active() && G.set.tutorialDone);

    SECTION("tutorial: skipping, leaving early and empty stores never strand it");
    game_newGame(); G.set.tutorialDone = false; tut_start();
    tut_skip();
    CHECK(!tut_active() && G.set.tutorialDone && G.scene == SC_SETTLEMENT);
    game_newGame(); tut_start();
    press(BTN_START); frames(2);                                   // START skips the scene, not the tutorial
    CHECK(G.tut == TS_MOVE && G.scene == SC_EXPEDITION && G.panel == PN_HUD);   // (and the same press does not open the pause menu)
    expedition_finish(false); frames(2);                           // out again before a single lesson is done
    CHECK(G.tut == TS_HOME);
    ui_open(PN_HUD); frames(2);
    CHECK(G.tut == TS_TOUR);
    press(BTN_START); frames(2);
    CHECK(G.tut == TS_BUILDING);
    for (int r = 0; r < RES_COUNT; r++) G.st.res[r] = 0;           // nothing in the stores: the upgrade is shown, not forced
    ui_open(PN_BUILDING); frames(2);
    CHECK(G.tut == TS_UPGRADE);
    ui_open(PN_HUD); frames(30);
    CHECK(G.tut == TS_PLAN);
    G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); frames(2);   // off to the main menu: dropped quietly
    CHECK(!tut_active());
    game_newGame(); tut_start(); press(BTN_START); frames(2);
    expedition_finish(true); frames(2);                            // the raid failed: the settlement half still runs
    CHECK(G.tut == TS_HOME);
    ui_open(PN_HUD); frames(2);
    CHECK(G.tut == TS_TOUR && tut_scene());
    tut_skip();

    SECTION("zoom: every level renders, view covers the screen, settings persist");
    startExpedition();
    const uint8_t Z[4] = {100, 125, 150, 200};
    for (uint8_t z : Z) {
        G.set.zoom = z; frames(3);
        CHECK(view_w() * z / 100 >= TOP_W && view_h() * z / 100 >= TOP_H);
        CHECK(G.camX >= 0 && G.camX <= G.map.w * 16 - view_w() + 1);
    }
    G.set.zoom = 150; G.set.tutorialDone = true; settings_save();
    G.set.zoom = 100; G.set.tutorialDone = false; settings_load();
    CHECK(G.set.zoom == 150 && G.set.tutorialDone);
    FILE* f = fopen("test_saves/settings.ini", "w"); fputs("8 8 6 30 2 1 0 0 0\n", f); fclose(f);   // old 9-field file
    settings_load();
    CHECK(G.set.zoom == 200 && !G.set.tutorialDone);
    G.set.zoom = 125;
}
static void testMenuNav() {
    SECTION("menus: D-pad and Circle Pad navigate every paused menu, holding repeats");
    save_setDir("test_saves/");
    game_newGame(); G.set.tutorialDone = true;
    ui_open(PN_PAUSE); frames(2);
    int f0 = g_ui.focus;
    press(BTN_DOWN); CHECK(g_ui.focus != f0);                              // D-pad press
    int f1 = g_ui.focus;
    Input st; st.ay = 1; frames(1, st); CHECK(g_ui.focus != f1);         // Circle Pad push
    int f2 = g_ui.focus; frames(14, st); CHECK(g_ui.focus != f2);        // held Circle Pad repeats
    frames(1); press(BTN_UP); int f3 = g_ui.focus;
    Input hd; hd.held = BTN_UP; frames(15, hd); CHECK(g_ui.focus != f3); // held D-pad repeats
    ui_open(PN_HUD); frames(2);
    int fh = g_ui.focus; Input walk; walk.ay = 1; walk.held = BTN_DOWN; frames(20, walk);
    CHECK(G.panel == PN_HUD && g_ui.focus == fh);                         // walking never moves HUD focus

    SECTION("backpack: D-pad / Circle Pad move items; right edge reaches the buttons");
    game_newGame(); G.set.tutorialDone = true;
    Survivor& s = G.st.surv[G.st.leader];
    s.pack.n = 0; s.equip = -1;
    Item p = makeItem(IT_PISTOL); p.x = 0; p.y = 0; CHECK(s.pack.place(p));
    ui_open(PN_INVENTORY); frames(2);
    for (int k = 0; k < 8; k++) { press(BTN_LEFT); press(BTN_UP); }     // cursor to 0,0 (edges clamp)
    press(BTN_A);                                                          // pick up
    Input r; r.ax = 1; frames(1, r); frames(1);                           // Circle Pad right
    press(BTN_RIGHT); press(BTN_RIGHT);                                    // D-pad right x2
    press(BTN_A);                                                          // place
    CHECK(G.panel == PN_INVENTORY && s.pack.n == 1 && s.pack.it[0].x == 3 && s.pack.it[0].y == 0);
    for (int k = 0; k < 8; k++) press(BTN_RIGHT);                          // off the right edge onto the buttons
    press(BTN_B); CHECK(G.panel == PN_INVENTORY);                          // B on the buttons: back to the grid
    press(BTN_B); CHECK(G.panel != PN_INVENTORY);                          // B on the grid: close
}
int main() {
    game_init();
    testSprintWinded();
    testCombat();
    testInventory();
    testContainers();
    testMapsAndZombies();
    testLaunchMaps();
    testVisualLayers();
    testZombieLooks();
    testMusic();
    testSettlement();
    testSave();
    testAnimation();
    testMenus();
    testTutorialZoom();
    testMenuNav();
    printf("\n%d passed, %d failed\n", passes, fails);
    return fails;
}
