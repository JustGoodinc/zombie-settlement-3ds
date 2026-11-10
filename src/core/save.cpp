// Versioned, CRC-checked, platform-independent save format with atomic replace.
// Layout: "ZSAV" | u16 version | u32 payload length | payload | u32 crc32(payload)
#include "../gameplay/game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum { SAVE_VERSION = 1, SAVE_CAP = 512 * 1024, SAVE_LOCS = 7 };
static const int LOC_SLOT[LOC_COUNT] = {0, 1, 2, 3, 6};   // Police, Hospital, Supermarket (was Grocery), Train Yard, Neighborhood (was Residential)
static char g_dir[192] = "saves/";
void save_setDir(const char* d) { snprintf(g_dir, sizeof g_dir, "%s", d); }
const char* save_dir() { return g_dir; }

static uint32_t crc32(const uint8_t* b, int n) {
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) { c ^= b[i]; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; }
    return c ^ 0xFFFFFFFFu;
}

struct W {
    uint8_t* b; int cap, n = 0; bool ok = true;
    void raw(const void* p, int len) { if (n + len > cap) { ok = false; return; } memcpy(b + n, p, len); n += len; }
    void u8(uint32_t v) { uint8_t x = (uint8_t)v; raw(&x, 1); }
    void u16(uint32_t v) { u8(v & 255); u8(v >> 8); }
    void u32(uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); }
    void f32(float f) { uint32_t v; memcpy(&v, &f, 4); u32(v); }
    void f64(double d) { uint64_t v; memcpy(&v, &d, 8); u32((uint32_t)v); u32((uint32_t)(v >> 32)); }
    void str(const char* s, int len) { raw(s, len); }
};
struct R {
    const uint8_t* b; int len, n = 0; bool ok = true;
    void raw(void* p, int l) { if (n + l > len) { ok = false; memset(p, 0, l); return; } memcpy(p, b + n, l); n += l; }
    uint32_t u8() { uint8_t x = 0; raw(&x, 1); return x; }
    uint32_t u16() { uint32_t a = u8(); return a | (u8() << 8); }
    uint32_t u32() { uint32_t a = u16(); return a | (u16() << 16); }
    float f32() { uint32_t v = u32(); float f; memcpy(&f, &v, 4); return f; }
    double f64() { uint64_t lo = u32(), hi = u32(); uint64_t v = lo | (hi << 32); double d; memcpy(&d, &v, 8); return d; }
};

static void wItem(W& w, const Item& i) { w.u8(i.id); w.u8(i.x); w.u8(i.y); w.u8(i.rot); w.u16(i.count); w.u16(i.dur); w.u8(i.mag); w.u8(i.mod); }
static void rItem(R& r, Item& i) { i.id = (uint8_t)r.u8(); i.x = (uint8_t)r.u8(); i.y = (uint8_t)r.u8(); i.rot = (uint8_t)r.u8(); i.count = (uint16_t)r.u16(); i.dur = (uint16_t)r.u16(); i.mag = (uint8_t)r.u8(); i.mod = (uint8_t)r.u8();
    if (i.id >= IT_COUNT) { i.id = IT_NONE; r.ok = false; } }
static void wCont(W& w, const Container& c) { w.u8(c.n); w.u8(c.cap); for (int i = 0; i < c.n; i++) wItem(w, c.it[i]); }
static void rCont(R& r, Container& c) { c.n = (uint8_t)r.u8(); c.cap = (uint8_t)r.u8(); if (c.n > Container::MAX) { c.n = 0; r.ok = false; return; } for (int i = 0; i < c.n; i++) rItem(r, c.it[i]); }
static void wGrid(W& w, const Grid& g) { w.u8(g.w); w.u8(g.h); w.u8(g.n); for (int i = 0; i < g.n; i++) wItem(w, g.it[i]); }
static void rGrid(R& r, Grid& g) { g.w = (uint8_t)r.u8(); g.h = (uint8_t)r.u8(); g.n = (uint8_t)r.u8(); if (g.n > Grid::MAX || g.w > 12 || g.h > 8) { g.n = 0; r.ok = false; return; } for (int i = 0; i < g.n; i++) rItem(r, g.it[i]); }

static void wSurv(W& w, const Survivor& s) {
    w.str(s.name, sizeof s.name); w.u8(s.variant); w.u8(s.alive); w.f32(s.hp); w.u8(s.maxHp);
    for (int k = 0; k < SK_COUNT; k++) { w.u8(s.skill[k]); w.u8(s.xp[k]); }
    w.u8(s.job); w.u8(s.trait); w.u8(s.infection); w.f32(s.infHours); w.u8(s.infKnown); w.u8(s.quarantined); w.u8(s.onExpedition);
    wGrid(w, s.pack); w.u8((uint8_t)s.equip); w.u16(s.kills); w.u8(s.deathDay);
}
static void rSurv(R& r, Survivor& s) {
    memset(&s, 0, sizeof s);
    r.raw(s.name, sizeof s.name); s.name[sizeof s.name - 1] = 0; s.variant = (uint8_t)r.u8(); s.alive = r.u8() != 0; s.hp = r.f32(); s.maxHp = (uint8_t)r.u8();
    for (int k = 0; k < SK_COUNT; k++) { s.skill[k] = (uint8_t)r.u8(); s.xp[k] = (uint8_t)r.u8(); }
    s.job = (uint8_t)r.u8(); s.trait = (uint8_t)r.u8(); s.infection = (uint8_t)r.u8(); s.infHours = r.f32(); s.infKnown = r.u8() != 0; s.quarantined = r.u8() != 0; s.onExpedition = r.u8() != 0;
    rGrid(r, s.pack); s.equip = (int8_t)r.u8(); s.kills = (uint16_t)r.u16(); s.deathDay = (uint8_t)r.u8();
    if (s.equip >= s.pack.n) s.equip = -1;
    if (s.job >= JOB_COUNT || s.infection > INF_CRITICAL) r.ok = false;
}

int save_serialize(uint8_t* buf, int cap) {
    W w{buf, cap};
    const Settlement& st = G.st;
    w.f64(G.clock); w.u32(G.day); w.f64(G.activeTime); w.f32(G.hourAcc); w.u32(G.rng.s); w.u8((uint8_t)G.expLoc);
    for (int k = 0; k < 3; k++) w.u8((uint8_t)G.squad[k]);
    for (int k = 0; k < RES_COUNT; k++) w.u32((uint32_t)st.res[k]);
    for (int b = 0; b < B_COUNT; b++) { w.u8(st.bLevel[b]); w.u8(st.bDamage[b]); }
    w.u8(st.wall); w.u8(st.generator); w.u8(st.scanner);
    wCont(w, st.stash);
    w.u8((uint8_t)st.nsurv); for (int i = 0; i < st.nsurv; i++) wSurv(w, st.surv[i]);
    w.u8((uint8_t)st.leader);
    w.u8((uint8_t)st.njournal); w.u8((uint8_t)st.unread);
    for (int i = 0; i < st.njournal; i++) { w.u16(st.journal[i].day); w.u8(st.journal[i].kind); w.str(st.journal[i].text, sizeof st.journal[i].text); }
    w.u8(st.visitorPresent); w.u8(st.visitorScreened); wSurv(w, st.visitor); w.u32(st.nextId);
    // the save format keeps the original 7 location slots; the launch locations map onto slots 0-3 and 6
    static LocState blank;
    for (int slot = 0; slot < SAVE_LOCS; slot++) {
        int l = -1; for (int k = 0; k < LOC_COUNT; k++) if (LOC_SLOT[k] == slot) l = k;
        const LocState& L = l >= 0 ? G.loc[l] : blank;
        w.u8(L.visited); w.u8(L.gen); w.f32(L.cooldown); w.u8(L.rescued);
        for (int c = 0; c < MAX_CONT; c++) { w.u8(L.generated[c]); if (L.generated[c]) wCont(w, L.cont[c]); }
        for (int k = 0; k < 3; k++) w.u32(L.deadMask[k]);
        for (int b = 0; b < Map::MAXBAR; b++) { w.u8(L.barState[b]); w.u16((uint16_t)L.barHp[b]); }
        for (int b = 0; b < MAX_BAGS; b++) { w.u8(L.bags[b].used); if (L.bags[b].used) { w.f32(L.bags[b].x); w.f32(L.bags[b].y); wCont(w, L.bags[b].c); } }
    }
    return w.ok ? w.n : -1;
}

bool save_deserialize(const uint8_t* buf, int len, char* err, int errLen) {
    R r{buf, len};
    // decode into a scratch copy first: a corrupt save must never clobber the running game
    static Game tmp; tmp = G;
    Settlement& st = tmp.st;
    tmp.clock = r.f64(); tmp.day = r.u32(); tmp.activeTime = r.f64(); tmp.hourAcc = r.f32(); tmp.rng.s = r.u32(); tmp.expLoc = (int8_t)r.u8();
    for (int k = 0; k < 3; k++) tmp.squad[k] = (int8_t)r.u8();
    for (int k = 0; k < RES_COUNT; k++) st.res[k] = (int)r.u32();
    for (int b = 0; b < B_COUNT; b++) { st.bLevel[b] = (uint8_t)r.u8(); st.bDamage[b] = (uint8_t)r.u8(); if (st.bLevel[b] > BUILDINGS[b].maxLevel) r.ok = false; }
    st.wall = (uint8_t)r.u8(); st.generator = r.u8() != 0; st.scanner = r.u8() != 0;
    rCont(r, st.stash);
    st.nsurv = (int)r.u8(); if (st.nsurv > MAX_SURV) { snprintf(err, errLen, "Corrupt survivor count"); return false; }
    for (int i = 0; i < st.nsurv; i++) rSurv(r, st.surv[i]);
    st.leader = (int8_t)r.u8();
    st.njournal = (int)r.u8(); st.unread = (int)r.u8(); if (st.njournal > MAX_JOURNAL) { snprintf(err, errLen, "Corrupt journal"); return false; }
    for (int i = 0; i < st.njournal; i++) { st.journal[i].day = (uint16_t)r.u16(); st.journal[i].kind = (uint8_t)r.u8(); r.raw(st.journal[i].text, sizeof st.journal[i].text); st.journal[i].text[sizeof st.journal[i].text - 1] = 0; }
    st.visitorPresent = r.u8() != 0; st.visitorScreened = r.u8() != 0; rSurv(r, st.visitor); st.nextId = r.u32();
    for (int slot = 0; slot < SAVE_LOCS; slot++) {
        int l = -1; for (int k = 0; k < LOC_COUNT; k++) if (LOC_SLOT[k] == slot) l = k;
        static LocState discard;   // retired locations (Scrapyard, Woods) are read and dropped
        LocState& L = l >= 0 ? tmp.loc[l] : discard; memset(&L, 0, sizeof L);
        L.visited = r.u8() != 0; L.gen = (uint8_t)r.u8(); L.cooldown = r.f32(); L.rescued = r.u8() != 0;
        for (int c = 0; c < MAX_CONT; c++) { L.generated[c] = r.u8() != 0; L.cont[c].cap = 8; if (L.generated[c]) rCont(r, L.cont[c]); }
        for (int k = 0; k < 3; k++) L.deadMask[k] = r.u32();
        for (int b = 0; b < Map::MAXBAR; b++) { L.barState[b] = (uint8_t)r.u8(); L.barHp[b] = (int16_t)r.u16(); }
        for (int b = 0; b < MAX_BAGS; b++) { L.bags[b].used = r.u8() != 0; if (L.bags[b].used) { L.bags[b].x = r.f32(); L.bags[b].y = r.f32(); rCont(r, L.bags[b].c); } }
    }
    if (!r.ok || r.n != len) { snprintf(err, errLen, "Save data is damaged (structure check failed)"); return false; }
    if (st.leader >= st.nsurv) st.leader = 0;
    G.st = tmp.st; memcpy(G.loc, tmp.loc, sizeof G.loc);
    G.clock = tmp.clock; G.day = tmp.day; G.activeTime = tmp.activeTime; G.hourAcc = tmp.hourAcc; G.rng = tmp.rng; G.expLoc = tmp.expLoc;
    memcpy(G.squad, tmp.squad, sizeof G.squad);
    return true;
}

static void path(char* out, int n, int slot, const char* ext) { snprintf(out, n, "%sslot%d%s", g_dir, slot + 1, ext); }

static bool readFile(const char* p, uint8_t** data, int* len) {
    FILE* f = fopen(p, "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > SAVE_CAP + 16) { fclose(f); return false; }
    *data = (uint8_t*)malloc(n); *len = (int)fread(*data, 1, n, f); fclose(f);
    return *len == n;
}

static bool validate(const uint8_t* d, int n, int& payload, char* err, int el) {
    if (n < 14 || memcmp(d, "ZSAV", 4) != 0) { snprintf(err, el, "Not a Staying Alive save"); return false; }
    int ver = d[4] | (d[5] << 8);
    if (ver != SAVE_VERSION) { snprintf(err, el, "Unsupported save version %d", ver); return false; }
    payload = d[6] | (d[7] << 8) | (d[8] << 16) | (d[9] << 24);
    if (payload <= 0 || 10 + payload + 4 != n) { snprintf(err, el, "Save file is truncated"); return false; }
    uint32_t c = d[10 + payload] | (d[11 + payload] << 8) | (d[12 + payload] << 16) | ((uint32_t)d[13 + payload] << 24);
    if (c != crc32(d + 10, payload)) { snprintf(err, el, "Save checksum mismatch (corrupted)"); return false; }
    return true;
}

bool save_write(int slot) {
    if (G.scene == SC_TITLE) return false;
    static uint8_t buf[SAVE_CAP];
    int n = save_serialize(buf + 10, SAVE_CAP - 14);
    if (n < 0) return false;
    memcpy(buf, "ZSAV", 4); buf[4] = SAVE_VERSION; buf[5] = 0;
    buf[6] = n & 255; buf[7] = (n >> 8) & 255; buf[8] = (n >> 16) & 255; buf[9] = (n >> 24) & 255;
    uint32_t c = crc32(buf + 10, n);
    buf[10 + n] = c & 255; buf[11 + n] = (c >> 8) & 255; buf[12 + n] = (c >> 16) & 255; buf[13 + n] = c >> 24;
    char tmp[256], main[256], bak[256];
    path(tmp, sizeof tmp, slot, ".tmp"); path(main, sizeof main, slot, ".sav"); path(bak, sizeof bak, slot, ".bak");
    FILE* f = fopen(tmp, "wb"); if (!f) return false;
    bool ok = (int)fwrite(buf, 1, n + 14, f) == n + 14;
    ok = fflush(f) == 0 && ok;
    fclose(f);
    if (!ok) { remove(tmp); return false; }   // never touch the valid save after a failed write
    // verify what landed on disk before replacing anything
    uint8_t* chk = nullptr; int cl = 0; int pl; char e[64];
    bool good = readFile(tmp, &chk, &cl) && validate(chk, cl, pl, e, sizeof e);
    free(chk);
    if (!good) { remove(tmp); return false; }
    remove(bak);
    rename(main, bak);              // keep the previous good save as a fallback
    if (rename(tmp, main) != 0) { rename(bak, main); return false; }
    return true;
}

bool save_read(int slot, char* err, int errLen) {
    const char* exts[2] = {".sav", ".bak"};
    char firstErr[96] = "No save in this slot";
    for (int k = 0; k < 2; k++) {
        char p[256]; path(p, sizeof p, slot, exts[k]);
        uint8_t* d = nullptr; int n = 0, payload = 0;
        if (!readFile(p, &d, &n)) { free(d); continue; }
        char e[96];
        bool ok = validate(d, n, payload, e, sizeof e) && save_deserialize(d + 10, payload, e, sizeof e);
        free(d);
        if (ok) { if (k == 1) snprintf(err, errLen, "Main save was damaged; restored the backup."); else err[0] = 0; return true; }
        if (k == 0) snprintf(firstErr, sizeof firstErr, "%s", e);
    }
    snprintf(err, errLen, "%s", firstErr);
    return false;
}

bool save_exists(int slot) { char p[256]; path(p, sizeof p, slot, ".sav"); FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != nullptr; }
bool save_delete(int slot) {
    char p[256]; path(p, sizeof p, slot, ".sav"); bool a = remove(p) == 0;
    path(p, sizeof p, slot, ".bak"); remove(p);
    return a;
}
bool save_info(int slot, char* out, int len) {
    char p[256]; path(p, sizeof p, slot, ".sav");
    uint8_t* d = nullptr; int n = 0, payload = 0; char e[64];
    if (!readFile(p, &d, &n)) { free(d); snprintf(out, len, "Empty"); return false; }
    bool ok = validate(d, n, payload, e, sizeof e);
    if (ok) {
        R r{d + 10, payload}; double clk = r.f64(); uint32_t day = r.u32();
        snprintf(out, len, "Day %u  %02d:%02d", day, ((int)clk / 60) % 24, (int)clk % 60);
    } else snprintf(out, len, "Damaged: %s", e);
    free(d);
    return ok;
}

// Settings live next to the saves (plain text, tolerant of missing/garbled files).
void settings_load() {
    char p[256]; snprintf(p, sizeof p, "%ssettings.ini", g_dir);
    FILE* f = fopen(p, "r"); if (!f) return;
    int a, b, c, d, e, s, fps, fs, slot, zoom = 200, tut = 0;
    int n = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &s, &fps, &fs, &slot, &zoom, &tut);
    if (n >= 9) {   // older files have 9 fields; zoom + tutorial flag were added later
        Settings& st = G.set;
        auto cl = [](int v, int lo, int hi) { return (uint8_t)(v < lo ? lo : (v > hi ? hi : v)); };
        st.master = cl(a, 0, 10); st.sfx = cl(b, 0, 10); st.music = cl(c, 0, 10);
        st.dayMinutes = (d == 15 || d == 30 || d == 60) ? (uint8_t)d : 30; st.winScale = cl(e, 1, 3);
        st.shake = s != 0; st.showFps = fps != 0; st.fullscreen = fs != 0; G.saveSlot = slot >= 0 && slot < 3 ? slot : 0;
        st.zoom = (zoom == 100 || zoom == 125 || zoom == 150 || zoom == 200) ? (uint8_t)zoom : 200;
        st.tutorialDone = n >= 11 && tut != 0;
    }
    fclose(f);
}
void settings_save() {
    char p[256]; snprintf(p, sizeof p, "%ssettings.ini", g_dir);
    FILE* f = fopen(p, "w"); if (!f) return;
    const Settings& s = G.set;
    fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d\n", s.master, s.sfx, s.music, s.dayMinutes, s.winScale, s.shake, s.showFps, s.fullscreen, G.saveSlot, s.zoom, s.tutorialDone);
    fclose(f);
}
