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
static void offerTutorial() { if (!G.set.tutorialDone) ask(CA_TUTORIAL, 0, "Play the tutorial? Recommended if you are new. You can skip it at any time."); }

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
    gfx_fill(c, 0, 224, BOT_W, 16, rgb(16, 16, 18));
    gfx_fill(c, 0, 224, BOT_W, 1, rgb(84, 82, 88));
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
    if (s.onExpedition) { col = pal::cyan; return "On expedition"; }
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
    bool dragging = false, touchHeld = false; int grabDX = 0, grabDY = 0, startTx = 0, startTy = 0;
};
static InvState inv;

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
        gfx_fill(c, gx + x * cell, gy + y * cell, cell, cell, rgb(28, 28, 32));
        gfx_rect(c, gx + x * cell, gy + y * cell, cell + 1, cell + 1, rgb(54, 54, 60));
    }
    gfx_rect(c, gx - 2, gy - 2, g.w * cell + 5, g.h * cell + 5, pal::border);
    int sc = cell / 8, pad = (cell - sc * 8) / 2;
    for (int i = 0; i < g.n; i++) {
        if (interactive && i == inv.held && inv.dragging) continue;
        const Item& it = g.it[i];
        int x = gx + it.x * cell, y = gy + it.y * cell, w = itemW(it) * cell, h = itemH(it) * cell;
        bool selected = interactive && i == inv.sel;
        bool heldHere = interactive && i == inv.held;
        gfx_fill(c, x + 1, y + 1, w - 1, h - 1, selected || heldHere ? pal::goldBg : rgb(52, 50, 48));
        gfx_blit(c, iconFor(it.id, it.rot), x + pad + (cell >= 16 ? 0 : 0), y + pad, sc);
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

static void invInfo(Canvas& c, Survivor& s, int x, int y, int w, int idx, bool canUse, bool showDrop) {
    ui_panel(c, x, y, w, 196);
    if (idx < 0 || idx >= s.pack.n) {
        gfx_text(c, x + 6, y + 6, s.name, pal::cream);
        char b[48]; snprintf(b, sizeof b, "Bag %dx%d  %d items", s.pack.w, s.pack.h, s.pack.n); gfx_text(c, x + 6, y + 18, b, pal::dim);
        gfx_wrap(c, x + 6, y + 34, w - 12, "Tap an item to select it. Drag to move. Rotate with Y or the button. Items can be stacked or swapped.", pal::dim);
        return;
    }
    Item& it = s.pack.it[idx]; const ItemDef& d = idef(it.id);
    gfx_text(c, x + 6, y + 6, d.name, pal::cream);
    int ly = y + 18;
    ly += 10 * gfx_wrap(c, x + 6, ly, w - 12, d.desc, pal::dim) + 2;
    char b[48];
    if (d.cat == IC_FIREARM) {
        snprintf(b, sizeof b, "Dmg %d%s  Mag %d/%d", d.wpn.dmg, d.wpn.pellets > 1 ? "x6" : "", it.mag, d.wpn.mag); gfx_text(c, x + 6, ly, b, pal::cream); ly += 10;
        snprintf(b, sizeof b, "Acc %+d  Rng %d", d.wpn.acc + it.mod * 8, d.wpn.range); gfx_text(c, x + 6, ly, b, pal::cream); ly += 10;
    }
    if (d.cat == IC_MELEE) { snprintf(b, sizeof b, "Dmg %d  Stamina %d", d.wpn.dmg, d.wpn.stamina); gfx_text(c, x + 6, ly, b, pal::cream); ly += 10; }
    if (d.wpn.dur) { snprintf(b, sizeof b, "Durability %d/%d", it.dur, d.wpn.dur); gfx_text(c, x + 6, ly, b, it.dur ? pal::cream : pal::red); ly += 10; }
    else if (d.cat == IC_MELEE) { gfx_text(c, x + 6, ly, "Never wears out", pal::cream); ly += 10; }
    if (d.cat == IC_MEDICAL && d.heal) { snprintf(b, sizeof b, "Heals %d", d.heal); gfx_text(c, x + 6, ly, b, pal::green); ly += 10; }
    if (it.mod) { snprintf(b, sizeof b, "Upgraded +%d", it.mod); gfx_text(c, x + 6, ly, b, pal::gold); ly += 10; }
    int by = y + 196 - 22 * (showDrop ? 3 : 2) - 4;
    bool usable = d.cat == IC_MEDICAL || d.cat == IC_BACKPACK || d.cat == IC_FIREARM || d.cat == IC_MELEE;
    const char* useLbl = (d.cat == IC_FIREARM || d.cat == IC_MELEE) ? (s.equip == idx ? "Equipped" : "Equip") : "Use";
    if (ui_button(c, x + 6, by, w - 12, 18, useLbl, 0, canUse && usable && s.equip != idx)) {
        int sv = (int)(&s - G.st.surv), ui_ = -1;
        for (int k = 0; k < G.nu; k++) if (G.u[k].sv == sv) ui_ = k;
        survivor_useItem(sv, idx, ui_);
        if (inv.sel >= s.pack.n) inv.sel = -1;
    }
    if (ui_button(c, x + 6, by + 22, w - 12, 18, "Rotate", 0, d.w != d.h)) {
        int nr = it.rot ^ 1;
        if (s.pack.move(idx, it.x, it.y, nr)) audio_play(SFX_INV); else say("No room to rotate here.");
    }
    if (showDrop && ui_button(c, x + 6, by + 44, w - 12, 18, G.scene == SC_SETTLEMENT ? "To Stash" : "Drop", 0, true)) {
        Item copy = it;
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
    // buttons / keyboard fallback: cursor + A pick/place, Y rotate, X use
    uint32_t d = in.down;
    if (!in.touching) {
        int mx = (d & BTN_RIGHT ? 1 : 0) - (d & BTN_LEFT ? 1 : 0), my = (d & BTN_DOWN ? 1 : 0) - (d & BTN_UP ? 1 : 0);
        if (mx || my) { inv.curX = (inv.curX + mx + g.w) % g.w; inv.curY = (inv.curY + my + g.h) % g.h; audio_play(SFX_MENU_MOVE, 0.4f); if (inv.held < 0) inv.sel = g.at(inv.curX, inv.curY); }
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
        if ((d & BTN_X) && inv.sel >= 0 && inv.sel < g.n) {
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
            gfx_blit(c, iconFor(it.id, inv.heldRot), gx + tx * cell, gy + ty * cell, cell / 8);
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
        gfx_blit(c, iconFor(it.id, inv.heldRot), in.tx - inv.grabDX * cell - cell / 2, in.ty - inv.grabDY * cell - cell / 2, cell / 8);
    }
}

static void panelInventory(Canvas& c) {
    Survivor* sp = invSurv();
    if (!sp) { ui_back(); return; }
    Survivor& s = *sp;
    static const char* TABS[] = {"Inventory", "Equip", "Items", "Key", "Sort"};
    static const int TW[] = {66, 52, 50, 40, 44};
    int x = 6;
    for (int t = 0; t < 5; t++) {
        if (ui_tab(c, x, 3, TW[t], TABS[t], tab == t)) {
            if (t == 4) { if (s.pack.sort()) { survivor_equipBest(s); say("Backpack sorted."); audio_play(SFX_INV); } else say("Could not sort."); }
            else { tab = t; inv.sel = -1; inv.held = -1; }
        }
        x += TW[t] + 3;
    }
    if (ui_shoulderL()) tab = (tab + 3) % 4; if (ui_shoulderR()) tab = (tab + 1) % 4;
    int cell = gridCell(s);
    if (tab == 0) {
        drawGrid(c, s, 8, 24, cell, true);
        gridInteract(c, s, 8, 24, cell);
        invInfo(c, s, 176, 22, 140, inv.sel, true, true);
        char b[40]; snprintf(b, sizeof b, "%s  HP %d/%d", s.name, (int)s.hp, s.maxHp);
        gfx_text(c, 8, 28 + s.pack.h * cell, b, pal::dim);
        footer(c, inv.held >= 0 ? "Place" : "Move", "Back", "Use", "Rotate");
    } else if (tab == 1 || tab == 2 || tab == 3) {
        // list views: equipment / all items / key items
        ui_panel(c, 6, 22, 164, 200);
        int n = 0, rows[48];
        for (int i = 0; i < s.pack.n; i++) {
            int cat = idef(s.pack.it[i].id).cat;
            bool ok = tab == 2 || (tab == 1 && (cat == IC_FIREARM || cat == IC_MELEE || cat == IC_BACKPACK)) || (tab == 3 && (cat == IC_KEY || cat == IC_EQUIPMENT));
            if (ok) rows[n++] = i;
        }
        if (!n) gfx_text(c, 14, 32, "Nothing here.", pal::dim);
        for (int k = 0; k < n && k < 15; k++) {
            const Item& it = s.pack.it[rows[k]];
            char b[40]; snprintf(b, sizeof b, "%s%s%s", rows[k] == s.equip ? "[E] " : "", idef(it.id).name, it.count > 1 ? "" : "");
            if (it.count > 1) snprintf(b, sizeof b, "%s x%d", idef(it.id).name, it.count);
            if (ui_button(c, 10, 26 + k * 13, 156, 12, b, UB_LEFT | (inv.sel == rows[k] ? UB_SELECTED : 0))) inv.sel = rows[k];
        }
        invInfo(c, s, 176, 22, 140, inv.sel, true, true);
        footer(c, "Select", "Back");
    }
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
    ui_panel(c, 4, 3, 312, 70, title);
    // slots (side by side like a drawer)
    int slots = ct->cap > 8 ? 8 : ct->cap; int pageStart = page * 8;
    if (ct->cap > 8) slots = 8;
    for (int k = 0; k < slots; k++) {
        int i = pageStart + k, x = 10 + k * 38, y = 22;
        bool has = i < ct->n;
        bool selected = sel2 == 0 && sel == i && has;
        if (ui_hit(x, y, 36, 30, has)) { sel = i; sel2 = 0; inv.sel = -1; audio_play(SFX_MENU_MOVE); }
        gfx_fill(c, x, y, 36, 30, selected ? pal::goldBg : rgb(28, 28, 32));
        gfx_rect(c, x, y, 36, 30, selected ? pal::gold : (g_ui.focus == g_ui.count - 1 ? pal::goldDk : rgb(64, 64, 70)));
        if (has) {
            const Item& it = ct->it[i]; const Sprite& ic = g_item[it.id];
            if (ic.w <= 32 && ic.h <= 24) gfx_blit(c, ic, x + 18 - ic.w / 2, y + 15 - ic.h / 2);
            else blitHalf(c, ic, x + 18 - ic.w / 4, y + 15 - ic.h / 4);
            if (it.count > 1) { char b[8]; snprintf(b, sizeof b, "%d", it.count); gfx_text(c, x + 34 - gfx_textW(b), y + 21, b, pal::cream); }
        }
    }
    if (ct->n == 0) gfx_textC(c, 160, 34, "Empty.", pal::dim);
    if (ct->cap > 8) {
        char b[24]; snprintf(b, sizeof b, "%d/%d", ct->n, ct->cap); gfx_text(c, 12, 56, b, pal::dim);
        if (ui_button(c, 250, 54, 28, 14, "<", 0, page > 0)) page--;
        if (ui_button(c, 282, 54, 28, 14, ">", 0, (page + 1) * 8 < ct->cap)) page++;
    }
    // compact backpack (12 px cells) + actions
    int cell = 12, gx = 8, gy = 92;
    gfx_text(c, 8, 80, s.name, pal::dim);
    char bb[24]; snprintf(bb, sizeof bb, "Bag %dx%d", s.pack.w, s.pack.h); gfx_text(c, 80, 80, bb, pal::dim);
    int prevSel = inv.sel;
    drawGrid(c, s, gx, gy, cell, true);
    gridInteract(c, s, gx, gy, cell);
    if (inv.sel != prevSel && inv.sel >= 0) sel2 = 1;
    int ax = 136, ay = 78;
    ui_panel(c, ax, ay, 180, 146);
    bool fromCont = sel2 == 0 && sel < ct->n;
    const Item* shown = fromCont ? &ct->it[sel] : (inv.sel >= 0 && inv.sel < s.pack.n ? &s.pack.it[inv.sel] : nullptr);
    if (shown) {
        gfx_text(c, ax + 6, ay + 5, idef(shown->id).name, pal::cream);
        gfx_wrap(c, ax + 6, ay + 16, 168, idef(shown->id).desc, pal::dim);
    } else gfx_wrap(c, ax + 6, ay + 6, 168, "Select loot above, or an item in your bag to leave it here.", pal::dim);
    auto takeOne = [&](int i) -> bool {
        Item it = ct->it[i];
        int left = s.pack.add(it);
        if (left == it.count) return false;
        if (left) ct->it[i].count = (uint16_t)left; else ct->remove(i);
        if (s.equip < 0) survivor_equipBest(s);
        return true;
    };
    if (ui_button(c, ax + 6, ay + 62, 82, 18, "Take All", UB_GOLD | hl(TH_TAKEALL), ct->n > 0)) {
        int moved = 0, start = ct->n;
        for (int i = ct->n - 1; i >= 0; i--) if (takeOne(i)) moved++;
        audio_play(moved ? SFX_PICKUP : SFX_EMPTY);
        if (moved < start) say("Some items didn't fit. They stay here.");
        sel = 0;
    }
    if (ui_button(c, ax + 92, ay + 62, 82, 18, "Take", 0, fromCont)) {
        if (takeOne(sel)) { audio_play(SFX_PICKUP); if (sel >= ct->n) sel = ct->n ? ct->n - 1 : 0; }
        else { audio_play(SFX_EMPTY); say("No room in the backpack. Item left in place."); }
    }
    bool canStore = !fromCont && inv.sel >= 0 && inv.sel < s.pack.n && ct->n < ct->cap;
    if (ui_button(c, ax + 6, ay + 84, 82, 18, "Store", 0, canStore)) {
        Item it = s.pack.it[inv.sel];
        if (ct->push(it)) { packRemove(s, inv.sel); inv.sel = -1; audio_play(SFX_INV); }
    }
    if (g_contProp == -2) {
        // stash only: hand supplies over to the settlement's shared resources
        const Item* d = shown;
        bool can = d && (idef(d->id).cat == IC_AMMO || idef(d->id).cat == IC_MEDICAL || idef(d->id).cat == IC_FOOD || idef(d->id).cat == IC_MATERIAL || idef(d->id).cat == IC_FUEL);
        if (ui_button(c, ax + 92, ay + 84, 82, 18, "Donate", 0, can)) {
            int amt = deliver_item(*d);
            char b[48]; snprintf(b, sizeof b, "+%d %s for the settlement.", amt, RES_NAMES[idef(d->id).res]); say(b);
            if (fromCont) ct->remove(sel); else { packRemove(s, inv.sel); inv.sel = -1; }
            audio_play(SFX_PICKUP);
        }
    } else if (ui_button(c, ax + 92, ay + 84, 82, 18, "Inspect", 0, shown != nullptr)) {
        char b[96]; const ItemDef& d = idef(shown->id);
        snprintf(b, sizeof b, "%s: %dx%d cells%s", d.name, d.w, d.h, d.wpn.dur ? ", has durability" : ""); say(b);
    }
    if (ui_button(c, ax + 6, ay + 106, 168, 18, "Back", 0, true) || (ui_pressedB() && inv.held < 0)) {
        if (g_contBag >= 0 && G.curLoc >= 0 && ct->n == 0) G.loc[G.curLoc].bags[g_contBag].used = false;
        ui_back();
    }
    footer(c, "Confirm", "Back", nullptr, "Rotate");
}

// ---------------------------------------------------------------- HUD (gameplay bottom screen)
static void minimap(Canvas& c, int x, int y, int w, int h) {
    Map& m = G.map;
    int scale = 2; if (m.w * 3 <= w && m.h * 3 <= h) scale = 3;
    int ox = x + (w - m.w * scale) / 2, oy = y + (h - m.h * scale) / 2;
    gfx_fill(c, x, y, w, h, rgb(18, 18, 22));
    for (int ty = 0; ty < m.h; ty++) for (int tx = 0; tx < m.w; tx++) {
        int i = m.idx(tx, ty);
        if (G.scene == SC_EXPEDITION && !G.explored[i]) continue;
        uint8_t t = m.t[i]; px col;
        if (t == T_WALL || t == T_PALISADE) col = rgb(150, 146, 136);
        else if (t == T_DOOR) col = m.bar[m.barAt[i]].kind == BK_WINDOW ? rgb(80, 120, 140) : rgb(170, 120, 60);
        else if (t == T_EXTRACT) col = pal::lime;
        else if (m.flags[i] & F_INDOOR) col = rgb(60, 58, 54);
        else col = rgb(38, 44, 32);
        if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].container >= 0) col = G.curLoc >= 0 && G.loc[G.curLoc].generated[m.prop[m.propAt[i]].container] ? rgb(90, 90, 90) : rgb(110, 160, 60);
        if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].type == PR_PLOT) col = G.st.bLevel[m.prop[m.propAt[i]].special] ? rgb(170, 130, 80) : rgb(90, 74, 56);
        else if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].solid && G.scene == SC_SETTLEMENT) col = rgb(110, 100, 80);
        if (t == T_GARDEN) col = rgb(70, 110, 50);
        if (t == T_DIRT && G.scene == SC_SETTLEMENT) col = rgb(80, 66, 48);
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
    if (G.scene == SC_SETTLEMENT) {
        if (G.st.visitorPresent) return "A stranger waits at the gate.";
        int sec = settle_security();
        if (sec < 25) return "Security is low! Assign guards.";
        return "Use the radio table to plan an expedition.";
    }
    for (int i = 0; i < G.nu; i++) if (G.u[i].escort && G.u[i].alive) return G.u[i].order == ORD_HOLD ? "Someone is trapped in the holding cells." : "Escort the survivor to the south road.";
    return "Search for supplies. Extract at the south road.";
}

static void panelHud(Canvas& c) {
    if (G.scene == SC_SETTLEMENT) {
        ui_panel(c, 4, 3, 312, 50, "Settlement");
        resRow(c, 12, 22, false);
        char b[64]; int sec = settle_security();
        snprintf(b, sizeof b, "Pop %d/%d   Security %d (%s)", settle_population(), settle_capacity(), sec, security_rating(sec));
        gfx_blit(c, g_spr[S_ICON_PEOPLE], 12, 36); gfx_text(c, 22, 36, b, sec < 25 ? pal::red : pal::cream);
        minimap(c, 4, 58, 150, 112);
    } else {
        char t[40]; snprintf(t, sizeof t, "%s", G.curLoc >= 0 ? LOCATIONS[G.curLoc].name : "");
        ui_panel(c, 4, 3, 312, 168, t);
        minimap(c, 10, 22, 156, 142);
    }
    int ox = 170, oy = G.scene == SC_SETTLEMENT ? 58 : 22;
    gfx_blit(c, g_spr[S_ICON_SHIELD], ox, oy);
    gfx_text(c, ox + 11, oy, "Objective", pal::gold);
    gfx_wrap(c, ox, oy + 12, 140, objective(), pal::cream);
    if (tut_active() && ui_button(c, ox, 150, 140, 18, "Skip Tutorial")) tut_skip();
    if (G.scene == SC_EXPEDITION) {
        Survivor* s = invSurv();
        if (s) {
            char b[40]; snprintf(b, sizeof b, "Bag %d items  %d/%d", s->pack.n, s->pack.w, s->pack.h);
            gfx_text(c, ox, oy + 60, b, pal::dim);
            if (s->infection && s->infKnown) gfx_text(c, ox, oy + 72, INF_NAMES[s->infection], pal::red);
        }
    }
    // touch buttons
    int by = 178;
    if (G.scene == SC_SETTLEMENT) {
        if (ui_button(c, 4, by, 76, 20, "Base")) ui_open(PN_BASE);
        if (ui_button(c, 82, by, 76, 20, "Survivors")) ui_open(PN_SURVIVORS);
        if (ui_button(c, 160, by, 76, 20, "Expedition", hl(TH_EXPEDITION))) ui_open(PN_WORLDMAP);
        char jb[24]; snprintf(jb, sizeof jb, G.st.unread ? "Journal(%d)" : "Journal", G.st.unread);
        if (ui_button(c, 238, by, 78, 20, jb, G.st.unread ? UB_GOLD : 0)) ui_open(PN_JOURNAL);
        if (ui_button(c, 4, by + 24, 154, 20, "Inventory")) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
        if (ui_button(c, 162, by + 24, 154, 20, "Pause")) ui_open(PN_PAUSE);
    } else {
        if (ui_button(c, 4, by, 102, 20, "Inventory")) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
        if (ui_button(c, 109, by, 102, 20, "Squad")) ui_open(PN_SQUAD);
        if (ui_button(c, 214, by, 102, 20, "Pause")) ui_open(PN_PAUSE);
        { int hx = 6; const uint32_t B[3] = {BTN_SELECT, BTN_START, BTN_R}; const char* L[3] = {"Backpack", "Pause", "Flashlight"};
          for (int i = 0; i < 3; i++) { hx += ui_btn(c, hx, by + 26, B[i]) + 4; gfx_text(c, hx, by + 28, L[i], pal::cream); hx += gfx_textW(L[i]) + 12; } }
    }
    footer(c, "Interact", "Sprint/Dodge", "Reload", "Fire");
    if (g_ui.in.down & BTN_SELECT) { g_invSurvivor = -1; ui_open(PN_INVENTORY); }
    else if (g_ui.in.down & BTN_START) ui_open(PN_PAUSE);
}

static void panelSquad(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Squad Orders");
    static const char* ON[] = {"Follow", "Hold", "Retreat"};
    int y = 24, n = 0;
    for (int i = 0; i < G.nu; i++) {
        Unit& u = G.u[i];
        if (i == G.ctrl || u.escort || u.sv < 0) continue;
        Survivor& s = G.st.surv[u.sv];
        n++;
        gfx_blit(c, g_portrait[s.variant % PORTRAITS], 10, y);
        char b[40]; snprintf(b, sizeof b, "%s %s", s.name, u.alive ? "" : "(dead)");
        gfx_text(c, 40, y + 2, b, u.alive ? pal::cream : pal::grey);
        ui_bar(c, 40, y + 13, 60, 5, s.hp / s.maxHp, pal::green);
        for (int o = 0; o < 3; o++) if (ui_button(c, 112 + o * 66, y + 2, 62, 18, ON[o], u.order == o ? UB_SELECTED : 0, u.alive)) u.order = (uint8_t)o;
        y += 30;
    }
    if (!n) gfx_wrap(c, 12, 26, 296, "You are alone on this expedition. Bring companions from the Expedition Preparation screen.", pal::dim);
    gfx_wrap(c, 12, 150, 296, "Follow: stay close and fight. Hold: guard this spot. Retreat: fall back to the extraction point. D-Pad Down cycles orders in the field.", pal::dim);
    if (ui_button(c, 110, 198, 100, 20, "Back") || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- pause / main
static void panelPause(Canvas& c) {
    ui_panel(c, 60, 8, 200, 210, "Paused");
    int y = 30;
    if (ui_button(c, 76, y, 168, 20, "Resume") || (g_ui.in.down & BTN_START)) { ui_open(PN_HUD); return; } y += 24;
    if (ui_button(c, 76, y, 168, 20, "Inventory")) { g_invSurvivor = -1; ui_push(PN_INVENTORY); } y += 24;
    if (ui_button(c, 76, y, 168, 20, "Survivor Status")) ui_push(PN_SURVIVORS); y += 24;
    if (ui_button(c, 76, y, 168, 20, "Options")) ui_push(PN_SETTINGS); y += 24;
    bool canSave = G.scene == SC_SETTLEMENT;
    if (ui_button(c, 76, y, 168, 20, canSave ? "Save Game" : "Save (settlement only)", 0, canSave)) { saveMode = 1; ui_push(PN_SAVELOAD); } y += 24;
    if (ui_button(c, 76, y, 168, 20, "Controls")) ui_push(PN_CONTROLS); y += 24;
    if (tut_active() && ui_button(c, 76, y + 24, 168, 20, "Skip Tutorial")) { tut_skip(); ui_open(PN_HUD); return; }
    if (ui_button(c, 76, y, 168, 20, "Main Menu")) ask(CA_MAINMENU, 0, G.scene == SC_EXPEDITION ? "Quit to menu? Expedition progress since the last save is lost." : "Return to the main menu? Unsaved progress is lost.");
    if (ui_pressedB()) ui_open(PN_HUD);
}

static void panelMain(Canvas& c) {
    ui_panel(c, 50, 6, 220, 214, "Main Menu");
    int y = 26;
    bool any = false; for (int s = 0; s < 3; s++) any |= save_exists(s);
    if (ui_button(c, 66, y, 188, 20, "New Game")) { saveMode = 2; ui_push(PN_SAVELOAD); } y += 24;
    if (ui_button(c, 66, y, 188, 20, "Continue", 0, save_exists(G.saveSlot) || any)) {
        int slot = G.saveSlot; if (!save_exists(slot)) for (int s = 0; s < 3; s++) if (save_exists(s)) { slot = s; break; }
        ask(CA_LOAD, slot, "Continue from slot %d?", slot + 1);
    } y += 24;
    if (ui_button(c, 66, y, 188, 20, "Load Game", 0, any)) { saveMode = 0; ui_push(PN_SAVELOAD); } y += 24;
    if (ui_button(c, 66, y, 188, 20, "Settings")) ui_push(PN_SETTINGS); y += 24;
    if (ui_button(c, 66, y, 188, 20, "Controls")) ui_push(PN_CONTROLS); y += 24;
    if (ui_button(c, 66, y, 188, 20, "Credits")) ui_push(PN_CREDITS); y += 24;
    if (g_canQuit && ui_button(c, 66, y, 188, 20, "Quit")) ask(CA_QUIT, 0, "Quit Zombie Settlement?");
    gfx_textC(c, 160, 206, "v0.1  Original game  -  PC + 3DS", pal::grey);
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
        journal(JK_WARN, "The expedition to the %s was abandoned. The squad straggled home.", LOCATIONS[G.expLoc].name);
        G.expLoc = -1;
    }
    level_enter(MAP_SETTLEMENT, -1);
    ui_open(PN_HUD);
    if (err[0]) game_toast("%s", err); else game_toast("Loaded slot %d.", slot + 1);
}

static void panelSaveLoad(Canvas& c) {
    static const char* T[] = {"Load Game", "Save Game", "New Game - choose a slot"};
    ui_panel(c, 8, 6, 304, 214, T[saveMode]);
    for (int s = 0; s < 3; s++) {
        int y = 28 + s * 52;
        char info[64]; bool ok = save_info(s, info, sizeof info);
        bool exists = save_exists(s);
        gfx_fill(c, 16, y, 288, 46, pal::panel2); gfx_rect(c, 16, y, 288, 46, G.saveSlot == s ? pal::goldDk : pal::border);
        char t[16]; snprintf(t, sizeof t, "Slot %d", s + 1);
        gfx_text(c, 24, y + 6, t, pal::gold);
        gfx_text(c, 24, y + 20, info, ok ? pal::cream : (exists ? pal::red : pal::dim));
        const char* lbl = saveMode == 0 ? "Load" : (saveMode == 1 ? "Save" : "Start");
        bool en = saveMode == 0 ? ok : true;
        if (ui_button(c, 200, y + 6, 46, 18, lbl, 0, en)) {
            if (saveMode == 0) ask(CA_LOAD, s, "Load slot %d? Unsaved progress is lost.", s + 1);
            else if (saveMode == 1) { if (exists) ask(CA_SAVE, s, "Overwrite slot %d?", s + 1); else { G.saveSlot = s; say(save_write(s) ? "Game saved." : "Save FAILED. Previous save kept."); } }
            else { if (exists) ask(CA_NEWGAME, s, "Start a new game in slot %d? Its save will be replaced.", s + 1); else { G.saveSlot = s; game_newGame(); save_write(s); offerTutorial(); } }
        }
        if (ui_button(c, 250, y + 6, 48, 18, "Delete", 0, exists && saveMode != 2)) ask(CA_DELETE, s, "Delete slot %d permanently?", s + 1);
    }
    if (ui_button(c, 110, 190, 100, 22, "Back") || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- settlement overview
static void tabsBase(Canvas& c, int active) {
    static const char* T[] = {"Base", "Survivors", "Jobs", "Resources"};
    for (int t = 0; t < 4; t++) if (ui_tab(c, 6 + t * 77, 3, 74, T[t], active == t)) {
        if (t == 1) { ui_open(PN_SURVIVORS); return; }
        if (G.panel != PN_BASE) ui_open(PN_BASE);
        tab = t;
    }
    if (ui_shoulderR()) { if (active == 0) tab = 2; else if (active == 2) tab = 3; else if (active == 1) { ui_open(PN_BASE); tab = 2; } }
    if (ui_shoulderL()) { if (active == 3) tab = 2; else if (active == 2) { ui_open(PN_SURVIVORS); } }
}

static void panelBase(Canvas& c) {
    tabsBase(c, tab == 1 ? 0 : tab);
    if (tab == 0) {
        ui_panel(c, 4, 22, 180, 200);
        for (int b = 0; b < B_COUNT; b++) {
            char t[40]; int lv = G.st.bLevel[b];
            snprintf(t, sizeof t, "    %s", BUILDINGS[b].name);
            int y = 26 + b * 26;
            if (ui_button(c, 10, y, 168, 23, t, UB_LEFT | (sel == b ? UB_SELECTED : 0))) { if (sel == b) { g_buildingSel = b; ui_push(PN_BUILDING); } sel = b; }
            iconThird(c, g_building[b][lv > 3 ? 3 : (lv ? lv : 0)], 13, y + 3);
            char l[12]; snprintf(l, sizeof l, lv ? "Lv %d" : "Empty", lv);
            gfx_text(c, 172 - gfx_textW(l), y + 7, l, lv ? pal::cream : pal::dim);
            if (G.st.bDamage[b]) gfx_text(c, 120, y + 7, "!", pal::red);
        }
        ui_panel(c, 188, 22, 128, 200);
        int b = sel < B_COUNT ? sel : 0, lv = G.st.bLevel[b];
        gfx_blit(c, g_building[b][lv > 3 ? 3 : lv], 228, 28);
        gfx_textC(c, 252, 80, BUILDINGS[b].name, pal::cream);
        gfx_wrap(c, 194, 92, 118, BUILDINGS[b].desc, pal::dim);
        if (G.st.bDamage[b]) gfx_text(c, 194, 160, G.st.bDamage[b] > 1 ? "Badly damaged" : "Damaged", pal::red);
        if (ui_button(c, 194, 194, 116, 22, "Manage", UB_GOLD)) { g_buildingSel = b; ui_push(PN_BUILDING); }
        footer(c, "Select", "Back");
    } else if (tab == 2) {
        ui_panel(c, 4, 22, 312, 200, "Jobs");
        int per = 7, start = page * per, shown = 0;
        int alive[MAX_SURV], na = 0;
        for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive) alive[na++] = i;
        for (int k = start; k < na && shown < per; k++, shown++) {
            Survivor& s = G.st.surv[alive[k]]; int y = 42 + shown * 22;
            headCrop(c, s, 10, y);
            gfx_text(c, 30, y + 5, s.name, s.onExpedition ? pal::cyan : pal::cream);
            char sk[24]; snprintf(sk, sizeof sk, "G%d M%d F%d R%d", s.skill[SK_GUARD], s.skill[SK_MEDIC], s.skill[SK_FARM], s.skill[SK_MECH]);
            gfx_text(c, 92, y + 5, sk, pal::dim);
            if (ui_button(c, 196, y, 18, 18, "<", 0, !s.onExpedition)) s.job = (uint8_t)((s.job + JOB_COUNT - 1) % JOB_COUNT);
            gfx_textC(c, 252, y + 5, JOB_NAMES[s.job], pal::gold);
            if (ui_button(c, 292, y, 18, 18, ">", 0, !s.onExpedition)) s.job = (uint8_t)((s.job + 1) % JOB_COUNT);
        }
        if (na > per) {
            if (ui_button(c, 12, 198, 40, 18, "<", 0, page > 0)) page--;
            if (ui_button(c, 268, 198, 40, 18, ">", 0, (page + 1) * per < na)) page++;
        }
        char b[48]; snprintf(b, sizeof b, "Security: %d (%s)", settle_security(), security_rating(settle_security()));
        gfx_textC(c, 160, 202, b, settle_security() < 25 ? pal::red : pal::cream);
        footer(c, "Change", "Back");
    } else {
        ui_panel(c, 4, 22, 312, 200, "Resources");
        static const int IC[RES_COUNT] = {S_ICON_FOOD, S_ICON_MED, S_ICON_AMMO, S_ICON_MAT, S_ICON_FUEL};
        for (int r = 0; r < RES_COUNT; r++) {
            int y = 44 + r * 16; char b[32];
            gfx_blit(c, g_spr[IC[r]], 14, y); gfx_text(c, 26, y, RES_NAMES[r], pal::cream);
            snprintf(b, sizeof b, "%d", G.st.res[r]); gfx_text(c, 120, y, b, pal::gold);
        }
        int farmers = 0, food = 0, pop = settle_population();
        for (int i = 0; i < G.st.nsurv; i++) { const Survivor& s = G.st.surv[i]; if (s.alive && !s.onExpedition && !s.quarantined && s.job == JOB_FARMER && G.st.bLevel[B_KITCHEN]) { farmers++; food += 1 + G.st.bLevel[B_KITCHEN] + s.skill[SK_FARM] / 3; } }
        if (G.st.generator) food += food / 2;
        food += G.st.bLevel[B_KITCHEN];
        char b[64];
        snprintf(b, sizeof b, "Food per day: +%d / -%d", food, pop); gfx_text(c, 160, 44, b, food >= pop ? pal::green : pal::red);
        snprintf(b, sizeof b, "Population: %d / %d", pop, settle_capacity()); gfx_text(c, 160, 60, b, pal::cream);
        int sec = settle_security();
        snprintf(b, sizeof b, "Security: %d %s", sec, security_rating(sec)); gfx_text(c, 160, 76, b, sec < 25 ? pal::red : pal::cream);
        ui_bar(c, 160, 88, 140, 6, sec / 100.0f, sec < 25 ? pal::red : (sec < 45 ? pal::gold : pal::green));
        snprintf(b, sizeof b, "Walls: %d%%", G.st.wall); gfx_text(c, 160, 100, b, pal::cream);
        gfx_text(c, 160, 116, G.st.generator ? "Generator: running" : "Generator: none", G.st.generator ? pal::green : pal::dim);
        gfx_text(c, 160, 128, G.st.scanner ? "Med scanner: installed" : "Med scanner: none", G.st.scanner ? pal::green : pal::dim);
        if (ui_button(c, 14, 150, 140, 20, "Incident Journal")) ui_push(PN_JOURNAL);
        if (ui_button(c, 14, 174, 140, 20, "Open Stash")) { g_contProp = -2; g_contBag = -1; ui_push(PN_CONTAINER); }
        footer(c, "Select", "Back");
    }
    if (ui_pressedB() && G.panel == PN_BASE) ui_back();
}

// ---------------------------------------------------------------- building menu
static void panelBuilding(Canvas& c) {
    static const char* T[] = {"Build", "Upgrade", "Repair"};
    for (int t = 0; t < 3; t++) if (ui_tab(c, 6 + t * 70, 3, 66, T[t], tab == t)) tab = t;
    if (ui_shoulderR()) tab = (tab + 1) % 3; if (ui_shoulderL()) tab = (tab + 2) % 3;
    int b = g_buildingSel;
    // facility cards
    for (int k = 0; k < B_COUNT; k++) {
        int x = 6 + (k % 3) * 104, y = 22 + (k / 3) * 50, lv = G.st.bLevel[k];
        bool on = k == b;
        if (ui_hit(x, y, 100, 46)) { g_buildingSel = k; b = k; audio_play(SFX_MENU_MOVE); }
        bool foc = g_ui.focus == g_ui.count - 1;
        gfx_fill(c, x, y, 100, 46, on ? pal::goldBg : pal::panel2);
        gfx_rect(c, x, y, 100, 46, on ? pal::gold : (foc ? pal::goldDk : pal::border));
        blitHalf(c, g_building[k][lv > 3 ? 3 : (lv ? lv : 1)], x + 4, y + 4);
        if (!lv) gfx_fillA(c, x + 4, y + 4, 24, 24, pal::ink, 140);
        gfx_text(c, x + 32, y + 6, BUILDINGS[k].name, pal::cream);
        char l[16]; snprintf(l, sizeof l, lv ? "Lv %d/%d" : "Not built", lv, BUILDINGS[k].maxLevel); gfx_text(c, x + 32, y + 18, l, lv ? pal::dim : pal::grey);
        if (G.st.bDamage[k]) gfx_text(c, x + 32, y + 30, "Damaged", pal::red);
    }
    const BuildingDef& d = BUILDINGS[b];
    int lv = G.st.bLevel[b];
    ui_panel(c, 6, 124, 308, 98);
    gfx_text(c, 12, 129, d.name, pal::cream);
    gfx_wrap(c, 12, 140, 186, d.desc, pal::dim);
    const uint8_t* cost = nullptr; bool can = false; const char* why = "";
    static const uint8_t repairCost[RES_COUNT] = {0, 0, 0, 6, 0};
    if (tab == 0) { cost = d.cost[0]; can = lv == 0; why = lv ? "Already built" : ""; }
    else if (tab == 1) { if (lv > 0 && lv < d.maxLevel) { cost = d.cost[lv]; can = true; } why = lv == 0 ? "Build it first" : (lv >= d.maxLevel ? "Max level" : ""); }
    else { cost = repairCost; can = G.st.bDamage[b] > 0; why = can ? "" : "No damage"; }
    if (cost) { gfx_text(c, 206, 129, "Cost:", pal::dim); costLine(c, 206, 141, cost); }
    if (tab == 1 && lv > 0 && lv < d.maxLevel) gfx_wrap(c, 206, 154, 104, d.perk[lv + 1], pal::green);
    else if (lv > 0) gfx_wrap(c, 206, 154, 104, d.perk[lv], pal::cream);
    if (why[0]) gfx_text(c, 206, 141, why, pal::grey);
    bool afford = cost && can_afford(cost);
    if (ui_button(c, 206, 198, 104, 20, T[tab], UB_GOLD, can && afford)) {
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
    // facility-specific actions
    int ax = 12, ay = 198;
    if (lv > 0) {
        if (b == B_INFIRMARY && ui_button(c, ax, ay, 90, 20, "Patients")) ui_push(PN_INFIRMARY);
        if (b == B_WORKSHOP && ui_button(c, ax, ay, 90, 20, "Weapons")) ui_push(PN_WORKSHOP);
        if (b == B_STORAGE && ui_button(c, ax, ay, 90, 20, "Open Stash")) { g_contProp = -2; g_contBag = -1; ui_push(PN_CONTAINER); }
        if (b == B_DEFENSES && ui_button(c, ax, ay, 90, 20, "Guards")) { ui_open(PN_BASE); tab = 2; }
        if (b == B_KITCHEN && ui_button(c, ax, ay, 90, 20, "Farmers")) { ui_open(PN_BASE); tab = 2; }
        if (b == B_QUARTERS) {
            int hour = ((int)G.clock / 60) % 24;
            if (ui_button(c, ax, ay, 90, 20, "Sleep")) {
                int hrs = hour >= 6 ? 30 - hour : 6 - hour; if (hrs < 1) hrs = 1;
                game_advanceHours((float)hrs);
                G.clock = 6 * 60 + 1;
                journal(JK_INFO, "%s slept until morning.", G.st.leader >= 0 ? G.st.surv[G.st.leader].name : "The leader");
                game_toast("You slept until 06:00.");
                ui_open(PN_HUD);
                return;
            }
            if (ui_button(c, ax + 94, ay, 90, 20, "Rest 2h")) { game_advanceHours(2); game_toast("Rested for two hours."); }
        }
    }
    if (ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- survivors
static void panelSurvivors(Canvas& c) {
    static const char* T[] = {"Survivors", "Squad", "Jobs", "Status"};
    for (int t = 0; t < 4; t++) if (ui_tab(c, 6 + t * 77, 3, 74, T[t], tab == t)) { if (t == 2) { ui_open(PN_BASE); tab = 2; return; } tab = t; sel = 0; page = 0; }
    if (ui_shoulderR()) tab = (tab + 1) % 4; if (ui_shoulderL()) tab = (tab + 3) % 4;
    int ids[MAX_SURV], n = 0;
    for (int i = 0; i < G.st.nsurv; i++) {
        bool ok = tab == 3 ? true : G.st.surv[i].alive;
        if (tab == 1) ok = G.st.surv[i].alive && !G.st.surv[i].quarantined;
        if (ok) ids[n++] = i;
    }
    if (sel >= n) sel = n ? n - 1 : 0;
    int per = 7;
    ui_panel(c, 4, 22, 150, 200);
    for (int k = page * per; k < n && k < (page + 1) * per; k++) {
        Survivor& s = G.st.surv[ids[k]]; int y = 26 + (k - page * per) * 24;
        char b[24]; snprintf(b, sizeof b, "%s", s.name);
        if (ui_button(c, 8, y, 142, 22, "", sel == k ? UB_SELECTED : 0)) sel = k;
        // portrait head crop
        for (int j = 0; j < 18; j++) for (int i = 0; i < 18; i++) gfx_pset(c, 11 + i, y + 2 + j, g_portrait[s.variant % PORTRAITS].d[(j + 2) * 24 + i + 3]);
        gfx_text(c, 34, y + 7, b, s.alive ? pal::cream : pal::grey);
        char l[12]; snprintf(l, sizeof l, s.alive ? "Lv %d" : "RIP", survLevel(s)); gfx_text(c, 146 - gfx_textW(l), y + 7, l, pal::dim);
        if (ids[k] == G.st.leader) gfx_text(c, 100, y + 7, "*", pal::gold);
    }
    if (n > per) {
        if (ui_button(c, 8, 200, 30, 18, "<", 0, page > 0)) { page--; sel = page * per; }
        if (ui_button(c, 120, 200, 30, 18, ">", 0, (page + 1) * per < n)) { page++; sel = page * per; }
    }
    ui_panel(c, 158, 22, 158, 200);
    if (!n) { gfx_text(c, 166, 30, "No survivors.", pal::dim); if (ui_pressedB()) ui_back(); return; }
    int si = ids[sel]; Survivor& s = G.st.surv[si];
    gfx_blit(c, g_portrait[s.variant % PORTRAITS], 164, 28, 2);
    gfx_text(c, 216, 30, s.name, pal::cream);
    char b[48]; snprintf(b, sizeof b, "Lv %d %s", survLevel(s), JOB_NAMES[s.job]); gfx_text(c, 216, 42, b, pal::dim);
    gfx_text(c, 216, 54, TRAIT_NAMES[s.trait], pal::gold);
    px sc; const char* st = survStatus(s, sc);
    gfx_wrap(c, 216, 66, 96, st, sc);
    gfx_text(c, 164, 82, "Health", pal::dim); ui_bar(c, 210, 83, 100, 6, s.hp / s.maxHp, pal::green);
    if (tab == 0 || tab == 1) {
        for (int k = 0; k < SK_COUNT; k++) {
            int y = 96 + k * 11;
            gfx_text(c, 164, y, SKILL_NAMES[k], pal::cream);
            ui_bar(c, 240, y + 1, 50, 6, s.skill[k] / 10.0f, pal::gold);
            char v[4]; snprintf(v, sizeof v, "%d", s.skill[k]); gfx_text(c, 296, y, v, pal::cream);
        }
        bool canAct = s.alive && !s.onExpedition && G.scene == SC_SETTLEMENT;
        if (ui_button(c, 164, 166, 72, 18, "Set Leader", 0, canAct && si != G.st.leader && !s.quarantined)) {
            G.st.leader = si; level_enter(MAP_SETTLEMENT, -1); game_toast("You now control %s.", s.name);
        }
        if (ui_button(c, 240, 166, 70, 18, "Backpack", 0, s.alive && !s.onExpedition)) { g_invSurvivor = si; ui_push(PN_INVENTORY); }
        if (ui_button(c, 164, 188, 72, 18, s.quarantined ? "Release" : "Quarantine", 0, canAct && G.st.bLevel[B_INFIRMARY] > 0)) {
            s.quarantined = !s.quarantined;
            journal(JK_INFO, "%s was %s quarantine.", s.name, s.quarantined ? "moved to" : "released from");
            if (s.quarantined && si == G.st.leader) { for (int i = 0; i < G.st.nsurv; i++) if (i != si && G.st.surv[i].alive && !G.st.surv[i].quarantined) { G.st.leader = i; break; } level_enter(MAP_SETTLEMENT, -1); }
        }
        char kb[24]; snprintf(kb, sizeof kb, "Kills: %d", s.kills); gfx_text(c, 244, 193, kb, pal::dim);
        footer(c, "Select", "Back");
    } else if (tab == 2) {
    } else {
        // Status: memorial and infection overview
        gfx_text(c, 164, 96, s.alive ? "Alive" : "Deceased", s.alive ? pal::green : pal::grey);
        if (!s.alive) { snprintf(b, sizeof b, "Died on day %d", s.deathDay); gfx_text(c, 164, 108, b, pal::dim); }
        if (s.infection && s.infKnown) { gfx_text(c, 164, 120, INF_NAMES[s.infection], pal::red); ui_bar(c, 164, 132, 140, 5, s.infHours / 20.0f, pal::red); }
        snprintf(b, sizeof b, "Kills: %d", s.kills); gfx_text(c, 164, 144, b, pal::cream);
        footer(c, "Select", "Back");
    }
    if (ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- recruitment
static void panelRecruit(Canvas& c) {
    Survivor& v = G.st.visitor;
    bool rescued = G.sum.rescued == 1;
    if (!G.st.visitorPresent && !rescued) { ui_back(); return; }
    ui_panel(c, 4, 3, 312, 218, rescued ? "Rescued Survivor" : "Stranger at the Gate");
    gfx_blit(c, g_portrait[v.variant % PORTRAITS], 12, 22, 2);
    gfx_text(c, 66, 24, v.name, pal::cream);
    char b[48];
    snprintf(b, sizeof b, "Health %d/%d", (int)v.hp, v.maxHp); gfx_text(c, 66, 36, b, pal::cream);
    gfx_text(c, 66, 48, TRAIT_NAMES[v.trait], pal::gold);
    int bestK = 0; for (int k = 1; k < SK_COUNT; k++) if (v.skill[k] > v.skill[bestK]) bestK = k;
    snprintf(b, sizeof b, "Specialty: %s", SKILL_NAMES[bestK]); gfx_text(c, 66, 60, b, pal::dim);
    for (int k = 0; k < SK_COUNT; k++) {
        int y = 78 + k * 11;
        gfx_text(c, 12, y, SKILL_NAMES[k], pal::cream); ui_bar(c, 90, y + 1, 60, 6, v.skill[k] / 10.0f, pal::gold);
    }
    const char* scr = G.st.visitorScreened ? (v.infection ? "Screening: INFECTED" : "Screening: clean") : "Screening: not done";
    gfx_text(c, 170, 78, scr, G.st.visitorScreened ? (v.infection ? pal::red : pal::green) : pal::dim);
    if (v.infection && v.infKnown) gfx_text(c, 170, 90, "Visibly infected", pal::red);
    snprintf(b, sizeof b, "Population %d/%d", settle_population(), settle_capacity()); gfx_text(c, 170, 104, b, pal::cream);
    bool room = settle_population() < settle_capacity() && G.st.nsurv < MAX_SURV;
    if (!room) gfx_wrap(c, 170, 116, 140, "No room. Upgrade the Living Quarters.", pal::red);
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
    if (ui_button(c, 12, 152, 96, 20, "Accept", UB_GOLD, room)) { accept(false); return; }
    if (ui_button(c, 112, 152, 96, 20, "Reject")) { ask(CA_REJECT, 0, "Turn %s away?", v.name); }
    if (ui_button(c, 212, 152, 96, 20, "Screen (1 Med)", 0, !G.st.visitorScreened && G.st.res[RES_MED] >= 1)) {
        G.st.res[RES_MED]--; G.st.visitorScreened = true; audio_play(SFX_MENU_OK);
        if (v.infection) v.infKnown = true;
    }
    if (ui_button(c, 12, 176, 196, 20, "Accept into Quarantine", 0, room && G.st.bLevel[B_INFIRMARY] > 0)) { accept(true); return; }
    if (ui_button(c, 212, 176, 96, 20, "Later") || ui_pressedB()) ui_back();
    gfx_textC(c, 160, 204, "Screening reliably detects infection.", pal::dim);
}

// ---------------------------------------------------------------- infirmary
static void panelInfirmary(Canvas& c) {
    int ids[MAX_SURV], n = 0;
    for (int i = 0; i < G.st.nsurv; i++) { const Survivor& s = G.st.surv[i]; if (s.alive && !s.onExpedition) ids[n++] = i; }
    ui_panel(c, 4, 3, 312, 218, "Infirmary - Treatment");
    if (!n) { gfx_text(c, 12, 24, "No patients.", pal::dim); if (ui_pressedB()) ui_back(); return; }
    if (sel >= n) sel = n - 1;
    for (int k = 0; k < n && k < 8; k++) {
        const Survivor& s = G.st.surv[ids[k]];
        px col; const char* st = survStatus(s, col);
        char nm[24]; snprintf(nm, sizeof nm, "   %s", s.name);
        if (ui_button(c, 8, 22 + k * 20, 100, 18, nm, UB_LEFT | (sel == k ? UB_SELECTED : 0))) sel = k;
        headCrop(c, s, 10, 22 + k * 20);
        gfx_fill(c, 104, 28 + k * 20, 4, 6, col); (void)st;
    }
    Survivor& s = G.st.surv[ids[sel]];
    gfx_blit(c, g_portrait[s.variant % PORTRAITS], 116, 22, 2);
    gfx_text(c, 170, 24, s.name, pal::cream);
    px col; const char* st = survStatus(s, col);
    gfx_wrap(c, 170, 36, 140, st, col);
    if (s.infection && s.infKnown) { gfx_blit(c, g_spr[S_ICON_BIO], 300, 24); ui_bar(c, 170, 60, 140, 6, (s.infection - 1 + s.infHours / 20.0f) / 3.0f, pal::red); }
    char b[64];
    snprintf(b, sizeof b, "Health %d/%d", (int)s.hp, s.maxHp); gfx_text(c, 116, 76, b, pal::cream);
    int lvl = G.st.bLevel[B_INFIRMARY];
    snprintf(b, sizeof b, "Infirmary Lv %d%s", lvl, G.st.scanner ? " + scanner" : ""); gfx_text(c, 116, 88, b, pal::dim);
    bool showInf = s.infection && s.infKnown;
    int cost = showInf ? treat_cost(s) : (s.hp < s.maxHp ? 1 : 0);
    const char* desc = showInf ? (s.infection == INF_EARLY ? "Early infection. Can be cured with treatment." : (s.infection == INF_ADVANCED ? "Advanced infection. Needs Infirmary Lv 2." : "Critical! Needs Infirmary Lv 3 or they will turn.")) : (s.hp < s.maxHp ? "Injured. Treatment restores health." : "Healthy.");
    gfx_wrap(c, 116, 102, 196, desc, pal::cream);
    snprintf(b, sizeof b, "Treat (Medicine x%d)", cost);
    if (ui_button(c, 116, 130, 196, 20, b, UB_GOLD, cost > 0 && G.st.res[RES_MED] >= cost && lvl > 0)) {
        bool cured = false;
        if (showInf || s.infection == INF_NONE) settle_treat(ids[sel], cured);
        else { G.st.res[RES_MED] -= 1; s.hp = fminf(s.maxHp, s.hp + 40); cured = true; }   // hidden infection: injury care only
        audio_play(SFX_HEAL);
        say(cured ? "Treatment succeeded." : "Treatment failed. The infection was held back.");
    }
    if (ui_button(c, 116, 154, 196, 20, s.quarantined ? "Release from Quarantine" : "Move to Quarantine", 0, lvl > 0)) {
        s.quarantined = !s.quarantined; journal(JK_INFO, "%s %s quarantine.", s.name, s.quarantined ? "entered" : "left");
    }
    if (ui_button(c, 116, 178, 196, 20, "Examine (1 Medicine)", 0, G.st.res[RES_MED] >= 1 && !s.infKnown)) {
        G.st.res[RES_MED]--;
        if (s.infection) { s.infKnown = true; say("Examination found an infection!"); journal(JK_WARN, "Screening found %s infected.", s.name); }
        else say("No infection found.");
    }
    if (ui_button(c, 116, 200, 196, 18, "Back") || ui_pressedB()) ui_back();
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
    ui_panel(c, 4, 3, 312, 218, "Workshop - Weapons");
    if (!n) { gfx_text(c, 12, 26, "No weapons to service.", pal::dim); if (ui_pressedB()) ui_back(); return; }
    int per = 8; if (sel >= n) sel = n - 1;
    for (int k = page * per; k < n && k < (page + 1) * per; k++) {
        Item& it = list[k].stash ? G.st.stash.it[list[k].idx] : G.st.surv[list[k].sv].pack.it[list[k].idx];
        const ItemDef& d = idef(it.id); int y = 22 + (k - page * per) * 20;
        if (ui_button(c, 8, y, 150, 18, d.name, UB_LEFT | (sel == k ? UB_SELECTED : 0))) sel = k;
        if (d.wpn.dur) ui_bar(c, 112, y + 7, 40, 5, (float)it.dur / d.wpn.dur, it.dur * 4 < d.wpn.dur ? pal::red : pal::green);
        else gfx_text(c, 120, y + 5, "inf", pal::dim);
    }
    if (n > per) {
        if (ui_button(c, 8, 186, 30, 16, "<", 0, page > 0)) { page--; sel = page * per; }
        if (ui_button(c, 128, 186, 30, 16, ">", 0, (page + 1) * per < n)) { page++; sel = page * per; }
    }
    Item& it = list[sel].stash ? G.st.stash.it[list[sel].idx] : G.st.surv[list[sel].sv].pack.it[list[sel].idx];
    const ItemDef& d = idef(it.id);
    gfx_blit(c, g_item[it.id], 166, 26);
    gfx_text(c, 166, 40, d.name, pal::cream);
    char b[48];
    snprintf(b, sizeof b, "Owner: %s", list[sel].stash ? "Stash" : G.st.surv[list[sel].sv].name); gfx_text(c, 166, 52, b, pal::dim);
    if (d.wpn.dur) snprintf(b, sizeof b, "Durability %d/%d", it.dur, d.wpn.dur); else snprintf(b, sizeof b, "Never wears out");
    gfx_text(c, 166, 64, b, pal::cream);
    snprintf(b, sizeof b, "Upgrade level %d/2", it.mod); gfx_text(c, 166, 76, b, pal::gold);
    int rc = repair_cost(it);
    uint8_t rcost[RES_COUNT] = {0, 0, 0, (uint8_t)rc, 0};
    gfx_text(c, 166, 94, "Repair:", pal::dim); costLine(c, 214, 94, rcost);
    if (ui_button(c, 166, 108, 144, 20, "Repair", UB_GOLD, rc > 0 && G.st.res[RES_MAT] >= rc && G.st.bLevel[B_WORKSHOP] > 0)) {
        G.st.res[RES_MAT] -= rc; it.dur = d.wpn.dur; audio_play(SFX_BUILD); say("Repaired.");
    }
    uint8_t ucost[RES_COUNT] = {0, 0, 0, 6, 2};
    bool canUp = G.st.bLevel[B_WORKSHOP] >= 2 && it.mod < 2 && d.cat == IC_FIREARM;
    gfx_text(c, 166, 136, "Upgrade:", pal::dim); costLine(c, 220, 136, ucost);
    if (ui_button(c, 166, 150, 144, 20, "Upgrade (+8 acc)", 0, canUp && can_afford(ucost))) { pay(ucost); it.mod++; audio_play(SFX_BUILD); say("Weapon upgraded."); }
    if (G.st.bLevel[B_WORKSHOP] < 2) gfx_wrap(c, 166, 174, 144, "Upgrades need Workshop Lv 2.", pal::dim);
    if (ui_button(c, 166, 196, 144, 20, "Back") || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- world map + prep
static int g_mapSel = 0;
static void mapThumb(Canvas& c, int x, int y, int w, int h, int loc) {
    gfx_fill(c, x, y, w, h, rgb(20, 22, 18));
    if (LOCATIONS[loc].map < 0) {
        gfx_textC(c, x + w / 2, y + h / 2 - 8, "UNSCOUTED", pal::grey);
        gfx_textC(c, x + w / 2, y + h / 2 + 4, "Route not yet found", pal::grey);
    } else {
        static Map thumb; static int loaded = -1;
        if (loaded != LOCATIONS[loc].map) { map_load(thumb, LOCATIONS[loc].map); loaded = LOCATIONS[loc].map; }
        int sc = 2, ox = x + (w - thumb.w * sc) / 2, oy = y + (h - thumb.h * sc) / 2;
        for (int ty = 0; ty < thumb.h; ty++) for (int tx = 0; tx < thumb.w; tx++) {
            uint8_t t = thumb.t[thumb.idx(tx, ty)];
            px col = t == T_WALL ? rgb(160, 150, 136) : t == T_DOOR ? rgb(170, 120, 60) : (t == T_GRASS ? rgb(56, 70, 40) : (t == T_ASPHALT || t == T_LINE ? rgb(54, 54, 58) : (t == T_EXTRACT ? pal::lime : rgb(110, 104, 94))));
            gfx_fill(c, ox + tx * sc, oy + ty * sc, sc, sc, col);
        }
    }
    gfx_rect(c, x, y, w, h, pal::border);
}

static void panelWorldMap(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "World Map - Select Location");
    for (int l = 0; l < LOC_COUNT; l++) {
        char ln[32]; snprintf(ln, sizeof ln, "   %s", LOCATIONS[l].name);
        static const int LIC[LOC_COUNT] = {S_ICON_SHIELD, S_ICON_MED, S_ICON_FOOD, S_ICON_TRAIN, S_ICON_MAT, S_ICON_TREE, S_ICON_HOUSE};
        if (ui_button(c, 8, 22 + l * 24, 130, 21, ln, UB_LEFT | (sel == l ? UB_SELECTED : 0))) sel = l;
        gfx_blit(c, g_spr[LIC[l]], 14, 28 + l * 24);
        if (LOCATIONS[l].map < 0) gfx_fillA(c, 8, 22 + l * 24, 130, 21, pal::ink, 100);
    }
    mapThumb(c, 144, 22, 168, 104, sel);
    const LocationDef& L = LOCATIONS[sel];
    gfx_text(c, 146, 130, L.name, pal::cream);
    char b[64]; snprintf(b, sizeof b, "Loot: %s", L.loot); gfx_text(c, 146, 142, b, pal::dim);
    snprintf(b, sizeof b, "Threat: %s", L.threat); gfx_text(c, 146, 154, b, pal::dim);
    const LocState& S = G.loc[sel];
    if (L.map < 0) snprintf(b, sizeof b, "Not yet surveyed");
    else if (S.cooldown >= 0) snprintf(b, sizeof b, "Restock in %d:%02d", (int)S.cooldown / 60, (int)S.cooldown % 60);
    else snprintf(b, sizeof b, S.visited ? "Cooldown: Ready" : "Unexplored");
    gfx_text(c, 146, 166, b, S.cooldown >= 0 ? pal::gold : pal::green);
    if (ui_button(c, 146, 196, 80, 20, "Deploy", UB_GOLD | (L.map >= 0 ? hl(TH_DEPLOY) : 0), L.map >= 0 && G.scene == SC_SETTLEMENT)) {
        G.squad[0] = (int8_t)G.st.leader; G.squad[1] = G.squad[2] = -1; g_mapSel = sel;
        ui_push(PN_PREP);
    }
    if (ui_button(c, 232, 196, 80, 20, "Back") || ui_pressedB()) ui_back();
}

static bool eligible(int i) { const Survivor& s = G.st.surv[i]; return s.alive && !s.quarantined && s.hp >= 30; }

static void panelPrep(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Expedition Preparation");
    if (G.squad[0] < 0 || !eligible(G.squad[0])) { for (int i = 0; i < G.st.nsurv; i++) if (eligible(i)) { G.squad[0] = (int8_t)i; break; } }
    if (G.squad[0] < 0) { gfx_text(c, 12, 26, "Nobody is fit to travel.", pal::red); if (ui_pressedB()) ui_back(); return; }
    Survivor& lead = G.st.surv[G.squad[0]];
    gfx_text(c, 12, 22, "Leader (you control):", pal::dim);
    if (ui_button(c, 12, 34, 20, 20, "<")) { int i = G.squad[0]; for (int k = 0; k < G.st.nsurv; k++) { i = (i + G.st.nsurv - 1) % G.st.nsurv; if (eligible(i) && i != G.squad[1] && i != G.squad[2]) break; } G.squad[0] = (int8_t)i; }
    gfx_blit(c, g_portrait[lead.variant % PORTRAITS], 36, 32);
    gfx_text(c, 64, 36, lead.name, pal::cream);
    char b[64]; snprintf(b, sizeof b, "HP %d  Shoot %d  Melee %d", (int)lead.hp, lead.skill[SK_SHOOT], lead.skill[SK_MELEE]); gfx_text(c, 64, 46, b, pal::dim);
    if (ui_button(c, 290, 34, 20, 20, ">")) { int i = G.squad[0]; for (int k = 0; k < G.st.nsurv; k++) { i = (i + 1) % G.st.nsurv; if (eligible(i) && i != G.squad[1] && i != G.squad[2]) break; } G.squad[0] = (int8_t)i; }
    // equipment summary
    const Item* w = lead.equip >= 0 && lead.equip < lead.pack.n ? &lead.pack.it[lead.equip] : nullptr;
    if (w) snprintf(b, sizeof b, "Weapon: %s", idef(w->id).name); else snprintf(b, sizeof b, "Weapon: none");
    if (w && idef(w->id).cat == IC_FIREARM) snprintf(b, sizeof b, "Weapon: %s %d+%d rds", idef(w->id).name, w->mag, lead.pack.countOf(idef(w->id).wpn.ammo));
    gfx_text(c, 12, 62, b, pal::cream);
    snprintf(b, sizeof b, "Backpack %dx%d, %d items", lead.pack.w, lead.pack.h, lead.pack.n); gfx_text(c, 12, 74, b, pal::cream);
    if (ui_button(c, 214, 58, 96, 18, "Review Pack")) { g_invSurvivor = G.squad[0]; ui_push(PN_INVENTORY); }
    // companions (up to two)
    gfx_text(c, 12, 90, "Companions (max 2):", pal::dim);
    int y = 102, shown = 0;
    for (int i = 0; i < G.st.nsurv && shown < 6; i++) {
        if (i == G.squad[0] || !eligible(i)) continue;
        Survivor& s = G.st.surv[i];
        bool on = G.squad[1] == i || G.squad[2] == i;
        snprintf(b, sizeof b, "%s %s (%s)", on ? "[x]" : "[ ]", s.name, JOB_NAMES[s.job]);
        int x = 12 + (shown % 2) * 150, yy = y + (shown / 2) * 22;
        if (ui_button(c, x, yy, 146, 20, b, UB_LEFT | (on ? UB_SELECTED : 0))) {
            if (on) { if (G.squad[1] == i) G.squad[1] = G.squad[2]; G.squad[2] = -1; }
            else if (G.squad[1] < 0) G.squad[1] = (int8_t)i; else if (G.squad[2] < 0) G.squad[2] = (int8_t)i; else say("Squad is full (3).");
        }
        shown++;
    }
    // security once the squad leaves
    bool saved[3]; for (int k = 0; k < 3; k++) { saved[k] = G.squad[k] >= 0 && G.st.surv[G.squad[k]].onExpedition; if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = true; }
    int sec = settle_security();
    for (int k = 0; k < 3; k++) if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = saved[k];
    snprintf(b, sizeof b, "Security while away: %d (%s)", sec, security_rating(sec));
    gfx_text(c, 12, 172, b, sec < 25 ? pal::red : pal::cream);
    if (sec < 25) gfx_text(c, 12, 184, "Warning: the settlement will be vulnerable!", pal::red);
    if (ui_button(c, 12, 198, 120, 20, "Deploy", UB_GOLD | hl(TH_DEPLOY))) {
        int dest = g_mapSel;
        for (int k = 0; k < 3; k++) if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = true;
        G.expLoc = (int8_t)dest;
        save_write(G.saveSlot);
        level_leave();
        level_enter(LOCATIONS[dest].map, dest);
        G.clock += 45; // travel time
        ui_open(PN_HUD);
        game_toast("Arrived at the %s. Find supplies and extract south.", LOCATIONS[dest].name);
        return;
    }
    if (ui_button(c, 190, 198, 120, 20, "Back") || ui_pressedB()) ui_back();
}

static void panelSummary(Canvas& c) {
    Summary& s = G.sum;
    ui_panel(c, 4, 3, 312, 218, s.failed ? "Expedition Failed" : "Extraction Summary");
    for (int i = 0; i < s.nlines; i++) gfx_text(c, 12, 22 + i * 12, s.lines[i], i == 0 ? pal::gold : (strstr(s.lines[i], "died") ? pal::red : pal::cream));
    resRow(c, 12, 150, false);
    if (G.st.unread) { char b[40]; snprintf(b, sizeof b, "%d new journal entries", G.st.unread); gfx_text(c, 12, 166, b, pal::gold); }
    if (s.rescued == 1 && ui_button(c, 12, 196, 140, 20, "Meet the rescued", UB_GOLD)) { ui_push(PN_RECRUIT); return; }
    if (ui_button(c, 168, 196, 140, 20, "Return to Settlement") || (ui_pressedB() && s.rescued != 1)) {
        if (s.rescued == 1) { s.rescued = -1; journal(JK_INFO, "%s decided to move on.", G.st.visitor.name); }
        ui_open(G.st.unread ? PN_JOURNAL : PN_HUD);
    }
}

static void panelJournal(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Incident Journal");
    int per = 8, n = G.st.njournal;
    for (int k = 0; k < per; k++) {
        int i = n - 1 - (page * per + k); if (i < 0) break;
        const JournalEntry& e = G.st.journal[i];
        static const px COL[4] = {pal::cream, pal::green, pal::gold, pal::red};
        char d[12]; snprintf(d, sizeof d, "D%u", e.day);
        gfx_text(c, 10, 22 + k * 22, d, pal::dim);
        if (e.kind == JK_DEATH) gfx_blit(c, g_spr[S_ICON_SKULL], 30, 22 + k * 22);
        gfx_wrap(c, 42, 22 + k * 22, 268, e.text, COL[e.kind & 3]);
    }
    if (!n) gfx_text(c, 12, 26, "Nothing has happened yet.", pal::dim);
    if (ui_button(c, 8, 198, 40, 18, "<", 0, page > 0)) page--;
    if (ui_button(c, 52, 198, 40, 18, ">", 0, (page + 1) * per < n)) page++;
    G.st.unread = 0;
    if (ui_button(c, 210, 198, 100, 18, "Back") || ui_pressedB()) ui_back();
}

// ---------------------------------------------------------------- settings / controls / credits
static void panelSettings(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Settings");
    Settings& s = G.set;
    auto slider = [&](int y, const char* name, uint8_t& v) {
        gfx_text(c, 14, y + 5, name, pal::cream);
        if (ui_button(c, 120, y, 22, 18, "-", 0, v > 0)) v--;
        ui_bar(c, 146, y + 5, 110, 8, v / 10.0f, pal::gold);
        if (ui_button(c, 260, y, 22, 18, "+", 0, v < 10)) v++;
    };
    slider(22, "Master", s.master); slider(44, "Effects", s.sfx); slider(66, "Music", s.music);
    char b[48];
    snprintf(b, sizeof b, "Day length: %d min", s.dayMinutes);
    if (ui_button(c, 14, 90, 140, 18, b)) s.dayMinutes = s.dayMinutes == 15 ? 30 : (s.dayMinutes == 30 ? 60 : 15);
    if (ui_button(c, 160, 90, 146, 18, s.shake ? "Screen shake: On" : "Screen shake: Off")) s.shake = !s.shake;
    if (ui_button(c, 14, 112, 140, 18, s.showFps ? "Show FPS: On" : "Show FPS: Off")) s.showFps = !s.showFps;
    snprintf(b, sizeof b, "Camera zoom: %d%%", s.zoom);
    if (ui_button(c, 160, 112, 146, 18, b)) { s.zoom = s.zoom >= 200 ? 100 : (s.zoom >= 150 ? 200 : (s.zoom >= 125 ? 150 : 125)); settings_save(); }
    int ry = 134;
    if (g_canQuit) {
        snprintf(b, sizeof b, "Window scale: %dx", s.winScale);
        if (ui_button(c, 160, ry, 146, 18, b)) s.winScale = s.winScale >= 3 ? 1 : s.winScale + 1;
        if (ui_button(c, 14, ry, 140, 18, s.fullscreen ? "Fullscreen: On" : "Fullscreen: Off")) s.fullscreen = !s.fullscreen;
        ry += 22;
    }
    if (ui_button(c, 14, ry, 140, 18, "Controls")) ui_push(PN_CONTROLS);
    bool canTut = G.scene == SC_SETTLEMENT && G.inLevel;
    if (ui_button(c, 160, ry, 146, 18, tut_active() ? "Tutorial running" : "Replay Tutorial", 0, canTut && !tut_active())) { tut_start(); ui_open(PN_HUD); return; }
    ry += 22;
    if (ui_button(c, 14, ry, 140, 18, "Animation Test Room")) { ui_push(PN_ANIMTEST); }
    if (!canTut) gfx_wrap(c, 160, ry + 2, 146, "Tutorial: start from the settlement", pal::dim);
    if (ui_button(c, 210, 200, 100, 18, "Back") || ui_pressedB()) ui_back();
}
static void panelControls(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Controls");
    // one row per action: the glyph shown is exactly what to press on this device
    struct Row { uint32_t b; const char* action; };
    static const Row rows[] = {
        {BTN_STICK, "Move / aim (facing follows)"}, {BTN_Y, "Fire weapon / swing"}, {BTN_A, "Interact, search, open doors"},
        {BTN_B, "Hold: sprint   Tap: dodge"}, {BTN_X, "Reload"}, {BTN_L, "Hold: lock facing direction"},
        {BTN_R, "Flashlight on / off"}, {BTN_DPAD_LR, "Switch weapon"}, {BTN_UP, "Quick heal"},
        {BTN_DOWN, "Squad order: follow/hold/retreat"}, {BTN_SELECT, "Backpack (pauses the game)"}, {BTN_START, "Pause menu"},
        {BTN_TOUCH, "Menus, drag items in the backpack"},
    };
    int y = 23;
    for (const Row& r : rows) {
        int w = ui_btnW(r.b);
        ui_btn(c, 74 - w, y, r.b);                  // glyphs right-aligned in a column
        gfx_text(c, 82, y + 2, r.action, pal::cream);
        y += 13;
    }
    if (g_canQuit) gfx_text(c, 12, 196, "F11 fullscreen  F12 screenshot", pal::dim), gfx_text(c, 12, 206, "Rebind: controls.ini", pal::dim);
    else gfx_text(c, 12, 200, "Menus also accept the D-Pad + A", pal::dim);
    if (ui_button(c, 210, 198, 100, 20, "Back") || ui_pressedB()) ui_back();
}
static void panelCredits(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Credits");
    gfx_wrap(c, 12, 24, 296, "ZOMBIE SETTLEMENT\n\nDesign, code, pixel art and audio created for this project. All art and sound are original and generated in code.\n\nBuilt with devkitPro / libctru (3DS) and Win32 (PC).\n\nThanks for playing.", pal::cream);
    if (ui_button(c, 210, 200, 100, 18, "Back") || ui_pressedB()) ui_back();
}
static void panelDeath(Canvas& c) {
    ui_panel(c, 30, 40, 260, 140, "Survivor Lost");
    gfx_wrap(c, 40, 62, 240, "Your survivor has died. Death is permanent. Choose who leads now.", pal::cream);
    if (ui_button(c, 80, 140, 160, 22, "Continue", UB_GOLD)) {
        if (settle_population() == 0) { journal(JK_DEATH, "The settlement has fallen."); G.scene = SC_TITLE; G.inLevel = false; ui_open(PN_MAIN); say("Everyone is gone. The settlement has fallen."); return; }
        level_enter(MAP_SETTLEMENT, -1); ui_open(PN_HUD);
    }
}

// animation test room: preview every state in all 8 directions (top screen)
int g_animTestState = 0, g_animTestVariant = 0; bool g_animTestZombie = false; float g_animTestSpeed = 1;
static void panelAnimTest(Canvas& c) {
    ui_panel(c, 4, 3, 312, 218, "Animation Test Room");
    char b[48];
    snprintf(b, sizeof b, "State: %s", anim_name((AnimState)g_animTestState));
    if (ui_button(c, 12, 24, 24, 20, "<")) g_animTestState = (g_animTestState + (int)AnimState::Count - 1) % (int)AnimState::Count;
    gfx_textC(c, 160, 30, b, pal::gold);
    if (ui_button(c, 284, 24, 24, 20, ">")) g_animTestState = (g_animTestState + 1) % (int)AnimState::Count;
    if (ui_button(c, 12, 50, 140, 20, g_animTestZombie ? "Set: Zombie" : "Set: Survivor")) g_animTestZombie = !g_animTestZombie;
    snprintf(b, sizeof b, "Variant %d", g_animTestVariant);
    if (ui_button(c, 160, 50, 148, 20, b)) g_animTestVariant = (g_animTestVariant + 1) % (g_animTestZombie ? ZOMB_VARIANTS : SURV_VARIANTS);
    snprintf(b, sizeof b, "Speed %.2fx", g_animTestSpeed);
    if (ui_button(c, 12, 76, 140, 20, b)) g_animTestSpeed = g_animTestSpeed >= 1 ? 0.25f : g_animTestSpeed * 2;
    const AnimDef& d = anim_def((AnimState)g_animTestState);
    snprintf(b, sizeof b, "%d frames  %s  prio %d", d.n, d.loop ? "loop" : "once", d.priority); gfx_text(c, 14, 104, b, pal::cream);
    int total = 0; for (int i = 0; i < d.n; i++) total += d.f[i].ms10 * 10;
    snprintf(b, sizeof b, "cycle %d ms  %s", total, d.interruptible ? "interruptible" : "locked"); gfx_text(c, 14, 116, b, pal::cream);
    for (int i = 0; i < d.n; i++) {
        snprintf(b, sizeof b, "f%d pose%-2d %3dms kick%+d%s%s", i, d.f[i].pose, d.f[i].ms10 * 10, d.f[i].kick,
                 d.f[i].event & EV_FOOTSTEP ? " STEP" : "", d.f[i].event & (EV_MUZZLE | EV_IMPACT) ? " HIT" : "");
        gfx_text(c, 14, 132 + i * 10, b, pal::dim);
    }
    if (ui_button(c, 210, 200, 100, 18, "Back") || ui_pressedB()) ui_back();
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
    if (G.panel == PN_HUD) { in.down &= ~(BTN_A | BTN_B | BTN_X | BTN_Y | BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_L | BTN_R); in.ax = in.ay = 0; }
    if (G.panel == PN_INVENTORY && tab == 0) { in.down &= ~(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT); in.ax = in.ay = 0; }  // grid cursor owns the D-pad
    if (G.panel == PN_CONTAINER) { in.down &= ~(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT); in.ax = in.ay = 0; }
    if (confirmBox.active) { in = g_input; }
    ui_beginFrame(in);
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
