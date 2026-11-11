// Every bottom-screen menu. Layout targets the native 320x240 touchscreen.
#include "ui.h"
#include "../core/art.h"
#include "../core/audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

int g_contProp = -1, g_contBag = -1, g_invSurvivor = -1, g_buildingSel = 0;
bool g_canQuit = false;                 // set by the Windows platform
const char* g_controlsText = "";        // set by each platform
static Input g_input;
static int tab = 0, sel = 0, page = 0, sel2 = 0;
static int saveMode = 0;                // 0 load, 1 save, 2 new game
static char msg[96];
static float msgT = 0;

struct Confirm { bool active; char text[96]; int action; int arg; };
static Confirm confirmBox;
static void ask(int action, int arg, const char* fmt, ...) {
    confirmBox.active = true; confirmBox.action = action; confirmBox.arg = arg;
    va_list a; va_start(a, fmt); vsnprintf(confirmBox.text, sizeof confirmBox.text, fmt, a); va_end(a);
    ui_resetFocus();
}
static void say(const char* s) { snprintf(msg, sizeof msg, "%s", s); msgT = 3; }
enum { CA_NEWGAME = 1, CA_LOAD, CA_SAVE, CA_DELETE, CA_MAINMENU, CA_REJECT, CA_QUIT, CA_TUTORIAL };
// tutorial nudges: pulse the button the current step wants (gold on/off)
static int hl(int which) { return (tut_highlight() == which && (((int)(G.uiTime * 3)) & 1)) ? UB_SELECTED : 0; }
static void offerTutorial() { if (!G.set.tutorialDone) ask(CA_TUTORIAL, 0, "Play the tutorial? A short first raid, then a tour of the settlement. Skip it any time."); }

void ui_update(const Input& in) { g_input = in; }
// The simulation changed panels this frame (e.g. extraction): the button press that caused it
// must not also activate a widget on the new panel.
void ui_dropInput() { g_input.down = 0; g_input.up = 0; g_input.touchDown = g_input.touchUp = false; }

void ui_open(Panel p) {
    if (p != G.panel) { G.panelT = 0; tab = 0; sel = 0; page = 0; sel2 = 0; }
    G.panel = p; G.nstack = 0; ui_resetFocus();
    audio_play(SFX_MENU_MOVE, 0.7f);
}
void ui_push(Panel p) {
    if (G.nstack < 8) G.stack[G.nstack++] = G.panel;
    Panel keep = G.panel; (void)keep;
    G.panel = p; G.panelT = 0; tab = 0; sel = 0; page = 0; sel2 = 0; ui_resetFocus();
    audio_play(SFX_MENU_MOVE, 0.7f);
}
void ui_back() {
    audio_play(SFX_MENU_BACK, 0.8f);
    G.panelT = 0; tab = 0; sel = 0; page = 0; ui_resetFocus();
    if (G.nstack > 0) { G.panel = G.stack[--G.nstack]; return; }
    G.panel = G.scene == SC_TITLE ? PN_MAIN : PN_HUD;
}
void ui_container(int prop, int bag) { g_contProp = prop; g_contBag = bag; ui_open(PN_CONTAINER); }

// ---------------------------------------------------------------- shared helpers
static Survivor* invSurv() {
    int sv = g_invSurvivor;
    if (G.panel == PN_CONTAINER || sv < 0 || sv >= G.st.nsurv || !G.st.surv[sv].alive) sv = G.inLevel && G.nu ? G.u[G.ctrl].sv : G.st.leader;
    return sv >= 0 ? &G.st.surv[sv] : nullptr;
}
static int invSurvIdx() { Survivor* s = invSurv(); return s ? (int)(s - G.st.surv) : -1; }
static void packRemove(Survivor& s, int i) {
    if (s.equip == i) s.equip = -1; else if (s.equip > i) s.equip--;
    s.pack.remove(i);
}
static Sprite rotIcon[IT_COUNT];
static const Sprite& iconFor(int id, bool rot) {
    if (!rot) return g_item[id];
    if (!rotIcon[id].d) rotIcon[id] = spr_rot90(g_item[id]);
    return rotIcon[id];
}
static void blitHalf(Canvas& c, const Sprite& s, int x, int y) {
    for (int j = 0; j < s.h / 2; j++) for (int i = 0; i < s.w / 2; i++) { px v = s.d[(j * 2) * s.w + i * 2]; if (v != KEYC) gfx_pset(c, x + i, y + j, v); }
}
static void footer(Canvas& c, const char* a, const char* b2, const char* x = nullptr, const char* y = nullptr) {
    // footer strip: glyphs match the active device (3DS buttons / keyboard keys / Xbox pad)
    gfx_fill(c, 0, 224, BOT_W, 16, rgb(24, 26, 42));
    gfx_fill(c, 0, 224, BOT_W, 1, rgb(56, 60, 88)); gfx_fill(c, 0, 223, BOT_W, 1, rgb(14, 14, 26));
    int px_ = 5, bw = b2 ? ui_promptW('B', b2) : 0;
    const char* items[3] = {a, y, x}; const char keys[3] = {'A', 'Y', 'X'};
    for (int i = 0; i < 3; i++) {
        if (!items[i]) continue;
        int w = ui_promptW(keys[i], items[i]);
        if (px_ + w > BOT_W - bw - 12) break;   // never overlap the Back prompt
        ui_prompt(c, px_, 226, keys[i], items[i]); px_ += w + 10;
    }
    if (b2) ui_prompt(c, BOT_W - bw - 5, 226, 'B', b2);
}
static void resRow(Canvas& c, int x, int y, bool compact) {
    static const int IC[RES_COUNT] = {S_ICON_FOOD, S_ICON_MED, S_ICON_AMMO, S_ICON_MAT, S_ICON_FUEL};
    for (int r = 0; r < RES_COUNT; r++) {
        char b[12]; snprintf(b, sizeof b, "%d", G.st.res[r]);
        gfx_blit(c, g_spr[IC[r]], x, y);
        gfx_text(c, x + 10, y, b, pal::cream);
        x += compact ? 36 : 60;
    }
}
static void costLine(Canvas& c, int x, int y, const uint8_t* cost) {
    static const int IC[RES_COUNT] = {S_ICON_FOOD, S_ICON_MED, S_ICON_AMMO, S_ICON_MAT, S_ICON_FUEL};
    bool any = false;
    for (int r = 0; r < RES_COUNT; r++) {
        if (!cost[r]) continue; any = true;
        char b[8]; snprintf(b, sizeof b, "%d", cost[r]);
        gfx_blit(c, g_spr[IC[r]], x, y);
        gfx_text(c, x + 10, y, b, G.st.res[r] >= cost[r] ? pal::cream : pal::red);
        x += 30;
    }
    if (!any) gfx_text(c, x, y, "Free", pal::dim);
}
static const char* survStatus(const Survivor& s, px& col) {
    col = pal::green;
    if (!s.alive) { col = pal::grey; return "Deceased"; }
    if (s.onExpedition) { col = pal::cyan; return "On a raid"; }
    if (s.quarantined) { col = pal::gold; return "Quarantined"; }
    if (s.infection && s.infKnown) { col = pal::red; return INF_NAMES[s.infection]; }
    if (s.hp < 50) { col = pal::gold; return "Injured"; }
    return "Healthy";
}
static int survLevel(const Survivor& s) { int t = 0; for (int k = 0; k < SK_COUNT; k++) t += s.skill[k]; return t / SK_COUNT; }
// small list icons
static void iconThird(Canvas& c, const Sprite& s, int x, int y) {
    for (int j = 0; j < s.h / 3; j++) for (int i = 0; i < s.w / 3; i++) { px v = s.d[(j * 3 + 1) * s.w + i * 3 + 1]; if (v != KEYC) gfx_pset(c, x + i, y + j, v); }
}
static void headCrop(Canvas& c, const Survivor& s, int x, int y) {
    const Sprite& p = g_portrait[s.variant % PORTRAITS];
    for (int j = 0; j < 18; j++) for (int i = 0; i < 18; i++) gfx_pset(c, x + i, y + j, p.d[(j + 2) * 24 + i + 3]);
    gfx_rect(c, x, y, 18, 18, pal::ink);
}

// ---------------------------------------------------------------- inventory grid
struct InvState {
    int sel = -1, held = -1, heldRot = 0, origX = 0, origY = 0, origRot = 0, curX = 0, curY = 0;
    bool dragging = false, touchHeld = false, onRail = false; int railFocus = 0;   // railFocus: -1 left rail, 1 right rail
    int grabDX = 0, grabDY = 0, startTx = 0, startTy = 0;
};
static InvState inv;
// after drawing a rail button: take focus there if the grid just handed it over to that side
static bool railGrab(bool act, int side) {
    if (inv.railFocus == side && g_ui.count && g_ui.rects[g_ui.count - 1].enabled) { g_ui.focus = g_ui.count - 1; inv.railFocus = 0; }
    return act;
}
static void railEnd() { if (inv.railFocus) { inv.railFocus = 0; inv.onRail = false; } }   // nothing to focus: stay on the grid

// ---------------------------------------------------------------- thumb rails (every screen)
// Tall buttons down both edges of the touchscreen, reachable from the Circle Pad and ABXY without
// letting go. Left rail: tabs / navigation. Right rail: actions, Back last. Content sits between.
enum { RW = 58, RLX = 2, RRX = BOT_W - RW - 2, CX = RLX + RW + 3, CW = RRX - CX - 3, RTOP = 2, RH = 220 };
static bool railBtn(Canvas& c, int x, int y, int w, int h, int icon, const char* label, int style = 0, uint32_t glyph = 0, bool enabled = true) {
    bool pressedNow = g_ui.pressed == g_ui.count && g_ui.in.touching;
    bool act = ui_button(c, x, y, w, h, "", style | UB_RAIL, enabled);
    bool sel = (style & UB_SELECTED) != 0;
    int depth = !enabled ? 1 : (h >= 22 ? 3 : 2), yo = pressedNow ? depth - 1 : 0, cy = y + (h - depth) / 2 + yo;
    px tc = enabled ? (sel ? pal::black : pal::cream) : pal::grey;
    // the selected block is gold: dark lettering, no drop shadow
    auto text = [&](int ty, const char* s, bool bold) {
        if (sel) { gfx_text(c, x + (w - gfx_textW(s)) / 2, ty, s, tc, 1, false); if (bold) gfx_text(c, x + (w - gfx_textW(s)) / 2 + 1, ty, s, tc, 1, false); }
        else if (bold) gfx_textCB(c, x + w / 2, ty, s, tc); else gfx_textC(c, x + w / 2, ty, s, tc);
    };
    bool big = icon >= 0 && h >= 34;
    if (big) { const Sprite& s = g_spr[icon]; int k = s.w > 12 ? 1 : 2; gfx_blit(c, s, x + (w - s.w * k) / 2, cy - s.h * k - 1, k); }
    // one line if it fits, else split at the first space onto two lines
    int ty = big ? cy + 4 : cy - 4;
    const char* sp = strchr(label, ' ');
    if (gfx_textWB(label) <= w - 4) text(ty, label, true);
    else if (gfx_textW(label) <= w - 2 || !sp) text(ty, label, false);
    else {
        char a[24]; int n = (int)(sp - label); if (n > 23) n = 23; memcpy(a, label, n); a[n] = 0;
        int y0 = big ? ty : ty - 5;
        text(y0, a, false); text(y0 + 10, sp + 1, false);
    }
    if (glyph && h >= 30) ui_btn(c, x + w - ui_btnW(glyph) - 3, y + h - 14 - depth + yo, glyph);
    return act;
}
// button i of n on a rail (side 0 left, 1 right); heights shrink to fit, capped so few buttons stay thumb-sized
static bool rail(Canvas& c, int side, int i, int n, const char* label, int style = 0, bool enabled = true, int icon = -1, uint32_t glyph = 0) {
    int h = (RH - (n - 1) * 4) / n; if (h > 68) h = 68;
    int y = RTOP + i * (h + 4);
    return railGrab(railBtn(c, side ? RRX : RLX, y, RW, h, icon, label, style, glyph, enabled), side ? 1 : -1);
}

static bool gridValid(const Grid& g) {
    for (int i = 0; i < g.n; i++) {
        const Item& a = g.it[i];
        if (a.x + itemW(a) > g.w || a.y + itemH(a) > g.h) return false;
        for (int j = i + 1; j < g.n; j++) {
            const Item& b = g.it[j];
            if (a.x < b.x + itemW(b) && b.x < a.x + itemW(a) && a.y < b.y + itemH(b) && b.y < a.y + itemH(a)) return false;
        }
    }
    return true;
}

// move / merge / swap; returns true if the grid changed
static bool invPlace(Survivor& s, int idx, int x, int y, int rot) {
    Grid& g = s.pack;
    Item& it = g.it[idx];
    if (g.move(idx, x, y, rot)) { audio_play(SFX_INV); return true; }
    int iw = rot ? idef(it.id).h : idef(it.id).w, ih = rot ? idef(it.id).w : idef(it.id).h;
    int other = -1, cnt = 0;
    for (int j = 0; j < g.n; j++) {
        if (j == idx) continue;
        const Item& b = g.it[j];
        if (x < b.x + itemW(b) && b.x < x + iw && y < b.y + itemH(b) && b.y < y + ih) { other = j; cnt++; }
    }
    if (cnt != 1) return false;
    Item& o = g.it[other];
    if (o.id == it.id && idef(it.id).maxStack > 1 && o.count < idef(it.id).maxStack) {
        int room = idef(it.id).maxStack - o.count, m = it.count < room ? it.count : room;
        o.count += m; it.count -= m;
        if (!it.count) packRemove(s, idx);
        audio_play(SFX_INV);
        return true;
    }
    Grid tmp = g;
    tmp.it[other].x = it.x; tmp.it[other].y = it.y;
    tmp.it[idx].x = (uint8_t)x; tmp.it[idx].y = (uint8_t)y; tmp.it[idx].rot = (uint8_t)rot;
    if (x < 0 || y < 0) return false;
    if (!gridValid(tmp)) { tmp.it[other].rot ^= 1; if (!gridValid(tmp)) return false; }
    g = tmp;
    audio_play(SFX_INV);
    return true;
}

static void drawGrid(Canvas& c, Survivor& s, int gx, int gy, int cell, bool interactive) {
    Grid& g = s.pack;
    for (int y = 0; y < g.h; y++) for (int x = 0; x < g.w; x++) {
        gfx_fill(c, gx + x * cell, gy + y * cell, cell, cell, rgb(24, 26, 42));
        gfx_rect(c, gx + x * cell, gy + y * cell, cell + 1, cell + 1, rgb(56, 60, 88));
    }
    gfx_rect(c, gx - 2, gy - 2, g.w * cell + 5, g.h * cell + 5, pal::border);
    int sc = cell / ITEM_UNIT < 1 ? 1 : cell / ITEM_UNIT;   // icons are drawn at 12 px per cell: 2x in the big starter grid, 1x in upgraded bags
    for (int i = 0; i < g.n; i++) {
        if (interactive && i == inv.held && inv.dragging) continue;
        const Item& it = g.it[i];
        int x = gx + it.x * cell, y = gy + it.y * cell, w = itemW(it) * cell, h = itemH(it) * cell;
        bool selected = interactive && i == inv.sel;
        bool heldHere = interactive && i == inv.held;
        gfx_fill(c, x + 1, y + 1, w - 1, h - 1, selected || heldHere ? pal::goldBg : rgb(44, 48, 74));
        { const Sprite& ic = iconFor(it.id, it.rot); gfx_blit(c, ic, x + (w - ic.w * sc) / 2 + 1, y + (h - ic.h * sc) / 2 + 1, sc); }
        if (selected || heldHere) { gfx_rect(c, x, y, w + 1, h + 1, pal::gold); gfx_rect(c, x + 1, y + 1, w - 1, h - 1, pal::goldDk); }
        if (it.count > 1) { char b[8]; snprintf(b, sizeof b, "%d", it.count); gfx_text(c, x + w - gfx_textW(b) - 1, y + h - 9, b, pal::cream); }
        const ItemDef& d = idef(it.id);
        if (d.wpn.dur && (d.cat == IC_FIREARM || d.cat == IC_MELEE)) {
            float f = (float)it.dur / d.wpn.dur;
            gfx_fill(c, x + 2, y + h - 3, (int)((w - 4) * f), 2, f < 0.25f ? pal::red : pal::green);
        }
        if (i == s.equip) { gfx_fill(c, x + 1, y + 1, 7, 8, pal::gold); gfx_text(c, x + 2, y + 1, "E", pal::ink, 1, false); }
    }
}

static int gridCell(const Survivor& s) { return s.pack.w <= 6 ? 24 : (s.pack.w <= 10 ? 16 : 12); }   // big chunky cells when they fit

// item details for the middle column (actions live on the right rail)
static void invInfo(Canvas& c, Survivor& s, int x, int y, int w, int h, int idx) {
    ui_panel(c, x, y, w, h);
    if (idx < 0 || idx >= s.pack.n) {
        char b[48]; snprintf(b, sizeof b, "%s  HP %d/%d", s.name, (int)s.hp, s.maxHp); gfx_text(c, x + 5, y + 5, b, pal::cream);
        snprintf(b, sizeof b, "Bag %dx%d  %d items", s.pack.w, s.pack.h, s.pack.n); gfx_text(c, x + 5, y + 16, b, pal::dim);
        int ln = gfx_wrap(c, x + 5, y + 30, w - 10, "Tap or drag items. D-Pad / Circle Pad moves the cursor; push past the bag's edge to reach the side buttons.", pal::dim);
        (void)ln;
        return;
    }
    Item& it = s.pack.it[idx]; const ItemDef& d = idef(it.id);
    gfx_textB(c, x + 5, y + 5, d.name, pal::cream);
    int ly = y + 17;
    ly += 10 * gfx_wrap(c, x + 5, ly, w - 10, d.desc, pal::dim) + 2;
    char b[48];
    if (d.cat == IC_FIREARM) {
        snprintf(b, sizeof b, "Dmg %d%s  Mag %d/%d", d.wpn.dmg, d.wpn.pellets > 1 ? "x6" : "", it.mag, d.wpn.mag); gfx_text(c, x + 5, ly, b, pal::cream); ly += 10;
        snprintf(b, sizeof b, "Acc %+d  Rng %d", d.wpn.acc + it.mod * 8, d.wpn.range); gfx_text(c, x + 5, ly, b, pal::cream); ly += 10;
    }
    if (d.cat == IC_MELEE) { snprintf(b, sizeof b, "Dmg %d  Stamina %d", d.wpn.dmg, d.wpn.stamina); gfx_text(c, x + 5, ly, b, pal::cream); ly += 10; }
    if (d.wpn.dur) { snprintf(b, sizeof b, "Durability %d/%d", it.dur, d.wpn.dur); gfx_text(c, x + 5, ly, b, it.dur ? pal::cream : pal::red); ly += 10; }
    else if (d.cat == IC_MELEE) { gfx_text(c, x + 5, ly, "Never wears out", pal::cream); ly += 10; }
    if (d.cat == IC_MEDICAL && d.heal) { snprintf(b, sizeof b, "Heals %d", d.heal); gfx_text(c, x + 5, ly, b, pal::green); ly += 10; }
    if (it.mod) { snprintf(b, sizeof b, "Upgraded +%d", it.mod); gfx_text(c, x + 5, ly, b, pal::gold); }
}

// right-rail item actions (buttons i0, i0+1, i0+2 of n): Use/Equip, Rotate, To Stash / Drop
static void itemActions(Canvas& c, Survivor& s, int idx, int i0, int n) {
    bool has = idx >= 0 && idx < s.pack.n;
    const ItemDef* d = has ? &idef(s.pack.it[idx].id) : nullptr;
    bool usable = d && (d->cat == IC_MEDICAL || d->cat == IC_BACKPACK || d->cat == IC_FIREARM || d->cat == IC_MELEE);
    const char* useLbl = d && (d->cat == IC_FIREARM || d->cat == IC_MELEE) ? (s.equip == idx ? "Equipped" : "Equip") : "Use";
    if (rail(c, 1, i0, n, useLbl, UB_GOLD, usable && s.equip != idx, -1, BTN_X)) {
        int sv = (int)(&s - G.st.surv), ui_ = -1;
        for (int k = 0; k < G.nu; k++) if (G.u[k].sv == sv) ui_ = k;
        survivor_useItem(sv, idx, ui_);
        if (inv.sel >= s.pack.n) inv.sel = -1;
        return;
    }
    if (rail(c, 1, i0 + 1, n, "Rotate", 0, d && d->w != d->h, -1, BTN_Y)) {
        Item& it = s.pack.it[idx];
        if (s.pack.move(idx, it.x, it.y, it.rot ^ 1)) audio_play(SFX_INV); else say("No room to rotate here.");
    }
    if (rail(c, 1, i0 + 2, n, G.scene == SC_SETTLEMENT ? "To Stash" : "Drop", 0, has)) {
        Item copy = s.pack.it[idx];
        if (G.scene == SC_SETTLEMENT) {
            G.st.stash.cap = (uint8_t)settle_stashCap();
            if (G.st.stash.push(copy)) { packRemove(s, idx); inv.sel = -1; audio_play(SFX_INV); } else say("The stash is full. Upgrade Storage.");
        } else if (G.curLoc >= 0) {
            LocState& L = G.loc[G.curLoc]; const Unit& u = G.u[G.ctrl];
            int b = -1;
            for (int k = 0; k < MAX_BAGS; k++) if (L.bags[k].used && fabsf(L.bags[k].x - u.x) < 20 && fabsf(L.bags[k].y - u.y) < 20 && L.bags[k].c.n < Container::MAX) b = k;
            if (b < 0) for (int k = 0; k < MAX_BAGS; k++) if (!L.bags[k].used) { b = k; L.bags[k].used = true; L.bags[k].x = u.x; L.bags[k].y = u.y + 6; L.bags[k].c.n = 0; L.bags[k].c.cap = Container::MAX; break; }
            if (b >= 0 && L.bags[b].c.push(copy)) { packRemove(s, idx); inv.sel = -1; audio_play(SFX_INV); say("Dropped. Pick it up again from the bag."); }
            else say("Too many dropped bags here.");
        }
    }
}

static void gridInteract(Canvas& c, Survivor& s, int gx, int gy, int cell) {
    const Input& in = g_ui.in;
    Grid& g = s.pack;
    auto cellAt = [&](int tx, int ty, int& cx, int& cy) { cx = (tx - gx) / cell; cy = (ty - gy) / cell; if (tx < gx) cx = -1; if (ty < gy) cy = -1; return cx >= 0 && cy >= 0 && cx < g.w && cy < g.h; };
    // stylus: press on an item to pick it up, drag, release to place
    int cx, cy;
    if (in.touchDown && cellAt(in.tx, in.ty, cx, cy)) {
        int i = g.at(cx, cy);
        if (i >= 0) {
            inv.sel = i; inv.held = i; inv.touchHeld = true; inv.dragging = false;
            inv.grabDX = cx - g.it[i].x; inv.grabDY = cy - g.it[i].y; inv.heldRot = g.it[i].rot;
            inv.startTx = in.tx; inv.startTy = in.ty;
            audio_play(SFX_MENU_MOVE, 0.6f);
        } else inv.sel = -1;
    }
    if (inv.touchHeld && in.touching && inv.held >= 0) {
        if (abs(in.tx - inv.startTx) + abs(in.ty - inv.startTy) > 3) inv.dragging = true;   // responds immediately to movement
    }
    if (inv.touchHeld && in.touchUp) {
        if (inv.dragging && inv.held >= 0 && inv.held < g.n) {
            int tx = (in.tx - gx) / cell - inv.grabDX, ty = (in.ty - gy) / cell - inv.grabDY;
            if (in.tx >= gx && in.ty >= gy && !invPlace(s, inv.held, tx, ty, inv.heldRot)) { audio_play(SFX_EMPTY, 0.5f); say("Doesn't fit there."); }
        }
        inv.touchHeld = false; inv.dragging = false; inv.held = -1;
    }
    // D-pad / circle pad: cursor (hold to repeat) + A pick/place, Y rotate, X use.
    // Pushing right off the grid hands focus to the action buttons; left from there comes back.
    if (in.touchDown && cellAt(in.tx, in.ty, cx, cy)) inv.onRail = false;
    if (inv.onRail && (g_ui.navEdge == 3 || g_ui.navEdge == 4)) inv.onRail = false;
    uint32_t d = in.down;
    if (!in.touching && !inv.onRail) {
        int dir = g_ui.navDir;
        int mx = dir == 4 ? 1 : dir == 3 ? -1 : 0, my = dir == 2 ? 1 : dir == 1 ? -1 : 0;
        if (mx > 0 && inv.curX == g.w - 1 && inv.held < 0) { inv.onRail = true; inv.railFocus = 1; mx = 0; }    // onto the right rail
        if (mx < 0 && inv.curX == 0 && inv.held < 0) { inv.onRail = true; inv.railFocus = -1; mx = 0; }          // onto the left rail
        if (my < 0 && inv.curY == 0 && inv.held < 0 && G.panel == PN_CONTAINER) { inv.onRail = true; inv.railFocus = 2; my = 0; }   // up into the loot slots
        if (mx || my) {
            int nx = inv.curX + mx, ny = inv.curY + my;
            if (nx < 0) nx = 0; if (ny < 0) ny = 0; if (ny >= g.h) ny = g.h - 1;
            if (nx >= g.w) nx = g.w - 1;   // holding an item: the right edge stops the cursor
            if (nx != inv.curX || ny != inv.curY) { inv.curX = nx; inv.curY = ny; audio_play(SFX_MENU_MOVE, 0.4f); }
            if (inv.held < 0) { int i = g.at(inv.curX, inv.curY); if (i >= 0) inv.sel = i; }   // selection sticks on empty cells
        }
        if (d & BTN_A) {
            if (inv.held < 0) { int i = g.at(inv.curX, inv.curY); if (i >= 0) { inv.held = i; inv.sel = i; inv.heldRot = g.it[i].rot; inv.grabDX = inv.curX - g.it[i].x; inv.grabDY = inv.curY - g.it[i].y; audio_play(SFX_MENU_MOVE); } }
            else {
                if (invPlace(s, inv.held, inv.curX - inv.grabDX, inv.curY - inv.grabDY, inv.heldRot)) { inv.held = -1; }
                else { audio_play(SFX_EMPTY, 0.5f); say("Doesn't fit there."); }
            }
        }
        if (d & BTN_Y) {
            int i = inv.held >= 0 ? inv.held : inv.sel;
            if (i >= 0 && i < g.n) {
                if (inv.held >= 0) { inv.heldRot ^= 1; audio_play(SFX_INV); }
                else if (g.move(i, g.it[i].x, g.it[i].y, g.it[i].rot ^ 1)) audio_play(SFX_INV); else say("No room to rotate here.");
            }
        }
        if ((d & BTN_X) && G.panel != PN_CONTAINER && inv.sel >= 0 && inv.sel < g.n) {   // in a container X is Take All
            int sv = (int)(&s - G.st.surv), ui_ = -1; for (int k = 0; k < G.nu; k++) if (G.u[k].sv == sv) ui_ = k;
            survivor_useItem(sv, inv.sel, ui_); if (inv.sel >= g.n) inv.sel = -1;
        }
        // keyboard-held item ghost
        if (inv.held >= 0 && inv.held < g.n) {
            const Item& it = g.it[inv.held];
            int w = inv.heldRot ? idef(it.id).h : idef(it.id).w, h = inv.heldRot ? idef(it.id).w : idef(it.id).h;
            int tx = inv.curX - inv.grabDX, ty = inv.curY - inv.grabDY;
            bool ok = g.fits(it.id, inv.heldRot, tx, ty, inv.held);
            gfx_fillA(c, gx + tx * cell, gy + ty * cell, w * cell, h * cell, ok ? pal::green : pal::red, 90);
            { const Sprite& ic = iconFor(it.id, inv.heldRot); int sc = cell / ITEM_UNIT < 1 ? 1 : cell / ITEM_UNIT; gfx_blit(c, ic, gx + tx * cell + (w * cell - ic.w * sc) / 2, gy + ty * cell + (h * cell - ic.h * sc) / 2, sc); }
        }
        gfx_rect(c, gx + inv.curX * cell, gy + inv.curY * cell, cell + 1, cell + 1, ((int)(G.uiTime * 4) & 1) ? pal::gold : pal::cream);
    }
    // stylus drag ghost follows the pen immediately
    if (inv.dragging && inv.held >= 0 && inv.held < g.n) {
        const Item& it = g.it[inv.held];
        int w = inv.heldRot ? idef(it.id).h : idef(it.id).w, h = inv.heldRot ? idef(it.id).w : idef(it.id).h;
        int tx = (in.tx - gx) / cell - inv.grabDX, ty = (in.ty - gy) / cell - inv.grabDY;
        bool ok = in.tx >= gx && in.ty >= gy && g.fits(it.id, inv.heldRot, tx, ty, inv.held);
        if (in.tx >= gx && in.ty >= gy) gfx_fillA(c, gx + tx * cell, gy + ty * cell, w * cell, h * cell, ok ? pal::green : pal::red, 80);
        { const Sprite& ic = iconFor(it.id, inv.heldRot); int sc = cell / ITEM_UNIT < 1 ? 1 : cell / ITEM_UNIT; gfx_blit(c, ic, in.tx - inv.grabDX * cell - cell / 2 + (w * cell - ic.w * sc) / 2, in.ty - inv.grabDY * cell - cell / 2 + (h * cell - ic.h * sc) / 2, sc); }
    }
}

static void panelInventory(Canvas& c) {
    Survivor* sp = invSurv();
    if (!sp) { ui_back(); return; }
    Survivor& s = *sp;
    static const char* TABS[] = {"Backpack", "Weapons", "Items", "Key Items", "Sort"};
    for (int t = 0; t < 5; t++) {
        if (rail(c, 0, t, 5, TABS[t], tab == t ? UB_SELECTED : 0)) {
            if (t == 4) { if (s.pack.sort()) { survivor_equipBest(s); say("Backpack sorted."); audio_play(SFX_INV); } else say("Could not sort."); }
            else { tab = t; inv.sel = -1; inv.held = -1; }
        }
    }
    if (ui_shoulderL()) tab = (tab + 3) % 4; if (ui_shoulderR()) tab = (tab + 1) % 4;
    if (tab == 0) {
        int cell = gridCell(s), gw = s.pack.w * cell, gh = s.pack.h * cell;
        int gx = CX + (CW - gw) / 2, gy = 5;
        drawGrid(c, s, gx, gy, cell, true);
        gridInteract(c, s, gx, gy, cell);
        invInfo(c, s, CX, gy + gh + 6, CW, 222 - (gy + gh + 6), inv.sel);
        footer(c, inv.onRail ? "Select" : (inv.held >= 0 ? "Place" : "Move"), "Back", "Use", "Rotate");
    } else {
        // list views: weapons / all items / key items
        int n = 0, rows[48];
        for (int i = 0; i < s.pack.n; i++) {
            int cat = idef(s.pack.it[i].id).cat;
            bool ok = tab == 2 || (tab == 1 && (cat == IC_FIREARM || cat == IC_MELEE || cat == IC_BACKPACK)) || (tab == 3 && (cat == IC_KEY || cat == IC_EQUIPMENT));
            if (ok) rows[n++] = i;
        }
        ui_panel(c, CX, 2, CW, 116);
        if (!n) gfx_text(c, CX + 8, 10, "Nothing here.", pal::dim);
        for (int k = 0; k < n && k < 8; k++) {
            const Item& it = s.pack.it[rows[k]];
            char b[40];
            if (it.count > 1) snprintf(b, sizeof b, "%s x%d", idef(it.id).name, it.count);
            else snprintf(b, sizeof b, "%s%s", rows[k] == s.equip ? "[E] " : "", idef(it.id).name);
            if (ui_button(c, CX + 3, 5 + k * 14, CW - 6, 13, b, UB_LEFT | (inv.sel == rows[k] ? UB_SELECTED : 0))) inv.sel = rows[k];
        }
        invInfo(c, s, CX, 122, CW, 100, inv.sel);
        footer(c, "Select", "Back");
    }
    itemActions(c, s, inv.sel, 0, 4);
    if (rail(c, 1, 3, 4, "Back", 0, true, -1, BTN_B)) { ui_back(); return; }
    railEnd();
    if (ui_pressedB() && inv.held < 0 && !inv.touchHeld) ui_back();
    else if (ui_pressedB()) { inv.held = -1; inv.touchHeld = false; inv.dragging = false; }
}

// ---------------------------------------------------------------- containers (desk, bag, stash)
static Container* curContainer(const char** title) {
    if (g_contProp == -2) { *title = "Settlement Stash"; G.st.stash.cap = (uint8_t)settle_stashCap(); return &G.st.stash; }
    if (G.curLoc < 0) return nullptr;
    if (g_contBag >= 0) { *title = "Dropped Bag"; return &G.loc[G.curLoc].bags[g_contBag].c; }
    if (g_contProp >= 0) {
        static const char* N[] = {"Desk", "Filing Cabinet", "Locker", "Crate", "Medical Cabinet", "Evidence Shelf", "Police Car Trunk", "Car Trunk"};
        int t = G.map.prop[g_contProp].type; *title = t < 8 ? N[t] : "Container";
        return &G.loc[G.curLoc].cont[G.map.prop[g_contProp].container];
    }
    return nullptr;
}

static void panelContainer(Canvas& c) {
    const char* title = "Container";
    Container* ct = curContainer(&title);
    Survivor* sp = invSurv();
    if (!ct || !sp) { ui_back(); return; }
    Survivor& s = *sp;
    // loot slots (a drawer, 5 per page) in the middle column
    const int PER = 5;
    bool paged = ct->cap > PER;
    ui_panel(c, CX, 2, CW, paged ? 66 : 54, title);
    int pageStart = page * PER;
    for (int k = 0; k < PER && pageStart + k < ct->cap; k++) {
        int i = pageStart + k, x = CX + 4 + k * 38, y = 19;
        bool has = i < ct->n;
        bool selected = sel2 == 0 && sel == i && has;
        if (railGrab(ui_hit(x, y, 36, 30, has), 2)) { sel = i; sel2 = 0; inv.sel = -1; audio_play(SFX_MENU_MOVE); }
        gfx_fill(c, x, y, 36, 30, selected ? pal::goldBg : rgb(44, 48, 74));
        gfx_rect(c, x, y, 36, 30, selected ? pal::gold : (g_ui.focus == g_ui.count - 1 && !g_ui.navLock ? pal::goldDk : rgb(22, 22, 34)));
        if (has) {
            const Item& it = ct->it[i]; const Sprite& ic = g_item[it.id];
            if (ic.w <= 32 && ic.h <= 24) gfx_blit(c, ic, x + 18 - ic.w / 2, y + 15 - ic.h / 2);
            else blitHalf(c, ic, x + 18 - ic.w / 4, y + 15 - ic.h / 4);
            if (it.count > 1) { char b[8]; snprintf(b, sizeof b, "%d", it.count); gfx_text(c, x + 34 - gfx_textW(b), y + 21, b, pal::cream); }
        }
    }
    if (ct->n == 0) gfx_textC(c, CX + CW / 2, 31, "Empty.", pal::dim);
    if (paged) {
        char b[24]; snprintf(b, sizeof b, "%d/%d", ct->n, ct->cap); gfx_text(c, CX + 6, 53, b, pal::dim);
        if (ui_button(c, CX + CW - 62, 51, 28, 13, "<", 0, page > 0)) page--;
        if (ui_button(c, CX + CW - 32, 51, 28, 13, ">", 0, (page + 1) * PER < ct->cap)) page++;
    }
    // backpack: as big as the space below the loot allows (same 24 px cells as the backpack screen when it fits)
    int top = paged ? 72 : 60, gy = top + 12, availW = CW - 6, availH = 222 - gy;
    int cell = 24;
    if (cell > availW / s.pack.w) cell = availW / s.pack.w;
    if (cell > availH / s.pack.h) cell = availH / s.pack.h;
    int gx = CX + (CW - s.pack.w * cell) / 2;
    gy += (availH - s.pack.h * cell) / 2;
    int prevSel = inv.sel;
    drawGrid(c, s, gx, gy, cell, true);
    gridInteract(c, s, gx, gy, cell);
    if (inv.sel != prevSel && inv.sel >= 0) sel2 = 1;
    bool fromCont = sel2 == 0 && sel < ct->n;
    const Item* shown = fromCont ? &ct->it[sel] : (inv.sel >= 0 && inv.sel < s.pack.n ? &s.pack.it[inv.sel] : nullptr);
    // one label line: the selected item's name, otherwise whose bag this is
    if (shown) gfx_textB(c, CX + 2, top, idef(shown->id).name, pal::cream);
    else { char bb[40]; snprintf(bb, sizeof bb, "%s  Bag %dx%d", s.name, s.pack.w, s.pack.h); gfx_text(c, CX + 2, top, bb, pal::dim); }
    auto takeOne = [&](int i) -> bool {
        Item it = ct->it[i];
        int left = s.pack.add(it);
        if (left == it.count) return false;
        if (left) ct->it[i].count = (uint16_t)left; else ct->remove(i);
        if (s.equip < 0) survivor_equipBest(s);
        return true;
    };
    // left rail: taking and storing
    if (rail(c, 0, 0, 3, "Take All", UB_GOLD | hl(TH_TAKEALL), ct->n > 0, -1, BTN_X) || (ct->n > 0 && (g_ui.in.down & BTN_X))) {   // X: take everything
        int moved = 0, start = ct->n;
        for (int i = ct->n - 1; i >= 0; i--) if (takeOne(i)) moved++;
        audio_play(moved ? SFX_PICKUP : SFX_EMPTY);
        if (moved < start) say("Some items didn't fit. They stay here.");
        sel = 0; page = 0;
    }
    if (rail(c, 0, 1, 3, "Take", 0, fromCont)) {
        if (takeOne(sel)) { audio_play(SFX_PICKUP); if (sel >= ct->n) sel = ct->n ? ct->n - 1 : 0; }
        else { audio_play(SFX_EMPTY); say("No room in the backpack. Item left in place."); }
    }
    bool canStore = !fromCont && inv.sel >= 0 && inv.sel < s.pack.n && ct->n < ct->cap;
    if (rail(c, 0, 2, 3, "Store", 0, canStore)) {
        Item it = s.pack.it[inv.sel];
        if (ct->push(it)) { packRemove(s, inv.sel); inv.sel = -1; audio_play(SFX_INV); }
    }
    // right rail: donate / inspect, rotate, back
    if (g_contProp == -2) {
        const Item* d = shown;
        bool can = d && (idef(d->id).cat == IC_AMMO || idef(d->id).cat == IC_MEDICAL || idef(d->id).cat == IC_FOOD || idef(d->id).cat == IC_MATERIAL || idef(d->id).cat == IC_FUEL);
        if (rail(c, 1, 0, 3, "Donate", 0, can)) {
            int amt = deliver_item(*d);
            char b[48]; snprintf(b, sizeof b, "+%d %s for the settlement.", amt, RES_NAMES[idef(d->id).res]); say(b);
            if (fromCont) ct->remove(sel); else { packRemove(s, inv.sel); inv.sel = -1; }
            audio_play(SFX_PICKUP);
        }
    } else if (rail(c, 1, 0, 3, "Inspect", 0, shown != nullptr)) {
        char b[96]; const ItemDef& d = idef(shown->id);
        snprintf(b, sizeof b, "%s: %dx%d cells%s", d.name, d.w, d.h, d.wpn.dur ? ", has durability" : ""); say(b);
    }
    bool canRot = !fromCont && inv.sel >= 0 && inv.sel < s.pack.n && idef(s.pack.it[inv.sel].id).w != idef(s.pack.it[inv.sel].id).h;
    if (rail(c, 1, 1, 3, "Rotate", 0, canRot, -1, BTN_Y)) {
        Item& it = s.pack.it[inv.sel];
        if (s.pack.move(inv.sel, it.x, it.y, it.rot ^ 1)) audio_play(SFX_INV); else say("No room to rotate here.");
    }
    if (rail(c, 1, 2, 3, "Back", 0, true, -1, BTN_B) || (ui_pressedB() && inv.held < 0)) {
        if (g_contBag >= 0 && G.curLoc >= 0 && ct->n == 0) G.loc[G.curLoc].bags[g_contBag].used = false;
        ui_back();
    }
    railEnd();
    footer(c, inv.onRail ? "Confirm" : (inv.held >= 0 ? "Place" : "Move"), "Back", "Take All", "Rotate");
}

// ---------------------------------------------------------------- HUD (gameplay bottom screen)
static void minimap(Canvas& c, int x, int y, int w, int h) {
    Map& m = G.map;
    int scale = 2; if (m.w * 3 <= w && m.h * 3 <= h) scale = 3;
    int ox = x + (w - m.w * scale) / 2, oy = y + (h - m.h * scale) / 2;
    gfx_fill(c, x, y, w, h, rgb(16, 16, 28));
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty);
        if (G.scene == SC_EXPEDITION && !G.explored[i]) continue;
        uint8_t t = m.t[i]; px col;
        if (t == T_WALL || t == T_PALISADE) col = rgb(120, 124, 160);
        else if (t == T_DOOR) col = m.bar[m.barAt[i]].kind == BK_WINDOW ? rgb(112, 186, 216) : rgb(236, 196, 72);
        else if (t == T_EXTRACT) col = pal::lime;
        else if (m.flags[i] & F_INDOOR) col = rgb(64, 68, 100);
        else col = (t == T_GRASS ? rgb(58, 96, 62) : (t == T_WATER ? rgb(30, 78, 104) : (t == T_RAIL ? rgb(96, 90, 86) : rgb(50, 52, 68))));
        if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].container >= 0) col = G.curLoc >= 0 && G.loc[G.curLoc].generated[m.prop[m.propAt[i]].container] ? rgb(96, 100, 124) : rgb(154, 228, 64);
        if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].type == PR_PLOT) col = G.st.bLevel[m.prop[m.propAt[i]].special] ? rgb(214, 164, 96) : rgb(110, 90, 66);
        else if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].solid && G.scene == SC_SETTLEMENT) col = rgb(140, 124, 96);
        if (t == T_GARDEN) col = rgb(96, 150, 70);
        if (t == T_DIRT && G.scene == SC_SETTLEMENT) col = rgb(132, 102, 72);
        gfx_fill(c, ox + tx * scale, oy + ty * scale, scale, scale, col);
    }
    for (int i = 0; i < G.nu; i++) if (G.u[i].alive) {
        int ux = ox + (int)(G.u[i].x / 16) * scale, uy = oy + (int)(G.u[i].y / 16) * scale;
        if (i == G.ctrl) {   // arrow-ish marker in the facing direction
            gfx_fill(c, ux - 1, uy - 1, 3, 3, pal::gold);
            gfx_pset(c, ux + (int)roundf(G.u[i].fx * 2), uy + (int)roundf(G.u[i].fy * 2), pal::gold);
        } else gfx_fill(c, ux, uy, 2, 2, pal::cyan);
    }
    gfx_rect(c, x, y, w, h, pal::border);
}

static const char* objective() {
    if (const char* lesson = tut_objective()) return lesson;   // the tutorial's current goal comes first
    if (G.scene == SC_SETTLEMENT) {
        if (G.st.visitorPresent) return "A stranger waits at the gate.";
        int sec = settle_security();
        if (sec < 25) return "Security is low! Assign guards.";
        return "Use the radio table to plan a raid.";
    }
    for (int i = 0; i < G.nu; i++) if (G.u[i].escort && G.u[i].alive) return G.u[i].order == ORD_HOLD ? (G.map.id == MAP_POLICE ? "Someone is trapped in the holding cells." : "Someone is trapped inside. Find them.") : "Escort the survivor to the south road.";
    return "Search for supplies. Extract at the south road.";
}

// A tutorial scene is playing on the top screen: the touch screen only offers Next and the two ways out.
static void panelScene(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, tut_sceneTitle());
    int total, at = tut_sceneBeat(total);
    for (int k = 0; k < total; k++) {   // where we are in the scene
        int x = CX + CW / 2 - total * 6 + k * 12, on = k <= at;
        gfx_fill(c, x, 28, 9, 5, pal::ink); gfx_fill(c, x + 1, 29, 7, 3, on ? pal::gold : pal::panelHi);
    }
    gfx_wrap(c, CX + 14, 60, CW - 28, "The scene plays on the top screen.", pal::cream);
    gfx_wrap(c, CX + 14, 90, CW - 28, "Next moves it along. Skip Scene goes straight to the part you play.", pal::dim);
    if (rail(c, 0, 0, 1, "Next", UB_GOLD, true, -1, BTN_A)) tut_sceneNext();
    if (rail(c, 1, 0, 2, "Skip Scene", 0, true, -1, BTN_START)) { tut_sceneSkip(); return; }
    if (rail(c, 1, 1, 2, "Skip Tutorial")) tut_skip();
}

static void panelHud(Canvas& c) {
    if (tut_scene()) { panelScene(c); return; }
    // thumb rails down both edges (reachable without letting go of the Circle Pad / ABXY);
    // information stays in the middle column
    const int RW = 60, LX = 2, RX = BOT_W - RW - 2, CX = LX + RW + 4, CW = RX - CX - 4, TOP = 2, H = 220;
    if (G.scene == SC_SETTLEMENT) {
        ui_panel(c, CX, TOP, CW, 48, "Settlement");
        resRow(c, CX + 5, TOP + 19, true);
        char b[64]; int sec = settle_security();
        snprintf(b, sizeof b, "Pop %d/%d  Security %d", settle_population(), settle_capacity(), sec);
        gfx_blit(c, g_spr[S_ICON_PEOPLE], CX + 5, TOP + 33); gfx_text(c, CX + 16, TOP + 33, b, sec < 25 ? pal::red : pal::cream);
        minimap(c, CX, TOP + 52, CW, 96);
    } else {
        char t[40]; snprintf(t, sizeof t, "%s", G.curLoc >= 0 ? LOCATIONS[G.curLoc].name : "");
        ui_panel(c, CX, TOP, CW, 146, t);
        minimap(c, CX + 4, TOP + 18, CW - 8, 124);
    }
    int oy = TOP + 152;
    gfx_blit(c, g_spr[S_ICON_SHIELD], CX + 2, oy);
    gfx_text(c, CX + 13, oy, "Objective", pal::gold);
    int lines = gfx_wrap(c, CX + 2, oy + 11, CW - 4, objective(), pal::cream);
    if (G.scene == SC_EXPEDITION) {
        Survivor* s = invSurv();
        if (s) {
            char b[40]; snprintf(b, sizeof b, "Bag %d items %dx%d", s->pack.n, s->pack.w, s->pack.h);
            gfx_text(c, CX + 2, oy + 13 + lines * 10, b, pal::dim);
            if (s->infection && s->infKnown) gfx_text(c, CX + 2, oy + 24 + lines * 10, INF_NAMES[s->infection], pal::red);
        }
    }
    if (tut_active() && ui_button(c, CX + 24, 202, CW - 48, 18, "Skip Tutorial")) tut_skip();
    if (G.scene == SC_SETTLEMENT) {
        const int bh = (H - 2 * 4) / 3;
        if (railBtn(c, LX, TOP, RW, bh, S_ICON_HOUSE, "Base", hl(TH_BASE))) ui_open(PN_BASE);
        if (railBtn(c, LX, TOP + bh + 4, RW, bh, S_ICON_PEOPLE, "Survivors")) ui_open(PN_SURVIVORS);
        if (railBtn(c, LX, TOP + 2 * (bh + 4), RW, bh, S_ICON_TRAIN, "Raid", hl(TH_RAID))) ui_open(PN_WORLDMAP);
        char jb[24]; snprintf(jb, sizeof jb, G.st.unread ? "Journal %d" : "Journal", G.st.unread);
        if (railBtn(c, RX, TOP, RW, bh, S_RADIO, jb, G.st.unread ? UB_GOLD : 0)) ui_open(PN_JOURNAL);
        if (railBtn(c, RX, TOP + bh + 4, RW, bh, S_ICON_MAT, "Backpack", 0, BTN_SELECT)) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
        if (railBtn(c, RX, TOP + 2 * (bh + 4), RW, bh, S_ICON_CLOCK, "Pause", 0, BTN_START)) ui_open(PN_PAUSE);
    } else {
        const int bh = (H - 4) / 2;
        if (railBtn(c, LX, TOP, RW, bh, S_ICON_MAT, "Backpack", 0, BTN_SELECT)) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
        if (railBtn(c, LX, TOP + bh + 4, RW, bh, S_ICON_PEOPLE, "Squad")) ui_open(PN_SQUAD);
        if (railBtn(c, RX, TOP, RW, bh, -1, G.flashlight ? "Light On" : "Light Off", G.flashlight ? UB_SELECTED : 0, BTN_R)) { G.flashlight = !G.flashlight; audio_play(SFX_MENU_MOVE, 0.8f, 0.6f); }
        if (railBtn(c, RX, TOP + bh + 4, RW, bh, S_ICON_CLOCK, "Pause", 0, BTN_START)) ui_open(PN_PAUSE);
    }
    footer(c, "Interact", "Sprint/Dodge", "Reload", "Fire");
    if (g_ui.in.down & BTN_SELECT) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
    else if (g_ui.in.down & BTN_START) ui_open(PN_PAUSE);
}

static void panelSquad(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Squad Orders");
    static const char* ON[] = {"Follow", "Hold", "Retreat"};
    int y = 20, n = 0;
    for (int i = 0; i < G.nu; i++) {
        Unit& u = G.u[i];
        if (i == G.ctrl || u.escort || u.sv < 0) continue;
        Survivor& s = G.st.surv[u.sv];
        n++;
        headCrop(c, s, CX + 5, y);
        char b[40]; snprintf(b, sizeof b, "%s%s", s.name, u.alive ? "" : " (dead)");
        gfx_text(c, CX + 27, y + 1, b, u.alive ? pal::cream : pal::grey);
        ui_bar(c, CX + 27, y + 11, 80, 5, s.hp / s.maxHp, pal::green);
        for (int o = 0; o < 3; o++) if (ui_button(c, CX + 5 + o * 62, y + 21, 59, 20, ON[o], u.order == o ? UB_SELECTED : 0, u.alive)) u.order = (uint8_t)o;
        y += 50;
    }
    if (!n) gfx_wrap(c, CX + 6, 22, CW - 12, "You are alone on this raid. Bring companions from Raid Prep.", pal::dim);
    gfx_wrap(c, CX + 6, 128, CW - 12, "Follow: stay close and fight. Hold: guard this spot. Retreat: fall back to the extraction point. D-Pad Down cycles orders in the field.", pal::dim);
    for (int o = 0; o < 3; o++) {
        char b[16]; snprintf(b, sizeof b, "All %s", ON[o]);
        if (rail(c, 0, o, 3, b, 0, n > 0)) for (int i = 0; i < G.nu; i++) if (i != G.ctrl && !G.u[i].escort && G.u[i].alive) G.u[i].order = (uint8_t)o;
    }
    if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- pause / main
static void panelPause(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Paused");
    char b[64];
    int hour = ((int)G.clock / 60) % 24, mins = (int)G.clock % 60;
    snprintf(b, sizeof b, "Day %u  %02d:%02d", G.day, hour, mins); gfx_textC(c, CX + CW / 2, 22, b, pal::cream);
    gfx_textC(c, CX + CW / 2, 34, G.scene == SC_SETTLEMENT ? "At the settlement" : (G.curLoc >= 0 ? LOCATIONS[G.curLoc].name : ""), pal::dim);
    Survivor* s = invSurv();
    if (s) {
        gfx_blit(c, g_portrait[s->variant % PORTRAITS], CX + CW / 2 - 24, 50, 2);
        gfx_textC(c, CX + CW / 2, 102, s->name, pal::cream);
        ui_bar(c, CX + 30, 114, CW - 60, 6, s->hp / s->maxHp, pal::green);
    }
    resRow(c, CX + 8, 130, true);
    gfx_wrap(c, CX + 8, 148, CW - 16, objective(), pal::dim);
    if (G.scene != SC_SETTLEMENT) gfx_wrap(c, CX + 8, 190, CW - 16, "Saving is only possible at the settlement.", pal::grey);
    // left: keep playing / look around.  right: system.
    if (rail(c, 0, 0, 4, "Resume", UB_GOLD, true, -1, BTN_START) || (g_ui.in.down & BTN_START)) { ui_open(PN_HUD); return; }
    if (rail(c, 0, 1, 4, "Backpack", 0, true, S_ICON_MAT)) { g_invSurvivor = -1; ui_push(PN_INVENTORY); }
    if (rail(c, 0, 2, 4, "Survivors", 0, true, S_ICON_PEOPLE)) ui_push(PN_SURVIVORS);
    if (rail(c, 0, 3, 4, "Options")) ui_push(PN_SETTINGS);
    bool canSave = G.scene == SC_SETTLEMENT, tut = tut_active();
    int nr = tut ? 4 : 3;
    if (rail(c, 1, 0, nr, "Save Game", 0, canSave)) { saveMode = 1; ui_push(PN_SAVELOAD); }
    if (rail(c, 1, 1, nr, "Controls")) ui_push(PN_CONTROLS);
    if (tut && rail(c, 1, 2, nr, "Skip Tutorial")) { tut_skip(); ui_open(PN_HUD); return; }
    if (rail(c, 1, nr - 1, nr, "Main Menu")) ask(CA_MAINMENU, 0, G.scene == SC_EXPEDITION ? "Quit to menu? Raid progress since the last save is lost." : "Return to the main menu? Unsaved progress is lost.");
    if (ui_pressedB()) ui_open(PN_HUD);
}

static void panelMain(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Main Menu");
    bool any = false; for (int s = 0; s < 3; s++) any |= save_exists(s);
    gfx_textCB(c, CX + CW / 2, 26, "STAYING ALIVE", pal::red);
    gfx_textC(c, CX + CW / 2, 38, "Scavenge. Survive. Rebuild.", pal::dim);
    for (int s = 0; s < 3; s++) {
        char info[64]; bool ok = save_info(s, info, sizeof info);
        int y = 58 + s * 40;
        gfx_fill(c, CX + 6, y, CW - 12, 34, pal::panel2); gfx_rect(c, CX + 6, y, CW - 12, 34, G.saveSlot == s ? pal::goldDk : pal::border);
        char t[16]; snprintf(t, sizeof t, "Slot %d", s + 1); gfx_text(c, CX + 12, y + 5, t, pal::gold);
        gfx_text(c, CX + 12, y + 18, ok ? info : "Empty", ok ? pal::cream : pal::dim);
    }
    gfx_textC(c, CX + CW / 2, 182, "v0.4.2  PC + 3DS", pal::grey);
    if (rail(c, 0, 0, 3, "Continue", UB_GOLD, save_exists(G.saveSlot) || any)) {
        int slot = G.saveSlot; if (!save_exists(slot)) for (int s = 0; s < 3; s++) if (save_exists(s)) { slot = s; break; }
        ask(CA_LOAD, slot, "Continue from slot %d?", slot + 1);
    }
    if (rail(c, 0, 1, 3, "New Game")) { saveMode = 2; ui_push(PN_SAVELOAD); }
    if (rail(c, 0, 2, 3, "Load Game", 0, any)) { saveMode = 0; ui_push(PN_SAVELOAD); }
    int nr = g_canQuit ? 4 : 3;
    if (rail(c, 1, 0, nr, "Settings")) ui_push(PN_SETTINGS);
    if (rail(c, 1, 1, nr, "Controls")) ui_push(PN_CONTROLS);
    if (rail(c, 1, 2, nr, "Credits")) ui_push(PN_CREDITS);
    if (g_canQuit && rail(c, 1, 3, nr, "Quit")) ask(CA_QUIT, 0, "Quit Staying Alive?");
}

static void doLoad(int slot) {
    char err[96];
    game_newGame();             // fresh world scaffolding, then overwrite with the save
    if (!save_read(slot, err, sizeof err)) {
        G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); say(err); return;
    }
    G.saveSlot = slot;
    // an expedition interrupted by quitting: the squad straggles home empty-handed
    if (G.expLoc >= 0) {
        for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].onExpedition) { G.st.surv[i].onExpedition = false; G.st.surv[i].hp = fmaxf(10, G.st.surv[i].hp - 15); }
        journal(JK_WARN, "The raid on the %s was abandoned. The squad straggled home.", LOCATIONS[G.expLoc].name);
        G.expLoc = -1;
    }
    level_enter(MAP_SETTLEMENT, -1);
    ui_open(PN_HUD);
    if (err[0]) game_toast("%s", err); else game_toast("Loaded slot %d.", slot + 1);
}

static void panelSaveLoad(Canvas& c) {
    static const char* T[] = {"Load Game", "Save Game", "New Game"};
    ui_panel(c, CX, 2, CW, 220, T[saveMode]);
    if (sel < 0 || sel > 2) sel = G.saveSlot;
    for (int s = 0; s < 3; s++) {
        int y = 22 + s * 62;
        char info[64]; bool ok = save_info(s, info, sizeof info);
        bool exists = save_exists(s);
        bool on = sel == s;
        if (ui_hit(CX + 6, y, CW - 12, 56)) { sel = s; audio_play(SFX_MENU_MOVE); }
        gfx_fill(c, CX + 6, y, CW - 12, 56, on ? pal::goldBg : pal::panel2); gfx_rect(c, CX + 6, y, CW - 12, 56, on ? pal::gold : pal::border);
        char t[16]; snprintf(t, sizeof t, "Slot %d", s + 1);
        gfx_textB(c, CX + 12, y + 6, t, pal::gold);
        gfx_wrap(c, CX + 12, y + 20, CW - 24, exists ? info : "Empty", ok ? pal::cream : (exists ? pal::red : pal::dim));
    }
    for (int s = 0; s < 3; s++) { char t[16]; snprintf(t, sizeof t, "Slot %d", s + 1); if (rail(c, 0, s, 3, t, sel == s ? UB_SELECTED : 0)) sel = s; }
    int s = sel; bool exists = save_exists(s); char info[64]; bool ok = save_info(s, info, sizeof info);
    const char* lbl = saveMode == 0 ? "Load" : (saveMode == 1 ? "Save" : "Start");
    if (rail(c, 1, 0, 3, lbl, UB_GOLD, saveMode == 0 ? ok : true)) {
        if (saveMode == 0) ask(CA_LOAD, s, "Load slot %d? Unsaved progress is lost.", s + 1);
        else if (saveMode == 1) { if (exists) ask(CA_SAVE, s, "Overwrite slot %d?", s + 1); else { G.saveSlot = s; say(save_write(s) ? "Game saved." : "Save FAILED. Previous save kept."); } }
        else { if (exists) ask(CA_NEWGAME, s, "Start a new game in slot %d? Its save will be replaced.", s + 1); else { G.saveSlot = s; game_newGame(); save_write(s); offerTutorial(); } }
    }
    if (rail(c, 1, 1, 3, "Delete", 0, exists && saveMode != 2)) ask(CA_DELETE, s, "Delete slot %d permanently?", s + 1);
    if (rail(c, 1, 2, 3, "Back", 0, true, -1, BTN_B) || ui_pressedB()) { sel = 0; ui_back(); }
}

// ---------------------------------------------------------------- settlement overview
// left rail tabs shared by the Base and Survivors screens
static bool baseTabs(Canvas& c, int active) {
    static const char* T[] = {"Buildings", "Survivors", "Jobs", "Resources"};
    static const int IC[] = {S_ICON_HOUSE, S_ICON_PEOPLE, S_ICON_CLOCK, S_ICON_MAT};
    for (int t = 0; t < 4; t++) if (rail(c, 0, t, 4, T[t], active == t ? UB_SELECTED : 0, true, IC[t])) {
        if (t == 1) { ui_open(PN_SURVIVORS); tab = 0; return true; }
        if (G.panel != PN_BASE) ui_open(PN_BASE);
        tab = t; return true;
    }
    if (ui_shoulderR()) { if (active == 0) { ui_open(PN_SURVIVORS); tab = 0; return true; } if (active == 2) tab = 3; }
    if (ui_shoulderL()) { if (active == 3) tab = 2; else if (active == 2) { ui_open(PN_SURVIVORS); tab = 0; return true; } }
    return false;
}

static void panelBase(Canvas& c) {
    int active = tab == 1 ? 0 : tab;
    if (baseTabs(c, active)) return;
    if (active == 0) {
        ui_panel(c, CX, 2, CW, 148, "Buildings");
        for (int b = 0; b < B_COUNT; b++) {
            char t[40]; int lv = G.st.bLevel[b];
            snprintf(t, sizeof t, "    %s", BUILDINGS[b].name);
            int y = 19 + b * 21;
            if (ui_button(c, CX + 4, y, CW - 8, 19, t, UB_LEFT | (sel == b ? UB_SELECTED : 0))) { if (sel == b) { g_buildingSel = b; ui_push(PN_BUILDING); } sel = b; }
            iconThird(c, g_building[b][lv > 3 ? 3 : (lv ? lv : 0)], CX + 7, y + 2);
            char l[12]; snprintf(l, sizeof l, lv ? "Lv %d" : "Empty", lv);
            gfx_text(c, CX + CW - 10 - gfx_textW(l), y + 6, l, lv ? pal::cream : pal::dim);
            if (G.st.bDamage[b]) gfx_text(c, CX + CW - 50, y + 6, "!", pal::red);
        }
        int b = sel < B_COUNT ? sel : 0, lv = G.st.bLevel[b];
        ui_panel(c, CX, 154, CW, 68);
        blitHalf(c, g_building[b][lv > 3 ? 3 : lv], CX + 5, 158);
        gfx_textB(c, CX + 34, 158, BUILDINGS[b].name, pal::cream);
        gfx_wrap(c, CX + 34, 170, CW - 40, BUILDINGS[b].desc, pal::dim);
        if (G.st.bDamage[b]) gfx_text(c, CX + 5, 186, "Damaged", pal::red);
        if (rail(c, 1, 0, 2, "Manage", UB_GOLD | hl(TH_BASE))) { g_buildingSel = b; ui_push(PN_BUILDING); }
        footer(c, "Select", "Back");
    } else if (active == 2) {
        ui_panel(c, CX, 2, CW, 220, "Jobs");
        int per = 8, start = page * per, shown = 0;
        int alive[MAX_SURV], na = 0;
        for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive) alive[na++] = i;
        for (int k = start; k < na && shown < per; k++, shown++) {
            Survivor& s = G.st.surv[alive[k]]; int y = 19 + shown * 22;
            headCrop(c, s, CX + 4, y);
            gfx_text(c, CX + 25, y + 5, s.name, s.onExpedition ? pal::cyan : pal::cream);
            if (ui_button(c, CX + 86, y, 18, 18, "<", 0, !s.onExpedition)) s.job = (uint8_t)((s.job + JOB_COUNT - 1) % JOB_COUNT);
            gfx_textC(c, CX + 136, y + 5, JOB_NAMES[s.job], pal::gold);
            if (ui_button(c, CX + CW - 22, y, 18, 18, ">", 0, !s.onExpedition)) s.job = (uint8_t)((s.job + 1) % JOB_COUNT);
        }
        char b[48]; snprintf(b, sizeof b, "Security: %d (%s)", settle_security(), security_rating(settle_security()));
        gfx_textC(c, CX + CW / 2, 204, b, settle_security() < 25 ? pal::red : pal::cream);
        if (rail(c, 1, 0, 3, "Prev Page", 0, page > 0)) page--;
        if (rail(c, 1, 1, 3, "Next Page", 0, (page + 1) * per < na)) page++;
        footer(c, "Change", "Back");
    } else {
        ui_panel(c, CX, 2, CW, 220, "Resources");
        static const int IC[RES_COUNT] = {S_ICON_FOOD, S_ICON_MED, S_ICON_AMMO, S_ICON_MAT, S_ICON_FUEL};
        for (int r = 0; r < RES_COUNT; r++) {
            int y = 22 + r * 14; char b[32];
            gfx_blit(c, g_spr[IC[r]], CX + 8, y); gfx_text(c, CX + 20, y, RES_NAMES[r], pal::cream);
            snprintf(b, sizeof b, "%d", G.st.res[r]); gfx_text(c, CX + CW - 10 - gfx_textW(b), y, b, pal::gold);
        }
        int food = 0, pop = settle_population();
        for (int i = 0; i < G.st.nsurv; i++) { const Survivor& s = G.st.surv[i]; if (s.alive && !s.onExpedition && !s.quarantined && s.job == JOB_FARMER && G.st.bLevel[B_KITCHEN]) food += 1 + G.st.bLevel[B_KITCHEN] + s.skill[SK_FARM] / 3; }
        if (G.st.generator) food += food / 2;
        food += G.st.bLevel[B_KITCHEN];
        char b[64]; int y = 100;
        snprintf(b, sizeof b, "Food per day: +%d / -%d", food, pop); gfx_text(c, CX + 8, y, b, food >= pop ? pal::green : pal::red); y += 13;
        snprintf(b, sizeof b, "Population: %d / %d", pop, settle_capacity()); gfx_text(c, CX + 8, y, b, pal::cream); y += 13;
        int sec = settle_security();
        snprintf(b, sizeof b, "Security: %d %s", sec, security_rating(sec)); gfx_text(c, CX + 8, y, b, sec < 25 ? pal::red : pal::cream); y += 11;
        ui_bar(c, CX + 8, y, CW - 16, 6, sec / 100.0f, sec < 25 ? pal::red : (sec < 45 ? pal::gold : pal::green)); y += 12;
        snprintf(b, sizeof b, "Walls: %d%%", G.st.wall); gfx_text(c, CX + 8, y, b, pal::cream); y += 13;
        gfx_text(c, CX + 8, y, G.st.generator ? "Generator: running" : "Generator: none", G.st.generator ? pal::green : pal::dim); y += 13;
        gfx_text(c, CX + 8, y, G.st.scanner ? "Med scanner: installed" : "Med scanner: none", G.st.scanner ? pal::green : pal::dim);
        if (rail(c, 1, 0, 3, "Journal", 0, true, S_RADIO)) ui_push(PN_JOURNAL);
        if (rail(c, 1, 1, 3, "Open Stash", 0, true, S_ICON_MAT)) { g_contProp = -2; g_contBag = -1; ui_push(PN_CONTAINER); }
        footer(c, "Select", "Back");
    }
    int nr = active == 0 ? 2 : 3;
    if (rail(c, 1, nr - 1, nr, "Back", 0, true, -1, BTN_B) || (ui_pressedB() && G.panel == PN_BASE)) ui_back();
}

// ---------------------------------------------------------------- building menu
static void panelBuilding(Canvas& c) {
    static const char* T[] = {"Build", "Upgrade", "Repair"};
    for (int t = 0; t < 3; t++) if (rail(c, 0, t, 3, T[t], tab == t ? UB_SELECTED : (t == 1 ? hl(TH_UPGRADE) : 0))) tab = t;   // (the tutorial pulses Upgrade)
    if (ui_shoulderR()) tab = (tab + 1) % 3; if (ui_shoulderL()) tab = (tab + 2) % 3;
    if (tab > 2) tab = 0;
    int b = g_buildingSel;
    // facility cards, 2 columns
    for (int k = 0; k < B_COUNT; k++) {
        int x = CX + (k % 2) * 98, y = 2 + (k / 2) * 40, lv = G.st.bLevel[k];
        bool on = k == b;
        if (ui_hit(x, y, 96, 38)) { g_buildingSel = k; b = k; audio_play(SFX_MENU_MOVE); }
        bool foc = g_ui.focus == g_ui.count - 1 && !g_ui.navLock;
        gfx_fill(c, x, y, 96, 38, on ? pal::goldBg : pal::panel2);
        gfx_rect(c, x, y, 96, 38, on ? pal::gold : (foc ? pal::goldDk : pal::border));
        blitHalf(c, g_building[k][lv > 3 ? 3 : (lv ? lv : 1)], x + 3, y + 7);
        if (!lv) gfx_fillA(c, x + 3, y + 7, 24, 24, pal::ink, 140);
        static const char* SHORT[B_COUNT] = {"Infirmary", "Workshop", "Storage", "Kitchen", "Quarters", "Defenses"};
        gfx_text(c, x + 30, y + 6, SHORT[k], pal::cream);
        char l[16]; snprintf(l, sizeof l, lv ? "Lv %d/%d" : "Not built", lv, BUILDINGS[k].maxLevel); gfx_text(c, x + 30, y + 17, l, lv ? pal::dim : pal::grey);
        if (G.st.bDamage[k]) gfx_text(c, x + 30, y + 27, "Damaged", pal::red);
    }
    const BuildingDef& d = BUILDINGS[b];
    int lv = G.st.bLevel[b];
    ui_panel(c, CX, 124, CW, 98);
    gfx_textB(c, CX + 5, 128, d.name, pal::cream);
    int ly = 140 + 10 * gfx_wrap(c, CX + 5, 140, CW - 10, d.desc, pal::dim);
    const uint8_t* cost = nullptr; bool can = false; const char* why = "";
    static const uint8_t repairCost[RES_COUNT] = {0, 0, 0, 6, 0};
    if (tab == 0) { cost = d.cost[0]; can = lv == 0; why = lv ? "Already built" : ""; }
    else if (tab == 1) { if (lv > 0 && lv < d.maxLevel) { cost = d.cost[lv]; can = true; } why = lv == 0 ? "Build it first" : (lv >= d.maxLevel ? "Max level" : ""); }
    else { cost = repairCost; can = G.st.bDamage[b] > 0; why = can ? "" : "No damage"; }
    ly += 2;
    if (why[0]) gfx_text(c, CX + 5, ly, why, pal::grey);   // the reason replaces the cost
    else if (cost) { gfx_text(c, CX + 5, ly, "Cost:", pal::dim); costLine(c, CX + 38, ly, cost); }
    ly += 12;
    if (tab == 1 && lv > 0 && lv < d.maxLevel) gfx_wrap(c, CX + 5, ly, CW - 10, d.perk[lv + 1], pal::green);
    else if (lv > 0) gfx_wrap(c, CX + 5, ly, CW - 10, d.perk[lv], pal::cream);
    bool afford = cost && can_afford(cost);
    // right rail: the tab's action, the facility's own actions, Back
    const char* fa[2] = {nullptr, nullptr};
    if (lv > 0) {
        if (b == B_INFIRMARY) fa[0] = "Patients";
        if (b == B_WORKSHOP) fa[0] = "Weapons";
        if (b == B_STORAGE) fa[0] = "Open Stash";
        if (b == B_DEFENSES) fa[0] = "Guards";
        if (b == B_KITCHEN) fa[0] = "Farmers";
        if (b == B_QUARTERS) { fa[0] = "Sleep"; fa[1] = "Rest 2h"; }
    }
    int nr = 2 + (fa[0] ? 1 : 0) + (fa[1] ? 1 : 0);
    if (rail(c, 1, 0, nr, T[tab], UB_GOLD | (tab == 1 ? hl(TH_UPGRADE) : 0), can && afford)) {
        pay(cost);
        if (tab == 2) { G.st.bDamage[b]--; journal(JK_INFO, "The %s was repaired.", d.name); }
        else {
            G.st.bLevel[b]++;
            journal(JK_GOOD, "%s %s to level %d.", d.name, tab == 0 ? "built" : "upgraded", G.st.bLevel[b]);
            for (int i = 0; i < G.map.nprop; i++) if (G.map.prop[i].type == PR_PLOT && G.map.prop[i].special == b) G.map.prop[i].solid = true;
            map_refresh(G.map);
            if (b == B_STORAGE) G.st.stash.cap = (uint8_t)settle_stashCap();
        }
        G.st.bBuildFx[b] = 2.0f;
        audio_play(SFX_BUILD);
        if (G.scene == SC_SETTLEMENT) save_write(G.saveSlot);
    }
    if (fa[0] && rail(c, 1, 1, nr, fa[0])) {
        if (b == B_INFIRMARY) ui_push(PN_INFIRMARY);
        else if (b == B_WORKSHOP) ui_push(PN_WORKSHOP);
        else if (b == B_STORAGE) { g_contProp = -2; g_contBag = -1; ui_push(PN_CONTAINER); }
        else if (b == B_DEFENSES || b == B_KITCHEN) { ui_open(PN_BASE); tab = 2; return; }
        else if (b == B_QUARTERS) {
            int hour = ((int)G.clock / 60) % 24;
            int hrs = hour >= 6 ? 30 - hour : 6 - hour; if (hrs < 1) hrs = 1;
            game_advanceHours((float)hrs);
            G.clock = 6 * 60 + 1;
            journal(JK_INFO, "%s slept until morning.", G.st.leader >= 0 ? G.st.surv[G.st.leader].name : "The leader");
            game_toast("You slept until 06:00.");
            ui_open(PN_HUD);
            return;
        }
    }
    if (fa[1] && rail(c, 1, 2, nr, fa[1])) { game_advanceHours(2); game_toast("Rested for two hours."); }
    if (rail(c, 1, nr - 1, nr, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- survivors
static void panelSurvivors(Canvas& c) {
    static const char* T[] = {"Survivors", "Squad", "Jobs", "Status"};
    static const int IC[] = {S_ICON_PEOPLE, S_ICON_SHIELD, S_ICON_CLOCK, S_ICON_SKULL};
    if (tab > 3) tab = 0;
    for (int t = 0; t < 4; t++) if (rail(c, 0, t, 4, T[t], tab == t ? UB_SELECTED : 0, true, IC[t])) { if (t == 2) { ui_open(PN_BASE); tab = 2; return; } tab = t; sel = 0; page = 0; }
    if (ui_shoulderR()) tab = (tab + 1) % 4; if (ui_shoulderL()) tab = (tab + 3) % 4;
    if (tab == 2) { ui_open(PN_BASE); tab = 2; return; }
    int ids[MAX_SURV], n = 0;
    for (int i = 0; i < G.st.nsurv; i++) {
        bool ok = tab == 3 ? true : G.st.surv[i].alive;
        if (tab == 1) ok = G.st.surv[i].alive && !G.st.surv[i].quarantined;
        if (ok) ids[n++] = i;
    }
    if (sel >= n) sel = n ? n - 1 : 0;
    const int LW = 80, per = 9;
    ui_panel(c, CX, 2, LW, 220);
    for (int k = page * per; k < n && k < (page + 1) * per; k++) {
        Survivor& s = G.st.surv[ids[k]]; int y = 5 + (k - page * per) * 22;
        if (ui_button(c, CX + 3, y, LW - 6, 20, "", sel == k ? UB_SELECTED : 0)) sel = k;
        for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) gfx_pset(c, CX + 5 + i, y + 2 + j, g_portrait[s.variant % PORTRAITS].d[(j + 3) * 24 + i + 4]);
        char nm[10]; snprintf(nm, sizeof nm, "%.8s", s.name);
        gfx_text(c, CX + 24, y + 6, nm, s.alive ? pal::cream : pal::grey);
        if (ids[k] == G.st.leader) gfx_text(c, CX + LW - 11, y + 6, "*", pal::gold);
    }
    if (n > per) {
        if (ui_button(c, CX + 3, 204, 34, 16, "<", 0, page > 0)) { page--; sel = page * per; }
        if (ui_button(c, CX + LW - 37, 204, 34, 16, ">", 0, (page + 1) * per < n)) { page++; sel = page * per; }
    }
    int DX = CX + LW + 3, DW = CW - LW - 3;
    ui_panel(c, DX, 2, DW, 220);
    bool canAct = false; int si = -1;
    if (!n) gfx_text(c, DX + 5, 10, "No survivors.", pal::dim);
    else {
        si = ids[sel]; Survivor& s = G.st.surv[si];
        gfx_blit(c, g_portrait[s.variant % PORTRAITS], DX + 5, 6);
        gfx_text(c, DX + 33, 7, s.name, pal::cream);
        char b[48]; snprintf(b, sizeof b, "Lv %d %s", survLevel(s), JOB_NAMES[s.job]); gfx_text(c, DX + 33, 18, b, pal::dim);
        gfx_text(c, DX + 5, 34, TRAIT_NAMES[s.trait], pal::gold);
        px sc; const char* st = survStatus(s, sc);
        gfx_wrap(c, DX + 5, 46, DW - 10, st, sc);
        gfx_text(c, DX + 5, 60, "Health", pal::dim); ui_bar(c, DX + 44, 61, DW - 50, 6, s.hp / s.maxHp, pal::green);
        if (tab == 0 || tab == 1) {
            for (int k = 0; k < SK_COUNT; k++) {
                int y = 74 + k * 12;
                gfx_text(c, DX + 5, y, SKILL_NAMES[k], pal::cream);
                ui_bar(c, DX + 58, y + 1, DW - 78, 6, s.skill[k] / 10.0f, pal::gold);
                char v[4]; snprintf(v, sizeof v, "%d", s.skill[k]); gfx_text(c, DX + DW - 16, y, v, pal::cream);
            }
            snprintf(b, sizeof b, "Kills: %d", s.kills); gfx_text(c, DX + 5, 74 + SK_COUNT * 12 + 4, b, pal::dim);
        } else {
            gfx_text(c, DX + 5, 76, s.alive ? "Alive" : "Deceased", s.alive ? pal::green : pal::grey);
            if (!s.alive) { snprintf(b, sizeof b, "Died on day %d", s.deathDay); gfx_text(c, DX + 5, 88, b, pal::dim); }
            if (s.infection && s.infKnown) { gfx_text(c, DX + 5, 100, INF_NAMES[s.infection], pal::red); ui_bar(c, DX + 5, 112, DW - 10, 5, s.infHours / 20.0f, pal::red); }
            snprintf(b, sizeof b, "Kills: %d", s.kills); gfx_text(c, DX + 5, 124, b, pal::cream);
        }
        canAct = s.alive && !s.onExpedition && G.scene == SC_SETTLEMENT;
    }
    Survivor* sp = si >= 0 ? &G.st.surv[si] : nullptr;
    if (rail(c, 1, 0, 4, "Set Leader", 0, sp && canAct && si != G.st.leader && !sp->quarantined)) {
        G.st.leader = si; level_enter(MAP_SETTLEMENT, -1); game_toast("You now control %s.", sp->name);
    }
    if (rail(c, 1, 1, 4, "Backpack", 0, sp && sp->alive && !sp->onExpedition, S_ICON_MAT)) { g_invSurvivor = si; ui_push(PN_INVENTORY); }
    if (rail(c, 1, 2, 4, sp && sp->quarantined ? "Release" : "Quarantine", 0, sp && canAct && G.st.bLevel[B_INFIRMARY] > 0)) {
        Survivor& s = *sp;
        s.quarantined = !s.quarantined;
        journal(JK_INFO, "%s was %s quarantine.", s.name, s.quarantined ? "moved to" : "released from");
        if (s.quarantined && si == G.st.leader) { for (int i = 0; i < G.st.nsurv; i++) if (i != si && G.st.surv[i].alive && !G.st.surv[i].quarantined) { G.st.leader = i; break; } level_enter(MAP_SETTLEMENT, -1); }
    }
    footer(c, "Select", "Back");
    if (rail(c, 1, 3, 4, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- recruitment
static void panelRecruit(Canvas& c) {
    Survivor& v = G.st.visitor;
    bool rescued = G.sum.rescued == 1;
    if (!G.st.visitorPresent && !rescued) { ui_back(); return; }
    ui_panel(c, CX, 2, CW, 220, rescued ? "Rescued Survivor" : "Stranger at the Gate");
    gfx_blit(c, g_portrait[v.variant % PORTRAITS], CX + 6, 20, 2);
    gfx_text(c, CX + 60, 22, v.name, pal::cream);
    char b[48];
    snprintf(b, sizeof b, "Health %d/%d", (int)v.hp, v.maxHp); gfx_text(c, CX + 60, 34, b, pal::cream);
    gfx_text(c, CX + 60, 46, TRAIT_NAMES[v.trait], pal::gold);
    int bestK = 0; for (int k = 1; k < SK_COUNT; k++) if (v.skill[k] > v.skill[bestK]) bestK = k;
    snprintf(b, sizeof b, "Best: %s", SKILL_NAMES[bestK]); gfx_text(c, CX + 60, 58, b, pal::dim);
    for (int k = 0; k < SK_COUNT; k++) {
        int y = 76 + k * 11;
        gfx_text(c, CX + 8, y, SKILL_NAMES[k], pal::cream); ui_bar(c, CX + 70, y + 1, CW - 80, 6, v.skill[k] / 10.0f, pal::gold);
    }
    int y = 76 + SK_COUNT * 11 + 4;
    const char* scr = G.st.visitorScreened ? (v.infection ? "Screening: INFECTED" : "Screening: clean") : "Screening: not done";
    gfx_text(c, CX + 8, y, scr, G.st.visitorScreened ? (v.infection ? pal::red : pal::green) : pal::dim); y += 12;
    if (v.infection && v.infKnown) { gfx_text(c, CX + 8, y, "Visibly infected", pal::red); y += 12; }
    snprintf(b, sizeof b, "Population %d/%d", settle_population(), settle_capacity()); gfx_text(c, CX + 8, y, b, pal::cream); y += 12;
    bool room = settle_population() < settle_capacity() && G.st.nsurv < MAX_SURV;
    if (!room) gfx_wrap(c, CX + 8, y, CW - 16, "No room. Upgrade the Living Quarters.", pal::red);
    else gfx_wrap(c, CX + 8, y, CW - 16, "Screening reliably detects infection.", pal::dim);
    auto accept = [&](bool quarantine) {
        Survivor s = v; s.quarantined = quarantine;
        if (G.st.visitorScreened && s.infection) s.infKnown = true;
        G.st.surv[G.st.nsurv++] = s;
        Settler& n = g_npc[G.st.nsurv - 1]; n.x = n.px = n.tx = G.map.exitX * 16 + 8.0f; n.y = n.py = n.ty = G.map.exitY * 16 - 8.0f; n.wait = 0; n.anim = Animator();
        journal(JK_GOOD, "%s joined the settlement%s.", s.name, quarantine ? " (in quarantine)" : "");
        G.st.visitorPresent = false; G.sum.rescued = -1;
        audio_play(SFX_HEAL);
        ui_open(PN_HUD);
    };
    if (rail(c, 1, 0, 3, "Accept", UB_GOLD, room)) { accept(false); return; }   // drawn first: default focus
    if (rail(c, 0, 0, 2, "Screen (1 Med)", 0, !G.st.visitorScreened && G.st.res[RES_MED] >= 1, S_ICON_MED)) {
        G.st.res[RES_MED]--; G.st.visitorScreened = true; audio_play(SFX_MENU_OK);
        if (v.infection) v.infKnown = true;
    }
    if (rail(c, 0, 1, 2, "Reject")) { ask(CA_REJECT, 0, "Turn %s away?", v.name); }
    if (rail(c, 1, 1, 3, "Quarantine", 0, room && G.st.bLevel[B_INFIRMARY] > 0)) { accept(true); return; }
    if (rail(c, 1, 2, 3, "Later", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- infirmary
static void panelInfirmary(Canvas& c) {
    int ids[MAX_SURV], n = 0;
    for (int i = 0; i < G.st.nsurv; i++) { const Survivor& s = G.st.surv[i]; if (s.alive && !s.onExpedition) ids[n++] = i; }
    ui_panel(c, CX, 2, CW, 220, "Infirmary");
    if (!n) {
        gfx_text(c, CX + 8, 22, "No patients.", pal::dim);
        if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
        return;
    }
    if (sel >= n) sel = n - 1;
    // patients: two columns of name buttons
    for (int k = 0; k < n && k < 8; k++) {
        const Survivor& s = G.st.surv[ids[k]];
        px col; survStatus(s, col);
        int x = CX + 4 + (k % 2) * 94, y = 19 + (k / 2) * 18;
        char nm[16]; snprintf(nm, sizeof nm, "  %.9s", s.name);
        if (ui_button(c, x, y, 92, 16, nm, UB_LEFT | (sel == k ? UB_SELECTED : 0))) sel = k;
        gfx_fill(c, x + 3, y + 5, 4, 6, col);
    }
    Survivor& s = G.st.surv[ids[sel]];
    int y0 = 19 + ((n < 8 ? n : 8) + 1) / 2 * 18 + 4;
    gfx_blit(c, g_portrait[s.variant % PORTRAITS], CX + 6, y0);
    gfx_text(c, CX + 34, y0 + 1, s.name, pal::cream);
    px col; const char* st = survStatus(s, col);
    gfx_text(c, CX + 34, y0 + 12, st, col);
    if (s.infection && s.infKnown) { gfx_blit(c, g_spr[S_ICON_BIO], CX + CW - 14, y0 + 1); ui_bar(c, CX + 6, y0 + 28, CW - 12, 6, (s.infection - 1 + s.infHours / 20.0f) / 3.0f, pal::red); }
    char b[64]; int y = y0 + 38;
    snprintf(b, sizeof b, "Health %d/%d", (int)s.hp, s.maxHp); gfx_text(c, CX + 6, y, b, pal::cream); y += 11;
    int lvl = G.st.bLevel[B_INFIRMARY];
    snprintf(b, sizeof b, "Infirmary Lv %d%s", lvl, G.st.scanner ? " + scanner" : ""); gfx_text(c, CX + 6, y, b, pal::dim); y += 13;
    bool showInf = s.infection && s.infKnown;
    int cost = showInf ? treat_cost(s) : (s.hp < s.maxHp ? 1 : 0);
    const char* desc = showInf ? (s.infection == INF_EARLY ? "Early infection. Can be cured with treatment." : (s.infection == INF_ADVANCED ? "Advanced infection. Needs Infirmary Lv 2." : "Critical! Needs Infirmary Lv 3 or they will turn.")) : (s.hp < s.maxHp ? "Injured. Treatment restores health." : "Healthy.");
    gfx_wrap(c, CX + 6, y, CW - 12, desc, pal::cream);
    snprintf(b, sizeof b, "Treat (Med x%d)", cost);
    if (rail(c, 1, 0, 4, b, UB_GOLD, cost > 0 && G.st.res[RES_MED] >= cost && lvl > 0, S_ICON_MED)) {
        bool cured = false;
        if (showInf || s.infection == INF_NONE) settle_treat(ids[sel], cured);
        else { G.st.res[RES_MED] -= 1; s.hp = fminf(s.maxHp, s.hp + 40); cured = true; }   // hidden infection: injury care only
        audio_play(SFX_HEAL);
        say(cured ? "Treatment succeeded." : "Treatment failed. The infection was held back.");
    }
    if (rail(c, 1, 1, 4, s.quarantined ? "Release" : "Quarantine", 0, lvl > 0)) {
        s.quarantined = !s.quarantined; journal(JK_INFO, "%s %s quarantine.", s.name, s.quarantined ? "entered" : "left");
    }
    if (rail(c, 1, 2, 4, "Examine (1 Med)", 0, G.st.res[RES_MED] >= 1 && !s.infKnown)) {
        G.st.res[RES_MED]--;
        if (s.infection) { s.infKnown = true; say("Examination found an infection!"); journal(JK_WARN, "Screening found %s infected.", s.name); }
        else say("No infection found.");
    }
    if (rail(c, 1, 3, 4, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
    if (rail(c, 0, 0, 2, "Prev", 0, sel > 0)) sel--;
    if (rail(c, 0, 1, 2, "Next", 0, sel < n - 1)) sel++;
}

// ---------------------------------------------------------------- workshop
struct WRef { int8_t sv; int8_t idx; bool stash; };
static void panelWorkshop(Canvas& c) {
    WRef list[64]; int n = 0;
    for (int i = 0; i < G.st.nsurv && n < 64; i++) {
        const Survivor& s = G.st.surv[i]; if (!s.alive || s.onExpedition) continue;
        for (int k = 0; k < s.pack.n && n < 64; k++) { int cat = idef(s.pack.it[k].id).cat; if (cat == IC_FIREARM || cat == IC_MELEE) list[n++] = WRef{(int8_t)i, (int8_t)k, false}; }
    }
    for (int k = 0; k < G.st.stash.n && n < 64; k++) { int cat = idef(G.st.stash.it[k].id).cat; if (cat == IC_FIREARM || cat == IC_MELEE) list[n++] = WRef{-1, (int8_t)k, true}; }
    ui_panel(c, CX, 2, CW, 220, "Workshop");
    if (!n) {
        gfx_text(c, CX + 8, 22, "No weapons to service.", pal::dim);
        if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
        return;
    }
    int per = 6; if (sel >= n) sel = n - 1;
    for (int k = page * per; k < n && k < (page + 1) * per; k++) {
        Item& it = list[k].stash ? G.st.stash.it[list[k].idx] : G.st.surv[list[k].sv].pack.it[list[k].idx];
        const ItemDef& d = idef(it.id); int y = 19 + (k - page * per) * 17;
        if (ui_button(c, CX + 4, y, CW - 8, 15, d.name, UB_LEFT | (sel == k ? UB_SELECTED : 0))) sel = k;
        if (d.wpn.dur) ui_bar(c, CX + CW - 52, y + 5, 44, 5, (float)it.dur / d.wpn.dur, it.dur * 4 < d.wpn.dur ? pal::red : pal::green);
        else gfx_text(c, CX + CW - 30, y + 4, "inf", pal::dim);
    }
    Item& it = list[sel].stash ? G.st.stash.it[list[sel].idx] : G.st.surv[list[sel].sv].pack.it[list[sel].idx];
    const ItemDef& d = idef(it.id);
    int y = 124;
    gfx_blit(c, g_item[it.id], CX + 6, y);
    gfx_text(c, CX + 6 + g_item[it.id].w + 6, y + 2, d.name, pal::cream); y += 20;
    char b[48];
    snprintf(b, sizeof b, "Owner: %s", list[sel].stash ? "Stash" : G.st.surv[list[sel].sv].name); gfx_text(c, CX + 6, y, b, pal::dim); y += 11;
    if (d.wpn.dur) snprintf(b, sizeof b, "Durability %d/%d", it.dur, d.wpn.dur); else snprintf(b, sizeof b, "Never wears out");
    gfx_text(c, CX + 6, y, b, pal::cream); y += 11;
    snprintf(b, sizeof b, "Upgrade level %d/2", it.mod); gfx_text(c, CX + 6, y, b, pal::gold); y += 13;
    int rc = repair_cost(it);
    uint8_t rcost[RES_COUNT] = {0, 0, 0, (uint8_t)rc, 0};
    gfx_text(c, CX + 6, y, "Repair:", pal::dim); costLine(c, CX + 56, y, rcost); y += 12;
    uint8_t ucost[RES_COUNT] = {0, 0, 0, 6, 2};
    gfx_text(c, CX + 6, y, "Upgrade:", pal::dim); costLine(c, CX + 56, y, ucost);
    if (G.st.bLevel[B_WORKSHOP] < 2) gfx_text(c, CX + 6, y + 12, "Upgrades need Workshop Lv 2.", pal::dim);
    bool canUp = G.st.bLevel[B_WORKSHOP] >= 2 && it.mod < 2 && d.cat == IC_FIREARM;
    if (rail(c, 1, 0, 3, "Repair", UB_GOLD, rc > 0 && G.st.res[RES_MAT] >= rc && G.st.bLevel[B_WORKSHOP] > 0)) {
        G.st.res[RES_MAT] -= rc; it.dur = d.wpn.dur; audio_play(SFX_BUILD); say("Repaired.");
    }
    if (rail(c, 1, 1, 3, "Upgrade +8 acc", 0, canUp && can_afford(ucost))) { pay(ucost); it.mod++; audio_play(SFX_BUILD); say("Weapon upgraded."); }
    if (rail(c, 1, 2, 3, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
    if (rail(c, 0, 0, 2, "Prev Page", 0, page > 0)) { page--; sel = page * per; }
    if (rail(c, 0, 1, 2, "Next Page", 0, (page + 1) * per < n)) { page++; sel = page * per; }
}

// ---------------------------------------------------------------- world map + prep
static int g_mapSel = 0;
static void mapThumb(Canvas& c, int x, int y, int w, int h, int loc) {
    gfx_fill(c, x, y, w, h, rgb(16, 16, 28));
    if (LOCATIONS[loc].map < 0) {
        gfx_textC(c, x + w / 2, y + h / 2 - 8, "UNSCOUTED", pal::grey);
        gfx_textC(c, x + w / 2, y + h / 2 + 4, "Route not yet found", pal::grey);
    } else {
        static Map thumb; static int loaded = -1;
        if (loaded != LOCATIONS[loc].map) { map_load(thumb, LOCATIONS[loc].map); loaded = LOCATIONS[loc].map; }
        int sc = 2, ox = x + (w - thumb.w * sc) / 2, oy = y + (h - thumb.h * sc) / 2;
        for (int ty = 0; ty < thumb.h; ty++) for (int tx = 0; tx < thumb.w; tx++) {
            uint8_t t = thumb.t[thumb.idx(tx, ty)];
            px col = t == T_WALL ? rgb(120, 124, 160) : t == T_DOOR ? rgb(236, 196, 72) : t == T_GRASS ? rgb(70, 116, 70) : (t == T_ASPHALT || t == T_LINE) ? rgb(62, 64, 80) : t == T_EXTRACT ? pal::lime
                   : t == T_WATER ? rgb(30, 78, 104) : t == T_RAIL ? rgb(96, 90, 86) : t == T_DIRT ? rgb(150, 116, 80) : t == T_WOOD ? rgb(166, 118, 76) : t == T_CARPET ? rgb(70, 90, 140) : t == T_CONCRETE ? rgb(140, 140, 138) : rgb(150, 160, 170);
            int X = ox + tx * sc, Y = oy + ty * sc;
            if (X >= x && Y >= y && X + sc <= x + w && Y + sc <= y + h) gfx_fill(c, X, Y, sc, sc, col);
        }
    }
    gfx_rect(c, x, y, w, h, pal::border);
}

static void panelWorldMap(Canvas& c) {
    // left rail: the locations (thumb-pickable); middle: preview + intel; right rail: Deploy / Back
    for (int l = 0; l < LOC_COUNT; l++) {
        const char* nm = LOCATIONS[l].name;   // the thumb buttons are 58 px wide: long single words get a short form (the full name is in the panel)
        if (!strcmp(nm, "Supermarket")) nm = "Market"; else if (!strcmp(nm, "Neighborhood")) nm = "Homes";
        if (rail(c, 0, l, LOC_COUNT, nm, sel == l ? UB_SELECTED : 0)) sel = l;
        if (LOCATIONS[l].map < 0) { int h = (RH - (LOC_COUNT - 1) * 4) / LOC_COUNT; gfx_fillA(c, RLX, RTOP + l * (h + 4), RW, h, pal::ink, 90); }
    }
    if (sel >= LOC_COUNT) sel = 0;
    ui_panel(c, CX, 2, CW, 220, "World Map");
    mapThumb(c, CX + 4, 18, CW - 8, 110, sel);
    const LocationDef& L = LOCATIONS[sel];
    gfx_textB(c, CX + 6, 132, L.name, pal::cream);
    char b[64]; int y = 145;
    snprintf(b, sizeof b, "Loot: %s", L.loot); y += 10 * gfx_wrap(c, CX + 6, y, CW - 12, b, pal::dim);
    snprintf(b, sizeof b, "Threat: %s", L.threat); gfx_text(c, CX + 6, y, b, pal::dim); y += 12;
    const LocState& S = G.loc[sel];
    if (L.map < 0) snprintf(b, sizeof b, "Not yet surveyed");
    else if (S.cooldown >= 0) snprintf(b, sizeof b, "Restock in %d:%02d", (int)S.cooldown / 60, (int)S.cooldown % 60);
    else snprintf(b, sizeof b, S.visited ? "Cooldown: Ready" : "Unexplored");
    gfx_text(c, CX + 6, y, b, S.cooldown >= 0 ? pal::gold : pal::green);
    if (rail(c, 1, 0, 2, "Deploy", UB_GOLD, L.map >= 0 && G.scene == SC_SETTLEMENT, S_ICON_TRAIN)) {
        G.squad[0] = (int8_t)G.st.leader; G.squad[1] = G.squad[2] = -1; g_mapSel = sel;
        ui_push(PN_PREP);
    }
    if (rail(c, 1, 1, 2, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

static bool eligible(int i) { const Survivor& s = G.st.surv[i]; return s.alive && !s.quarantined && s.hp >= 30; }

static void panelPrep(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Raid Prep");
    if (G.squad[0] < 0 || !eligible(G.squad[0])) { for (int i = 0; i < G.st.nsurv; i++) if (eligible(i)) { G.squad[0] = (int8_t)i; break; } }
    if (G.squad[0] < 0) {
        gfx_text(c, CX + 8, 22, "Nobody is fit to travel.", pal::red);
        if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
        return;
    }
    auto cycle = [&](int dir) { int i = G.squad[0]; for (int k = 0; k < G.st.nsurv; k++) { i = (i + G.st.nsurv + dir) % G.st.nsurv; if (eligible(i) && i != G.squad[1] && i != G.squad[2]) break; } G.squad[0] = (int8_t)i; };
    if (rail(c, 0, 0, 2, "Prev Leader")) cycle(-1);
    if (rail(c, 0, 1, 2, "Next Leader")) cycle(1);
    Survivor& lead = G.st.surv[G.squad[0]];
    gfx_text(c, CX + 6, 18, "Leader (you control):", pal::dim);
    gfx_blit(c, g_portrait[lead.variant % PORTRAITS], CX + 6, 29);
    gfx_text(c, CX + 34, 31, lead.name, pal::cream);
    char b[64]; snprintf(b, sizeof b, "HP %d  Sh %d  Me %d", (int)lead.hp, lead.skill[SK_SHOOT], lead.skill[SK_MELEE]); gfx_text(c, CX + 34, 42, b, pal::dim);
    const Item* w = lead.equip >= 0 && lead.equip < lead.pack.n ? &lead.pack.it[lead.equip] : nullptr;
    if (w) snprintf(b, sizeof b, "Weapon: %s", idef(w->id).name); else snprintf(b, sizeof b, "Weapon: none");
    if (w && idef(w->id).cat == IC_FIREARM) snprintf(b, sizeof b, "%s %d+%d rds", idef(w->id).name, w->mag, lead.pack.countOf(idef(w->id).wpn.ammo));
    gfx_text(c, CX + 6, 58, b, pal::cream);
    snprintf(b, sizeof b, "Backpack %dx%d, %d items", lead.pack.w, lead.pack.h, lead.pack.n); gfx_text(c, CX + 6, 69, b, pal::cream);
    gfx_text(c, CX + 6, 84, "Companions (max 2):", pal::dim);
    int shown = 0;
    for (int i = 0; i < G.st.nsurv && shown < 6; i++) {
        if (i == G.squad[0] || !eligible(i)) continue;
        Survivor& s = G.st.surv[i];
        bool on = G.squad[1] == i || G.squad[2] == i;
        snprintf(b, sizeof b, "%s %s (%s)", on ? "[x]" : "[ ]", s.name, JOB_NAMES[s.job]);
        if (ui_button(c, CX + 4, 95 + shown * 16, CW - 8, 15, b, UB_LEFT | (on ? UB_SELECTED : 0))) {
            if (on) { if (G.squad[1] == i) G.squad[1] = G.squad[2]; G.squad[2] = -1; }
            else if (G.squad[1] < 0) G.squad[1] = (int8_t)i; else if (G.squad[2] < 0) G.squad[2] = (int8_t)i; else say("Squad is full (3).");
        }
        shown++;
    }
    bool saved[3]; for (int k = 0; k < 3; k++) { saved[k] = G.squad[k] >= 0 && G.st.surv[G.squad[k]].onExpedition; if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = true; }
    int sec = settle_security();
    for (int k = 0; k < 3; k++) if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = saved[k];
    snprintf(b, sizeof b, "Security while away: %d", sec);
    gfx_text(c, CX + 6, 196, b, sec < 25 ? pal::red : pal::cream);
    gfx_text(c, CX + 6, 207, sec < 25 ? "Settlement left exposed!" : security_rating(sec), sec < 25 ? pal::red : pal::dim);
    if (rail(c, 1, 0, 3, "Deploy", UB_GOLD, true, S_ICON_TRAIN)) { expedition_begin(g_mapSel); return; }
    if (rail(c, 1, 1, 3, "Review Pack", 0, true, S_ICON_MAT)) { g_invSurvivor = G.squad[0]; ui_push(PN_INVENTORY); }
    if (rail(c, 1, 2, 3, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

static void panelSummary(Canvas& c) {
    Summary& s = G.sum;
    ui_panel(c, CX, 2, CW, 220, s.failed ? "Raid Failed" : "Extraction");
    int y = 20;
    for (int i = 0; i < s.nlines; i++) y += 10 * gfx_wrap(c, CX + 6, y, CW - 12, s.lines[i], i == 0 ? pal::gold : (strstr(s.lines[i], "died") ? pal::red : pal::cream)) + 2;
    resRow(c, CX + 6, 190, true);
    if (G.st.unread) { char b[40]; snprintf(b, sizeof b, "%d new journal entries", G.st.unread); gfx_text(c, CX + 6, 204, b, pal::gold); }
    if (s.rescued == 1 && rail(c, 0, 0, 1, "Meet the rescued", UB_GOLD, true, S_ICON_PEOPLE)) { ui_push(PN_RECRUIT); return; }
    if (rail(c, 1, 0, 1, "Return Home", s.rescued == 1 ? 0 : UB_GOLD, true, S_ICON_HOUSE) || (ui_pressedB() && s.rescued != 1)) {
        if (s.rescued == 1) { s.rescued = -1; journal(JK_INFO, "%s decided to move on.", G.st.visitor.name); }
        ui_open(G.st.unread ? PN_JOURNAL : PN_HUD);
    }
}

static void panelJournal(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Incident Journal");
    int per = 6, n = G.st.njournal;
    int y = 19;
    for (int k = 0; k < per; k++) {
        int i = n - 1 - (page * per + k); if (i < 0) break;
        const JournalEntry& e = G.st.journal[i];
        static const px COL[4] = {pal::cream, pal::green, pal::gold, pal::red};
        char d[12]; snprintf(d, sizeof d, "D%u", e.day);
        gfx_text(c, CX + 5, y, d, pal::dim);
        if (e.kind == JK_DEATH) gfx_blit(c, g_spr[S_ICON_SKULL], CX + 22, y);
        int lines = gfx_wrap(c, CX + 34, y, CW - 40, e.text, COL[e.kind & 3]);
        y += lines * 10 + 5;
        if (y > 206) break;
    }
    if (!n) gfx_text(c, CX + 8, 22, "Nothing has happened yet.", pal::dim);
    G.st.unread = 0;
    if (rail(c, 0, 0, 2, "Newer", 0, page > 0)) page--;
    if (rail(c, 0, 1, 2, "Older", 0, (page + 1) * per < n)) page++;
    if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- settings / controls / credits
static void panelSettings(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Settings");
    Settings& s = G.set;
    auto slider = [&](int y, const char* name, uint8_t& v) {
        gfx_text(c, CX + 5, y + 5, name, pal::cream);
        if (ui_button(c, CX + 54, y, 20, 18, "-", 0, v > 0)) v--;
        ui_bar(c, CX + 78, y + 5, CW - 108, 8, v / 10.0f, pal::gold);
        if (ui_button(c, CX + CW - 26, y, 20, 18, "+", 0, v < 10)) v++;
    };
    slider(18, "Master", s.master); slider(39, "Effects", s.sfx); slider(60, "Music", s.music);
    char b[48]; int y = 84;
    auto row = [&](const char* label) { bool a = ui_button(c, CX + 4, y, CW - 8, 18, label); y += 21; return a; };
    snprintf(b, sizeof b, "Day length: %d min", s.dayMinutes);
    if (row(b)) s.dayMinutes = s.dayMinutes == 15 ? 30 : (s.dayMinutes == 30 ? 60 : 15);
    snprintf(b, sizeof b, "Camera zoom: %d%%", s.zoom);
    if (row(b)) { s.zoom = s.zoom >= 200 ? 100 : (s.zoom >= 150 ? 200 : (s.zoom >= 125 ? 150 : 125)); settings_save(); }
    if (row(s.shake ? "Screen shake: On" : "Screen shake: Off")) s.shake = !s.shake;
    if (row(s.showFps ? "Show FPS: On" : "Show FPS: Off")) s.showFps = !s.showFps;
    if (!g_canQuit) {   // the console: Fast keeps the Old 3DS at 30 fps
        snprintf(b, sizeof b, "Graphics: %s", s.gfx == 1 ? "Full" : (s.gfx == 2 ? "Fast" : (g_lowEnd ? "Auto (Fast)" : "Auto (Full)")));
        if (row(b)) s.gfx = (uint8_t)((s.gfx + 1) % 3);
    }
    if (g_canQuit) {
        snprintf(b, sizeof b, "Window scale: %dx", s.winScale);
        if (row(b)) s.winScale = s.winScale >= 3 ? 1 : s.winScale + 1;
        if (row(s.fullscreen ? "Fullscreen: On" : "Fullscreen: Off")) s.fullscreen = !s.fullscreen;
    }
    bool canTut = G.scene == SC_SETTLEMENT && G.inLevel;
    if (rail(c, 0, 0, 3, "Controls")) ui_push(PN_CONTROLS);
    if (rail(c, 0, 1, 3, tut_active() ? "Tutorial running" : "Replay Tutorial", 0, canTut && !tut_active())) { tut_start(); ui_open(PN_HUD); return; }
    if (rail(c, 0, 2, 3, "Anim Test Room")) { ui_push(PN_ANIMTEST); }
    if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) { settings_save(); ui_back(); }
}
static void panelControls(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Controls");
    // one row per action: the glyph shown is exactly what to press on this device
    struct Row { uint32_t b; const char* action; };
    static const Row rows[] = {
        {BTN_STICK, "Move / aim"}, {BTN_Y, "Fire / swing"}, {BTN_A, "Interact, search, doors"},
        {BTN_B, "Hold sprint, tap dodge"}, {BTN_X, "Reload"}, {BTN_L, "Hold: lock facing"},
        {BTN_R, "Flashlight"}, {BTN_DPAD_LR, "Switch weapon"}, {BTN_UP, "Quick heal"},
        {BTN_DOWN, "Squad orders"}, {BTN_SELECT, "Backpack"}, {BTN_START, "Pause"},
        {BTN_TOUCH, "Side buttons, drag items"},
    };
    int y = 19;
    for (const Row& r : rows) {
        int w = ui_btnW(r.b);
        ui_btn(c, CX + 46 - w, y, r.b);                  // glyphs right-aligned in a column
        gfx_text(c, CX + 52, y + 2, r.action, pal::cream);
        y += 14;
    }
    if (g_canQuit) gfx_text(c, CX + 5, 206, "F11 fullscreen  F12 screenshot", pal::dim);
    else gfx_text(c, CX + 5, 206, "Menus: D-Pad / Circle Pad + A", pal::dim);
    if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}
static void panelCredits(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Credits");
    gfx_wrap(c, CX + 6, 22, CW - 12, "STAYING ALIVE\n\nDesign, code, pixel art and audio created for this project. All art and sound are original and generated in code.\n\nSoundtrack: seven original pieces for a small synthesiser, one for the menus and one for each place.\n\nBuilt with devkitPro / libctru (3DS) and Win32 (PC).\n\nThanks for playing.", pal::cream);
    if (rail(c, 1, 0, 1, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}
static void panelDeath(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Survivor Lost");
    gfx_blit(c, g_spr[S_ICON_SKULL], CX + CW / 2 - 9, 30, 2);
    gfx_wrap(c, CX + 8, 64, CW - 16, "Your survivor has died. Death is permanent. Choose who leads now.", pal::cream);
    if (rail(c, 1, 0, 1, "Continue", UB_GOLD)) {
        if (settle_population() == 0) { journal(JK_DEATH, "The settlement has fallen."); G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); say("Everyone is gone. The settlement has fallen."); return; }
        level_enter(MAP_SETTLEMENT, -1); ui_open(PN_HUD);
    }
}

// animation test room: preview every state in all 8 directions (top screen)
int g_animTestState = 0, g_animTestVariant = 0; bool g_animTestZombie = false; float g_animTestSpeed = 1;
static void panelAnimTest(Canvas& c) {
    ui_panel(c, CX, 2, CW, 220, "Animation Test");
    char b[48];
    snprintf(b, sizeof b, "State: %s", anim_name((AnimState)g_animTestState));
    gfx_textC(c, CX + CW / 2, 22, b, pal::gold);
    snprintf(b, sizeof b, "%s %d  speed %.2fx", g_animTestZombie ? "Zombie" : "Survivor", g_animTestVariant, g_animTestSpeed); gfx_textC(c, CX + CW / 2, 34, b, pal::cream);
    const AnimDef& d = anim_def((AnimState)g_animTestState);
    snprintf(b, sizeof b, "%d frames  %s  prio %d", d.n, d.loop ? "loop" : "once", d.priority); gfx_text(c, CX + 6, 52, b, pal::cream);
    int total = 0; for (int i = 0; i < d.n; i++) total += d.f[i].ms10 * 10;
    snprintf(b, sizeof b, "cycle %d ms  %s", total, d.interruptible ? "interrupt" : "locked"); gfx_text(c, CX + 6, 64, b, pal::cream);
    for (int i = 0; i < d.n; i++) {
        snprintf(b, sizeof b, "f%d p%-2d %3dms k%+d%s%s", i, d.f[i].pose, d.f[i].ms10 * 10, d.f[i].kick,
                 d.f[i].event & EV_FOOTSTEP ? " STEP" : "", d.f[i].event & (EV_MUZZLE | EV_IMPACT) ? " HIT" : "");
        gfx_text(c, CX + 6, 80 + i * 10, b, pal::dim);
    }
    if (rail(c, 0, 0, 4, "Prev State")) g_animTestState = (g_animTestState + (int)AnimState::Count - 1) % (int)AnimState::Count;
    if (rail(c, 0, 1, 4, "Next State")) g_animTestState = (g_animTestState + 1) % (int)AnimState::Count;
    if (rail(c, 0, 2, 4, g_animTestZombie ? "Zombie" : "Survivor")) { g_animTestZombie = !g_animTestZombie; g_animTestVariant = 0; }
    if (rail(c, 0, 3, 4, "Variant")) g_animTestVariant = (g_animTestVariant + 1) % (g_animTestZombie ? ZOMB_VARIANTS : SURV_VARIANTS);
    if (rail(c, 1, 0, 2, "Speed")) g_animTestSpeed = g_animTestSpeed >= 1 ? 0.25f : g_animTestSpeed * 2;
    if (rail(c, 1, 1, 2, "Back", 0, true, -1, BTN_B) || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- dispatcher
static void confirmDialog(Canvas& c) {
    gfx_fillA(c, 0, 0, BOT_W, BOT_H, pal::ink, 150);
    ui_panel(c, 30, 60, 260, 110, "Confirm");
    gfx_wrap(c, 40, 82, 240, confirmBox.text, pal::cream);
    bool yes = ui_button(c, 46, 140, 104, 22, "Yes", UB_GOLD);
    bool no = ui_button(c, 170, 140, 104, 22, "No") || ui_pressedB();
    if (no) {
        if (confirmBox.action == CA_TUTORIAL) { G.set.tutorialDone = true; settings_save(); game_toast("Tutorial skipped. Replay it from Settings."); }
        confirmBox.active = false; ui_resetFocus(); return;
    }
    if (!yes) return;
    confirmBox.active = false; ui_resetFocus();
    int a = confirmBox.arg;
    switch (confirmBox.action) {
    case CA_NEWGAME: G.saveSlot = a; save_delete(a); game_newGame(); save_write(a); offerTutorial(); break;
    case CA_TUTORIAL: tut_start(); break;
    case CA_LOAD: doLoad(a); break;
    case CA_SAVE: G.saveSlot = a; say(save_write(a) ? "Game saved." : "Save FAILED. Previous save kept."); break;
    case CA_DELETE: save_delete(a); say("Slot deleted."); break;
    case CA_MAINMENU: level_leave(); G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); break;
    case CA_REJECT: G.st.visitorPresent = false; G.sum.rescued = -1; journal(JK_INFO, "%s was turned away.", G.st.visitor.name); ui_open(PN_HUD); break;
    case CA_QUIT: G.quitRequested = true; break;
    }
}

void ui_render(Canvas& bot) {
    ui_bg(bot);
    Input in = g_input;
    const uint32_t PLAY = BTN_A | BTN_B | BTN_X | BTN_Y | BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_L | BTN_R;
    if (G.panel == PN_HUD) { in.down &= ~PLAY; in.held &= ~PLAY; in.ax = in.ay = 0; }   // gameplay owns the pad; HUD is touch-only
    // backpack grids own the D-pad / circle pad until the cursor is pushed off the right edge onto the buttons
    bool gridPanel = G.panel == PN_CONTAINER || (G.panel == PN_INVENTORY && tab == 0);
    if (confirmBox.active) { in = g_input; gridPanel = false; }
    if (!gridPanel) inv.onRail = false;
    int preFocus = g_ui.focus; bool wasRail = inv.onRail;
    if (gridPanel && !inv.onRail) g_ui.navLockNext = true;
    ui_beginFrame(in);
    if (G.panel == PN_HUD && !confirmBox.active) g_ui.navLock = true;   // no focus box on the touch-only HUD
    if (gridPanel && wasRail && preFocus < g_ui.prevCount) {
        const UiRect& r = g_ui.prev[preFocus];
        bool onRight = r.x >= RRX, onLeft = r.x + r.w <= RLX + RW;
        if ((onRight && g_ui.navDir == 3) || (onLeft && g_ui.navDir == 4) || (in.down & BTN_B)) {
            g_ui.focus = preFocus; inv.onRail = false; g_ui.navLock = true; g_ui.navDir = 0;   // left (or B) from the buttons: back to the grid
            g_ui.in.down &= ~BTN_B;
        }
    }
    // short slide/fade-in for panel changes (UI keeps animating while the world is paused)
    float t = ui_ease(G.panelT / 0.15f);
    bot.ox = (int)((1 - t) * 10);
    Ui saved = g_ui;
    if (confirmBox.active) { g_ui.in.down = 0; g_ui.in.touchDown = g_ui.in.touchUp = false; }
    switch (G.panel) {
    case PN_MAIN: panelMain(bot); break;
    case PN_HUD: panelHud(bot); break;
    case PN_PAUSE: panelPause(bot); break;
    case PN_INVENTORY: panelInventory(bot); break;
    case PN_CONTAINER: panelContainer(bot); break;
    case PN_BASE: panelBase(bot); break;
    case PN_BUILDING: panelBuilding(bot); break;
    case PN_SURVIVORS: panelSurvivors(bot); break;
    case PN_RECRUIT: panelRecruit(bot); break;
    case PN_INFIRMARY: panelInfirmary(bot); break;
    case PN_WORKSHOP: panelWorkshop(bot); break;
    case PN_WORLDMAP: panelWorldMap(bot); break;
    case PN_PREP: panelPrep(bot); break;
    case PN_SUMMARY: panelSummary(bot); break;
    case PN_JOURNAL: panelJournal(bot); break;
    case PN_SETTINGS: panelSettings(bot); break;
    case PN_CONTROLS: panelControls(bot); break;
    case PN_SAVELOAD: panelSaveLoad(bot); break;
    case PN_CREDITS: panelCredits(bot); break;
    case PN_DEATH: panelDeath(bot); break;
    case PN_ANIMTEST: panelAnimTest(bot); break;
    case PN_SQUAD: panelSquad(bot); break;
    default: ui_open(G.scene == SC_TITLE ? PN_MAIN : PN_HUD); break;
    }
    bot.ox = 0;
    if (confirmBox.active) {
        // modal: only the dialog's own widgets are focusable
        g_ui.count = 0; g_ui.in = in; g_ui.consumed = false;
        if (g_ui.prevCount > 2) { g_ui.prevCount = 0; g_ui.focus = 0; }
        confirmDialog(bot);
    }
    (void)saved;
    if (msgT > 0) {
        msgT -= G.frameMs / 1000.0f;
        int w = gfx_textW(msg) + 12; if (w > 316) w = 316;
        gfx_fill(bot, 160 - w / 2, 208, w, 14, pal::ink); gfx_rect(bot, 160 - w / 2, 208, w, 14, pal::gold);
        gfx_textC(bot, 160, 211, msg, pal::cream);
    }
    ui_endFrame();
    for (int b = 0; b < B_COUNT; b++) if (G.st.bBuildFx[b] > 0) G.st.bBuildFx[b] -= G.frameMs / 1000.0f;
}
