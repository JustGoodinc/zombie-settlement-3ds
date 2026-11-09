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
    // gritty charcoal backdrop, generated once
    static px bg[BOT_W * BOT_H]; static bool made = false;
    if (!made) {
        made = true;
        for (int y = 0; y < BOT_H; y++) for (int x = 0; x < BOT_W; x++) {
            uint32_t h = hash32(x * 7 + y * 4099);
            px b = gfx_mix(rgb(30, 30, 34), rgb(22, 22, 26), y);
            if (h % 11 == 0) b = gfx_scale(b, 230);
            if (h % 97 == 0) b = gfx_mix(b, rgb(60, 56, 50), 60);
            bg[y * BOT_W + x] = b;
        }
    }
    if (c.w == BOT_W && c.h == BOT_H && c.ox == 0 && c.oy == 0) memcpy(c.p, bg, sizeof bg);
    else for (int y = 0; y < c.h && y < BOT_H; y++) for (int x = 0; x < c.w && x < BOT_W; x++) gfx_pset(c, x, y, bg[y * BOT_W + x]);
}

void ui_panel(Canvas& c, int x, int y, int w, int h, const char* title) {
    gfx_fill(c, x + 2, y + 2, w, h, rgb(10, 10, 12));               // drop shadow
    gfx_fill(c, x, y, w, h, pal::panel);
    gfx_rect(c, x, y, w, h, pal::border);
    gfx_rect(c, x + 1, y + 1, w - 2, h - 2, pal::borderLo);
    // worn corners
    gfx_pset(c, x, y, pal::borderLo); gfx_pset(c, x + w - 1, y + h - 1, pal::borderLo);
    if (title) {
        gfx_fill(c, x + 2, y + 2, w - 4, 13, pal::panel2);
        gfx_fill(c, x + 2, y + 15, w - 4, 1, pal::borderLo);
        gfx_textC(c, x + w / 2, y + 4, title, pal::cream);
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
    gfx_fill(c, x + 1, y + 1, (int)((w - 2) * v), h - 2, col);
    if (h > 3) gfx_fill(c, x + 1, y + 1, (int)((w - 2) * v), 1, gfx_mix(col, rgb(255, 255, 255), 70));
}
void ui_icon(Canvas& c, int x, int y, int id) { gfx_blit(c, g_spr[id], x, y); }

bool ui_button(Canvas& c, int x, int y, int w, int h, const char* label, int style, bool enabled) {
    bool focused = g_ui.focus == g_ui.count;
    bool pressedNow = g_ui.pressed == g_ui.count && g_ui.in.touching;
    bool act = ui_hit(x, y, w, h, enabled);
    bool sel = (style & UB_SELECTED) != 0;
    px fill = !enabled ? rgb(34, 34, 38) : (sel ? pal::goldBg : pal::panel2);
    if (pressedNow) fill = gfx_mix(fill, pal::gold, 60);
    int yo = pressedNow ? 1 : 0;
    gfx_fill(c, x, y + yo, w, h, fill);
    px border = !enabled ? rgb(60, 60, 64) : ((focused || sel || (style & UB_GOLD)) ? pal::gold : pal::border);
    gfx_rect(c, x, y + yo, w, h, border);
    if (focused && enabled) gfx_rect(c, x + 1, y + 1 + yo, w - 2, h - 2, pal::goldDk);
    if (label && *label) {
        px tc = !enabled ? pal::grey : pal::cream;
        int ty = y + (h - 8) / 2 + yo;
        if (style & UB_LEFT) gfx_text(c, x + 5, ty, label, tc);
        else gfx_textC(c, x + w / 2, ty, label, tc);
    }
    if (act) audio_play(SFX_MENU_OK, 0.8f);
    return act;
}

bool ui_tab(Canvas& c, int x, int y, int w, const char* label, bool active) {
    bool focused = g_ui.focus == g_ui.count;
    bool act = ui_hit(x, y, w, 15, true);
    gfx_fill(c, x, y, w, 15, active ? pal::goldBg : pal::panel2);
    gfx_rect(c, x, y, w, 15, active ? pal::gold : (focused ? pal::goldDk : pal::border));
    if (active) gfx_fill(c, x + 2, y + 13, w - 4, 1, pal::gold);
    gfx_textC(c, x + w / 2, y + 4, label, active ? pal::cream : pal::dim);
    if (act) audio_play(SFX_MENU_MOVE);
    return act;
}
