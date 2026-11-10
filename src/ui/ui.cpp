#include "ui.h"
#include "../core/audio.h"
#include "../core/art.h"
#include "../core/rng.h"
#include <string.h>

Ui g_ui;

void ui_resetFocus() { g_ui.focus = 0; g_ui.prevCount = 0; g_ui.pressed = -1; }

void ui_beginFrame(const Input& in) {
    Ui& u = g_ui;
    u.in = in; u.count = 0; u.consumed = false; u.navMoved = false; u.navEdge = 0;
    u.navLock = u.navLockNext; u.navLockNext = false;
    // D-pad and circle pad both navigate; holding either repeats (0.3 s delay, then every 0.1 s)
    uint32_t d = in.down, h = in.held | in.down;
    static float repeatT = 0; static int lastDir = 0;
    int dir = (d & BTN_UP) ? 1 : (d & BTN_DOWN) ? 2 : (d & BTN_LEFT) ? 3 : (d & BTN_RIGHT) ? 4 : 0;
    int heldDir = (h & BTN_UP) ? 1 : (h & BTN_DOWN) ? 2 : (h & BTN_LEFT) ? 3 : (h & BTN_RIGHT) ? 4 : 0;
    if (!heldDir) {
        float ax = in.ax < 0 ? -in.ax : in.ax, ay = in.ay < 0 ? -in.ay : in.ay;
        if (ay > 0.5f && ay >= ax) heldDir = in.ay < 0 ? 1 : 2;
        else if (ax > 0.5f) heldDir = in.ax < 0 ? 3 : 4;
    }
    if (dir) repeatT = 0.3f;
    else if (heldDir) {
        if (heldDir != lastDir) { dir = heldDir; repeatT = 0.3f; }
        else { repeatT -= 1.0f / 30; if (repeatT <= 0) { dir = heldDir; repeatT = 0.1f; } }
    }
    lastDir = heldDir;
    u.navDir = dir;
    if (u.focus >= u.prevCount) u.focus = u.prevCount ? u.prevCount - 1 : 0;
    if (dir && u.prevCount > 0 && !u.navLock) {
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
        else u.navEdge = dir;   // nothing further that way (panels can hand focus back to a grid)
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
    if ((u.in.down & BTN_A) && u.focus == id && !u.in.touching && !u.navLock) act = true;
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

// ---- button glyphs: 3DS buttons, keyboard keycaps (with the player's own bindings) or Xbox pad
int g_inputStyle = 0;                 // 0 = 3DS, 1 = keyboard, 2 = Xbox pad (set by the platform layer)
const char* g_keyName[12] = {"A", "B", "X", "Y", "L", "R", "START", "SELECT", "^", "v", "<", ">"};
const char* g_stickName = "WASD";

static int bitIndex(uint32_t b) { for (int i = 0; i < 12; i++) if (b == (1u << i)) return i; return -1; }
static const char* xboxName(uint32_t b) {
    switch (b) { case BTN_A: return "A"; case BTN_B: return "B"; case BTN_Y: return "X"; case BTN_X: return "Y";
                 case BTN_L: return "LB"; case BTN_R: return "RB"; case BTN_START: return "Menu"; case BTN_SELECT: return "View"; }
    return "?";
}
static void dpadIcon(Canvas& c, int x, int y, uint32_t dir) {
    gfx_fill(c, x + 3, y, 5, 11, pal::ink); gfx_fill(c, x, y + 3, 11, 5, pal::ink);
    gfx_fill(c, x + 4, y + 1, 3, 9, rgb(150, 148, 152)); gfx_fill(c, x + 1, y + 4, 9, 3, rgb(150, 148, 152));
    px h = pal::gold;
    if (dir == BTN_UP) gfx_fill(c, x + 4, y + 1, 3, 3, h);
    if (dir == BTN_DOWN) gfx_fill(c, x + 4, y + 7, 3, 3, h);
    if (dir == BTN_LEFT) gfx_fill(c, x + 1, y + 4, 3, 3, h);
    if (dir == BTN_RIGHT) gfx_fill(c, x + 7, y + 4, 3, 3, h);
    if (dir == (BTN_LEFT | BTN_RIGHT)) { gfx_fill(c, x + 1, y + 4, 3, 3, h); gfx_fill(c, x + 7, y + 4, 3, 3, h); }
}
static int keycap(Canvas& c, int x, int y, const char* label, bool draw) {
    int w = gfx_textW(label) + 7; if (w < 11) w = 11;
    if (!draw) return w;
    gfx_fill(c, x, y, w, 12, pal::ink);
    gfx_fill(c, x + 1, y, w - 2, 10, rgb(226, 220, 206)); gfx_fill(c, x + 1, y + 10, w - 2, 1, rgb(150, 144, 132));
    gfx_text(c, x + (w - gfx_textW(label)) / 2, y + 1, label, pal::ink, 1, false);
    return w;
}
static int pill(Canvas& c, int x, int y, const char* label, px fill, bool draw) {
    int w = gfx_textW(label) + 8;
    if (!draw) return w;
    gfx_fill(c, x + 1, y, w - 2, 11, pal::ink); gfx_fill(c, x, y + 1, w, 9, pal::ink);
    gfx_fill(c, x + 1, y + 1, w - 2, 9, fill);
    gfx_text(c, x + 4, y + 2, label, rgb(240, 236, 226), 1, false);
    return w;
}
static int face(Canvas& c, int x, int y, const char* letter, px col, bool draw) {
    if (!draw) return 11;
    gfx_circle(c, x + 5, y + 5, 5, col, true);
    gfx_circle(c, x + 5, y + 5, 5, pal::ink, false);
    gfx_pset(c, x + 3, y + 2, gfx_mix(col, rgb(255, 255, 255), 140));
    gfx_text(c, x + 3, y + 2, letter, rgb(250, 248, 240), 1, false);
    return 11;
}

static int btnGlyph(Canvas& c, int x, int y, uint32_t b, bool draw) {
    if (b == BTN_STICK) {
        if (g_inputStyle == 1) return keycap(c, x, y, g_stickName, draw);
        if (draw) { gfx_circle(c, x + 5, y + 5, 5, pal::ink, true); gfx_circle(c, x + 5, y + 5, 4, rgb(90, 90, 96), true); gfx_circle(c, x + 5, y + 5, 2, rgb(180, 178, 182), true); }
        return 11;
    }
    if (b == BTN_TOUCH) return g_inputStyle == 1 ? keycap(c, x, y, "Mouse", draw) : pill(c, x, y, "Touch", rgb(70, 110, 150), draw);
    if (b == BTN_DPAD_LR || b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT) {
        if (g_inputStyle == 1 && b != BTN_DPAD_LR) return keycap(c, x, y, g_keyName[bitIndex(b)], draw);
        if (g_inputStyle == 1) { int w = keycap(c, x, y, g_keyName[10], draw); return w + 2 + keycap(c, x + w + 2, y, g_keyName[11], draw); }
        if (draw) dpadIcon(c, x, y, b == BTN_DPAD_LR ? (BTN_LEFT | BTN_RIGHT) : b);
        return 11;
    }
    if (g_inputStyle == 1) { int i = bitIndex(b); return keycap(c, x, y, i >= 0 ? g_keyName[i] : "?", draw); }
    if (g_inputStyle == 2) {
        const char* n = xboxName(b);
        if (b == BTN_A) return face(c, x, y, n, rgb(70, 160, 60), draw);
        if (b == BTN_B) return face(c, x, y, n, rgb(190, 50, 46), draw);
        if (b == BTN_Y) return face(c, x, y, n, rgb(50, 100, 200), draw);
        if (b == BTN_X) return face(c, x, y, n, rgb(210, 170, 30), draw);
        return pill(c, x, y, n, rgb(70, 70, 76), draw);
    }
    // Nintendo 3DS colours: A red, B yellow, X blue, Y green
    switch (b) {
    case BTN_A: return face(c, x, y, "A", rgb(196, 56, 48), draw);
    case BTN_B: return face(c, x, y, "B", rgb(206, 166, 36), draw);
    case BTN_X: return face(c, x, y, "X", rgb(52, 100, 196), draw);
    case BTN_Y: return face(c, x, y, "Y", rgb(56, 150, 70), draw);
    case BTN_L: return pill(c, x, y, "L", rgb(80, 80, 88), draw);
    case BTN_R: return pill(c, x, y, "R", rgb(80, 80, 88), draw);
    case BTN_START: return pill(c, x, y, "START", rgb(80, 80, 88), draw);
    case BTN_SELECT: return pill(c, x, y, "SELECT", rgb(80, 80, 88), draw);
    }
    return 0;
}
int ui_btn(Canvas& c, int x, int y, uint32_t b) { return btnGlyph(c, x, y, b, true); }
int ui_btnW(uint32_t b) { Canvas dummy; return btnGlyph(dummy, 0, 0, b, false); }

static uint32_t charBtn(char b) {
    switch (b) { case 'A': return BTN_A; case 'B': return BTN_B; case 'X': return BTN_X; case 'Y': return BTN_Y; case 'L': return BTN_L; case 'R': return BTN_R; }
    return BTN_A;
}
void ui_glyph(Canvas& c, int x, int y, char b, bool) { ui_btn(c, x, y, charBtn(b)); }
int ui_promptW(char b, const char* label) { return ui_btnW(charBtn(b)) + 4 + gfx_textW(label); }
void ui_prompt(Canvas& c, int x, int y, char b, const char* label) {
    int w = ui_btn(c, x, y, charBtn(b));
    gfx_text(c, x + w + 4, y + 2, label, pal::cream);
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
    bool focused = g_ui.focus == g_ui.count && !g_ui.navLock;
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
