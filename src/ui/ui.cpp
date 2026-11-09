#include "ui.h"
#include "../core/audio.h"
#include "../core/art.h"
#include "../core/rng.h"
#include <string.h>

Ui g_ui;

void ui_resetFocus() { g_ui.focus = 0; g_ui.prevCount = 0; g_ui.pressed = -1; }

void ui_beginFrame(const Input& in) {
    Ui& u = g_ui;
    u.in = in; u.count = 0; u.consumed = false; u.navMoved = false;
    // D-pad / circle-pad focus navigation over last frame's widget rects
    uint32_t d = in.down;
    static float repeatT = 0; static int lastDir = 0;
    int dir = (d & BTN_UP) ? 1 : (d & BTN_DOWN) ? 2 : (d & BTN_LEFT) ? 3 : (d & BTN_RIGHT) ? 4 : 0;
    int heldDir = (in.ay < -0.6f) ? 1 : (in.ay > 0.6f) ? 2 : (in.ax < -0.6f) ? 3 : (in.ax > 0.6f) ? 4 : 0;
    if (!dir && heldDir) {
        if (heldDir != lastDir) { dir = heldDir; repeatT = 0.35f; }
        else { repeatT -= 1.0f / 30; if (repeatT <= 0) { dir = heldDir; repeatT = 0.15f; } }
    }
    lastDir = heldDir;
    if (u.focus >= u.prevCount) u.focus = u.prevCount ? u.prevCount - 1 : 0;
    if (dir && u.prevCount > 0) {
        const UiRect& f = u.prev[u.focus];
        int fx = f.x + f.w / 2, fy = f.y + f.h / 2, best = -1; long bestScore = 1L << 30;
        for (int i = 0; i < u.prevCount; i++) {
            if (i == u.focus || !u.prev[i].enabled) continue;
            int cx = u.prev[i].x + u.prev[i].w / 2, cy = u.prev[i].y + u.prev[i].h / 2, dx = cx - fx, dy = cy - fy;
            int along = dir == 1 ? -dy : dir == 2 ? dy : dir == 3 ? -dx : dx;
            int across = (dir <= 2) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
            if (along <= 2) continue;
            long score = (long)along + (long)across * 3;
            if (score < bestScore) { bestScore = score; best = i; }
        }
        if (best >= 0) { u.focus = best; u.navMoved = true; audio_play(SFX_MENU_MOVE, 0.6f); }
    }
}
void ui_endFrame() {
    Ui& u = g_ui;
    memcpy(u.prev, u.rects, sizeof(UiRect) * u.count);
    u.prevCount = u.count;
    if (u.focus < u.count && !u.rects[u.focus].enabled) {
        for (int i = 0; i < u.count; i++) if (u.rects[i].enabled) { u.focus = i; break; }
    }
    if (u.in.touchUp) u.pressed = -1;
}

int ui_lastId() { return g_ui.count - 1; }
bool ui_pressedB() { return (g_ui.in.down & BTN_B) != 0; }
bool ui_shoulderL() { return (g_ui.in.down & BTN_L) != 0; }
bool ui_shoulderR() { return (g_ui.in.down & BTN_R) != 0; }
float ui_ease(float t) { if (t >= 1) return 1; if (t <= 0) return 0; float k = 1 - t; return 1 - k * k * k; }

static bool inside(const Input& in, int x, int y, int w, int h) { return in.tx >= x && in.ty >= y && in.tx < x + w && in.ty < y + h; }

bool ui_hit(int x, int y, int w, int h, bool enabled) {
    Ui& u = g_ui;
    if (u.count >= 96) return false;
    int id = u.count;
    u.rects[u.count++] = UiRect{(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, enabled};
    if (!enabled || u.consumed) return false;
    if (u.in.touchDown && inside(u.in, x, y, w, h)) { u.pressed = id; u.focus = id; }
    bool act = false;
    if (u.in.touchUp && u.pressed == id && inside(u.in, x, y, w, h)) act = true;
    if ((u.in.down & BTN_A) && u.focus == id && !u.in.touching) act = true;
    if (act) u.consumed = true;
    return act;
}
bool ui_focused(int, int, int, int) { return g_ui.focus == g_ui.count; }

void ui_bg(Canvas& c) {
    // gritty charcoal backdrop with a worn metal frame, generated once
    static px bg[BOT_W * BOT_H]; static bool made = false;
    if (!made) {
        made = true;
        for (int y = 0; y < BOT_H; y++) for (int x = 0; x < BOT_W; x++) {
            uint32_t h = hash32(x * 7 + y * 4099);
            px b = gfx_mix(rgb(34, 33, 36), rgb(20, 20, 23), y);
            if (h % 11 == 0) b = gfx_scale(b, 225);
            if (h % 97 == 0) b = gfx_mix(b, rgb(70, 62, 52), 70);
            if ((x + y * 3) % 61 == 0 && h % 3 == 0) b = gfx_mix(b, rgb(10, 10, 12), 90);   // scratches
            int e = x < y ? x : y; int e2 = BOT_W - 1 - x < BOT_H - 1 - y ? BOT_W - 1 - x : BOT_H - 1 - y; if (e2 < e) e = e2;
            if (e == 0) b = rgb(12, 12, 14);
            else if (e == 1) b = rgb(78, 76, 80);
            else if (e == 2) b = rgb(46, 45, 48);
            else if (e < 8) b = gfx_scale(b, 200 + e * 7);                                         // inner vignette
            bg[y * BOT_W + x] = b;
        }
    }
    if (c.w == BOT_W && c.h == BOT_H && c.ox == 0 && c.oy == 0) memcpy(c.p, bg, sizeof bg);
    else for (int y = 0; y < c.h && y < BOT_H; y++) for (int x = 0; x < c.w && x < BOT_W; x++) gfx_pset(c, x, y, bg[y * BOT_W + x]);
}

// bevel: light top/left, dark bottom/right (Try 2 panels look stamped out of metal)
static void bevel(Canvas& c, int x, int y, int w, int h, px lo, px hi) {
    gfx_fill(c, x, y, w, 1, hi); gfx_fill(c, x, y, 1, h, hi);
    gfx_fill(c, x, y + h - 1, w, 1, lo); gfx_fill(c, x + w - 1, y, 1, h, lo);
}

void ui_panel(Canvas& c, int x, int y, int w, int h, const char* title) {
    gfx_fillA(c, x + 3, y + 3, w, h, rgb(0, 0, 0), 150);                  // drop shadow
    gfx_fill(c, x, y, w, h, pal::ink);
    gfx_fill(c, x + 1, y + 1, w - 2, h - 2, pal::panel);
    bevel(c, x + 1, y + 1, w - 2, h - 2, rgb(24, 24, 28), rgb(84, 82, 88));
    gfx_rect(c, x + 2, y + 2, w - 4, h - 4, rgb(30, 30, 34));
    for (int j = y + 3; j < y + h - 3; j += 2) gfx_fillA(c, x + 3, j, w - 6, 1, rgb(0, 0, 0), 18);   // faint brushed texture
    // rivets
    gfx_pset(c, x + 3, y + 3, rgb(110, 108, 112)); gfx_pset(c, x + w - 4, y + 3, rgb(110, 108, 112));
    gfx_pset(c, x + 3, y + h - 4, rgb(70, 68, 72)); gfx_pset(c, x + w - 4, y + h - 4, rgb(70, 68, 72));
    if (title) {
        for (int j = 0; j < 13; j++) gfx_fill(c, x + 3, y + 3 + j, w - 6, 1, gfx_mix(rgb(70, 68, 74), rgb(42, 41, 46), j * 20));
        gfx_fill(c, x + 3, y + 16, w - 6, 1, pal::ink);
        gfx_fill(c, x + 3, y + 17, w - 6, 1, rgb(66, 64, 70));
        gfx_textCB(c, x + w / 2, y + 5, title, pal::cream);
    }
}

void ui_glyph(Canvas& c, int x, int y, char b, bool hi) {
    gfx_circle(c, x + 4, y + 4, 4, hi ? pal::gold : pal::cream, true);
    gfx_circle(c, x + 4, y + 4, 4, pal::ink, false);
    char s[2] = {b, 0};
    gfx_text(c, x + 2, y + 1, s, pal::ink, 1, false);
}
void ui_prompt(Canvas& c, int x, int y, char b, const char* label) {
    ui_glyph(c, x, y, b);
    gfx_text(c, x + 12, y + 1, label, pal::cream);
}
void ui_bar(Canvas& c, int x, int y, int w, int h, float v, px col) {
    if (v < 0) v = 0; if (v > 1) v = 1;
    gfx_fill(c, x, y, w, h, pal::ink);
    gfx_fill(c, x + 1, y + 1, w - 2, h - 2, rgb(36, 34, 38));
    int fw = (int)((w - 2) * v);
    gfx_fill(c, x + 1, y + 1, fw, h - 2, col);
    if (h > 3) { gfx_fill(c, x + 1, y + 1, fw, 1, gfx_mix(col, rgb(255, 255, 255), 80)); gfx_fill(c, x + 1, y + h - 2, fw, 1, gfx_scale(col, 170)); }
    for (int i = x + 6; i < x + w - 1; i += 6) gfx_fill(c, i, y + 1, 1, h - 2, gfx_scale(i < x + 1 + fw ? col : rgb(36, 34, 38), 150));   // segment ticks
}
void ui_icon(Canvas& c, int x, int y, int id) { gfx_blit(c, g_spr[id], x, y); }

bool ui_button(Canvas& c, int x, int y, int w, int h, const char* label, int style, bool enabled) {
    bool focused = g_ui.focus == g_ui.count;
    bool pressedNow = g_ui.pressed == g_ui.count && g_ui.in.touching;
    bool act = ui_hit(x, y, w, h, enabled);
    bool sel = (style & UB_SELECTED) != 0;
    bool gold = focused || sel || (style & UB_GOLD);
    int yo = pressedNow ? 1 : 0;
    px top = !enabled ? rgb(38, 38, 42) : (sel ? rgb(84, 72, 40) : rgb(64, 63, 68));
    px bot = !enabled ? rgb(30, 30, 34) : (sel ? rgb(54, 46, 26) : rgb(42, 41, 46));
    if (pressedNow) { px t = top; top = bot; bot = t; }
    gfx_fill(c, x, y + yo, w, h, pal::ink);
    for (int j = 1; j < h - 1; j++) gfx_fill(c, x + 1, y + yo + j, w - 2, 1, gfx_mix(top, bot, j * 256 / h));
    bevel(c, x + 1, y + yo + 1, w - 2, h - 2, gfx_scale(bot, 160), gfx_mix(top, rgb(255, 255, 255), 50));
    if (gold && enabled) {
        gfx_rect(c, x, y + yo, w, h, (focused || (style & UB_GOLD)) ? pal::gold : pal::goldDk);
        if (focused) gfx_rect(c, x + 1, y + 1 + yo, w - 2, h - 2, pal::goldDk);
    }
    if (label && *label) {
        px tc = !enabled ? pal::grey : (sel ? rgb(250, 236, 196) : pal::cream);
        int ty = y + (h - 8) / 2 + yo;
        bool bold = gfx_textWB(label) <= w - 8;   // bold when it fits
        if (style & UB_LEFT) { if (bold) gfx_textB(c, x + 5, ty, label, tc); else gfx_text(c, x + 5, ty, label, tc); }
        else { if (bold) gfx_textCB(c, x + w / 2, ty, label, tc); else gfx_textC(c, x + w / 2, ty, label, tc); }
    }
    if (act) audio_play(SFX_MENU_OK, 0.8f);
    return act;
}

bool ui_tab(Canvas& c, int x, int y, int w, const char* label, bool active) {
    bool focused = g_ui.focus == g_ui.count;
    bool act = ui_hit(x, y, w, 15, true);
    gfx_fill(c, x, y, w, 15, pal::ink);
    px top = active ? rgb(110, 92, 44) : rgb(56, 55, 60), bot = active ? rgb(70, 58, 28) : rgb(38, 37, 42);
    for (int j = 1; j < 14; j++) gfx_fill(c, x + 1, y + j, w - 2, 1, gfx_mix(top, bot, j * 18));
    gfx_rect(c, x, y, w, 15, active ? pal::gold : (focused ? pal::goldDk : rgb(80, 78, 84)));
    if (active) gfx_fill(c, x + 1, y + 1, w - 2, 1, rgb(250, 220, 120));
    if (gfx_textWB(label) <= w - 4) gfx_textCB(c, x + w / 2, y + 4, label, active ? rgb(255, 244, 210) : pal::dim);
    else gfx_textC(c, x + w / 2, y + 4, label, active ? rgb(255, 244, 210) : pal::dim);
    if (act) audio_play(SFX_MENU_MOVE);
    return act;
}
