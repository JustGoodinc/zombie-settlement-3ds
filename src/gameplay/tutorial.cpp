// The guided first run. It opens with a short scene at the settlement and takes the player straight out on a raid:
// move, search, loot, backpack, fight, reload, dodge, extract. Back home a second scene walks the camera round the
// six buildings, and the last lessons put the haul into an upgrade. Lessons finish when the player actually does the
// thing (no fake "press next"); the two scenes advance on A. Everything can be skipped.
#include "game.h"
#include "../ui/ui.h"
#include "../core/audio.h"
#include "../core/art.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- state
enum { CB_SAY, CB_WALK, CB_CARD };
struct Beat {
    uint8_t kind;
    int8_t face;          // portrait variant of the speaker, -1 nobody (narration or a building), -2 the radio
    int8_t plot;          // building this line is about (framed on screen, pictured beside the text), -1 none
    char who[16], text[140];
    float fx, fy;         // CB_SAY: the world point the camera settles on. CB_WALK: where the leader walks to
};
static struct Tut {
    float t;                              // seconds in the current lesson
    float dist, lx, ly;                   // TS_MOVE: distance walked
    bool flag;                            // TS_PACK: the backpack has been opened
    uint32_t reloads, dodges, sprint10;   // the player's counters when the lesson began
    int levels;                           // sum of building levels when the lesson began
    int prop;                             // the container the raid lessons point at, -1 none
    Beat beat[10]; int nbeat, ib;         // the scene being played
    float bt, st, fade;                   // seconds in the beat and in the scene; black-out 0..1
    bool carded;                          // the title card has already moved the game to the raid
    bool high; int bannerH;               // the lesson banner sits under the HUD because the player is at the foot of the screen; its height
} T;
enum { TYPE_CPS = 48 };                   // characters a second as a line types itself out

struct StepDef { const char* title; const char* goal; };
static const StepDef STEPS[TS_COUNT] = {
    {"", ""},
    {"Move",      "Walk to the marked car."},
    {"Search",    "Search the marked car."},
    {"Loot",      "Take what you found."},
    {"Backpack",  "Open your backpack, then close it."},
    {"Fight",     "Kill a zombie."},
    {"Reload",    "Reload your weapon."},
    {"Dodge",     "Dodge or sprint."},
    {"Extract",   "Extract at the yellow chevrons."},
    {"Home",      "Return home."},
    {"", ""},
    {"Buildings", "Open a building."},
    {"Upgrade",   "Upgrade a building."},
    {"Next raid", "Plan the next raid at the radio table."},
};

bool tut_active() { return G.tut >= 0; }
bool tut_scene() { return G.tut == TS_INTRO || G.tut == TS_TOUR; }

static void finish(bool skipped) {
    G.tut = -1; T.fade = 0;
    G.set.tutorialDone = true;
    settings_save();
    game_toast(skipped ? "Tutorial skipped. Replay it any time from Settings." : "Tutorial complete. Stay alive.");
}
void tut_skip() { if (tut_active()) finish(true); }

static int levels() { int n = 0; for (int b = 0; b < B_COUNT; b++) n += G.st.bLevel[b]; return n; }
// the cheapest upgrade the stores can pay for right now, or -1
static int cheapest() {
    int best = -1, bestMat = 1 << 30;
    for (int b = 0; b < B_COUNT; b++) {
        int lv = G.st.bLevel[b];
        if (lv < 1 || lv >= BUILDINGS[b].maxLevel || !can_afford(BUILDINGS[b].cost[lv])) continue;
        if (BUILDINGS[b].cost[lv][RES_MAT] < bestMat) { bestMat = BUILDINGS[b].cost[lv][RES_MAT]; best = b; }
    }
    return best;
}
// centre of the first prop of a type (plots: of the building `special`)
static bool propPos(int type, int special, float& x, float& y) {
    for (int i = 0; i < G.map.nprop; i++) {
        const Prop& p = G.map.prop[i];
        if (p.type == type && (special < 0 || p.special == special)) { x = p.tx * 16 + p.tw * 8.0f; y = p.ty * 16 + p.th * 8.0f; return true; }
    }
    return false;
}

// ---------------------------------------------------------------- the fight lesson
// Two slow zombies shamble in from one side of the street, east if there is room. They are marked as lesson zombies:
// their bites are weak and cannot infect (see zombie_attack_land), so the first fight teaches without ending a run.
static void lessonZombies() {
    const Unit& u = G.u[G.ctrl]; int ux = (int)(u.x / 16), uy = (int)(u.y / 16);
    static const int8_t OFF[5][2] = {{8, 0}, {9, 1}, {8, -1}, {9, 2}, {7, -2}};
    for (int side = 1; side >= -1; side -= 2) {
        int spot[2][2], n = 0;
        for (auto& o : OFF) {
            int tx = ux + o[0] * side, ty = uy + o[1];
            if (n == 2 || !G.map.in(tx, ty) || (G.map.flags[G.map.idx(tx, ty)] & (F_BLOCK | F_INDOOR))) continue;
            if (!map_los(G.map, tx * 16 + 8.0f, ty * 16 + 8.0f, u.x, u.y)) continue;
            spot[n][0] = tx; spot[n][1] = ty; n++;
        }
        if (n < 2 && side == 1) continue;
        for (int k = 0; k < n && G.nz < MAX_ZOMB; k++) {
            Zombie& z = G.z[G.nz++]; z = Zombie{};
            z.x = z.px = spot[k][0] * 16 + 8.0f; z.y = z.py = spot[k][1] * 16 + 10.0f;
            z.hp = 100; z.spawnIdx = SPAWN_LESSON; z.speed = 0.75f;
            z.variant = (uint8_t)zombie_look(G.map.id, false, hash32(k * 977u + 31u + G.day));
            z.state = ZS_CHASE; z.target = (int8_t)G.ctrl; z.tx = u.x; z.ty = u.y;
            z.hx = (float)-side; z.hy = 0; z.dir = (uint8_t)dir8(z.hx, z.hy);
            z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
            if (!k) audio_playAt(SFX_GROAN, z.x, z.y, 1.0f);
        }
        return;
    }
}

// The marked container holds a fixed, useful haul the first time it is searched in the tutorial: enough materials
// to matter when the buildings are shown, rounds for the fight, and food.
void tut_loot(int prop, Container& c) {
    if (G.tut != TS_SEARCH || prop != T.prop) return;
    c.n = 0;
    c.push(makeItem(IT_TOOLS)); c.push(makeItem(IT_SCRAP, 3)); c.push(makeItem(IT_AMMO_9MM, 12)); c.push(makeItem(IT_CANNED, 2));
}

// ---------------------------------------------------------------- scenes
static Beat& beat(int kind, float fx, float fy) {
    Beat& b = T.beat[T.nbeat < 10 ? T.nbeat++ : 9];
    b = Beat{}; b.kind = (uint8_t)kind; b.face = b.plot = -1; b.fx = fx; b.fy = fy;
    return b;
}
static Beat& say(int face, const char* who, float fx, float fy, const char* fmt, ...) {
    Beat& b = beat(CB_SAY, fx, fy); b.face = (int8_t)face;
    snprintf(b.who, sizeof b.who, "%s", who);
    va_list a; va_start(a, fmt); vsnprintf(b.text, sizeof b.text, fmt, a); va_end(a);
    return b;
}
static void sceneBegin() { T.nbeat = T.ib = 0; T.bt = T.st = T.fade = 0; T.carded = false; }

// before the raid: the fire, the radio, the leader, the walk to the gate, the title card
static void sceneIntro() {
    sceneBegin();
    const Unit& u = G.u[G.ctrl]; const Survivor& lead = G.st.surv[u.sv];
    float fx = u.x, fy = u.y, rx = u.x, ry = u.y, gx = 0, gy = 0; int gates = 0;
    propPos(PR_FIRE, -1, fx, fy); propPos(PR_RADIO, -1, rx, ry);
    for (int i = 0; i < G.map.nbar; i++) if (G.map.bar[i].kind == BK_GATE) { gx += G.map.bar[i].x * 16 + 8.0f; gy += G.map.bar[i].y * 16 + 8.0f; gates++; }
    say(-1, "", fx, fy, "Day %u. %d survivors behind one fence, and the food will not last the week.", (unsigned)G.day, settle_population());
    say(-2, "RADIO", rx, ry, "...police station on Fifth... the armoury is still locked... one of ours is alive in the cells...");
    say(lead.variant, lead.name, u.x, u.y, "I'll make the first raid alone. In, grab what I can carry, out. Keep the gate shut behind me.");
    if (gates) beat(CB_WALK, gx / gates, gy / gates - 6);
    beat(CB_CARD, 0, 0);
}
// after the raid: once round the six buildings
static void sceneTour() {
    sceneBegin();
    const Unit& u = G.u[G.ctrl]; const Survivor& lead = G.st.surv[u.sv];
    int g = -1;
    for (int i = 0; i < G.st.nsurv && g < 0; i++) if (i != u.sv && G.st.surv[i].alive && !G.st.surv[i].quarantined) g = i;
    if (g >= 0) say(G.st.surv[g].variant, G.st.surv[g].name, u.x, u.y, "You're back in one piece. Walk with me: this is what we have to work with.");
    else say(-1, "", u.x, u.y, "Home. This is what there is to work with.");
    static const struct { uint8_t b; const char* text; } STOP[B_COUNT] = {
        {B_STORAGE,   "Everything you haul home is kept here. A bigger store holds more."},
        {B_DEFENSES,  "The watchtower and the wall. Guards posted here keep the dead out."},
        {B_WORKSHOP,  "Worn weapons are repaired at the bench. Upgraded, it improves them too."},
        {B_KITCHEN,   "Farmers grow food here every day. A bigger garden feeds more mouths."},
        {B_QUARTERS,  "Beds. Sleep here to pass the night. More beds make room for more survivors."},
        {B_INFIRMARY, "Wounds and bites are treated here. A better infirmary treats worse infections."},
    };
    for (auto& s : STOP) {
        float x, y; if (!propPos(PR_PLOT, s.b, x, y)) continue;
        say(-1, BUILDINGS[s.b].name, x, y, "%s", s.text).plot = (int8_t)s.b;
    }
    say(lead.variant, lead.name, u.x, u.y, "Every one of them gets better with materials. Let's put today's haul to work.");
}

static void go(int step) {
    G.tut = (int8_t)step; T.t = 0; T.flag = false; T.dist = 0;
    T.reloads = G.statReloads; T.dodges = G.statDodges; T.sprint10 = (uint32_t)(G.statSprint * 10); T.levels = levels();
    if (G.inLevel && G.nu) { T.lx = G.u[G.ctrl].x; T.ly = G.u[G.ctrl].y; }
    if (step == TS_FIGHT) lessonZombies();
    if (step == TS_TOUR) sceneTour();
    audio_play(SFX_MENU_OK, 0.7f);
}

// out of the gate and on to the police station, alone; the nearest container becomes the lesson, and the street
// around the arrival point is kept quiet (those zombies simply are not there this visit)
static void deploy() {
    int loc = 0; for (int l = 0; l < LOC_COUNT; l++) if (LOCATIONS[l].map == MAP_POLICE) loc = l;
    G.squad[0] = G.u[G.ctrl].sv; G.squad[1] = G.squad[2] = -1;
    expedition_begin(loc);
    G.toastT = 0;
    const Unit& u = G.u[G.ctrl]; float bd = 1e9f; T.prop = -1;
    for (int i = 0; i < G.map.nprop; i++) {
        const Prop& p = G.map.prop[i]; if (p.container < 0) continue;
        float d = hypotf(p.tx * 16 + p.tw * 8.0f - u.x, p.ty * 16 + p.th * 8.0f - u.y);
        if (d < bd) { bd = d; T.prop = i; }
    }
    for (int i = G.nz - 1; i >= 0; i--) if (hypotf(G.z[i].x - u.x, G.z[i].y - u.y) < 224) G.z[i] = G.z[--G.nz];
    T.carded = true;
}
static void sceneEnd() {
    T.fade = 0;
    if (G.tut == TS_INTRO) { if (!T.carded) deploy(); go(TS_MOVE); }
    else go(TS_BUILDING);
}
static void nextBeat() {
    if (T.ib + 1 >= T.nbeat) { sceneEnd(); return; }
    T.ib++; T.bt = 0;
    if (T.beat[T.ib].kind == CB_WALK) {   // the gate swings open for the leader
        for (int i = 0; i < G.map.nbar; i++) if (G.map.bar[i].kind == BK_GATE) G.map.bar[i].state = BS_OPEN;
        map_refresh(G.map);
        audio_play(SFX_DOOR, 0.8f);
    } else audio_play(SFX_MENU_MOVE, 0.6f);
}
static void sceneUpdate(float dt) {
    T.bt += dt; T.st += dt;
    const Beat& b = T.beat[T.ib];
    if (b.kind == CB_WALK) {
        const Unit& u = G.u[G.ctrl];
        if (hypotf(b.fx - u.x, b.fy - u.y) < 7 || T.bt > 6) nextBeat();
    } else if (b.kind == CB_CARD) {
        const float OUT = 0.4f, HOLD = 1.7f, IN = 0.45f;
        if (T.bt < OUT) T.fade = T.bt / OUT;
        else {
            if (!T.carded) deploy();
            T.fade = T.bt < OUT + HOLD ? 1.0f : 1.0f - (T.bt - OUT - HOLD) / IN;
        }
        if (T.bt >= OUT + HOLD + IN) sceneEnd();
    }
}
void tut_sceneNext() {
    if (!tut_scene()) return;
    const Beat& b = T.beat[T.ib];
    if (b.kind != CB_SAY) return;                                   // the walk and the title card play themselves
    int len = (int)strlen(b.text);
    if (T.bt * TYPE_CPS < len) T.bt = (len + 1.0f) / TYPE_CPS;     // first press shows the whole line
    else nextBeat();
}
void tut_sceneSkip() { if (tut_scene()) sceneEnd(); }
const char* tut_sceneTitle() { return G.tut == TS_INTRO ? "The First Raid" : "Home"; }
int tut_sceneBeat(int& total) { total = T.nbeat; return T.ib; }

// While a scene plays the pad belongs to it: A is next, START skips, and the leader is walked by the scene.
void tut_input(Input& in) {
    if (!tut_scene()) return;
    uint32_t down = in.down;
    in = Input();
    if (G.panel != PN_HUD) return;
    if (down & BTN_START) { tut_sceneSkip(); return; }
    if (down & BTN_A) tut_sceneNext();
    if (!tut_scene()) return;
    const Beat& b = T.beat[T.ib];
    if (b.kind != CB_WALK) return;
    const Unit& u = G.u[G.ctrl];
    float dx = b.fx - u.x, dy = b.fy - u.y, l = hypotf(dx, dy);
    if (l > 1) { in.ax = dx / l * 0.8f; in.ay = dy / l * 0.8f; }
}
// Where a scene wants the middle of the picture (the band between the bars sits above the screen centre).
bool tut_camera(float& x, float& y) {
    if (!tut_scene()) return false;
    const Beat& b = T.beat[T.ib];
    if (b.kind != CB_SAY) return false;
    x = b.fx - view_w() / 2; y = b.fy - view_h() / 2 + 19.0f * 100 / view_zoom();
    return true;
}

void tut_start() {
    if (G.scene != SC_SETTLEMENT || !G.inLevel || !G.nu) return;
    T = Tut{}; T.prop = -1;
    G.tut = TS_INTRO; G.toastT = 0;
    sceneIntro();
}

// ---------------------------------------------------------------- lessons
int tut_highlight() {
    switch (G.tut) {
    case TS_LOOT: return TH_TAKEALL;
    case TS_BUILDING: return TH_BASE;
    case TS_UPGRADE: return TH_UPGRADE;
    case TS_PLAN: return TH_RAID;
    }
    return TH_NONE;
}
const char* tut_objective() { return tut_active() && !tut_scene() ? STEPS[G.tut].goal : nullptr; }

void tut_update() {
    if (!tut_active()) return;
    float dt = G.frameMs / 1000.0f;
    T.t += dt;
    if (!G.inLevel || G.scene == SC_TITLE || G.panel == PN_DEATH || !G.nu) { G.tut = -1; T.fade = 0; return; }   // left for the main menu, or nobody left
    if (tut_scene()) { sceneUpdate(dt); return; }
    const Unit& u = G.u[G.ctrl];
    // home again, however the raid ended (extracted early, or carried out): on to the settlement half
    if (G.tut >= TS_MOVE && G.tut <= TS_EXTRACT && G.scene == SC_SETTLEMENT) go(TS_HOME);
    switch (G.tut) {
    case TS_MOVE:
        T.dist += hypotf(u.x - T.lx, u.y - T.ly); T.lx = u.x; T.ly = u.y;
        if (T.dist > 48) go(TS_SEARCH);
        break;
    case TS_SEARCH: if (G.panel == PN_CONTAINER) go(TS_LOOT); break;
    case TS_LOOT: if (G.panel == PN_HUD) go(TS_PACK); break;
    case TS_PACK:
        if (G.panel == PN_INVENTORY) T.flag = true;
        else if (T.flag && G.panel == PN_HUD) go(TS_FIGHT);
        break;
    case TS_FIGHT: {
        int alive = 0, dead = 0;
        for (int i = 0; i < G.nz; i++) if (G.z[i].spawnIdx == SPAWN_LESSON) { if (G.z[i].state == ZS_DEAD) dead++; else alive++; }
        if (dead || (!alive && T.t > 1) || T.t > 60) go(TS_RELOAD);
        break;
    }
    case TS_RELOAD: {
        const Survivor& s = G.st.surv[u.sv];
        const Item* w = s.equip >= 0 && s.equip < s.pack.n ? &s.pack.it[s.equip] : nullptr;
        bool gun = w && idef(w->id).cat == IC_FIREARM;
        bool nothing = !gun || w->mag >= idef(w->id).wpn.mag || s.pack.countOf(idef(w->id).wpn.ammo) <= 0;   // full, or nothing to load
        if (G.statReloads > T.reloads || (nothing && u.reloadT <= 0)) go(TS_DODGE);
        break;
    }
    case TS_DODGE: if (G.statDodges > T.dodges || (uint32_t)(G.statSprint * 10) > T.sprint10 + 5) go(TS_EXTRACT); break;
    case TS_EXTRACT: break;   // the settlement check above
    case TS_HOME: if (G.panel == PN_HUD) go(TS_TOUR); break;
    case TS_BUILDING: if (G.panel == PN_BUILDING) go(TS_UPGRADE); break;
    case TS_UPGRADE:
        if (levels() > T.levels) go(TS_PLAN);
        else if (cheapest() < 0 && G.panel == PN_HUD && T.t > 0.5f) go(TS_PLAN);   // nothing affordable: shown, not forced
        break;
    case TS_PLAN: if (G.panel == PN_WORLDMAP || T.t > 25) finish(false); break;
    }
}

// ---------------------------------------------------------------- drawing
static uint32_t glyphOf(const char* name, int n) {
    static const struct { const char* n; uint32_t b; } GL[] = {{"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y}, {"L", BTN_L},
        {"R", BTN_R}, {"START", BTN_START}, {"SELECT", BTN_SELECT}, {"PAD", BTN_STICK}};
    for (auto& g : GL) if ((int)strlen(g.n) == n && !strncmp(g.n, name, n)) return g.b;
    return 0;
}
// Text with button glyphs written as {A} {B} {X} {Y} {L} {R} {START} {SELECT} {PAD}. Wraps to maxW and draws the
// first `reveal` characters (a glyph counts as one) when a canvas is given; returns the line count and the total.
static int rich(Canvas* c, int x, int y, int maxW, const char* s, px col, int reveal, int* total = nullptr) {
    int cx = 0, line = 0, shown = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char* e = s; int w = 0;   // one word: letters and glyphs up to the next space
        while (*e && *e != ' ') {
            if (*e == '{') { const char* q = strchr(e, '}'); uint32_t b = q ? glyphOf(e + 1, (int)(q - e - 1)) : 0; if (b) { w += ui_btnW(b) + 1; e = q + 1; continue; } }
            w += 6; e++;
        }
        if (cx && cx + w > maxW) { cx = 0; line++; }
        for (const char* p = s; p < e;) {
            if (*p == '{') {
                const char* q = strchr(p, '}'); uint32_t b = q ? glyphOf(p + 1, (int)(q - p - 1)) : 0;
                if (b) { if (c && shown < reveal) ui_btn(*c, x + cx, y + line * 12 - 2, b); cx += ui_btnW(b) + 1; shown++; p = q + 1; continue; }
            }
            if (c && shown < reveal) { char ch[2] = {*p, 0}; gfx_text(*c, x + cx, y + line * 12, ch, col); }
            cx += 6; shown++; p++;
        }
        cx += 6;
        s = e;
    }
    if (total) *total = shown;
    return line + 1;
}

static void lessonText(char* out, int cap) {
    const char* thing = "car";
    if (T.prop >= 0 && T.prop < G.map.nprop && G.scene == SC_EXPEDITION) thing = G.map.prop[T.prop].type == PR_WRECK ? "wrecked car" : (G.map.prop[T.prop].type == PR_CAR ? "car" : "container");
    switch (G.tut) {
    case TS_MOVE: snprintf(out, cap, "Move with {PAD}. You always face the way you move. Walk up to the %s.", thing); break;
    case TS_SEARCH: snprintf(out, cap, "Anything outlined in green can be searched. Stand beside the %s and press {A}.", thing); break;
    case TS_LOOT: snprintf(out, cap, "Take All grabs everything that fits in your backpack. Leftovers stay put. Then go Back."); break;
    case TS_PACK: snprintf(out, cap, "Open your backpack with {SELECT}. Everything you carry takes up room, and the world waits while it is open. Close it again."); break;
    case TS_FIGHT: snprintf(out, cap, "They heard you. Face a zombie and fire with {Y}. Hold {L} to keep your aim while you back away."); break;
    case TS_RELOAD: snprintf(out, cap, "Reload with {X}. The rounds left in the magazine are shown at the top right."); break;
    case TS_DODGE: snprintf(out, cap, "Tap {B} to dodge a lunge, or hold it to sprint. Both use stamina, the yellow bar."); break;
    case TS_EXTRACT: snprintf(out, cap, "That is a raid: get in, take what you can carry, get out. Go back to the yellow chevrons and press {A}."); break;
    case TS_HOME: snprintf(out, cap, "Food, materials and fuel you carried out go into the settlement's stores. Weapons, ammo and medicine stay in your pack."); break;
    case TS_BUILDING: snprintf(out, cap, "Walk up to a building and press {A}, or tap Base and then Manage. The marked one is the cheapest to improve."); break;
    case TS_UPGRADE: {
        int b = cheapest();
        if (b < 0) snprintf(out, cap, "The stores cannot pay for an upgrade yet. Materials come from raids. Go Back for now.");
        else if (G.panel == PN_BUILDING) snprintf(out, cap, "Pick %s, choose the Upgrade tab, then press Upgrade. The cost comes out of the stores.", BUILDINGS[b].name);
        else snprintf(out, cap, "Open a building again (press {A} beside it, or tap Base and then Manage) and upgrade it.");
        break;
    }
    case TS_PLAN: snprintf(out, cap, "That is the loop: raid, haul it home, build. Plan the next raid at the radio table, or tap Raid."); break;
    default: out[0] = 0;
    }
}

// what the lesson points at in the world, if anything
static bool target(float& x, float& y) {
    switch (G.tut) {
    case TS_MOVE: case TS_SEARCH: {
        if (T.prop < 0 || T.prop >= G.map.nprop || G.scene != SC_EXPEDITION) return false;
        const Prop& p = G.map.prop[T.prop]; x = p.tx * 16 + p.tw * 8.0f; y = p.ty * 16 - 6.0f;
        return true;
    }
    case TS_EXTRACT: x = G.map.exitX * 16 + 8.0f; y = G.map.exitY * 16 + 2.0f; return G.scene == SC_EXPEDITION;
    case TS_BUILDING: {
        int b = cheapest(); if (b < 0) b = B_STORAGE;
        if (!propPos(PR_PLOT, b, x, y)) return false;
        y += 20 - g_bldWorld[b][G.st.bLevel[b] > 3 ? 3 : G.st.bLevel[b]].h;   // above the roof
        return true;
    }
    }
    return false;
}
// a gold chevron bobbing over the target, or an arrow on the screen edge pointing the way when it is out of view
static void marker(Canvas& c) {
    float wx, wy;
    if (G.panel != PN_HUD || !target(wx, wy)) return;
    float z = view_zoom() / 100.0f;
    int sx = (int)((wx - G.camX) * z), sy = (int)((wy - G.camY) * z), bob = (int)(sinf(G.uiTime * 6) * 2.5f);
    const px ink = rgb(22, 22, 34), gold = pal::gold;
    // the part of the screen that is clear of the HUD and of the lesson banner (which may be at the top or the bottom)
    const int y0 = T.high ? 56 + T.bannerH : 50, y1 = T.high ? TOP_H - 8 : TOP_H - 12 - T.bannerH;
    if (sx > 16 && sx < TOP_W - 16 && sy > y0 + 8 && sy < y1) {
        int y = sy - 12 + bob;
        for (int r = 0; r < 8; r++) gfx_fill(c, sx - 7 + r, y - 1 + r, 15 - 2 * r, 1, ink);
        for (int r = 0; r < 6; r++) gfx_fill(c, sx - 5 + r, y + r, 11 - 2 * r, 1, r < 2 ? rgb(255, 236, 150) : gold);
        return;
    }
    const float cy = (y0 + y1) / 2.0f, reach = (y1 - y0) / 2.0f - 10;
    float dx = sx - TOP_W / 2.0f, dy = sy - cy, l = hypotf(dx, dy); if (l < 1) return;
    dx /= l; dy /= l;
    float k = fminf(dx ? (TOP_W / 2.0f - 22) / fabsf(dx) : 1e9f, dy ? reach / fabsf(dy) : 1e9f);   // out to the edge of the clear area
    float ex = TOP_W / 2.0f + dx * (k + bob), ey = cy + dy * (k + bob);
    for (int pass = 0; pass < 2; pass++) {
        int half = pass ? 5 : 7, len = pass ? 9 : 12; float tx = ex + dx * (pass ? 0 : 2), ty = ey + dy * (pass ? 0 : 2);
        for (int i = -half * 2; i <= half * 2; i++) {
            float bx = tx - dx * len - dy * i * 0.5f, by = ty - dy * len + dx * i * 0.5f;
            gfx_line(c, (int)tx, (int)ty, (int)bx, (int)by, pass ? gold : ink);
        }
    }
}

static void banner(Canvas& c) {
    Panel p = G.panel;   // shown when the lesson is about what is on screen
    bool show = p == PN_HUD || (G.tut == TS_LOOT && p == PN_CONTAINER) || (G.tut == TS_PACK && p == PN_INVENTORY) || G.tut == TS_HOME ||
                ((G.tut == TS_BUILDING || G.tut == TS_UPGRADE) && (p == PN_BASE || p == PN_BUILDING)) || (G.tut == TS_PLAN && p == PN_WORLDMAP);
    if (!show) return;
    char text[220]; lessonText(text, sizeof text);
    const int w = 392, x = (TOP_W - w) / 2, lines = rich(nullptr, 0, 0, w - 20, text, 0, 0), h = 22 + lines * 12;
    float t = T.t < 0.25f ? T.t / 0.25f : 1;   // slides in for each new lesson
    // along the bottom, unless the player is standing under it (the camera stops at the edge of a map): then under the HUD
    if (p == PN_HUD && G.nu) {
        float py = (G.u[G.ctrl].y - G.camY) * view_zoom() / 100.0f;
        if (py > TOP_H - 8 - h) T.high = true; else if (py < TOP_H - 70 - h) T.high = false;
    } else T.high = false;
    T.bannerH = h;
    int y = T.high ? 50 - (int)((1 - t) * 40) : TOP_H - 4 - h + (int)((1 - t) * 40);
    gfx_fillA(c, x + 2, y + 2, w, h, rgb(0, 0, 0), 130);
    gfx_fill(c, x, y, w, h, rgb(22, 22, 34));
    gfx_fill(c, x + 1, y + 1, w - 2, h - 2, rgb(32, 34, 52));
    gfx_fill(c, x + 1, y + 1, w - 2, 1, rgb(64, 68, 96));
    gfx_fill(c, x + 1, y + 1, 3, h - 2, ((int)(G.uiTime * 2.5f) & 1) ? pal::gold : pal::goldDk);
    char head[48]; int num = G.tut - (G.tut > TS_TOUR ? 1 : 0);
    snprintf(head, sizeof head, "TUTORIAL %d/%d  %s", num, TS_COUNT - 2, STEPS[G.tut].title);
    gfx_textB(c, x + 10, y + 5, head, pal::gold);
    gfx_text(c, x + w - gfx_textW("Skip: Pause") - 6, y + 5, "Skip: Pause", pal::dim);
    rich(&c, x + 10, y + 18, w - 20, text, pal::cream, 9999);
}

// gold corner brackets round a building while the tour talks about it
static void frame(Canvas& c, int b) {
    float x, y; if (!propPos(PR_PLOT, b, x, y)) return;
    const Sprite& s = g_bldWorld[b][G.st.bLevel[b] > 3 ? 3 : G.st.bLevel[b]];
    float z = view_zoom() / 100.0f;
    int x0 = (int)((x - s.w / 2 - 4 - G.camX) * z), y0 = (int)((y + 24 - s.h - 4 - G.camY) * z);
    int x1 = (int)((x + s.w / 2 + 4 - G.camX) * z), y1 = (int)((y + 24 + 4 - G.camY) * z), L = 10;
    px col = ((int)(G.uiTime * 3) & 1) ? pal::gold : rgb(255, 236, 150);
    for (int k = 0; k < 2; k++) {
        gfx_fill(c, x0, y0 + k, L, 1, col); gfx_fill(c, x0 + k, y0, 1, L, col); gfx_fill(c, x1 - L, y0 + k, L, 1, col); gfx_fill(c, x1 - 1 - k, y0, 1, L, col);
        gfx_fill(c, x0, y1 - 1 - k, L, 1, col); gfx_fill(c, x0 + k, y1 - L, 1, L, col); gfx_fill(c, x1 - L, y1 - 1 - k, L, 1, col); gfx_fill(c, x1 - 1 - k, y1 - L, 1, L, col);
    }
}
static void sceneRender(Canvas& c) {
    const Beat& b = T.beat[T.ib];
    const px ink = rgb(16, 16, 26), edge = rgb(64, 68, 96);
    float in = T.st < 0.3f ? T.st / 0.3f : 1;
    int topB = (int)(18 * in), botB = (int)(58 * in), y0 = TOP_H - botB;
    if (b.kind == CB_SAY && b.plot >= 0 && T.bt > 0.5f) frame(c, b.plot);
    gfx_fill(c, 0, 0, TOP_W, topB, ink); gfx_fill(c, 0, topB, TOP_W, 1, edge);
    gfx_fill(c, 0, y0, TOP_W, botB, ink); gfx_fill(c, 0, y0 - 1, TOP_W, 1, edge);
    if (in >= 1) {
        char up[24]; int n = 0; for (const char* s = tut_sceneTitle(); *s && n < 23; s++) up[n++] = (*s >= 'a' && *s <= 'z') ? *s - 32 : *s;
        up[n] = 0;
        gfx_textB(c, 8, 5, up, pal::gold);
        int sw = ui_btnW(BTN_START) + 4 + gfx_textW("Skip");
        ui_btn(c, TOP_W - 8 - sw, 3, BTN_START); gfx_text(c, TOP_W - 8 - gfx_textW("Skip"), 5, "Skip", pal::dim);
        if (b.kind == CB_SAY) {
            int tx = 12;
            if (b.face >= 0 || b.face == -2 || b.plot >= 0) {   // who (or what) is talking, in a well beside the line
                gfx_fill(c, 8, y0 + 8, 28, 28, rgb(22, 22, 34)); gfx_rect(c, 8, y0 + 8, 28, 28, rgb(108, 112, 146));
                if (b.face >= 0) gfx_blit(c, g_portrait[b.face % PORTRAITS], 10, y0 + 10);
                else if (b.face == -2) { gfx_blit(c, g_spr[S_RADIO], 14, y0 + 14); if ((int)(G.uiTime * 4) & 1) gfx_fill(c, 22, y0 + 19, 2, 2, rgb(255, 120, 100)); }
                else {
                    const Sprite& s = g_building[b.plot][G.st.bLevel[b.plot] > 3 ? 3 : G.st.bLevel[b.plot]];
                    for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) { px v = s.d[(j * 2) * s.w + i * 2]; if (v != KEYC) gfx_pset(c, 10 + i, y0 + 10 + j, v); }
                }
                tx = 44;
            }
            int ty = y0 + 9, total = 0;
            if (b.who[0]) { gfx_textB(c, tx, ty, b.who, b.face == -2 ? pal::cyan : pal::gold); ty += 12; } else ty += 4;
            rich(&c, tx, ty, TOP_W - tx - 26, b.text, b.face == -2 ? rgb(170, 214, 232) : pal::cream, (int)(T.bt * TYPE_CPS), &total);
            if ((int)(T.bt * TYPE_CPS) >= total && ((int)(G.uiTime * 2.5f) & 1)) ui_btn(c, TOP_W - 20, TOP_H - 17, BTN_A);
        }
    }
    if (T.fade > 0) {
        if (T.fade >= 1) gfx_fill(c, 0, 0, TOP_W, TOP_H, rgb(8, 8, 14)); else gfx_fillA(c, 0, 0, TOP_W, TOP_H, rgb(8, 8, 14), (int)(T.fade * 255));
        if (T.fade >= 1 && T.carded && G.curLoc >= 0) {   // title card: where we are going
            char up[32]; int n = 0; for (const char* s = LOCATIONS[G.curLoc].name; *s && n < 31; s++) up[n++] = (*s >= 'a' && *s <= 'z') ? *s - 32 : *s;
            up[n] = 0;
            int w = gfx_textW(up, 3), x = (TOP_W - w) / 2, y = 92;
            gfx_text(c, x - 2, y, up, rgb(232, 60, 124), 3, false); gfx_text(c, x + 2, y, up, rgb(60, 204, 228), 3, false); gfx_text(c, x, y, up, pal::cream, 3, false);
            gfx_fill(c, x, y + 30, w, 2, pal::gold);
            char sub[32]; snprintf(sub, sizeof sub, "Day %u  %s", (unsigned)G.day, clock_str());
            gfx_textC(c, TOP_W / 2, y + 38, sub, pal::dim);
        }
    }
}

void tut_render(Canvas& c) {
    if (!tut_active()) return;
    if (tut_scene()) { sceneRender(c); return; }
    marker(c);
    banner(c);
}
