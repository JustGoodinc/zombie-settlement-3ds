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

static void gallery(const char* dir) {
    // characters: rows = variant x direction, columns = poses (2x scale)
    {
        int cols = P_COUNT, rows = (SURV_VARIANTS + ZOMB_VARIANTS) * 5, S = 2;
        int W = cols * 26 * S, H = rows * 26 * S;
        px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(60, 58, 54);
        Canvas c; c.init(img, W, H);
        static const int D5[5] = {2, 1, 0, 7, 6};
        for (int z = 0; z < 2; z++) for (int v = 0; v < (z ? ZOMB_VARIANTS : SURV_VARIANTS); v++) for (int d = 0; d < 5; d++) for (int p = 0; p < P_COUNT; p++) {
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
        int W = 6 * 92, H = 6 * 52; px* img = (px*)malloc(W * H * sizeof(px)); for (int i = 0; i < W * H; i++) img[i] = rgb(40, 40, 44);
        Canvas c; c.init(img, W, H);
        for (int i = 1; i < IT_COUNT; i++) { int x = ((i - 1) % 6) * 92 + 4, y = ((i - 1) / 6) * 52 + 4; gfx_blit(c, g_item[i], x, y, 2); gfx_text(c, x, y + 40, idef(i).name, pal::cream); }
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
    ui_open(PN_BASE); step(10); shot(dir, "03_settlement_base");
    g_buildingSel = B_INFIRMARY; ui_open(PN_BUILDING); step(10); shot(dir, "04_build_menu");
    ui_open(PN_SURVIVORS); step(10); shot(dir, "05_survivors");
    ui_open(PN_WORLDMAP); step(10); shot(dir, "06_world_map");
    tap(220, 206);  // Deploy -> prep
    step(5); shot(dir, "07_expedition_prep");
    G.squad[1] = 4; step(2);
    tap(70, 206);   // Deploy
    step(40); shot(dir, "08_police_arrival");
    // walk north toward the lobby doors and open them
    Input up; up.ay = -1;
    for (int i = 0; i < 70; i++) step(1, up);
    press(BTN_A); step(20);
    for (int i = 0; i < 40; i++) step(1, up);
    step(5); shot(dir, "09_police_lobby");
    // find and search the nearest container
    Unit& u = G.u[G.ctrl];
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < G.map.nprop; i++) { const Prop& p = G.map.prop[i]; if (p.container < 0) continue; float dx = p.tx * 16 + 8 - u.x, dy = p.ty * 16 + 8 - u.y; if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; } }
    if (best >= 0) { const Prop& p = G.map.prop[best]; u.x = u.px = p.tx * 16 + 8.0f; u.y = u.py = p.ty * 16 + 26.0f; u.fx = 0; u.fy = -1; }
    step(6); shot(dir, "10_container_highlight");
    press(BTN_A); step(14); shot(dir, "11_searching");
    step(40); shot(dir, "12_container_menu");
    tap(176, 148);  // Take All
    step(4); shot(dir, "13_container_taken");
    press(BTN_B); step(4);
    ui_open(PN_INVENTORY); step(8); shot(dir, "14_inventory");
    ui_open(PN_HUD);
    // combat: spawn a few zombies in front and fire
    G.nz = 0;
    for (int k = 0; k < 4; k++) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x + 30 + k * 12; z.y = z.py = u.y + 10 - k * 8; z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = (uint8_t)k; z.hx = -1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
    }
    u.fx = 1; u.fy = 0; u.dir = 0;
    Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; }
    press(BTN_Y); step(2); shot(dir, "15_combat_muzzle");
    for (int i = 0; i < 4; i++) { press(BTN_Y); step(9); }
    shot(dir, "16_combat_hits");
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
    G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); step(8); shot(dir, "30_main_menu");
}

static double ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

static void bench() {
    game_newGame();
    G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = 4;
    level_leave(); level_enter(MAP_POLICE, 0); ui_open(PN_HUD);
    G.clock = 23 * 60;   // night: flashlight + full lighting path
    Unit& u = G.u[0];
    while (G.nz < 60) {
        Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
        z.x = z.px = u.x - 160 + (G.nz % 12) * 26; z.y = z.py = u.y - 140 + (G.nz / 12) * 22;
        z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = (uint8_t)(G.nz & 3); z.hx = 0; z.hy = 1;
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
    }
    int alive = 0; for (int i = 0; i < G.nz; i++) alive += G.z[i].state != ZS_DEAD;
    printf("bench: %d zombies (%d alive at end), 3 survivors, night+flashlight, %d frames\n", 60, alive, frames);
    printf("  sim    avg %.3f ms/frame\n  render avg %.3f ms/frame\n  worst  %.3f ms\n", simT / frames, renT / frames, worst);
    printf("  memory: Game state %u KB, static frame buffers %u KB\n", (unsigned)(sizeof(Game) / 1024), (unsigned)((sizeof topB + sizeof botB) / 1024));
}

static void icons(const char* dir) {
    // 48x48 icon: zombie head silhouette over a red moon
    static px ic[48 * 48]; Canvas c; c.init(ic, 48, 48);
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) ic[y * 48 + x] = gfx_mix(rgb(20, 22, 34), rgb(60, 30, 30), y * 5);
    gfx_circle(c, 32, 14, 9, rgb(150, 50, 40), true);
    gfx_blit(c, g_portrait[0], 0, 0);
    bool f; gfx_blit(c, art_char(true, 0, 2, P_STAND, f), 8, 14, 2);
    gfx_fill(c, 0, 44, 48, 4, rgb(40, 30, 26));
    char p[512]; snprintf(p, sizeof p, "%s/icon.png", dir); gfx_writePNG(p, ic, 48, 48);
    // banner 256x128 (3DS HOME menu top screen)
    static px bn[256 * 128]; Canvas b; b.init(bn, 256, 128);
    for (int y = 0; y < 128; y++) for (int x = 0; x < 256; x++) bn[y * 256 + x] = gfx_mix(rgb(14, 16, 26), rgb(46, 36, 36), y * 2);
    for (int x = 0; x < 256; x += 16) gfx_blit(b, g_tile[TA_WALL_FRONT][(x / 16) & 3], x, 96), gfx_blit(b, g_tile[TA_ASPHALT][(x / 16) & 3], x, 112);
    for (int i = 0; i < 5; i++) gfx_blit(b, art_char(true, i % 4, 2, P_STAND, f), 20 + i * 48, 92, 1);
    gfx_text(b, 16 + 2, 18 + 2, "ZOMBIE", rgb(80, 10, 10), 4, false); gfx_text(b, 16, 18, "ZOMBIE", pal::cream, 4, false);
    gfx_text(b, 18 + 2, 56 + 2, "SETTLEMENT", rgb(80, 10, 10), 2, false); gfx_text(b, 18, 56, "SETTLEMENT", pal::red, 2, false);
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

int main(int argc, char** argv) {
    game_init();
    save_setDir("tool_saves/");
    if (argc >= 3 && !strcmp(argv[1], "--gallery")) gallery(argv[2]);
    else if (argc >= 3 && !strcmp(argv[1], "--shots")) shots(argv[2]);
    else if (argc >= 2 && !strcmp(argv[1], "--bench")) bench();
    else if (argc >= 3 && !strcmp(argv[1], "--icons")) icons(argv[2]);
    else { printf("usage: zs_tool --gallery DIR | --shots DIR | --bench | --icons DIR\n"); return 1; }
    return 0;
}
