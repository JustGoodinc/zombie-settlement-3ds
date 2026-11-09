#pragma once
#include "../core/gfx.h"
#include "../core/anim.h"
#include "../core/rng.h"
#include "../world/world.h"
#include "inventory.h"
#include "combat.h"
#include "content.h"

// ---------------------------------------------------------------- input (platform-neutral)
enum Btn : uint32_t {
    BTN_A = 1, BTN_B = 2, BTN_X = 4, BTN_Y = 8, BTN_L = 16, BTN_R = 32, BTN_START = 64, BTN_SELECT = 128,
    BTN_UP = 256, BTN_DOWN = 512, BTN_LEFT = 1024, BTN_RIGHT = 2048
};
struct Input {
    float ax = 0, ay = 0;            // circle pad -1..1
    uint32_t held = 0, down = 0, up = 0;
    bool touching = false, touchDown = false, touchUp = false;
    int tx = 0, ty = 0;              // bottom-screen touch coordinates
};

enum { MAX_SURV = 24, MAX_ZOMB = 72, MAX_PROJ = 48, MAX_PART = 192, MAX_FLOAT = 12, MAX_UNITS = 4, MAX_JOURNAL = 40, MAX_BAGS = 8, MAX_CONT = 80 };
enum { TICK_HZ = 30 };
constexpr float DT = 1.0f / TICK_HZ;

enum Trait : uint8_t { TR_NONE, TR_STEADY, TR_TOUGH, TR_GREEN, TR_MEDIC, TR_TINKER, TR_WATCHFUL, TR_COUNT };
extern const char* TRAIT_NAMES[TR_COUNT];
enum Infection : uint8_t { INF_NONE, INF_EARLY, INF_ADVANCED, INF_CRITICAL };
extern const char* INF_NAMES[4];

struct Survivor {
    char name[14];
    uint8_t variant;          // sprite + portrait variant
    bool alive;
    float hp;
    uint8_t maxHp;
    uint8_t skill[SK_COUNT];  // 1..10
    uint8_t xp[SK_COUNT];
    uint8_t job, trait;
    uint8_t infection;
    float infHours;           // hours spent in current infection stage
    bool infKnown, quarantined, onExpedition;
    Grid pack;
    int8_t equip;             // index into pack of equipped weapon, -1 = none
    uint16_t kills;
    uint8_t deathDay;
};

enum ZState : uint8_t { ZS_IDLE, ZS_WANDER, ZS_LYING, ZS_RISING, ZS_CHASE, ZS_INVESTIGATE, ZS_DEAD };
struct Zombie {
    float x, y, px, py;       // px/py = previous tick (render interpolation)
    float hp;
    uint8_t state, variant, dir;
    int8_t spawnIdx;          // persistent kill mask index, -1 = transient (turned survivor)
    float timer, slowT, atkCd, lostT, flash, groanT, kx, ky;
    float tx, ty, hx, hy, speed;
    int8_t target;
    bool atkPending;
    Animator anim;
};

enum Order : uint8_t { ORD_FOLLOW, ORD_HOLD, ORD_RETREAT };
struct Unit {
    int8_t sv;                // survivor index, -1 = rescued NPC (escort)
    bool alive, escort;
    float x, y, px, py, fx, fy;
    float stam, cd, reloadT, searchT, dodgeT, dvx, dvy, hurtT, stuckT, muzzleT, healT;
    int16_t searchProp;
    uint8_t order, dir, npcVariant;
    bool sprinting, meleePending;
    float bHeld;           // how long B has been held (tap = dodge, hold = sprint)
    Animator anim;
};

struct Projectile {
    bool active, untargeted;
    float x, y, vx, vy, life;
    int8_t target; uint8_t result, dmg, owner, item;
};
enum PartKind : uint8_t { PK_BLOOD, PK_SPARK, PK_CASING, PK_SMOKE, PK_DUST, PK_LEAF, PK_MUZZLE, PK_SPLINTER, PK_GLASS, PK_EMBER };
struct Particle { float x, y, z, vx, vy, vz, life, max; px col; uint8_t kind; };
struct FloatText { float x, y, life; char txt[12]; px col; };

struct Bag { bool used; float x, y; Container c; };
struct LocState {
    bool visited;
    uint8_t gen;              // loot generation (increments on refresh)
    float cooldown;           // active seconds until refresh, <0 = timer not running
    bool generated[MAX_CONT];
    Container cont[MAX_CONT];
    uint32_t deadMask[3];
    uint8_t barState[Map::MAXBAR];
    int16_t barHp[Map::MAXBAR];
    bool rescued;
    Bag bags[MAX_BAGS];
};

struct JournalEntry { uint16_t day; uint8_t kind; char text[72]; };
enum JKind : uint8_t { JK_INFO, JK_GOOD, JK_WARN, JK_DEATH };

struct Settlement {
    int res[RES_COUNT];
    uint8_t bLevel[B_COUNT];
    uint8_t bDamage[B_COUNT];    // 0 none, 1 damaged, 2 badly damaged
    float bBuildFx[B_COUNT];     // construction effect timer (render)
    uint8_t wall;                // wall condition 0..100
    bool generator, scanner;
    Container stash;
    Survivor surv[MAX_SURV];
    int nsurv;
    int leader;
    JournalEntry journal[MAX_JOURNAL];
    int njournal, unread;
    bool visitorPresent, visitorScreened;
    Survivor visitor;
    uint32_t nextId;
};

struct Settings {
    uint8_t master = 8, sfx = 8, music = 6;   // 0..10
    uint8_t dayMinutes = 30;                  // real minutes per in-game day
    bool shake = true, showFps = false;
    uint8_t winScale = 2;                     // Windows window scale
    bool fullscreen = false;
};

enum Scene : uint8_t { SC_TITLE, SC_SETTLEMENT, SC_EXPEDITION };
enum Panel : uint8_t {
    PN_MAIN, PN_HUD, PN_PAUSE, PN_INVENTORY, PN_CONTAINER, PN_BASE, PN_BUILDING, PN_SURVIVORS, PN_RECRUIT,
    PN_INFIRMARY, PN_WORKSHOP, PN_WORLDMAP, PN_PREP, PN_SUMMARY, PN_JOURNAL, PN_SETTINGS, PN_CONTROLS,
    PN_SAVELOAD, PN_CREDITS, PN_DEATH, PN_ANIMTEST, PN_SQUAD, PN_COUNT
};

struct Summary {
    int resGained[RES_COUNT];
    char lines[10][48]; int nlines;
    int8_t rescued;            // visitor slot filled with rescued survivor
    bool failed;
};

struct Game {
    Scene scene = SC_TITLE;
    Panel panel = PN_MAIN;
    Panel stack[8]; int nstack = 0;
    Settlement st;
    LocState loc[LOC_COUNT];
    int curLoc = -1;
    // live level
    Map map;
    Zombie z[MAX_ZOMB]; int nz = 0;
    Unit u[MAX_UNITS]; int nu = 0; int ctrl = 0;
    Projectile proj[MAX_PROJ];
    Particle part[MAX_PART];
    FloatText ft[MAX_FLOAT];
    uint16_t flowZ[Map::MAXW * Map::MAXH], flowC[Map::MAXW * Map::MAXH], flowExit[Map::MAXW * Map::MAXH];
    float flowT = 0;
    uint8_t light[Map::MAXW * Map::MAXH];     // per-tile visibility 0..255 (render + gameplay visibility)
    uint8_t explored[Map::MAXW * Map::MAXH];
    float camX = 0, camY = 0, shake = 0;
    bool flashlight = true; float flashAnim = 1;
    Rng rng{12345};
    double clock = 8 * 60;     // minutes since midnight
    uint32_t day = 1;
    double activeTime = 0;     // seconds of unpaused gameplay
    float hourAcc = 0;
    Summary sum;
    Settings set;
    int saveSlot = 0;
    char toast[72] = {0}; float toastT = 0;
    uint32_t tick = 0;
    float uiTime = 0, panelT = 0, tabT = 0;   // UI animation clocks (run while paused)
    bool lockFacing = false;
    bool quitRequested = false;
    float fps = 0, frameMs = 0;
    int debugAnim = 0;         // anim test room selection
    bool inLevel = false;
    int8_t squad[3] = {-1, -1, -1};   // survivors chosen for the next expedition (0 = controlled)
    int8_t expLoc = -1;              // location of an in-progress expedition (persisted)
    struct Decal { float x, y; uint8_t kind; } decal[64]; int ndecal = 0;
};

struct Settler {           // lightweight settlement NPC (one per survivor)
    float x, y, px, py, tx, ty, wait;
    uint8_t dir; bool asleep, working;
    Animator anim;
};
extern Settler g_npc[MAX_SURV];
extern Game G;

// ---- game.cpp
void game_init();
void game_frame(const Input& in, float realDt);   // runs fixed-step sim + UI
void game_render(Canvas& top, Canvas& bot, float alpha);
bool game_paused();
void game_newGame();
void game_toast(const char* fmt, ...);
void level_enter(int mapId, int locIndex);
void level_leave();
int  unit_ctrl();
void game_advanceHours(float hours);   // sleeping / time skip, processes settlement ticks
void spawn_particles(int kind, float x, float y, int n, px col);
void float_text(float x, float y, const char* s, px col);
bool survivor_useItem(int sv, int itemIdx, int unitIdx);
void survivor_equipBest(Survivor& s);
const char* clock_str();
bool is_night();
float daylight();   // 0 (night) .. 1 (day)

// ---- test / tool hooks (game.cpp)
void dbg_zombieHit(int zi, int result, int dmg);
void dbg_fire(int ui);
void expedition_finish(bool failed);
void spawn_turned(int sv);

// ---- settlement.cpp
void settle_newGame();
void settle_hour();
void settle_day();
int  settle_security();
const char* security_rating(int s);
int  settle_capacity();
int  settle_population();
int  settle_stashCap();
void journal(int kind, const char* fmt, ...);
Survivor make_survivor(Rng& r, bool allowInfected);
bool can_afford(const uint8_t* cost);
void pay(const uint8_t* cost);
int  deliver_item(const Item& it);   // converts to settlement resources, returns amount
void skill_xp(Survivor& s, int sk, int amt);
void survivor_kill(int sv, const char* cause);
bool settle_treat(int sv, bool& cured);
int  treat_cost(const Survivor& s);
int  repair_cost(const Item& it);
void settle_spawnVisitor();
void loot_generate(int loc, int contIdx, int propType, int special, Container& out);
void loc_tick(float dt);

// ---- save.cpp
bool save_write(int slot);
bool save_read(int slot, char* err, int errLen);
bool save_info(int slot, char* out, int len);
bool save_exists(int slot);
bool save_delete(int slot);
const char* save_dir();
void save_setDir(const char* d);
void settings_load();
void settings_save();
// serialisation for tests
int  save_serialize(uint8_t* buf, int cap);
bool save_deserialize(const uint8_t* buf, int len, char* err, int errLen);

// ---- menus.cpp
void ui_update(const Input& in);
void ui_dropInput();
void ui_render(Canvas& bot);
void ui_open(Panel p);
void ui_push(Panel p);
void ui_back();
void ui_container(int propIdx, int bagIdx);
extern int g_contProp, g_contBag, g_invSurvivor, g_buildingSel;

// ---- render.cpp
void render_world(Canvas& top, float alpha);
void render_title(Canvas& top);
void render_compute_light();
