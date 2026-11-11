// Headless developer tool:
//   zs_tool --gallery <dir>   export every generated sprite sheet as PNG (asset review)
//   zs_tool --shots <dir>     scripted playthrough, captures native-resolution screenshots
//   zs_tool --bench           heavy-scene timing (sim + render per frame)
//   zs_tool --icons <dir>     3DS icon (48x48), banner (256x128) and Windows .ico
#include "../src/gameplay/game.h"
#include "../src/core/art.h"
#include "../src/core/audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <math.h>

static px topB[TOP_W * TOP_H], botB[BOT_W * BOT_H];
extern float g_renderAlpha;

static void step(int n, Input in = Input()) {
    for (int i = 0; i < n; i++) {
        game_frame(in, DT);
        Canvas t, b; t.init(topB, TOP_W, TOP_H); b.init(botB, BOT_W, BOT_H);
        game_render(t, b, g_renderAlpha);
        in.down = in.up = 0; in.touchDown = in.touchUp = false;
    }
}
static void tap(int x, int y) {
    Input a; a.touching = true; a.touchDown = true; a.tx = x; a.ty = y; step(1, a);
    Input b; b.touchUp = true; b.tx = x; b.ty = y; step(1, b);
    step(1);
}
static void press(uint32_t btn, int hold = 1) { Input a; a.down = btn; a.held = btn; step(1, a); a.down = 0; for (int i = 1; i < hold; i++) step(1, a); Input u; u.up = btn; step(1, u); }

// composite like the Windows window: top screen, gap, bottom screen centred
static void shot(const char* dir, const char* name) {
    enum { W = TOP_W, H = TOP_H + 8 + BOT_H };
    static px img[W * H];
    for (int i = 0; i < W * H; i++) img[i] = rgb(16, 16, 20);
    memcpy(img, topB, sizeof topB);
    for (int y = 0; y < BOT_H; y++) memcpy(img + (TOP_H + 8 + y) * W + 40, botB + y * BOT_W, BOT_W * sizeof(px));
    char p[512]; snprintf(p, sizeof p, "%s/%s.png", dir, name);
    gfx_writePNG(p, img, W, H);
    printf("  wrote %s\n", p);
}

// art review: the Try 2 style-guide lineup (3 survivors + 3 zombies facing the camera) and every
// direction's walk cycle + aim, at 4x, for side-by-side comparison with the mockup
// one character frame at a scale (the frame is looked up first: it says whether to mirror)
static void blitChar(Canvas& c, bool zombie, int variant, int dir, Pose pose, int x, int y, int scale) {
    bool flip; const Sprite& s = art_char(zombie, variant, dir, pose, flip);
    gfx_blit(c, s, x, y, scale, flip);
}
static void lineup(const char* path) {
    const int S = 4, W = 8 * 20 * S, H = 4 * 28 * S;
    px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(66, 64, 66);
    Canvas c; c.init(img, W, H);
    const int SV[3] = {2, 1, 3};
    for (int i = 0; i < 3; i++) blitChar(c, false, SV[i], 2, P_STAND, (1 + i) * 20 * S, 2 * S, S);
    for (int i = 0; i < 3; i++) blitChar(c, true, i, 2, P_STAND, (4 + i) * 20 * S, 2 * S, S);
    const int D[3] = {2, 0, 6}; const Pose PS[6] = {P_STAND, P_WALK_A, P_STAND, P_WALK_B, P_AIM, P_RUN_A};
    for (int r = 0; r < 3; r++) for (int k = 0; k < 6; k++) blitChar(c, false, 0, D[r], PS[k], (1 + k) * 20 * S, (1 + r) * 28 * S + 2 * S, S);
    gfx_writePNG(path, img, W, H); free(img);
    printf("lineup written to %s\n", path);
}
// art review (zs_tool --zombies DIR): zombie_looks.png shows every look from the front, in 3/4 view, from behind and
// mid-step at 3x on alternating light-floor and dark-road cells; zombie_versions.png shows the four versions of each
// of the sixteen place looks side by side (one row per version)
static void zombies(const char* dir) {
    char path[512];
    {
        const int S = 3, CELL_W = 78, CELL_H = 30, COLS = 6, ROWS = (ZOMB_LOOKS + COLS - 1) / COLS, W = COLS * CELL_W * S, H = ROWS * CELL_H * S;
        px* img = (px*)malloc(W * H * sizeof(px));
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) img[y * W + x] = ((x / (CELL_W * S) + y / (CELL_H * S)) & 1) ? rgb(70, 74, 92) : rgb(196, 186, 166);
        Canvas c; c.init(img, W, H);
        static const int D[4] = {2, 0, 6, 0}; static const Pose P[4] = {P_STAND, P_STAND, P_STAND, P_WALK_A};
        for (int v = 0; v < ZOMB_LOOKS; v++) for (int k = 0; k < 4; k++) blitChar(c, true, v, D[k], P[k], ((v % COLS) * CELL_W + 3 + k * 18) * S, ((v / COLS) * CELL_H + 3) * S, S);
        snprintf(path, sizeof path, "%s/zombie_looks.png", dir); gfx_writePNG(path, img, W, H); free(img); printf("  wrote %s\n", path);
    }
    {
        const int S = 3, CELL_W = 20, CELL_H = 28, W = ZL_SURVIVOR * CELL_W * S, H = ZOMB_ALTS * CELL_H * S;
        px* img = (px*)malloc(W * H * sizeof(px));
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) img[y * W + x] = (x / (CELL_W * S)) & 1 ? rgb(70, 74, 92) : rgb(196, 186, 166);
        Canvas c; c.init(img, W, H);
        for (int a = 0; a < ZOMB_ALTS; a++) for (int v = 0; v < ZL_SURVIVOR; v++) blitChar(c, true, v + ZOMB_LOOKS * a, 2, P_STAND, (v * CELL_W + 2) * S, (a * CELL_H + 2) * S, S);
        snprintf(path, sizeof path, "%s/zombie_versions.png", dir); gfx_writePNG(path, img, W, H); free(img); printf("  wrote %s\n", path);
    }
}
// a look for a zombie the tools place by hand, picked the way the game picks one for a spawn on that spot
static uint8_t toolLook(const Zombie& z, int k) {
    int tx = (int)(z.x / 16), ty = (int)(z.y / 16);
    return (uint8_t)zombie_look(G.map.id, G.map.in(tx, ty) && (G.map.flags[G.map.idx(tx, ty)] & F_INDOOR), hash32(k * 977u + 13));
}
// the Try 2 "Exploration & Combat" panel restaged: leader facing east firing at zombies (top screen, 200% zoom)
static void duel(const char* path) {
    game_newGame(); G.set.tutorialDone = true; G.set.zoom = 200;
    G.squad[0] = 0; G.squad[1] = -1; G.squad[2] = -1; G.st.surv[0].onExpedition = true;
    level_leave(); level_enter(MAP_POLICE, 0); ui_open(PN_HUD); step(10);
    Unit& u = G.u[G.ctrl];
    u.x = u.px = 24 * 16 + 8.0f; u.y = u.py = 34 * 16 + 4.0f; G.clock = 21 * 60 + 30;   // station entrance at night (Try 2 combat panel)
    G.camX = u.x - view_w() / 2; G.camY = u.y - view_h() / 2; step(30);
    G.nz = 0;
    for (int k = 0; k < 2; k++) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x + 64 + k * 10; z.y = z.py = u.y - 20 + k * 34; z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 0.01f; z.variant = toolLook(z, k); z.hx = -1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
    }
    u.fx = 1; u.fy = 0; u.dir = 0; step(25);   // let the flashlight beam swing round first
    Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; }
    Input y; y.down = BTN_Y; y.held = BTN_Y; step(1, y);
    gfx_writePNG(path, topB, TOP_W, TOP_H);
    printf("duel written to %s\n", path);
}
// the Try 2 "Settlement (Overview)" panel restaged: every facility built, by day and at dusk,
// plus a 100%-zoom overview of the whole compound. Writes <dir>/base_day.png, base_dusk.png, base_all.png
static void base(const char* dir) {
    game_newGame(); G.set.tutorialDone = true;
    const int LV[BLD_TYPES] = {2, 3, 2, 2, 3, 3};
    for (int b = 0; b < BLD_TYPES; b++) G.st.bLevel[b] = (uint8_t)LV[b];
    G.st.generator = true;
    level_enter(MAP_SETTLEMENT, -1); ui_open(PN_HUD);
    char p[512];
    const struct { const char* n; float clock; int zoom; } shots[3] = {{"base_day", 11 * 60, 200}, {"base_dusk", 18 * 60 + 40, 200}, {"base_all", 13 * 60, 100}};
    for (auto& s : shots) {
        G.clock = s.clock; G.set.zoom = (uint8_t)s.zoom; step(360);   // let settlers walk to their evening spots
        snprintf(p, sizeof p, "%s/%s.png", dir, s.n); gfx_writePNG(p, topB, TOP_W, TOP_H);
        printf("  wrote %s\n", p);
    }
}

static void gallery(const char* dir) {
    // characters: rows = variant x direction, columns = poses (2x scale)
    {
        int cols = P_COUNT, rows = (SURV_VARIANTS + ZOMB_LOOKS) * 5, S = 2;
        int W = cols * 26 * S, H = rows * 26 * S;
        px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(60, 58, 54);
        Canvas c; c.init(img, W, H);
        static const int D5[5] = {2, 1, 0, 7, 6};
        for (int z = 0; z < 2; z++) for (int v = 0; v < (z ? ZOMB_LOOKS : SURV_VARIANTS); v++) for (int d = 0; d < 5; d++) for (int p = 0; p < P_COUNT; p++) {
            bool f; const Sprite& s = art_char(z != 0, v, D5[d], (Pose)p, f);
            int row = (z ? SURV_VARIANTS : 0) * 5 + v * 5 + d;
            gfx_blit(c, s, p * 26 * S + S, row * 26 * S + S, S, f);
        }
        char path[512]; snprintf(path, sizeof path, "%s/characters.png", dir); gfx_writePNG(path, img, W, H); free(img);
    }
    // tiles
    {
        int W = 4 * 18 * 2, H = TA_COUNT * 18 * 2; px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(60, 58, 54);
        Canvas c; c.init(img, W, H);
        for (int t = 0; t < TA_COUNT; t++) for (int v = 0; v < 4; v++) gfx_blit(c, g_tile[t][v], v * 36 + 2, t * 36 + 2, 2);
        char path[512]; snprintf(path, sizeof path, "%s/tiles.png", dir); gfx_writePNG(path, img, W, H); free(img);
    }
    // props + icons
    {
        int W = 8 * 36 * 2, H = 5 * 36 * 2; px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(60, 58, 54);
        Canvas c; c.init(img, W, H);
        for (int s = 0; s < S_COUNT; s++) gfx_blit(c, g_spr[s], (s % 8) * 72 + 2, (s / 8) * 72 + 2, 2);
        char path[512]; snprintf(path, sizeof path, "%s/props_icons.png", dir); gfx_writePNG(path, img, W, H); free(img);
    }
    // items at inventory scale (2x)
    {
        int W = 6 * 124, H = 6 * 86; px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(44, 48, 74);
        Canvas c; c.init(img, W, H);
        for (int i = 1; i < IT_COUNT; i++) { int x = ((i - 1) % 6) * 124 + 4, y = ((i - 1) / 6) * 86 + 4; gfx_blit(c, g_item[i], x, y, 2); gfx_text(c, x, y + 40 > y + g_item[i].h * 2 + 2 ? y + 40 : y + g_item[i].h * 2 + 2, idef(i).name, pal::cream); }
        char path[512]; snprintf(path, sizeof path, "%s/items.png", dir); gfx_writePNG(path, img, W, H); free(img);
    }
    // portraits + buildings
    {
        int W = BLD_LEVELS * 100 + 20, H = BLD_TYPES * 52 + 60; px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(70, 84, 44);
        Canvas c; c.init(img, W, H);
        for (int b = 0; b < BLD_TYPES; b++) for (int l = 0; l < BLD_LEVELS; l++) gfx_blit(c, g_building[b][l], l * 100 + 10, b * 52 + 2);
        for (int p = 0; p < PORTRAITS; p++) gfx_blit(c, g_portrait[p], p * 50 + 4, BLD_TYPES * 52 + 6, 2);
        char path[512]; snprintf(path, sizeof path, "%s/buildings_portraits.png", dir); gfx_writePNG(path, img, W, H); free(img);
    }
    printf("gallery written to %s\n", dir);
}

static void shots(const char* dir) {
    G.set.dayMinutes = 30;
    step(20); shot(dir, "01_title");
    game_newGame(); step(40);
    shot(dir, "02_settlement_day");
    tut_start(); step(40); press(BTN_A); press(BTN_A); step(60); shot(dir, "02b_tutorial_scene"); tut_skip(); G.toastT = 0;   // the opening scene, at the radio line
    ui_open(PN_CONTROLS); step(8); shot(dir, "02c_controls_3ds");
    { extern int g_inputStyle; extern const char* g_keyName[12]; static const char* PC[12] = {"E", "Shift", "R", "Space", "Q", "F", "Esc", "Tab", "^", "v", "<", ">"}; for (int i = 0; i < 12; i++) g_keyName[i] = PC[i]; g_inputStyle = 1; step(2); shot(dir, "02d_controls_keyboard"); ui_open(PN_HUD); step(4); shot(dir, "02e_hud_keyboard_prompts"); g_inputStyle = 0; }
    ui_open(PN_BASE); step(10); shot(dir, "03_settlement_base");
    g_buildingSel = B_INFIRMARY; ui_open(PN_BUILDING); step(10); shot(dir, "04_build_menu");
    ui_open(PN_SURVIVORS); step(10); shot(dir, "05_survivors");
    ui_open(PN_WORLDMAP); step(10); shot(dir, "06_world_map");
    tap(289, 36);   // Deploy (right rail, top) -> prep
    step(5); shot(dir, "07_raid_prep");
    G.squad[1] = 4; step(2);
    tap(289, 36);   // Deploy
    step(40); shot(dir, "08_police_arrival");
    // stand at the lobby doors (tiles 26-27, row 32), open them and walk in. Wandering zombies are cleared so the
    // scripted route is the same every run; the combat shots below bring their own.
    Unit& u = G.u[G.ctrl];
    G.nz = 0;
    for (int k = 0; k < G.nu; k++) { Unit& q = G.u[k]; if (q.escort) continue; q.x = q.px = 27 * 16.0f + (k == G.ctrl ? 0 : 14); q.y = q.py = 33 * 16 + (k == G.ctrl ? 6.0f : 14.0f); }
    Input up; up.ay = -1;
    step(4, up); press(BTN_A); step(20);
    for (int i = 0; i < 30; i++) step(1, up);
    G.nz = 0; step(5); shot(dir, "09_police_lobby");
    // find and search the nearest container
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < G.map.nprop; i++) { const Prop& p = G.map.prop[i]; if (p.container < 0) continue; float dx = p.tx * 16 + 8 - u.x, dy = p.ty * 16 + 8 - u.y; if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; } }
    if (best >= 0) { const Prop& p = G.map.prop[best]; u.x = u.px = p.tx * 16 + 8.0f; u.y = u.py = p.ty * 16 + 26.0f; u.fx = 0; u.fy = -1; }
    step(6); shot(dir, "10_container_highlight");
    press(BTN_A); step(14); shot(dir, "11_searching");
    step(40); shot(dir, "12_container_menu");
    tap(30, 36);    // Take All (left rail, top)
    step(4); shot(dir, "13_container_taken");
    press(BTN_B); step(4);
    ui_open(PN_INVENTORY); step(8); shot(dir, "14_inventory");
    ui_open(PN_HUD);
    // combat: spawn a few zombies in front and fire
    G.nz = 0;
    for (int k = 0; k < 4; k++) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x + 30 + k * 12; z.y = z.py = u.y + 10 - k * 8; z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = toolLook(z, k); z.hx = -1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
    }
    u.fx = 1; u.fy = 0; u.dir = 0; step(25);   // let the flashlight beam swing round first
    Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; }
    press(BTN_Y); step(2); shot(dir, "15_combat_muzzle");
    for (int i = 0; i < 4; i++) { press(BTN_Y); step(9); }
    shot(dir, "16_combat_hits");
    G.set.zoom = 100; step(3); shot(dir, "16b_zoom_100");
    G.set.zoom = 200; step(3); shot(dir, "16c_zoom_200");
    G.set.zoom = 125;
    // night with flashlight
    G.clock = 23 * 60; step(10); shot(dir, "17_night_flashlight");
    G.flashlight = false; step(10); shot(dir, "18_night_no_light");
    G.flashlight = true;
    ui_open(PN_PAUSE); step(8); shot(dir, "19_pause");
    ui_open(PN_SQUAD); step(8); shot(dir, "20_squad");
    ui_open(PN_HUD);
    // extract
    u.x = u.px = G.map.exitX * 16 + 8.0f; u.y = u.py = G.map.exitY * 16 + 8.0f; step(4);
    press(BTN_A); step(10); shot(dir, "21_extraction_summary");
    ui_open(PN_HUD); G.clock = 21 * 60; step(30); shot(dir, "22_settlement_night");
    G.clock = 12 * 60; ui_open(PN_INFIRMARY); step(8); shot(dir, "23_infirmary");
    ui_open(PN_WORKSHOP); step(8); shot(dir, "24_workshop");
    G.st.visitorPresent = true; { Rng r(3); G.st.visitor = make_survivor(r, true); }
    ui_open(PN_RECRUIT); step(8); shot(dir, "25_recruit");
    ui_open(PN_JOURNAL); step(8); shot(dir, "26_journal");
    ui_open(PN_SETTINGS); step(8); shot(dir, "27_settings");
    ui_open(PN_ANIMTEST); step(20); shot(dir, "28_anim_test_room");
    ui_open(PN_SAVELOAD); step(8); shot(dir, "29_save_load");
    // the tutorial: a lesson out on the raid, the tour of the buildings, the upgrade lesson
    game_newGame(); tut_start(); press(BTN_START); step(30); shot(dir, "31_tutorial_raid_lesson");
    expedition_finish(false); ui_open(PN_HUD); step(4); press(BTN_A); press(BTN_A); step(80); shot(dir, "32_tutorial_tour");
    press(BTN_START); step(4); g_buildingSel = B_STORAGE; ui_open(PN_BUILDING); step(8); shot(dir, "33_tutorial_upgrade");
    tut_skip(); G.toastT = 0;
    G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); step(8); shot(dir, "30_main_menu");
}

static double ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

// settlement benchmark: every facility built, survivors walking about, at day / dusk / night, with a stage breakdown
extern double (*g_profNow)(); extern double g_prof[8];
static double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void benchBase() {
    game_newGame(); G.set.tutorialDone = true; G.set.zoom = 200;
    for (int b = 0; b < BLD_TYPES; b++) G.st.bLevel[b] = 3;
    G.st.generator = true;
    level_enter(MAP_SETTLEMENT, -1); ui_open(PN_HUD);
    g_profNow = nowMs;
    const struct { const char* n; float clock; } T[3] = {{"day", 11 * 60}, {"dusk", 18 * 60 + 40}, {"night", 23 * 60}};
    static const char* STAGE[6] = {"light map", "tiles+sprites", "shading", "colour grade", "pools/windows", "zoom scale"};
    for (auto& t : T) {
        G.clock = t.clock; step(30);
        for (double& p : g_prof) p = 0;
        double simT = 0, renT = 0, worst = 0; int frames = 240;
        for (int i = 0; i < frames; i++) {
            Input in; in.ax = (i / 60) % 2 ? 1.0f : -1.0f;
            auto a = std::chrono::steady_clock::now();
            game_frame(in, DT);
            auto b = std::chrono::steady_clock::now();
            Canvas tc, bo; tc.init(topB, TOP_W, TOP_H); bo.init(botB, BOT_W, BOT_H);
            game_render(tc, bo, g_renderAlpha);
            auto c = std::chrono::steady_clock::now();
            simT += ms(a, b); renT += ms(b, c); if (ms(a, c) > worst) worst = ms(a, c);
        }
        printf("settlement %-5s sim %.3f  render %.3f  worst %.3f ms |", t.n, simT / frames, renT / frames, worst);
        for (int k = 0; k < 6; k++) printf(" %s %.3f", STAGE[k], g_prof[k] / frames);
        printf("\n");
    }
    g_profNow = nullptr;
}
static void bench() {
    game_newGame();
    G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = 4;
    level_leave(); level_enter(MAP_POLICE, 0); ui_open(PN_HUD);
    G.clock = 23 * 60;   // night: flashlight + full lighting path
    Unit& u = G.u[0];
    while (G.nz < 60) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x - 160 + (G.nz % 12) * 26; z.y = z.py = u.y - 140 + (G.nz / 12) * 22;
        z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = toolLook(z, G.nz); z.hx = 0; z.hy = 1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
    }
    Survivor& s = G.st.surv[0];
    double simT = 0, renT = 0, worst = 0; int frames = 240;
    for (int i = 0; i < frames; i++) {
        Input in; in.ax = (i / 60) % 2 ? 1.0f : -1.0f; if (i % 6 == 0) { in.down = BTN_Y; in.held = BTN_Y; }
        if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; }
        s.hp = 100;
        auto a = std::chrono::steady_clock::now();
        game_frame(in, DT);
        auto b = std::chrono::steady_clock::now();
        Canvas t, bo; t.init(topB, TOP_W, TOP_H); bo.init(botB, BOT_W, BOT_H);
        game_render(t, bo, g_renderAlpha);
        auto c = std::chrono::steady_clock::now();
        simT += ms(a, b); renT += ms(b, c); if (ms(a, c) > worst) worst = ms(a, c);
        static int lastDead = 0; int dead = 0; for (int k = 0; k < G.nu; k++) dead += !G.u[k].alive;
        if (ms(a, b) > 1.0 || dead != lastDead) printf("  frame %d: sim %.2f ms, squad members down %d\n", i, ms(a, b), dead);   // slow frames and deaths
        lastDead = dead;
    }
    int alive = 0; for (int i = 0; i < G.nz; i++) alive += G.z[i].state != ZS_DEAD;
    printf("bench: %d zombies (%d alive at end), 3 survivors, night+flashlight, %d frames\n", 60, alive, frames);
    printf("  sim    avg %.3f ms/frame\n  render avg %.3f ms/frame\n  worst  %.3f ms\n", simT / frames, renT / frames, worst);
    printf("  memory: Game state %u KB, static frame buffers %u KB\n", (unsigned)(sizeof(Game) / 1024), (unsigned)((sizeof topB + sizeof botB) / 1024));
}

static void icons(const char* dir) {
    // icon and banner are cut from the title screen so the HOME menu matches the game: dusk sky, sun, skyline, logo
    static px tt[TOP_W * TOP_H]; Canvas tc; tc.init(tt, TOP_W, TOP_H);
    G.uiTime = 0.4f; render_title(tc);
    // 48x48 icon: the sun over the skyline (half scale), a zombie in front
    static px ic[48 * 48]; Canvas c; c.init(ic, 48, 48);
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) ic[y * 48 + x] = tt[(98 + y * 2) * TOP_W + 70 + x * 2];
    bool f; const Sprite& zs = art_char(true, ZL_OFFICE + ZOMB_LOOKS, 2, P_STAND, f);   // the icon zombie: green, shirt and tie
    gfx_fill(c, 0, 40, 48, 8, rgb(26, 22, 44));
    gfx_blit(c, zs, 24 - zs.w, 46 - zs.h * 2, 2, f);
    gfx_rect(c, 0, 0, 48, 48, rgb(22, 22, 34));
    char p[512]; snprintf(p, sizeof p, "%s/icon.png", dir); gfx_writePNG(p, ic, 48, 48);
    // banner 256x128 (3DS HOME menu top screen): the logo over the skyline
    static px bn[256 * 128];
    for (int y = 0; y < 128; y++) memcpy(bn + y * 256, tt + (y + 12) * TOP_W + 72, 256 * sizeof(px));
    snprintf(p, sizeof p, "%s/banner.png", dir); gfx_writePNG(p, bn, 256, 128);
    // Windows .ico containing the PNG (Vista+ format)
    snprintf(p, sizeof p, "%s/icon.png", dir);
    FILE* in = fopen(p, "rb"); fseek(in, 0, SEEK_END); long n = ftell(in); fseek(in, 0, SEEK_SET);
    unsigned char* d = (unsigned char*)malloc(n); fread(d, 1, n, in); fclose(in);
    snprintf(p, sizeof p, "%s/icon.ico", dir);
    FILE* o = fopen(p, "wb");
    unsigned char h[22] = {0, 0, 1, 0, 1, 0, 48, 48, 0, 0, 1, 0, 32, 0, (unsigned char)(n & 255), (unsigned char)((n >> 8) & 255), (unsigned char)((n >> 16) & 255), 0, 22, 0, 0, 0};
    fwrite(h, 1, 22, o); fwrite(d, 1, n, o); fclose(o); free(d);
    // banner sting (PCM16 mono 22050 Hz, 1.5 s) for bannertool
    {
        const int N = 33075; static short pcm[N];
        for (int i = 0; i < N; i++) {
            float t = i / 22050.0f, env = t < 0.05f ? t / 0.05f : expf(-(t - 0.05f) * 2.2f);
            pcm[i] = (short)(8000 * env * (sinf(t * 6.2832f * 110) * 0.6f + sinf(t * 6.2832f * 164.8f) * 0.4f + (t < 0.4f ? sinf(t * 6.2832f * 55) * 0.5f : 0)));
        }
        snprintf(p, sizeof p, "%s/banner.wav", dir);
        FILE* w = fopen(p, "wb");
        unsigned int fl = 16, dsz = N * 2, rsz = 36 + dsz, sr = 22050, br = 44100;
        unsigned short fmt[2] = {1, 1}, ba[2] = {2, 16};
        fwrite("RIFF", 1, 4, w); fwrite(&rsz, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w); fwrite(&fl, 4, 1, w);
        fwrite(fmt, 2, 2, w); fwrite(&sr, 4, 1, w); fwrite(&br, 4, 1, w); fwrite(ba, 2, 2, w);
        fwrite("data", 1, 4, w); fwrite(&dsz, 4, 1, w); fwrite(pcm, 2, N, w); fclose(w);
    }
    printf("icons written to %s\n", dir);
}

#include "handoff.inc"
#include "playtest.inc"
#include "music.inc"

int main(int argc, char** argv) {
    game_init();
    save_setDir("tool_saves/");
    if (argc >= 3 && !strcmp(argv[1], "--gallery")) gallery(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--shots")) shots(argv[2]);
    else if (argc >= 2 && !strcmp(argv[1], "--bench")) bench();
    else if (argc >= 2 && !strcmp(argv[1], "--bench-base")) benchBase();
    else if (argc >= 3 && !strcmp(argv[1], "--icons")) icons(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--handoff")) handoff(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--lineup")) lineup(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--zombies")) zombies(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--tutorial")) tutorialShots(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--music")) musicTool(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--duel")) duel(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--base")) base(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--playtest")) playtest(argv[2], argc >= 4 ? atoi(argv[3]) : 30);
    else if (argc >= 3 && !strcmp(argv[1], "--mapshots")) mapShots(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--settlement")) settlementShots(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--combat")) combatShots(argv[2]);
    else if (argc >= 2 && !strcmp(argv[1], "--bench-bake")) bakeBench();
    else if (argc >= 3 && !strcmp(argv[1], "--rain")) rainShots(argv[2]);
    else { printf("usage: zs_tool --gallery DIR | --shots DIR | --bench | --icons DIR\n"); return 1; }
    return 0;
}
