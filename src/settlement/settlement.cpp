// Settlement economy: resources, buildings, jobs, infection, incidents, recruitment, loot.
#include "../gameplay/game.h"
#include "../core/audio.h"
#include "../core/art.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

void spawn_turned(int sv);

void journal(int kind, const char* fmt, ...) {
    Settlement& st = G.st;
    if (st.njournal >= MAX_JOURNAL) { memmove(st.journal, st.journal + 1, sizeof(JournalEntry) * (MAX_JOURNAL - 1)); st.njournal = MAX_JOURNAL - 1; }
    JournalEntry& e = st.journal[st.njournal++];
    e.day = (uint16_t)G.day; e.kind = (uint8_t)kind;
    va_list a; va_start(a, fmt); vsnprintf(e.text, sizeof e.text, fmt, a); va_end(a);
    st.unread++;
}

static const char* NAMES[] = {"Marcus", "Elena", "Riley", "Javier", "Nora", "Owen", "Priya", "Dmitri", "Hana", "Caleb", "Rosa", "Theo",
                              "Ines", "Bram", "Maya", "Silas", "Wren", "Tobias", "Lena", "Felix", "Ada", "Hugo", "Mira", "Jonah", "Clara", "Vik"};

Survivor make_survivor(Rng& r, bool allowInfected) {
    Survivor s; memset(&s, 0, sizeof s);
    snprintf(s.name, sizeof s.name, "%s", NAMES[r.range((int)(sizeof(NAMES) / sizeof(NAMES[0])))]);
    s.variant = (uint8_t)r.range(SURV_VARIANTS);
    s.alive = true; s.maxHp = 100;
    for (int k = 0; k < SK_COUNT; k++) s.skill[k] = (uint8_t)r.between(1, 4);
    s.skill[r.range(SK_COUNT)] = (uint8_t)r.between(5, 7);
    s.trait = (uint8_t)r.range(TR_COUNT);
    if (s.trait == TR_TOUGH) s.maxHp = 120;
    if (s.trait == TR_STEADY) s.skill[SK_SHOOT]++;
    if (s.trait == TR_GREEN) s.skill[SK_FARM] += 2;
    if (s.trait == TR_MEDIC) s.skill[SK_MEDIC] += 2;
    if (s.trait == TR_TINKER) s.skill[SK_MECH] += 2;
    if (s.trait == TR_WATCHFUL) s.skill[SK_GUARD] += 2;
    for (int k = 0; k < SK_COUNT; k++) if (s.skill[k] > 10) s.skill[k] = 10;
    s.hp = s.maxHp * (0.6f + r.f() * 0.4f);
    s.pack.w = 6; s.pack.h = 4;
    s.equip = -1;
    s.pack.add(makeItem(r.chance(0.5f) ? IT_KNIFE : IT_CROWBAR));
    if (r.chance(0.4f)) { s.pack.add(makeItem(IT_PISTOL)); s.pack.it[s.pack.n - 1].mag = (uint8_t)r.between(2, 8); }
    survivor_equipBest(s);
    if (allowInfected && r.chance(0.22f)) { s.infection = INF_EARLY; s.infKnown = false; s.infHours = (float)r.range(12); }
    // pick a best-fitting job
    int bestK = 0; for (int k = 1; k < SK_COUNT; k++) if (s.skill[k] > s.skill[bestK]) bestK = k;
    static const uint8_t JOB_FOR[SK_COUNT] = {JOB_GUARD, JOB_GUARD, JOB_MEDIC, JOB_MECHANIC, JOB_FARMER, JOB_GUARD};
    s.job = JOB_FOR[bestK];
    return s;
}

void settle_newGame() {
    Settlement& st = G.st;
    memset(&st, 0, sizeof st);
    st.res[RES_FOOD] = 30; st.res[RES_MED] = 6; st.res[RES_AMMO] = 20; st.res[RES_MAT] = 30; st.res[RES_FUEL] = 6;
    for (int b = 0; b < B_COUNT; b++) st.bLevel[b] = 1;
    st.bLevel[B_WORKSHOP] = 1;
    st.wall = 100;
    st.stash.cap = (uint8_t)settle_stashCap();
    struct Init { const char* name; uint8_t variant, job; uint8_t sk[SK_COUNT]; uint8_t trait; } init[5] = {
        {"Marcus", 0, JOB_GUARD, {6, 4, 2, 2, 2, 5}, TR_STEADY},
        {"Elena", 1, JOB_MEDIC, {3, 2, 6, 2, 3, 2}, TR_MEDIC},
        {"Riley", 2, JOB_FARMER, {3, 3, 2, 3, 5, 3}, TR_GREEN},
        {"Javier", 3, JOB_MECHANIC, {3, 4, 2, 6, 2, 3}, TR_TINKER},
        {"Nora", 7, JOB_GUARD, {4, 3, 3, 2, 2, 5}, TR_WATCHFUL},
    };
    Rng r(777);
    for (int i = 0; i < 5; i++) {
        Survivor& s = st.surv[i]; memset(&s, 0, sizeof s);
        snprintf(s.name, sizeof s.name, "%s", init[i].name);
        s.variant = init[i].variant; s.alive = true; s.maxHp = 100; s.hp = 100;
        memcpy(s.skill, init[i].sk, SK_COUNT); s.job = init[i].job; s.trait = init[i].trait;
        s.pack.w = 6; s.pack.h = 4; s.equip = -1;
    }
    st.nsurv = 5;
    Survivor& m = st.surv[0];
    m.pack.add(makeItem(IT_PISTOL)); m.pack.add(makeItem(IT_AMMO_9MM, 24)); m.pack.add(makeItem(IT_KNIFE)); m.pack.add(makeItem(IT_BANDAGE, 2));
    st.surv[1].pack.add(makeItem(IT_KNIFE)); st.surv[1].pack.add(makeItem(IT_BANDAGE, 2)); st.surv[1].pack.add(makeItem(IT_PILLS, 1));
    st.surv[2].pack.add(makeItem(IT_BAT)); st.surv[2].pack.add(makeItem(IT_CANNED, 1));
    st.surv[3].pack.add(makeItem(IT_CROWBAR)); st.surv[3].pack.add(makeItem(IT_REVOLVER)); st.surv[3].pack.add(makeItem(IT_AMMO_38, 12));
    st.surv[4].pack.add(makeItem(IT_PISTOL)); st.surv[4].pack.add(makeItem(IT_AMMO_9MM, 12)); st.surv[4].pack.add(makeItem(IT_KNIFE));
    for (int i = 0; i < 5; i++) survivor_equipBest(st.surv[i]);
    st.leader = 0;
    st.nextId = 100;
}

int settle_capacity() { static const int C[5] = {5, 8, 12, 16, 20}; return C[G.st.bLevel[B_QUARTERS] > 4 ? 4 : G.st.bLevel[B_QUARTERS]]; }
int settle_population() { int n = 0; for (int i = 0; i < G.st.nsurv; i++) n += G.st.surv[i].alive; return n; }
int settle_stashCap() { static const int C[4] = {8, 16, 24, 32}; return C[G.st.bLevel[B_STORAGE] > 3 ? 3 : G.st.bLevel[B_STORAGE]]; }

int settle_security() {
    const Settlement& st = G.st;
    float sec = st.bLevel[B_DEFENSES] * 12.0f;
    int guards = 0;
    for (int i = 0; i < st.nsurv; i++) {
        const Survivor& s = st.surv[i];
        if (!s.alive || s.onExpedition || s.quarantined || s.job != JOB_GUARD || s.hp < 25) continue;
        guards++;
        sec += 12 + s.skill[SK_GUARD] * 3 + (s.trait == TR_WATCHFUL ? 5 : 0);
    }
    if (guards > 0) sec += st.res[RES_AMMO] >= guards ? 8 : -10;
    sec -= st.bDamage[B_DEFENSES] * 8;
    sec *= 0.5f + st.wall / 200.0f;
    int pop = settle_population();
    if (pop > 8) sec -= (pop - 8) * 1.5f;   // bigger camp draws more attention
    for (int i = 0; i < st.nsurv; i++) if (st.surv[i].alive && st.surv[i].infection && !st.surv[i].quarantined && st.surv[i].infKnown) sec -= 6;
    return sec < 0 ? 0 : (sec > 100 ? 100 : (int)sec);
}
const char* security_rating(int s) { return s >= 70 ? "Safe" : (s >= 45 ? "Guarded" : (s >= 25 ? "Risky" : "DANGEROUS")); }

bool can_afford(const uint8_t* cost) { for (int r = 0; r < RES_COUNT; r++) if (G.st.res[r] < cost[r]) return false; return true; }
void pay(const uint8_t* cost) { for (int r = 0; r < RES_COUNT; r++) G.st.res[r] -= cost[r]; }

int deliver_item(const Item& it) {
    const ItemDef& d = idef(it.id);
    if (d.cat == IC_EQUIPMENT) {
        if (it.id == IT_GENERATOR) { if (G.st.generator) G.st.res[RES_MAT] += 12; G.st.generator = true; journal(JK_GOOD, "A generator was installed. Kitchen and workshop are powered."); }
        else { if (G.st.scanner) G.st.res[RES_MAT] += 10; G.st.scanner = true; journal(JK_GOOD, "A medical scanner was installed at the infirmary."); }
        return 1;
    }
    if (d.cat == IC_FIREARM || d.cat == IC_MELEE || d.cat == IC_KEY || d.cat == IC_BACKPACK) {
        int v = d.resAmt + 2; G.st.res[RES_MAT] += v; return v;   // scrapped for parts
    }
    int amt = d.cat == IC_AMMO ? (it.count + 5) / 6 : d.resAmt * it.count;
    G.st.res[d.res] += amt;
    return amt;
}

void skill_xp(Survivor& s, int sk, int amt) {
    if (s.skill[sk] >= 10) return;
    int x = s.xp[sk] + amt;
    while (s.skill[sk] < 10 && x >= 6 + s.skill[sk] * 4) {
        x -= 6 + s.skill[sk] * 4; s.skill[sk]++;
        game_toast("%s's %s improved to %d!", s.name, SKILL_NAMES[sk], s.skill[sk]);
    }
    s.xp[sk] = (uint8_t)(x > 255 ? 255 : x);
}

void survivor_kill(int sv, const char* cause) {
    Survivor& s = G.st.surv[sv];
    if (!s.alive) return;
    s.alive = false; s.hp = 0; s.deathDay = (uint8_t)(G.day > 255 ? 255 : G.day);
    s.quarantined = false;
    journal(JK_DEATH, "%s died: %s.", s.name, cause);
    if (G.st.leader == sv) {
        G.st.leader = -1;
        for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive && !G.st.surv[i].onExpedition) { G.st.leader = i; break; }
    }
}

int treat_cost(const Survivor& s) {
    if (s.infection == INF_EARLY) return 1;
    if (s.infection == INF_ADVANCED) return 3;
    if (s.infection == INF_CRITICAL) return 5;
    return s.hp < s.maxHp ? 1 : 0;
}

bool settle_treat(int sv, bool& cured) {
    Survivor& s = G.st.surv[sv];
    cured = false;
    int cost = treat_cost(s);
    if (!cost || G.st.res[RES_MED] < cost || G.st.bLevel[B_INFIRMARY] == 0) return false;
    int level = G.st.bLevel[B_INFIRMARY] + (G.st.scanner ? 1 : 0);
    int medic = 0;
    for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive && !G.st.surv[i].onExpedition && G.st.surv[i].job == JOB_MEDIC && G.st.surv[i].skill[SK_MEDIC] > medic) medic = G.st.surv[i].skill[SK_MEDIC];
    G.st.res[RES_MED] -= cost;
    if (s.infection == INF_NONE) { s.hp = fminf(s.maxHp, s.hp + 40 + medic * 3); cured = true; return true; }
    float chance = s.infection == INF_EARLY ? 1.0f : (s.infection == INF_ADVANCED ? 0.85f : 0.6f);
    if (level < s.infection) chance *= 0.35f;          // under-equipped infirmary
    chance += medic * 0.03f;
    if (G.rng.chance(chance)) { s.infection = INF_NONE; s.infHours = 0; s.infKnown = false; cured = true; journal(JK_GOOD, "%s was cured of infection.", s.name); }
    else { s.infHours = 0; journal(JK_WARN, "Treatment for %s failed; the infection is held back.", s.name); }
    return true;
}

int repair_cost(const Item& it) {
    const ItemDef& d = idef(it.id);
    if (!d.wpn.dur || it.dur >= d.wpn.dur) return 0;
    int c = (int)ceilf((d.wpn.dur - it.dur) * 8.0f / d.wpn.dur);
    if (G.st.bLevel[B_WORKSHOP] >= 3) c = (c + 1) / 2;
    int mech = 0;
    for (int i = 0; i < G.st.nsurv; i++) if (G.st.surv[i].alive && G.st.surv[i].job == JOB_MECHANIC && G.st.surv[i].skill[SK_MECH] > mech) mech = G.st.surv[i].skill[SK_MECH];
    c -= mech / 4;
    return c < 1 ? 1 : c;
}

void settle_spawnVisitor() {
    Rng r(G.rng.next());
    G.st.visitor = make_survivor(r, true);
    G.st.visitorPresent = true; G.st.visitorScreened = false;
    journal(JK_INFO, "A stranger named %s is waiting at the gate.", G.st.visitor.name);
}

static void incident() {
    Settlement& st = G.st;
    int sec = settle_security(), pop = settle_population();
    float chance = 0.30f - sec * 0.0035f + pop * 0.004f;
    if (chance < 0.03f) chance = 0.03f; if (chance > 0.45f) chance = 0.45f;
    // guards gain experience on watch
    for (int i = 0; i < st.nsurv; i++) if (st.surv[i].alive && !st.surv[i].onExpedition && st.surv[i].job == JOB_GUARD) skill_xp(st.surv[i], SK_GUARD, 2);
    if (!G.rng.chance(chance)) return;
    static const char* SIDES[] = {"eastern", "western", "northern", "southern"};
    const char* side = SIDES[G.rng.range(4)];
    if (sec >= 45) {
        st.wall = (uint8_t)(st.wall > 5 ? st.wall - 5 : 0);
        journal(JK_WARN, "Zombies hit the %s fence overnight. The guards drove them off.", side);
        return;
    }
    st.wall = (uint8_t)(st.wall > 15 ? st.wall - 15 : 0);
    int b = G.rng.range(B_COUNT);
    if (st.bLevel[b] > 0 && st.bDamage[b] < 2) st.bDamage[b]++;
    int lost = G.rng.between(2, 6); if (lost > st.res[RES_FOOD]) lost = st.res[RES_FOOD];
    st.res[RES_FOOD] -= lost;
    int deaths = 0;
    if (sec < 25) {
        // only an under-defended camp can lose people
        int want = G.rng.chance(0.5f) ? 1 : 0;
        if (sec < 12 && G.rng.chance(0.25f)) want++;
        for (int k = 0; k < want; k++) {
            int cand[MAX_SURV], nc = 0;
            for (int i = 0; i < st.nsurv; i++) if (st.surv[i].alive && !st.surv[i].onExpedition && i != st.leader) cand[nc++] = i;
            if (!nc) break;
            survivor_kill(cand[G.rng.range(nc)], "killed when the walls were breached");
            deaths++;
        }
    } else {
        for (int i = 0; i < st.nsurv; i++) if (st.surv[i].alive && !st.surv[i].onExpedition && G.rng.chance(0.2f)) { st.surv[i].hp -= 25; if (st.surv[i].hp < 5) st.surv[i].hp = 5; break; }
    }
    if (deaths) journal(JK_DEATH, "Zombies broke through the %s %s. %d killed, the %s was damaged.", side, st.bLevel[B_DEFENSES] ? "gate" : "fence", deaths, BUILDINGS[b].name);
    else journal(JK_WARN, "Zombies broke through the %s fence. %d food lost, the %s was damaged.", side, lost, BUILDINGS[b].name);
    audio_play(SFX_ALARM, 0.7f);
}

void settle_hour() {
    Settlement& st = G.st;
    int hour = ((int)G.clock / 60) % 24;
    for (int i = 0; i < st.nsurv; i++) {
        Survivor& s = st.surv[i];
        if (!s.alive) continue;
        if (!s.onExpedition && s.hp < s.maxHp && !s.infection) s.hp = fminf(s.maxHp, s.hp + 0.4f + (st.bLevel[B_INFIRMARY] && s.quarantined ? 0.6f : 0));
        if (s.infection) {
            s.infHours += 1;
            if (s.infHours >= 20) {
                s.infHours = 0;
                if (s.infection < INF_CRITICAL) {
                    s.infection++;
                    if (!s.infKnown && s.infection >= INF_ADVANCED) { s.infKnown = true; journal(JK_WARN, "%s is showing signs of infection.", s.name); }
                    else if (s.infKnown) journal(JK_WARN, "%s's infection has worsened (%s).", s.name, INF_NAMES[s.infection]);
                } else {
                    bool quarantined = s.quarantined;
                    survivor_kill(i, "succumbed to infection");
                    if (quarantined) journal(JK_DEATH, "%s turned in quarantine. The guards put them down.", s.name);
                    else spawn_turned(i);
                }
            }
        }
    }
    if (hour == 2) incident();
    if (hour == 18 && settle_security() < 25) { game_toast("Warning: settlement security is DANGEROUS."); journal(JK_WARN, "Security is dangerously low. Assign more guards."); }
    if (hour == 6) settle_day();
}

void settle_day() {
    Settlement& st = G.st;
    G.day++;
    int pop = settle_population(), food = 0, mats = 0;
    for (int i = 0; i < st.nsurv; i++) {
        Survivor& s = st.surv[i];
        if (!s.alive || s.onExpedition || s.quarantined) continue;
        switch (s.job) {
        case JOB_FARMER: if (st.bLevel[B_KITCHEN]) food += 1 + st.bLevel[B_KITCHEN] + s.skill[SK_FARM] / 3; skill_xp(s, SK_FARM, 2); break;
        case JOB_WORKER: mats += 1 + (s.skill[SK_MECH] > 5); break;
        case JOB_MEDIC: skill_xp(s, SK_MEDIC, 1); break;
        case JOB_MECHANIC:
            skill_xp(s, SK_MECH, 2);
            for (int b = 0; b < B_COUNT; b++) if (st.bDamage[b] && st.res[RES_MAT] >= 4) { st.bDamage[b]--; st.res[RES_MAT] -= 4; journal(JK_INFO, "%s repaired the %s.", s.name, BUILDINGS[b].name); break; }
            if (st.wall < 100 && st.res[RES_MAT] >= 2) { st.wall = (uint8_t)(st.wall + 15 > 100 ? 100 : st.wall + 15); st.res[RES_MAT] -= 2; }
            break;
        case JOB_GUARD: if (st.res[RES_AMMO] > 0) st.res[RES_AMMO]--; break;
        }
    }
    if (st.generator) food += food / 2;
    if (st.bLevel[B_KITCHEN]) food += st.bLevel[B_KITCHEN];   // garden baseline
    st.res[RES_FOOD] += food - pop;
    st.res[RES_MAT] += mats;
    if (st.res[RES_FOOD] < 0) {
        st.res[RES_FOOD] = 0;
        for (int i = 0; i < st.nsurv; i++) if (st.surv[i].alive) { st.surv[i].hp -= 10; if (st.surv[i].hp < 1) st.surv[i].hp = 1; }
        journal(JK_WARN, "Day %u: food ran out. Everyone went hungry.", G.day);
    } else journal(JK_INFO, "Day %u: +%d food, -%d eaten. Security %s.", G.day, food, pop, security_rating(settle_security()));
    if (!st.visitorPresent && pop < settle_capacity() && G.rng.chance(0.35f)) settle_spawnVisitor();
    else if (st.visitorPresent && G.rng.chance(0.4f)) { st.visitorPresent = false; journal(JK_INFO, "%s gave up waiting and left.", st.visitor.name); }
    if (G.scene == SC_SETTLEMENT) save_write(G.saveSlot);
}

// ---------------------------------------------------------------- loot
struct LootEntry { uint8_t item, minC, maxC, weight; };
static const LootEntry L_DESK[] = {{IT_BANDAGE, 1, 1, 4}, {IT_PILLS, 1, 1, 3}, {IT_AMMO_9MM, 4, 10, 5}, {IT_CANNED, 1, 1, 3}, {IT_SCRAP, 1, 1, 2}, {IT_AMMO_38, 3, 6, 2}};
static const LootEntry L_FILING[] = {{IT_AMMO_9MM, 4, 8, 3}, {IT_AMMO_38, 3, 6, 2}, {IT_PILLS, 1, 1, 2}, {IT_SCRAP, 1, 1, 2}, {IT_TOOLS, 1, 1, 1}, {IT_BANDAGE, 1, 1, 2}};
static const LootEntry L_LOCKER[] = {{IT_PISTOL, 1, 1, 3}, {IT_REVOLVER, 1, 1, 2}, {IT_BAT, 1, 1, 2}, {IT_CROWBAR, 1, 1, 2}, {IT_AMMO_9MM, 6, 14, 5}, {IT_AMMO_38, 4, 10, 3},
                                     {IT_AMMO_SHELL, 3, 8, 2}, {IT_PACK_M, 1, 1, 1}, {IT_BANDAGE, 1, 2, 3}, {IT_MRE, 1, 1, 2}};
static const LootEntry L_CRATE[] = {{IT_CANNED, 1, 2, 4}, {IT_MRE, 1, 1, 3}, {IT_SCRAP, 1, 2, 4}, {IT_PLANKS, 1, 1, 3}, {IT_FUEL, 1, 1, 2}, {IT_AMMO_SHELL, 3, 6, 2}, {IT_TOOLS, 1, 1, 1}};
static const LootEntry L_MED[] = {{IT_BANDAGE, 1, 2, 5}, {IT_PILLS, 1, 2, 4}, {IT_ANTIBIOTICS, 1, 1, 3}, {IT_MEDKIT, 1, 1, 2}, {IT_MEDMACHINE, 1, 1, 1}};
static const LootEntry L_SHELF[] = {{IT_SHOTGUN, 1, 1, 2}, {IT_RIFLE, 1, 1, 2}, {IT_SMG, 1, 1, 2}, {IT_MACHETE, 1, 1, 2}, {IT_AMMO_9MM, 8, 20, 4}, {IT_AMMO_SHELL, 4, 10, 3},
                                    {IT_AMMO_RIFLE, 3, 8, 3}, {IT_PACK_L, 1, 1, 1}, {IT_GENERATOR, 1, 1, 1}, {IT_AMMO_38, 6, 12, 2}};
static const LootEntry L_CAR[] = {{IT_AMMO_SHELL, 3, 8, 3}, {IT_FUEL, 1, 1, 3}, {IT_AMMO_9MM, 6, 12, 3}, {IT_MRE, 1, 1, 2}, {IT_CROWBAR, 1, 1, 1}, {IT_TOOLS, 1, 1, 2}, {IT_SHOTGUN, 1, 1, 1}};
static const LootEntry L_WRECK[] = {{IT_FUEL, 1, 1, 3}, {IT_SCRAP, 1, 2, 4}, {IT_TOOLS, 1, 1, 2}, {IT_CANNED, 1, 1, 2}, {IT_PLANKS, 1, 1, 1}};

void loot_generate(int loc, int ci, int propType, int special, Container& out) {
    out.n = 0; out.cap = 8;
    Rng r(hash32((uint32_t)(loc * 7919 + ci * 104729 + G.loc[loc].gen * 1299709 + 17)));
    const LootEntry* t = L_DESK; int nt = 6, maxItems = 2;
#define T(arr, mx) { t = arr; nt = (int)(sizeof(arr) / sizeof(arr[0])); maxItems = mx; }
    switch (propType) {
    case PR_DESK: T(L_DESK, 2); break; case PR_FILING: T(L_FILING, 2); break; case PR_LOCKER: T(L_LOCKER, 3); break;
    case PR_CRATE: T(L_CRATE, 3); break; case PR_MEDCAB: T(L_MED, 3); break; case PR_SHELF: T(L_SHELF, 3); break;
    case PR_CAR: T(L_CAR, 3); break; case PR_WRECK: T(L_WRECK, 2); break;
    }
#undef T
    if (special == 1 && G.loc[loc].gen == 0) out.push(makeItem(IT_KEYCARD));
    if (special == 2 && G.loc[loc].gen == 0) out.push(makeItem(IT_CELLKEY));
    if (r.chance(0.2f)) return;   // empty
    int n = r.between(1, maxItems), total = 0;
    for (int i = 0; i < nt; i++) total += t[i].weight;
    for (int k = 0; k < n; k++) {
        int v = r.range(total), i = 0;
        while (v >= t[i].weight) { v -= t[i].weight; i++; }
        Item it = makeItem(t[i].item, r.between(t[i].minC, t[i].maxC));
        const ItemDef& d = idef(it.id);
        if (d.wpn.dur) it.dur = (uint16_t)(d.wpn.dur * (40 + r.range(61)) / 100);
        if (d.cat == IC_FIREARM) it.mag = (uint8_t)r.range(d.wpn.mag + 1);
        out.push(it);
    }
}

void loc_tick(float dt) {
    for (int l = 0; l < LOC_COUNT; l++) {
        LocState& L = G.loc[l];
        if (L.cooldown < 0 || dt <= 0) continue;
        if (G.scene == SC_EXPEDITION && G.curLoc == l) continue;   // never restock under the player's feet
        L.cooldown -= dt;
        if (L.cooldown <= 0) {
            L.cooldown = -1; L.gen++;
            for (int c = 0; c < MAX_CONT; c++) { L.generated[c] = false; L.cont[c].n = 0; }
            memset(L.deadMask, 0, sizeof L.deadMask);   // the area repopulates; doors, windows and rescues persist
            journal(JK_INFO, "Scouts report fresh supplies at the %s.", LOCATIONS[l].name);
        }
    }
}
