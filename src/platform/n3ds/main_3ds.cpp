// Nintendo 3DS platform layer (libctru). The shared renderer draws RGB565 into
// row-major buffers; they are copied into the rotated (column-major) LCD framebuffers.
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../../gameplay/game.h"
#include "../../core/audio.h"
#include "../../core/art.h"

extern const char* g_controlsText;
extern float g_renderAlpha;

static px topBuf[TOP_W * TOP_H], botBuf[BOT_W * BOT_H];

// The LCDs are rotated: screen column x is a run of h pixels, bottom row first. Copied eight rows at a time, so each
// column gets one 16-byte write instead of eight scattered 2-byte ones, and the eight source rows are read in order.
static void rotateInto(u16* fb, const px* src, int w, int h) {   // h a multiple of 8
    for (int y0 = 0; y0 < h; y0 += 8) {
        const px* r[8]; for (int k = 0; k < 8; k++) r[k] = src + (h - 1 - (y0 + k)) * w;
        u32* d = (u32*)(fb + y0);
        for (int x = 0; x < w; x++, d += h / 2) {
            d[0] = r[0][x] | (u32)r[1][x] << 16; d[1] = r[2][x] | (u32)r[3][x] << 16;
            d[2] = r[4][x] | (u32)r[5][x] << 16; d[3] = r[6][x] | (u32)r[7][x] << 16;
        }
    }
}
static void blitRotated(gfxScreen_t screen, const px* src, int w, int h) {
    u16 fw, fh;
    rotateInto((u16*)gfxGetFramebuffer(screen, GFX_LEFT, &fw, &fh), src, w, h);
}
// bench check: the fast copy gives exactly what the plain one-pixel-at-a-time copy gives
static bool rotateCheck() {
    static u16 a[TOP_W * TOP_H], b[TOP_W * TOP_H];
    for (int i = 0; i < TOP_W * TOP_H; i++) topBuf[i] = (px)(i * 2654435761u >> 7);
    rotateInto(a, topBuf, TOP_W, TOP_H);
    for (int x = 0; x < TOP_W; x++) for (int y = 0; y < TOP_H; y++) b[x * TOP_H + y] = topBuf[(TOP_H - 1 - y) * TOP_W + x];
    return !memcmp(a, b, sizeof a);
}

// NDSP audio: 4 rotating wave buffers
enum { ABUF = 1024 };
static ndspWaveBuf wbuf[4];
static s16* adata;
static bool audioOk;
static void audioInit() {
    if (R_FAILED(ndspInit())) { audioOk = false; return; }   // needs dspfirm.cdc on real hardware
    audioOk = true;
    ndspSetOutputMode(NDSP_OUTPUT_MONO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, AUDIO_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {1.0f, 1.0f}; ndspChnSetMix(0, mix);
    adata = (s16*)linearAlloc(ABUF * 4 * sizeof(s16));
    memset(wbuf, 0, sizeof wbuf);
    for (int i = 0; i < 4; i++) {
        wbuf[i].data_vaddr = adata + i * ABUF; wbuf[i].nsamples = ABUF;
        audio_mix(adata + i * ABUF, ABUF);
        DSP_FlushDataCache(adata + i * ABUF, ABUF * sizeof(s16));
        ndspChnWaveBufAdd(0, &wbuf[i]);
    }
}
static void audioPump() {
    if (!audioOk) return;
    for (int i = 0; i < 4; i++) if (wbuf[i].status == NDSP_WBUF_DONE) {
        audio_mix(adata + i * ABUF, ABUF);
        DSP_FlushDataCache(adata + i * ABUF, ABUF * sizeof(s16));
        ndspChnWaveBufAdd(0, &wbuf[i]);
    }
}

// Draws both screens and hands them to the display. With Fast graphics, while you play (the HUD), the touch screen is
// redrawn every other frame (15 fps), and at once for anything it listens to: the stylus, START, SELECT, and A or B
// (dialogs). The HUD ignores the other buttons, which belong to the game, so no press is ever late or lost. On the
// frames between, its old picture simply stays up (that screen is not swapped). Menus are always redrawn.
static void drawFrame(const Input& in) {
    static unsigned frame = 0; frame++;
    const uint32_t HEARD = BTN_START | BTN_SELECT | BTN_A | BTN_B;
    g_skipBottom = gfx_fast() && G.panel == PN_HUD && (frame & 1) && !((in.down | in.up) & HEARD) && !in.touching && !in.touchDown && !in.touchUp;
    Canvas top, bot; top.init(topBuf, TOP_W, TOP_H); bot.init(botBuf, BOT_W, BOT_H);
    game_render(top, bot, g_renderAlpha);
    blitRotated(GFX_TOP, topBuf, TOP_W, TOP_H);
    gfxScreenSwapBuffers(GFX_TOP, false);
    if (!g_skipBottom) { blitRotated(GFX_BOTTOM, botBuf, BOT_W, BOT_H); gfxScreenSwapBuffers(GFX_BOTTOM, false); }
}

// ---- frame profiler on the console itself. If sdmc:/3ds/ZombieSettlement/bench exists, the game plays three fixed
// scenes instead of starting, times every stage of every frame with the system tick, writes perf.txt next to it and
// quits. This is how the 3DS cost is measured rather than guessed from the PC.
extern double (*g_profNow)(); extern double g_prof[8];
static double tickMs() { return svcGetSystemTick() / (double)CPU_TICKS_PER_MSEC; }
static void benchScene(FILE* f, const char* name, int frames, bool fight) {
    static const char* STAGE[8] = {"light", "world", "shade", "grade", "pools", "zoom", "hud", "bottom"};
    double sum[8] = {0}, simT = 0, blitT = 0, audT = 0, tot = 0, worst = 0, wb[11] = {0}; static double each[600]; int n = 0, wi = 0;
    for (int i = 0; i < frames + 30; i++) {
        Input in; in.ax = (i / 60) % 2 ? 1.0f : -1.0f;
        if (fight && i % 6 == 0) { in.down = BTN_Y; in.held = BTN_Y; }
        if (fight) { Survivor& s = G.st.surv[G.u[G.ctrl].sv]; s.hp = 100; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; } }
        for (double& p : g_prof) p = 0;
        double a = tickMs();
        game_frame(in, 1.0f / 30);
        double b = tickMs();
        drawFrame(in);
        double d = tickMs();
        audio_mix(adata ? adata : (s16*)topBuf, AUDIO_RATE / 30);   // one frame's worth of sound
        double e = tickMs();
        if (i < 30) continue;   // let the scene settle first
        double rend = 0; for (int k = 0; k < 8; k++) { sum[k] += g_prof[k]; rend += g_prof[k]; }
        simT += b - a; blitT += (d - b) - rend; audT += e - d; tot += e - a;   // blit: what drawing took beyond the stages
        if (e - a > worst) { worst = e - a; wi = i - 30; wb[0] = b - a; for (int k = 0; k < 8; k++) wb[1 + k] = g_prof[k]; wb[9] = (d - b) - rend; wb[10] = e - d; }
        if (n < 600) each[n++] = e - a;
        gfxFlushBuffers();   // never waits for the display
    }
    int over33 = 0, over16 = 0; for (int k = 0; k < n; k++) { over33 += each[k] > 33.3; over16 += each[k] > 16.7; }
    fprintf(f, "%-22s total %6.2f ms avg, worst %6.2f | sim %5.2f", name, tot / frames, worst, simT / frames);
    for (int k = 0; k < 8; k++) fprintf(f, " %s %5.2f", STAGE[k], sum[k] / frames);
    fprintf(f, " blit %5.2f audio %5.2f | frames over 16.7 ms: %d%%, over 33.3 ms: %d%%\n", blitT / frames, audT / frames, over16 * 100 / n, over33 * 100 / n);
    fprintf(f, "    worst frame (#%d): sim %.2f", wi, wb[0]);
    for (int k = 0; k < 8; k++) fprintf(f, " %s %.2f", STAGE[k], wb[1 + k]);
    fprintf(f, " blit %.2f audio %.2f\n", wb[9], wb[10]);
}
static void runBench() {
    FILE* f = fopen("sdmc:/3ds/ZombieSettlement/perf.txt", "w"); if (!f) return;
    bool n3ds = false; APT_CheckNew3DS(&n3ds);
    fprintf(f, "Staying Alive frame profile. Console: %s. Times are CPU work per frame (ms); 30 fps needs under 33.3.\n", n3ds ? "New 3DS" : "Old 3DS");
    fprintf(f, "screen copy check: %s\n", rotateCheck() ? "exact" : "WRONG");
    for (int pass = 0; pass < 2; pass++) {   // graphics Full, then Fast
    G.set.gfx = pass ? 2 : 1;
    fprintf(f, "-- Graphics: %s\n", pass ? "Fast" : "Full");
    mkdir("sdmc:/3ds/ZombieSettlement/bench_saves", 0777); save_setDir("sdmc:/3ds/ZombieSettlement/bench_saves/");   // never touch the real saves
    g_profNow = tickMs;
    G.set.tutorialDone = true;
    game_newGame(); ui_open(PN_HUD);
    G.clock = 11 * 60; benchScene(f, "settlement day", 240, false);
    G.clock = 23 * 60; benchScene(f, "settlement night", 240, false);
    G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = 4;
    { double t = tickMs(); level_leave(); level_enter(MAP_POLICE, 0); ui_open(PN_HUD); fprintf(f, "loading the police station: %.0f ms\n", tickMs() - t); }
    G.clock = 11 * 60; benchScene(f, "police day", 240, false);
    G.clock = 23 * 60;
    Unit& u = G.u[G.ctrl];
    while (G.nz < 40) {   // a big fight at night: 40 zombies closing in, the flashlight on, shooting
        Zombie& z = G.z[G.nz++]; z = Zombie{};
        z.x = z.px = u.x - 160 + (G.nz % 10) * 32; z.y = z.py = u.y - 120 + (G.nz / 10) * 30;
        z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 1; z.variant = zombie_look(G.map.id, false, G.nz * 2654435761u); z.hx = 0; z.hy = 1;
        z.anim = Animator(); z.anim.play(AnimState::ZWalk, true); art_zombie_warm(z.variant);   // as level loading does
    }
    benchScene(f, "police night fight", 240, true);
    }
    g_profNow = nullptr;
    fclose(f);
}

int main(int, char**) {
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    osSetSpeedupEnable(true);   // New 3DS: 804 MHz if available (optional; O3DS is the baseline)
    { bool n3 = false; APT_CheckNew3DS(&n3); g_lowEnd = !n3; }   // Graphics: Auto means Fast on an Old 3DS
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ZombieSettlement", 0777);
    save_setDir("sdmc:/3ds/ZombieSettlement/");
    g_controlsText =
        "Circle Pad: move and face\nY: fire   X: reload   A: interact/search\nB: hold sprint, tap dodge\nL: lock facing   R: flashlight\n"
        "D-Pad Left/Right: weapons  Up: heal  Down: squad orders\nSELECT: inventory   START: pause\nTouchscreen: menus, drag items with the stylus";
    game_init();
    settings_load();
    audioInit();
    if (FILE* b = fopen("sdmc:/3ds/ZombieSettlement/bench", "r")) {
        fclose(b); runBench();
        if (audioOk) { ndspExit(); linearFree(adata); }
        gfxExit(); return 0;
    }
    Settings lastSet = G.set; int lastSlot = G.saveSlot;
    u64 last = osGetTime();
    float fpsAcc = 0; int fpsN = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld(), up = hidKeysUp();
        Input in;
        auto map = [](u32 k) {
            uint32_t b = 0;
            if (k & KEY_A) b |= BTN_A; if (k & KEY_B) b |= BTN_B; if (k & KEY_X) b |= BTN_X; if (k & KEY_Y) b |= BTN_Y;
            if (k & KEY_L) b |= BTN_L; if (k & KEY_R) b |= BTN_R; if (k & KEY_START) b |= BTN_START; if (k & KEY_SELECT) b |= BTN_SELECT;
            if (k & KEY_DUP) b |= BTN_UP; if (k & KEY_DDOWN) b |= BTN_DOWN; if (k & KEY_DLEFT) b |= BTN_LEFT; if (k & KEY_DRIGHT) b |= BTN_RIGHT;
            return b;
        };
        in.down = map(down); in.held = map(held); in.up = map(up);
        circlePosition cp; hidCircleRead(&cp);
        in.ax = cp.dx / 156.0f; in.ay = -cp.dy / 156.0f;
        if (in.ax > 1) in.ax = 1; if (in.ax < -1) in.ax = -1; if (in.ay > 1) in.ay = 1; if (in.ay < -1) in.ay = -1;
        in.touching = (held & KEY_TOUCH) != 0; in.touchDown = (down & KEY_TOUCH) != 0; in.touchUp = (up & KEY_TOUCH) != 0;
        static int lastTx, lastTy;
        if (in.touching) { touchPosition tp; hidTouchRead(&tp); lastTx = tp.px; lastTy = tp.py; }
        in.tx = lastTx; in.ty = lastTy;

        u64 now = osGetTime();
        float dt = (now - last) / 1000.0f; last = now;
        fpsAcc += dt; fpsN++; if (fpsAcc >= 0.5f) { G.fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0; }

        u64 t0 = svcGetSystemTick();
        game_frame(in, dt);
        drawFrame(in);
        G.frameMs = (svcGetSystemTick() - t0) / (float)CPU_TICKS_PER_MSEC;   // CPU work per frame (excludes vsync wait)
        if (memcmp(&lastSet, &G.set, sizeof lastSet) || lastSlot != G.saveSlot) { lastSet = G.set; lastSlot = G.saveSlot; settings_save(); }
        audioPump();
        gfxFlushBuffers();
        gspWaitForVBlank();
    }
    if (audioOk) { ndspExit(); linearFree(adata); }
    gfxExit();
    return 0;
}
