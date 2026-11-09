// Skippable guided first run. Each step shows a banner on the top screen and
// completes when the player actually performs the action (no fake "press next").
#include "game.h"
#include "../ui/ui.h"
#include "../core/audio.h"
#include <math.h>
#include <stdio.h>

enum Step { ST_MOVE, ST_BACKPACK, ST_PLAN, ST_DEPLOY, ST_SEARCH, ST_LOOT, ST_FIRE, ST_RELOAD, ST_SPRINT, ST_EXTRACT, ST_DONE, ST_COUNT };
struct StepDef { uint32_t btn; const char* title; const char* text; };
static const StepDef STEPS[ST_COUNT] = {
    {BTN_STICK,  "Move",            "Walk around the camp. You always face the way you move."},
    {BTN_SELECT, "Backpack",        "Open your backpack. The world pauses while it is open. Close it again."},
    {BTN_TOUCH,  "Plan a run",      "Tap Expedition (or press A at the radio table) to open the world map."},
    {BTN_TOUCH,  "Deploy",          "Pick the Police Station, then press Deploy, then Deploy again."},
    {BTN_A,      "Search",          "Walk up to a glowing green container and search it."},
    {BTN_TOUCH,  "Loot",            "Take All grabs what fits. Leftovers stay put. Then go Back."},
    {BTN_Y,      "Fire",            "Face a zombie and fire. Headshots kill instantly; gunshots are loud."},
    {BTN_X,      "Reload",          "Reload your weapon. Dodging cancels a reload without losing ammo."},
    {BTN_B,      "Sprint & dodge",  "Hold to sprint, tap to dodge. Both use stamina."},
    {BTN_A,      "Extract",         "Return to the yellow chevrons on the road and extract."},
    {0,          "Tutorial done",   "You're ready. Scavenge, survive, rebuild."},
};

bool tut_active() { return G.tut >= 0; }

void tut_start() {
    G.tut = ST_MOVE; G.tutT = 0; G.tutDist = 0; G.tutFlag = false;
    if (G.inLevel && G.nu) { G.tutLX = G.u[G.ctrl].x; G.tutLY = G.u[G.ctrl].y; }
}
static void finish(bool skipped) {
    G.tut = -1;
    G.set.tutorialDone = true;
    settings_save();
    game_toast(skipped ? "Tutorial skipped. Replay it any time from Settings." : "Tutorial complete!");
}
void tut_skip() { if (tut_active()) finish(true); }

static uint32_t base[4];   // counters captured when a step starts
static void next() {
    G.tut++; G.tutT = 0; G.tutFlag = false;
    base[0] = G.statShots; base[1] = G.statReloads; base[2] = G.statDodges; base[3] = (uint32_t)(G.statSprint * 10);
    audio_play(SFX_MENU_OK, 0.7f);
}

int tut_highlight() {
    switch (G.tut) {
    case ST_PLAN: return TH_EXPEDITION;
    case ST_DEPLOY: return TH_DEPLOY;
    case ST_LOOT: return TH_TAKEALL;
    }
    return TH_NONE;
}

void tut_update() {
    if (!tut_active()) return;
    G.tutT += G.frameMs / 1000.0f;
    if (!G.inLevel) { G.tut = -1; return; }   // left to the main menu: drop it quietly
    const Unit& u = G.u[G.ctrl];
    // dying or extracting early ends the expedition part of the tutorial gracefully
    if (G.tut >= ST_SEARCH && G.tut < ST_DONE && G.panel == PN_SUMMARY) { G.tut = ST_DONE; G.tutT = 0; return; }
    switch (G.tut) {
    case ST_MOVE:
        G.tutDist += sqrtf((u.x - G.tutLX) * (u.x - G.tutLX) + (u.y - G.tutLY) * (u.y - G.tutLY));
        G.tutLX = u.x; G.tutLY = u.y;
        if (G.tutDist > 64) next();
        break;
    case ST_BACKPACK:
        if (G.panel == PN_INVENTORY) G.tutFlag = true;
        else if (G.tutFlag && G.panel == PN_HUD) next();
        break;
    case ST_PLAN: if (G.panel == PN_WORLDMAP) next(); break;
    case ST_DEPLOY: if (G.scene == SC_EXPEDITION) next(); break;
    case ST_SEARCH: if (G.panel == PN_CONTAINER) next(); break;
    case ST_LOOT: if (G.panel == PN_HUD) next(); break;
    case ST_FIRE: if (G.statShots > base[0]) next(); break;
    case ST_RELOAD: if (G.statReloads > base[1]) next(); break;
    case ST_SPRINT: if (G.statDodges > base[2] || (uint32_t)(G.statSprint * 10) > base[3] + 5) next(); break;
    case ST_EXTRACT: if (G.scene == SC_SETTLEMENT) next(); break;
    case ST_DONE: if (G.tutT > 5) finish(false); break;
    }
}

void tut_render(Canvas& c) {
    if (!tut_active() || G.tut >= ST_COUNT) return;
    if (G.panel != PN_HUD && G.tut != ST_BACKPACK && G.tut != ST_LOOT && G.tut != ST_PLAN && G.tut != ST_DEPLOY && G.tut != ST_DONE) return;
    const StepDef& s = STEPS[G.tut];
    // slide in for each new step
    float t = G.tutT < 0.25f ? G.tutT / 0.25f : 1;
    int w = 392, h = 44, x = (TOP_W - w) / 2, y = 192 + (int)((1 - t) * 40);
    gfx_fillA(c, x + 2, y + 2, w, h, rgb(0, 0, 0), 140);
    gfx_fill(c, x, y, w, h, pal::ink);
    gfx_fill(c, x + 1, y + 1, w - 2, h - 2, rgb(40, 38, 34));
    bool pulse = ((int)(G.uiTime * 2.5f)) & 1;
    gfx_rect(c, x, y, w, h, pulse ? pal::gold : pal::goldDk);
    char head[48];
    if (G.tut == ST_DONE) snprintf(head, sizeof head, "%s", s.title);
    else snprintf(head, sizeof head, "TUTORIAL %d/%d  %s", G.tut + 1, (int)ST_DONE, s.title);
    gfx_textB(c, x + 6, y + 4, head, pal::gold);
    int gx = x + 6;
    if (s.btn) gx += ui_btn(c, x + 6, y + 18, s.btn) + 5;
    gfx_wrap(c, gx, y + 19, x + w - gx - 6, s.text, pal::cream);
    if (G.tut != ST_DONE) gfx_text(c, x + w - gfx_textW("Skip: Pause") - 6, y + 4, "Skip: Pause", pal::dim);
}
