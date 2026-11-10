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

// C material kit: charcoal woven cloth, tan dashed stitching, leather corners, wood actions, gold focus
namespace cloth {
// greys sit exactly on RGB565 levels (G = 2R); scaling dark greys rounds red/blue down first and tints them green
constexpr px base = rgb(32, 36, 40), stitch = rgb(180, 153, 123), stitchDk = rgb(150, 128, 102);
constexpr px leather = rgb(146, 96, 54), leatherDk = rgb(74, 52, 32), brass = rgb(214, 176, 92);
}
static px step(px c, int n = 1) {   // n whole 565 steps darker, hue kept
    int r = (c >> 11) - n, g = ((c >> 5) & 63) - 2 * n, b = (c & 31) - n;
    return (px)(((r < 0 ? 0 : r) << 11) | ((g < 0 ? 0 : g) << 5) | (b < 0 ? 0 : b));
}
static px weave(px b, int x, int y) {   // fine over-under weave, flat enough to read text over
    return (((x >> 1) + (y >> 1)) & 1) && ((x + y) & 1) ? step(b) : b;
}
// dashed stitch ring (3 on, 2 off) along a rect
static void stitchRect(Canvas& c, int x, int y, int w, int h, px col) {
    for (int i = 0; i < w; i++) if (i % 5 < 3) { gfx_pset(c, x + i, y, col); gfx_pset(c, x + i, y + h - 1, col); }
    for (int j = 0; j < h; j++) if (j % 5 < 3) { gfx_pset(c, x, y + j, col); gfx_pset(c, x + w - 1, y + j, col); }
}

void ui_bg(Canvas& c) {
    // charcoal cloth with a stitched hem, generated once
    static px bg[BOT_W * BOT_H]; static bool made = false;
    if (!made) {
        made = true;
        for (int y = 0; y < BOT_H; y++) for (int x = 0; x < BOT_W; x++) {
            uint32_t h = hash32(x * 7 + y * 4099);
            px b = weave((int)(h % 480) < y ? step(cloth::base) : cloth::base, x, y);   // dithered darkening towards the bottom
            if (h % 61 == 0) b = step(b);                                     // slubs in the cloth
            int e = x < y ? x : y; int e2 = BOT_W - 1 - x < BOT_H - 1 - y ? BOT_W - 1 - x : BOT_H - 1 - y; if (e2 < e) e = e2;
            if (e == 0) b = rgb(8, 8, 8);
            else if (e < 5) b = step(b, e < 3 ? 2 : 1);                       // rolled hem
            bg[y * BOT_W + x] = b;
        }
        Canvas t; t.init(bg, BOT_W, BOT_H);
        stitchRect(t, 1, 1, BOT_W - 2, BOT_H - 2, cloth::stitch);
        for (int x = 6; x < BOT_W - 6; x++) if (x % 5 < 3) gfx_pset(t, x, 223, cloth::stitchDk);   // seam above the prompt footer
    }
    if (c.w == BOT_W && c.h == BOT_H && c.ox == 0 && c.oy == 0) memcpy(c.p, bg, sizeof bg);
    else for (int y = 0; y < c.h && y < BOT_H; y++) for (int x = 0; x < c.w && x < BOT_W; x++) gfx_pset(c, x, y, bg[y * BOT_W + x]);
}


// leather corner patches with a brass rivet, sewn over the hem of the bottom screen
void ui_frame(Canvas& c) {
    const int N = 10;
    for (int k = 0; k < 4; k++) {
        bool r = k & 1, b = k & 2;
        for (int j = 0; j < N; j++) for (int i = 0; i < N - j; i++) {
            int x = r ? BOT_W - 1 - i : i, y = b ? BOT_H - 1 - j : j;
            px col = (i + j == N - 1) ? rgb(10, 12, 14) : (i + j == N - 2) ? cloth::leatherDk : (i + j == N - 4 && (i & 1)) ? cloth::stitch : gfx_scale(cloth::leather, (i * 3 + j) % 4 ? 256 : 226);
            gfx_pset(c, x, y, col);
        }
        int rx = r ? BOT_W - 4 : 3, ry = b ? BOT_H - 4 : 3;
        gfx_pset(c, rx, ry, cloth::brass); gfx_pset(c, rx + (r ? -1 : 1), ry + (b ? -1 : 1), cloth::leatherDk);
    }
}

void ui_strap(Canvas& c, int x, int y, int w) {   // 14 px tall: canvas band rising to the right, stitched edges, leather keeper, brass stud
    const px olive = rgb(96, 108, 56), oliveLo = rgb(64, 72, 36), oliveHi = rgb(128, 140, 76);
    for (int i = 0; i < w; i++) {
        int lift = i < w / 3 ? 0 : (i - w / 3) * 3 / (w - w / 3 + 1);   // the flap rises towards the buckle side
        int top = y + 3 - lift, bot = y + 13;
        gfx_pset(c, x + i, top, pal::ink); gfx_pset(c, x + i, bot, pal::ink);
        for (int yy = top + 1; yy < bot; yy++) {
            px col = yy == top + 1 ? oliveHi : yy >= bot - 2 ? oliveLo : (((x + i + yy) & 3) == 0 ? oliveLo : olive);
            gfx_pset(c, x + i, yy, col);
        }
        if (i % 5 < 3 && i > 2 && i < w - 3) { gfx_pset(c, x + i, top + 2, cloth::stitch); gfx_pset(c, x + i, bot - 2, gfx_mix(olive, cloth::stitch, 120)); }
    }
    int lx = x + w * 3 / 4;
    gfx_fill(c, lx, y, 9, 14, pal::ink); gfx_fill(c, lx + 1, y + 1, 7, 12, cloth::leather);
    gfx_fill(c, lx + 1, y + 1, 7, 1, gfx_mix(cloth::leather, rgb(255, 230, 190), 80)); gfx_fill(c, lx + 1, y + 12, 7, 1, cloth::leatherDk);
    gfx_fill(c, lx + 6, y + 2, 1, 10, cloth::leatherDk);
    gfx_circle(c, lx - 8, y + 8, 2, pal::ink, true); gfx_pset(c, lx - 8, y + 8, cloth::brass); gfx_pset(c, lx - 9, y + 7, rgb(250, 220, 140));
}

// dark data field: flat charcoal well with a slate rim, so text, bars and cells read cleanly
void ui_panel(Canvas& c, int x, int y, int w, int h, const char* title) {
    gfx_fillA(c, x + 2, y + 2, w, h, rgb(0, 0, 0), 120);                  // drop shadow
    gfx_fill(c, x, y, w, h, pal::ink);
    gfx_fill(c, x + 1, y + 1, w - 2, h - 2, rgb(24, 28, 32));
    gfx_rect(c, x + 1, y + 1, w - 2, h - 2, rgb(72, 80, 88));
    gfx_fill(c, x + 2, y + 2, w - 4, 1, rgb(16, 20, 24));                  // inset shadow under the rim
    gfx_pset(c, x + 1, y + 1, pal::ink); gfx_pset(c, x + w - 2, y + 1, pal::ink);   // rounded corners
    gfx_pset(c, x + 1, y + h - 2, pal::ink); gfx_pset(c, x + w - 2, y + h - 2, pal::ink);
    if (title) {   // slate header strip (kit: faded civic blue)
        for (int j = 0; j < 13; j++) gfx_fill(c, x + 2, y + 3 + j, w - 4, 1, gfx_mix(rgb(70, 90, 104), rgb(40, 52, 62), j * 20));
        gfx_fill(c, x + 2, y + 3, w - 4, 1, rgb(110, 128, 138));
        gfx_fill(c, x + 2, y + 16, w - 4, 1, pal::ink);
        gfx_fill(c, x + 2, y + 17, w - 4, 1, rgb(60, 70, 76));
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
    // kit: rails are stitched cloth, actions are muted wood, the active choice is olive-gold, disabled is flat charcoal
    bool clothBtn = (style & UB_RAIL) && !sel;
    px top = !enabled ? rgb(40, 44, 48) : sel ? rgb(112, 80, 40) : clothBtn ? rgb(48, 52, 56) : rgb(110, 80, 52);
    px bot = !enabled ? rgb(32, 36, 40) : sel ? rgb(72, 50, 26) : clothBtn ? rgb(32, 36, 40) : rgb(74, 52, 33);
    if (pressedNow) { px t = top; top = bot; bot = t; }
    gfx_fill(c, x, y + yo, w, h, pal::ink);
    for (int j = 1; j < h - 1; j++) {
        px row = gfx_mix(top, bot, j * 256 / h);
        if (enabled && !clothBtn && !sel) {   // wood grain: a few long darker streaks, a plank seam on tall buttons
            uint32_t g = hash32(x * 13 + j * 977);
            if (g % 5 == 0) row = gfx_scale(row, 222);
            if (h > 24 && j == h / 2) row = gfx_scale(row, 170);
        }
        gfx_fill(c, x + 1, y + yo + j, w - 2, 1, row);
        if (clothBtn && enabled) for (int i = 1; i < w - 1; i++) if (((x + i + j) & 1) == 0) gfx_pset(c, x + i, y + yo + j, step(row));
    }
    if (enabled) {
        gfx_fill(c, x + 1, y + yo + 1, w - 2, 1, gfx_mix(top, rgb(255, 240, 210), 60));   // lit top edge
        gfx_fill(c, x + 1, y + yo + h - 2, w - 2, 1, gfx_scale(bot, 150));
        if (clothBtn && w > 8 && h > 8) stitchRect(c, x + 2, y + yo + 2, w - 4, h - 4, cloth::stitchDk);
    } else gfx_rect(c, x + 1, y + yo + 1, w - 2, h - 2, rgb(56, 60, 64));
    gfx_pset(c, x, y + yo, rgb(0, 0, 0)); gfx_pset(c, x + w - 1, y + yo, rgb(0, 0, 0));          // rounded corners
    gfx_pset(c, x, y + yo + h - 1, rgb(0, 0, 0)); gfx_pset(c, x + w - 1, y + yo + h - 1, rgb(0, 0, 0));
    if (gold && enabled) {
        gfx_rect(c, x, y + yo, w, h, (focused || (style & UB_GOLD)) ? pal::gold : pal::goldDk);
        gfx_rect(c, x + 1, y + 1 + yo, w - 2, h - 2, pal::goldDk);
        if (focused && w > 12 && h > 10) {   // corner brackets: focus reads by shape, not colour alone
            int k = h > 20 ? 4 : 3;
            for (int q = 0; q < 4; q++) {
                int bx = q & 1 ? x + w - 3 : x + 2, by = (q & 2 ? y + h - 3 : y + 2) + yo, sx = q & 1 ? -1 : 1, sy = q & 2 ? -1 : 1;
                for (int i = 0; i < k; i++) { gfx_pset(c, bx + i * sx, by, pal::gold); gfx_pset(c, bx, by + i * sy, pal::gold); }
            }
        }
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
    px top = active ? rgb(112, 80, 40) : rgb(48, 52, 56), bot = active ? rgb(72, 50, 26) : rgb(32, 36, 40);
    for (int j = 1; j < 14; j++) gfx_fill(c, x + 1, y + j, w - 2, 1, gfx_mix(top, bot, j * 18));
    gfx_rect(c, x, y, w, 15, active ? pal::gold : (focused ? pal::goldDk : rgb(72, 80, 88)));
    if (active) { gfx_rect(c, x + 1, y + 1, w - 2, 13, pal::goldDk); gfx_fill(c, x + 2, y + 2, w - 4, 1, rgb(160, 136, 72)); }
    if (gfx_textWB(label) <= w - 4) gfx_textCB(c, x + w / 2, y + 4, label, active ? rgb(255, 244, 210) : pal::dim);
    else gfx_textC(c, x + w / 2, y + 4, label, active ? rgb(255, 244, 210) : pal::dim);
    if (act) audio_play(SFX_MENU_MOVE);
    return act;
}
