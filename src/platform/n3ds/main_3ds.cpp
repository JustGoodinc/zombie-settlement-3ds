// Nintendo 3DS platform layer (libctru). The shared renderer draws RGB565 into
// row-major buffers; they are copied into the rotated (column-major) LCD framebuffers.
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../../gameplay/game.h"
#include "../../core/audio.h"

extern const char* g_controlsText;
extern float g_renderAlpha;

static px topBuf[TOP_W * TOP_H], botBuf[BOT_W * BOT_H];

static void blitRotated(gfxScreen_t screen, const px* src, int w, int h) {
    u16 fw, fh;
    u16* fb = (u16*)gfxGetFramebuffer(screen, GFX_LEFT, &fw, &fh);   // fw = 240 (height), fh = width
    for (int x = 0; x < w; x++) {
        u16* col = fb + x * h;
        const px* s = src + (h - 1) * w + x;
        for (int y = 0; y < h; y++) { col[y] = *s; s -= w; }
    }
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

int main(int, char**) {
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
    osSetSpeedupEnable(true);   // New 3DS: 804 MHz if available (optional; O3DS is the baseline)
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ZombieSettlement", 0777);
    save_setDir("sdmc:/3ds/ZombieSettlement/");
    g_controlsText =
        "Circle Pad: move and face\nY: fire   X: reload   A: interact/search\nB: hold sprint, tap dodge\nL: lock facing   R: flashlight\n"
        "D-Pad Left/Right: weapons  Up: heal  Down: squad orders\nSELECT: inventory   START: pause\nTouchscreen: menus, drag items with the stylus";
    game_init();
    audioInit();
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
        Canvas top, bot; top.init(topBuf, TOP_W, TOP_H); bot.init(botBuf, BOT_W, BOT_H);
        game_render(top, bot, g_renderAlpha);
        blitRotated(GFX_TOP, topBuf, TOP_W, TOP_H);
        blitRotated(GFX_BOTTOM, botBuf, BOT_W, BOT_H);
        G.frameMs = (svcGetSystemTick() - t0) / (float)CPU_TICKS_PER_MSEC;   // CPU work per frame (excludes vsync wait)
        audioPump();
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    if (audioOk) { ndspExit(); linearFree(adata); }
    gfxExit();
    return 0;
}
