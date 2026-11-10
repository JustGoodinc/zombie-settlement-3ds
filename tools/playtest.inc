// Map playtest bot (zs_tool --playtest DIR [runs]).
// Plays every launch map through the real game loop (Input -> game_frame): walks to every container and
// searches it, picks up keys, opens doors, rescues trapped survivors and walks out to the extraction.
// Runs are randomised (target order, stick wobble, analog strength, sprint bursts, start offset):
//   runs 0-9   day, no zombies, solo          -> pure navigation: stuck spots and wall scraping
//   runs 10-19 day, zombies, squad of three  -> crowding, companions keeping up through doors
//   runs 20-29 night, zombies, squad, torch  -> containers have to be found in the dark
// Also a static pass per map: 1-tile pinch points, doors blocked by furniture, unreachable loot/floor.
// Writes DIR/playtest.md and DIR/<map>_heat.png (red stuck, orange scraping, violet companions, yellow pinch).

extern int g_highlightProp;
static float len2(float x, float y) { return sqrtf(x * x + y * y); }
static Item* equipped(Survivor& s) { return s.equip >= 0 && s.equip < s.pack.n ? &s.pack.it[s.equip] : nullptr; }
static void ptStep(Input in = Input()) { step(1, in); }   // full frame: the light map (what can be seen and searched) is computed around the camera during render

struct PtMap {
    int runs = 0, contTotal = 0, contFound = 0, extracted = 0, rescues = 0, rescueTotal = 0;
    int rescueDead = 0, rescueMissed = 0;
    int stuck = 0, stuckCrowd = 0, stuckSquad = 0, scrape = 0, follow = 0, unreach = 0, deaths = 0;
    double simTime = 0, worstRun = 0;
    int heatStuck[Map::MAXW * Map::MAXH], heatScrape[Map::MAXW * Map::MAXH], heatFollow[Map::MAXW * Map::MAXH];
    int unreachProp[Map::MAXPROP];
};

static bool ptPass(const Map& m, int i, bool card, bool cell) {
    uint8_t f = m.flags[i];
    if (!(f & F_BLOCK)) return true;
    if (!(f & F_BARRIER)) return false;
    const Barrier& b = m.bar[m.barAt[i]];
    if (b.kind == BK_WINDOW) return false;
    if (b.state == BS_LOCKED) return b.kind == BK_CELLDOOR ? cell : card;
    return true;   // closed door or gate: the bot opens it
}
// BFS distance to any goal tile (4-neighbour), walls and furniture block, doors open
static void ptFlow(const Map& m, const uint8_t* goal, uint16_t* d, bool card, bool cell) {
    static uint16_t q[Map::MAXW * Map::MAXH];
    int n = m.w * m.h, h = 0, t = 0;
    for (int i = 0; i < n; i++) { d[i] = 0xFFFF; if (goal[i] && ptPass(m, i, card, cell)) { d[i] = 0; q[t++] = (uint16_t)i; } }
    while (h < t) {
        int c = q[h++], cx = c % m.w, cy = c / m.w;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& o : D) {
            int nx = cx + o[0], ny = cy + o[1]; if (!m.in(nx, ny)) continue;
            int ni = m.idx(nx, ny); if (d[ni] != 0xFFFF || !ptPass(m, ni, card, cell)) continue;
            d[ni] = (uint16_t)(d[c] + 1); q[t++] = (uint16_t)ni;
        }
    }
}
static bool solidAt(const Map& m, int x, int y) {   // blocks movement for good (not a door)
    if (!m.in(x, y)) return false;   // the map edge is open ground, not a squeeze
    int i = m.idx(x, y);
    return (m.flags[i] & F_BLOCK) && !(m.flags[i] & F_BARRIER);
}

struct PtTarget { int kind, idx, tries; };   // kind 0 container prop, 1 rescue unit, 2 extraction
enum { PT_CONT, PT_RESCUE, PT_EXIT };

static void ptGoal(const Map& m, const PtTarget& tg, uint8_t* goal) {
    memset(goal, 0, m.w * m.h);
    if (tg.kind == PT_CONT) {
        const Prop& p = m.prop[tg.idx];
        for (int y = p.ty - 1; y <= p.ty + p.th; y++) for (int x = p.tx - 1; x <= p.tx + p.tw; x++) {
            bool inside = x >= p.tx && x < p.tx + p.tw && y >= p.ty && y < p.ty + p.th;
            bool corner = (x < p.tx || x >= p.tx + p.tw) && (y < p.ty || y >= p.ty + p.th);
            if (!inside && !corner && m.in(x, y)) goal[m.idx(x, y)] = 1;
        }
    } else if (tg.kind == PT_RESCUE) {
        const Unit& e = G.u[tg.idx]; int tx = (int)(e.x / 16), ty = (int)(e.y / 16);
        for (int k = 0; k < 5; k++) { static const int D[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}; if (m.in(tx + D[k][0], ty + D[k][1])) goal[m.idx(tx + D[k][0], ty + D[k][1])] = 1; }
    } else for (int i = 0; i < m.w * m.h; i++) goal[i] = m.t[i] == T_EXTRACT;
}

static void ptTakeKeys(Survivor& s) {
    if (G.curLoc < 0) return;
    LocState& L = G.loc[G.curLoc];
    for (int c = 0; c < MAX_CONT; c++) if (L.generated[c]) for (int i = L.cont[c].n - 1; i >= 0; i--) {
        int id = L.cont[c].it[i].id;
        if ((id == IT_KEYCARD || id == IT_CELLKEY) && s.pack.countOf(id) == 0) {
            if (s.pack.add(L.cont[c].it[i]) == 0) { for (int j = i; j < L.cont[c].n - 1; j++) L.cont[c].it[j] = L.cont[c].it[j + 1]; L.cont[c].n--; }
        }
    }
}

static void ptRun(int loc, int run, PtMap& R, Rng& rng) {
    const int mapId = LOCATIONS[loc].map;
    bool zombies = run >= 10, squad = run >= 10, night = run >= 20;
    game_newGame(); G.set.tutorialDone = true;
    memset(&G.loc[loc], 0, sizeof G.loc[loc]); G.loc[loc].cooldown = -1;
    G.squad[0] = 0; G.squad[1] = squad ? 1 : -1; G.squad[2] = squad ? 2 : -1;
    for (int k = 0; k < 3; k++) if (G.squad[k] >= 0) G.st.surv[G.squad[k]].onExpedition = true;
    level_leave(); level_enter(mapId, loc); ui_open(PN_HUD);
    G.clock = night ? 22 * 60 + 30 : 10 * 60;
    if (!zombies) G.nz = 0;
    Map& m = G.map;
    Unit& u0 = G.u[G.ctrl];
    u0.x += rng.between(-5, 5); u0.y += rng.between(-3, 3); u0.px = u0.x; u0.py = u0.y;
    if (night) G.flashlight = true;
    // targets: every container + rescue + extraction last, containers shuffled
    static PtTarget tq[Map::MAXPROP * 3 + 8]; int nq = 0;
    for (int i = 0; i < m.nprop; i++) if (m.prop[i].container >= 0) tq[nq++] = PtTarget{PT_CONT, i, 0};
    for (int i = nq - 1; i > 0; i--) { int j = rng.range(i + 1); PtTarget t = tq[i]; tq[i] = tq[j]; tq[j] = t; }
    for (int i = 0; i < G.nu; i++) if (G.u[i].escort) { tq[nq++] = PtTarget{PT_RESCUE, i, 0}; R.rescueTotal++; }
    int contTotal = 0; for (int i = 0; i < nq; i++) contTotal += tq[i].kind == PT_CONT;
    R.contTotal += contTotal;
    tq[nq++] = PtTarget{PT_EXIT, 0, 0};
    static uint8_t goal[Map::MAXW * Map::MAXH]; static uint16_t dist[Map::MAXW * Map::MAXH];
    static bool scraped[Map::MAXW * Map::MAXH]; memset(scraped, 0, sizeof scraped);
    float wobble = 0, wobbleV = 0, strength = 0.75f + rng.range(26) / 100.0f, sprintT = 0;
    bool sprinting = false; double t = 0; const double LIMIT = 1200;
    float followT[MAX_UNITS] = {0};
    int found = 0;
    for (int qi = 0; qi < nq && t < LIMIT; qi++) {
        PtTarget tg = tq[qi];
        if (tg.kind == PT_CONT && G.loc[loc].generated[m.prop[tg.idx].container]) { found++; continue; }   // searched on the way
        if (tg.kind == PT_RESCUE && G.u[tg.idx].order != ORD_HOLD) continue;
        Survivor& s = G.st.surv[G.u[G.ctrl].sv];
        bool card = s.pack.countOf(IT_KEYCARD) > 0, cell = s.pack.countOf(IT_CELLKEY) > 0;
        ptGoal(m, tg, goal); ptFlow(m, goal, dist, card, cell);
        double t0 = t, lastProg = t; int best = 0xFFFF; float slowT = 0, recoverT = 0, recX = 0, recY = 0, doorCd = 0, aCd = 0, fireCd = 0;
        bool done = false, gaveUp = false;
        while (!done && t - t0 < 40 && t < LIMIT) {
            Unit& u = G.u[G.ctrl];
            if (!u.alive) { R.deaths++; t = LIMIT; break; }
            for (int k = 0; k < G.nu; k++) if (!G.u[k].escort && G.u[k].sv >= 0) { Survivor& q = G.st.surv[G.u[k].sv]; q.hp = (float)q.maxHp; q.infection = 0; }
            Item* w = equipped(G.st.surv[u.sv]); if (w && idef(w->id).cat == IC_FIREARM) w->mag = (uint8_t)idef(w->id).wpn.mag;
            ptTakeKeys(s);   /* pick up any key the moment a search turns one up */
            int tx = (int)(u.x / 16), ty = (int)(u.y / 16), ti = m.idx(tx, ty);
            if (dist[ti] == 0xFFFF) {   // doors changed or we were shoved: re-plan
                bool c2 = s.pack.countOf(IT_KEYCARD) > 0, l2 = s.pack.countOf(IT_CELLKEY) > 0;
                ptFlow(m, goal, dist, c2, l2);
                if (dist[ti] == 0xFFFF) { gaveUp = true; break; }
            }
            if (dist[ti] < best) { best = dist[ti]; lastProg = t; }
            Input in;
            // arrived?
            if (tg.kind == PT_CONT) {
                if (G.loc[loc].generated[m.prop[tg.idx].container]) { done = true; break; }
                if (u.searchT > 0) { ptStep(); t += DT; continue; }
                bool other = dist[ti] == 0 && g_highlightProp >= 0 && m.prop[g_highlightProp].container >= 0 && !G.loc[loc].generated[m.prop[g_highlightProp].container];   /* at the spot: search whatever the game offers */
                if ((g_highlightProp == tg.idx || other) && aCd <= 0) { in.down = BTN_A; in.held = BTN_A; aCd = 0.4f; ptStep(in); t += DT; ptTakeKeys(s); if (G.panel != PN_HUD) ui_open(PN_HUD); continue; }
            } else if (tg.kind == PT_RESCUE) {
                const Unit& e = G.u[tg.idx];
                if (e.order != ORD_HOLD) { done = true; break; }
                if (!e.alive) { gaveUp = true; tg.tries = 9; break; }   /* killed before we got there: nothing left to do */
                extern const char* g_prompt;
                if (g_prompt && !strcmp(g_prompt, "Rescue") && aCd <= 0) { /* act on what the game offers, like a player */ in.down = BTN_A; in.held = BTN_A; aCd = 0.4f; ptStep(in); t += DT; continue; }
            } else if (m.t[ti] == T_EXTRACT) { done = true; break; }
            // next tile downhill (8-way, no corner cutting)
            int nx = tx, ny = ty; uint16_t bd = dist[ti];
            static const int D8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
            for (int k = 0; k < 8; k++) {
                int x = tx + D8[k][0], y = ty + D8[k][1]; if (!m.in(x, y)) continue;
                if (k >= 4 && (!ptPass(m, m.idx(tx + D8[k][0], ty), card, cell) || !ptPass(m, m.idx(tx, ty + D8[k][1]), card, cell) ||
                               (m.flags[m.idx(tx + D8[k][0], ty)] & F_BLOCK) || (m.flags[m.idx(tx, ty + D8[k][1])] & F_BLOCK))) continue;
                if (dist[m.idx(x, y)] < bd) { bd = dist[m.idx(x, y)]; nx = x; ny = y; }
            }
            int ni = m.idx(nx, ny);
            float gx = nx * 16 + 8.0f, gy = ny * 16 + 8.0f;
            if (nx == tx && ny == ty) { gx = tx * 16 + 8.0f; gy = ty * 16 + 8.0f; }   // at a goal tile: centre up so the prop is in reach
            // closed door ahead: open it
            if ((m.flags[ni] & F_BARRIER) && (m.flags[ni] & F_BLOCK) && doorCd <= 0 && len2(gx - u.x, gy - u.y) < 22) {
                in.down = BTN_A; in.held = BTN_A; doorCd = 0.5f;
            }
            float dx = gx - u.x, dy = gy - u.y, dl = len2(dx, dy);
            wobbleV += (rng.range(201) - 100) / 4000.0f; wobbleV *= 0.95f; wobble += wobbleV; if (wobble > 0.35f) wobble = 0.35f; if (wobble < -0.35f) wobble = -0.35f;
            if (recoverT > 0) { dx = recX; dy = recY; dl = 1; recoverT -= DT; }
            else if (dl > 0.5f) { float c = cosf(wobble), sn = sinf(wobble); float rx = dx * c - dy * sn, ry = dx * sn + dy * c; dx = rx / dl; dy = ry / dl; }
            float mag = dl < 6 ? 0.45f : strength;
            in.ax = dx * mag; in.ay = dy * mag;
            // sprint in bursts (exercises running out of stamina while B is held)
            sprintT -= DT; if (sprintT <= 0) { sprinting = !sprinting && rng.chance(0.35f); sprintT = 1.0f + rng.range(40) / 10.0f; }
            if (sprinting) in.held |= BTN_B;
            // defend: shoot the nearest close zombie
            fireCd -= DT;
            if (zombies && fireCd <= 0) {
                int zi = -1; float zd = 64;
                for (int k = 0; k < G.nz; k++) if (G.z[k].state != ZS_DEAD && G.z[k].state != ZS_LYING) { float d = len2(G.z[k].x - u.x, G.z[k].y - u.y); if (d < zd && map_los(m, u.x, u.y, G.z[k].x, G.z[k].y)) { zd = d; zi = k; } }
                if (zi >= 0) { in.ax = (G.z[zi].x - u.x) / zd; in.ay = (G.z[zi].y - u.y) / zd; in.down |= BTN_Y; in.held |= BTN_Y; in.held &= ~BTN_B; fireCd = 0.35f; }
            }
            if (getenv("PT_TRACE") && atoi(getenv("PT_TRACE")) == loc && tg.kind == PT_RESCUE && ((int)(t * 30) % 15) == 0) { extern const char* g_prompt; const Unit& e = G.u[tg.idx]; printf("t%.1f bot(%.0f,%.0f) tile(%d,%d) d=%d R(%.0f,%.0f) ord %d alive %d prompt %s in(%.2f,%.2f) A%d\n", t, u.x, u.y, tx, ty, dist[ti], e.x, e.y, e.order, (int)e.alive, g_prompt ? g_prompt : "-", in.ax, in.ay, (in.down & BTN_A) ? 1 : 0); }
            if (getenv("PT_TRACE")) { static bool was = true; bool now = false; for (int k = 0; k < G.nu; k++) if (G.u[k].escort) now = G.u[k].alive; if (was && !now) { int bi = m.barAt[m.idx(43, 9)]; printf("ESCORT DIED t%.1f celldoor state %d hp %d nz %d\n", t, bi >= 0 ? m.bar[bi].state : -1, bi >= 0 ? m.bar[bi].hp : -1, G.nz); } was = now; }
            float ox = u.x, oy = u.y;
            ptStep(in); t += DT; doorCd -= DT; aCd -= DT;
            if (G.panel != PN_HUD) ui_open(PN_HUD);

            // wall scraping: moving at well under the commanded pace for a while
            float moved = len2(u.x - ox, u.y - oy), want = 54.0f * mag * DT;
            if (recoverT <= 0 && dl > 6 && moved < want * 0.4f && u.searchT <= 0 && u.dodgeT <= 0) slowT += DT; else slowT = 0;
            int ci = m.idx((int)(u.x / 16), (int)(u.y / 16));
            bool crowd = false; for (int k = 0; k < G.nz; k++) if (G.z[k].state != ZS_DEAD && len2(G.z[k].x - u.x, G.z[k].y - u.y) < 40) crowd = true;
            bool squadBlock = false; for (int k = 0; k < G.nu; k++) if (k != G.ctrl && G.u[k].alive && len2(G.u[k].x - u.x, G.u[k].y - u.y) < 14) squadBlock = true;
            if (squadBlock && !crowd && t - lastProg > 3.0) R.stuckSquad++;
            crowd = crowd || squadBlock;
            if (slowT > 0.4f && !scraped[ci] && !crowd) { scraped[ci] = true; R.scrape++; R.heatScrape[ci]++; }
            if (t - lastProg > 3.0) {   // no progress for 3 s
                if (crowd) R.stuckCrowd++; else { R.stuck++; R.heatStuck[ci]++; }
                lastProg = t; recoverT = 0.6f; float a = rng.range(628) / 100.0f; recX = cosf(a); recY = sinf(a);
            }
            // companions left behind
            for (int k = 0; k < G.nu; k++) {
                const Unit& c = G.u[k];
                if (k == G.ctrl || !c.alive || (c.escort && c.order == ORD_HOLD)) { followT[k] = 0; continue; }
                if (len2(c.x - u.x, c.y - u.y) > 150) followT[k] += DT; else followT[k] = 0;
                if (followT[k] > 6) { R.follow++; R.heatFollow[m.idx((int)(c.x / 16), (int)(c.y / 16))]++; followT[k] = 0; }
            }
        }
        if (done) { if (tg.kind == PT_CONT) found++; if (tg.kind == PT_RESCUE) R.rescues++; if (tg.kind == PT_EXIT) R.extracted++; }
        else if (gaveUp || t - t0 >= 40) {
            if (tg.tries < 2 && tg.kind != PT_EXIT) { tg.tries++; tq[nq] = tq[nq - 1]; tq[nq - 1] = tg; nq++; }   // retry before the exit (a key may turn up)
            else if (tg.kind == PT_CONT) { R.unreach++; R.unreachProp[tg.idx]++; }
        }
    }
    for (int k = 0; k < G.nu; k++) if (G.u[k].escort && G.u[k].order == ORD_HOLD) { if (G.u[k].alive) R.rescueMissed++; else R.rescueDead++; }
    R.contFound += found; R.runs++; R.simTime += t; if (t > R.worstRun) R.worstRun = t;
}

static void ptStatic(FILE* f, const Map& m, PtMap& R, uint8_t* pinch) {
    int pinches = 0, doorsBlocked = 0, noAccess = 0, decor = 0, cont = 0, floor = 0, unreachFloor = 0;
    memset(pinch, 0, m.w * m.h);
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
        int i = m.idx(x, y);
        if (m.flags[i] & F_BLOCK) continue;
        floor++;
        if ((solidAt(m, x - 1, y) && solidAt(m, x + 1, y)) || (solidAt(m, x, y - 1) && solidAt(m, x, y + 1))) { pinch[i] = 1; pinches++; }
    }
    fprintf(f, "- Static: ");
    for (int b = 0; b < m.nbar; b++) {
        const Barrier& br = m.bar[b]; if (br.kind == BK_WINDOW) continue;
        bool ns = solidAt(m, br.x - 1, br.y) || solidAt(m, br.x + 1, br.y);   // wall runs east-west: passage is north-south
        bool blocked = ns ? (solidAt(m, br.x, br.y - 1) || solidAt(m, br.x, br.y + 1)) : (solidAt(m, br.x - 1, br.y) || solidAt(m, br.x + 1, br.y));
        if (blocked) doorsBlocked++;
    }
    for (int p = 0; p < m.nprop; p++) {
        const Prop& pr = m.prop[p];
        if (pr.container < 0) { if (pr.type != PR_PLOT) decor++; continue; }
        cont++;
        bool ok = false;
        for (int y = pr.ty - 1; y <= pr.ty + pr.th; y++) for (int x = pr.tx - 1; x <= pr.tx + pr.tw; x++) {
            bool inside = x >= pr.tx && x < pr.tx + pr.tw && y >= pr.ty && y < pr.ty + pr.th, corner = (x < pr.tx || x >= pr.tx + pr.tw) && (y < pr.ty || y >= pr.ty + pr.th);
            if (!inside && !corner && m.in(x, y) && !(m.flags[m.idx(x, y)] & F_BLOCK)) ok = true;
        }
        if (!ok) noAccess++;
    }
    { static uint8_t g[Map::MAXW * Map::MAXH]; static uint16_t d[Map::MAXW * Map::MAXH]; memset(g, 0, sizeof g); g[m.idx(m.spawnX, m.spawnY)] = 1; ptFlow(m, g, d, true, true);
      for (int i = 0; i < m.w * m.h; i++) if (!(m.flags[i] & F_BLOCK) && d[i] == 0xFFFF) unreachFloor++; }
    fprintf(f, "%d floor tiles, %d containers, %d decor props (%.1f props per 100 floor tiles); %d one-tile pinch tiles; %d doors blocked by furniture; %d containers with no open side; %d floor tiles unreachable from the start\n",
            floor, cont, decor, 100.0 * (cont + decor) / (floor ? floor : 1), pinches, doorsBlocked, noAccess, unreachFloor);
    if (pinches) {
        fprintf(f, "  - pinch tiles:");
        int shown = 0; for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) if (pinch[m.idx(x, y)] && shown++ < 40) fprintf(f, " (%d,%d)", x, y);
        fprintf(f, "%s\n", pinches > 40 ? " ..." : "");
    }
    for (int b = 0; b < m.nbar; b++) {
        const Barrier& br = m.bar[b]; if (br.kind == BK_WINDOW) continue;
        bool ns = solidAt(m, br.x - 1, br.y) || solidAt(m, br.x + 1, br.y);
        bool blocked = ns ? (solidAt(m, br.x, br.y - 1) || solidAt(m, br.x, br.y + 1)) : (solidAt(m, br.x - 1, br.y) || solidAt(m, br.x + 1, br.y));
        if (blocked) fprintf(f, "  - door at (%d,%d) has furniture or wall in its path\n", br.x, br.y);
    }
}

static void ptHeat(const char* path, const Map& m, const PtMap& R, const uint8_t* pinch) {
    const int S = 6, W = m.w * S, H = m.h * S;
    px* img = (px*)malloc(W * H * sizeof(px));
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
        int i = m.idx(x, y); uint8_t t = m.t[i];
        px c = t == T_WALL ? rgb(150, 142, 130) : t == T_DOOR ? rgb(170, 120, 60) : t == T_WATER ? rgb(40, 70, 90) : t == T_GRASS ? rgb(56, 70, 40)
             : (t == T_ASPHALT || t == T_LINE) ? rgb(54, 54, 58) : t == T_RAIL ? rgb(90, 84, 76) : t == T_EXTRACT ? pal::lime : t == T_FENCE ? rgb(120, 110, 80) : rgb(104, 100, 92);
        if (m.propAt[i] >= 0 && m.prop[m.propAt[i]].solid) c = m.prop[m.propAt[i]].container >= 0 ? rgb(60, 120, 170) : rgb(40, 44, 50);
        for (int j = 0; j < S; j++) for (int k = 0; k < S; k++) {
            px v = c;
            if (pinch[i] && (j == 0 || k == 0 || j == S - 1 || k == S - 1)) v = rgb(240, 220, 60);
            img[(y * S + j) * W + x * S + k] = v;
        }
        auto dot = [&](int n, px col, int ox, int oy) { if (!n) return; int r = n > 3 ? 2 : 1; for (int j = -r; j <= r; j++) for (int k = -r; k <= r; k++) { int X = x * S + ox + k, Y = y * S + oy + j; if (X >= 0 && Y >= 0 && X < W && Y < H) img[Y * W + X] = col; } };
        dot(R.heatScrape[i], rgb(250, 150, 40), 2, 2); dot(R.heatFollow[i], rgb(190, 90, 240), 4, 2); dot(R.heatStuck[i], rgb(250, 40, 40), 3, 4);
    }
    int sx = m.spawnX * S, sy = m.spawnY * S; for (int j = 0; j < S; j++) for (int k = 0; k < S; k++) img[(sy + j) * W + sx + k] = rgb(255, 255, 255);
    gfx_writePNG(path, img, W, H); free(img);
}

static void playtest(const char* dir, int runs) {
    if (getenv("PT_TRACE")) { static PtMap R1; memset(&R1, 0, sizeof R1); Rng rr(1); ptRun(atoi(getenv("PT_TRACE")), getenv("PT_RUN") ? atoi(getenv("PT_RUN")) : 0, R1, rr); printf("rescues %d stuck %d\n", R1.rescues, R1.stuck); return; }
    char p[512]; snprintf(p, sizeof p, "%s/playtest.md", dir);
    FILE* f = fopen(p, "w"); if (!f) { printf("cannot write %s\n", p); return; }
    fprintf(f, "# Map playtest (%d runs per map)\n\nBot drives the real game input. Runs 0-9: day, no zombies, solo. 10-19: day, zombies, squad of 3. 20-29: night, zombies, squad, flashlight.\n"
               "Stuck = no progress for 3 s with no zombie/companion pressed against the player. Scrape = moving under 40%% of the commanded pace for 0.4 s (counted once per tile per run). "
               "Companion lag = a follower more than 150 px behind for 6 s.\n\n", runs);
    static PtMap R; int totalStuck = 0;
    for (int loc = 0; loc < LOC_COUNT; loc++) {
        if (getenv("PT_ONLY") && atoi(getenv("PT_ONLY")) != loc) continue;   // iterate on one map
        memset(&R, 0, sizeof R);
        Rng rng(hash32(loc * 7919 + 13));
        for (int r = 0; r < runs; r++) ptRun(loc, r, R, rng);
        fprintf(f, "## %s\n\n", LOCATIONS[loc].name);
        map_load(G.map, LOCATIONS[loc].map); map_refresh(G.map);
        static uint8_t pinch[Map::MAXW * Map::MAXH];
        ptStatic(f, G.map, R, pinch);
        fprintf(f, "- Runs: %d. Containers searched %d / %d (%.1f%%). Rescues %d / %d (failed: %d killed before reached, %d not reached). Reached extraction %d / %d. Squad deaths %d.\n",
                R.runs, R.contFound, R.contTotal, 100.0 * R.contFound / (R.contTotal ? R.contTotal : 1), R.rescues, R.rescueTotal, R.rescueDead, R.rescueMissed, R.extracted, R.runs, R.deaths);
        fprintf(f, "- Average full clear %.0f s of game time (slowest %.0f s).\n", R.simTime / (R.runs ? R.runs : 1), R.worstRun);
        fprintf(f, "- Stuck (map) %d, stuck while crowded %d (by own squad only: %d), wall-scrape tiles %d, companion lag %d, containers given up on %d.\n", R.stuck, R.stuckCrowd, R.stuckSquad, R.scrape, R.follow, R.unreach);
        auto top = [&](const char* label, const int* heat) {
            int idx[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
            for (int i = 0; i < G.map.w * G.map.h; i++) if (heat[i]) { for (int k = 0; k < 8; k++) if (idx[k] < 0 || heat[i] > heat[idx[k]]) { for (int j = 7; j > k; j--) idx[j] = idx[j - 1]; idx[k] = i; break; } }
            if (idx[0] < 0) return;
            fprintf(f, "  - %s hotspots:", label);
            for (int k = 0; k < 8 && idx[k] >= 0; k++) fprintf(f, " (%d,%d)x%d", idx[k] % G.map.w, idx[k] / G.map.w, heat[idx[k]]);
            fprintf(f, "\n");
        };
        top("stuck", R.heatStuck); top("scrape", R.heatScrape); top("companion lag", R.heatFollow);
        for (int i = 0; i < G.map.nprop; i++) if (R.unreachProp[i]) fprintf(f, "  - container at (%d,%d) given up %d times\n", G.map.prop[i].tx, G.map.prop[i].ty, R.unreachProp[i]);
        fprintf(f, "\n");
        char hp[512]; snprintf(hp, sizeof hp, "%s/%d_%s_heat.png", dir, loc, map_name(LOCATIONS[loc].map)); for (char* c = hp + strlen(dir) + 1; *c; c++) if (*c == ' ' || *c == '&') *c = '_';
        ptHeat(hp, G.map, R, pinch);
        printf("%-15s cont %d/%d  rescue %d/%d  exit %d/%d  stuck %d (+%d crowd, %d squad)  scrape %d  lag %d  gaveup %d  avg %.0fs\n", LOCATIONS[loc].name, R.contFound, R.contTotal,
               R.rescues, R.rescueTotal, R.extracted, R.runs, R.stuck, R.stuckCrowd, R.stuckSquad, R.scrape, R.follow, R.unreach, R.simTime / (R.runs ? R.runs : 1));
        totalStuck += R.stuck;
    }
    fclose(f);
    printf("report: %s\n", p);
}

// map review (zs_tool --mapshots DIR): a stitched whole-map overview at 100% zoom by day, plus in-game
// screenshots (top + bottom screen, default zoom) at the arrival point, the middle of the map, the far end, and at night
static void mapShots(const char* dir) {
    for (int loc = 0; loc < LOC_COUNT; loc++) {
        game_newGame(); G.set.tutorialDone = true;
        G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = -1; G.st.surv[0].onExpedition = G.st.surv[1].onExpedition = true;
        level_leave(); level_enter(LOCATIONS[loc].map, loc); ui_open(PN_HUD);
        Map& m = G.map; G.clock = 11 * 60;
        char name[64], p[512]; snprintf(name, sizeof name, "%d_%s", loc, LOCATIONS[loc].name); for (char* c = name; *c; c++) if (*c == ' ') *c = '_';
        uint8_t zoom = G.set.zoom; G.set.zoom = 100; step(2);
        {   int W = m.w * 16, H = m.h * 16; px* img = (px*)malloc(W * H * sizeof(px));
            static px vb[TOP_W * TOP_H];
            for (int cy = 0; cy < H; cy += TOP_H) for (int cx = 0; cx < W; cx += TOP_W) {
                int ox = cx + TOP_W > W ? W - TOP_W : cx, oy = cy + TOP_H > H ? H - TOP_H : cy;
                G.camX = (float)ox; G.camY = (float)oy; Canvas v; v.init(vb, TOP_W, TOP_H); render_world(v, 1);
                for (int y = 0; y < TOP_H; y++) memcpy(img + (oy + y) * W + ox, vb + y * TOP_W, TOP_W * sizeof(px));
            }
            snprintf(p, sizeof p, "%s/%s_overview.png", dir, name); gfx_writePNG(p, img, W, H); free(img); printf("  wrote %s\n", p); }
        G.set.zoom = zoom;
        // points of interest: arrival, map centre, the container furthest from the start
        int fx = m.spawnX, fy = m.spawnY, best = -1;
        for (int i = 0; i < m.nprop; i++) if (m.prop[i].container >= 0) { int d = abs(m.prop[i].tx - m.spawnX) + abs(m.prop[i].ty - m.spawnY); if (d > best) { best = d; fx = m.prop[i].tx; fy = m.prop[i].ty + m.prop[i].th; } }
        auto openNear = [&](int& tx, int& ty) { for (int r = 0; r < 6; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (m.in(tx + dx, ty + dy) && !(m.flags[m.idx(tx + dx, ty + dy)] & F_BLOCK)) { tx += dx; ty += dy; return; } };
        struct { const char* tag; int tx, ty; float clock; } pts[4] = {{"arrival", m.spawnX, m.spawnY - 3, 11 * 60.0f}, {"middle", m.w / 2, m.h / 2, 11 * 60.0f}, {"far", fx, fy, 15 * 60.0f}, {"night", m.w / 2, m.h / 2, 22 * 60 + 30.0f}};
        for (auto& pt : pts) {
            int tx = pt.tx, ty = pt.ty; openNear(tx, ty);
            for (int k = 0; k < G.nu; k++) if (!G.u[k].escort) { G.u[k].x = G.u[k].px = tx * 16 + 8.0f + k * 12; G.u[k].y = G.u[k].py = ty * 16 + 8.0f; }
            G.clock = pt.clock; G.flashlight = pt.clock > 20 * 60;
            G.camX = G.u[0].x - view_w() / 2; G.camY = G.u[0].y - view_h() / 2; step(20);
            snprintf(p, sizeof p, "%s_%s", name, pt.tag); shot(dir, p);
        }
    }
}

// settlement review (zs_tool --settlement DIR): every facility built, stitched whole-compound overviews by day and dusk
static void settlementShots(const char* dir) {
    game_newGame(); G.set.tutorialDone = true;
    const int LV[BLD_TYPES] = {2, 3, 2, 2, 3, 3};
    for (int b = 0; b < BLD_TYPES; b++) G.st.bLevel[b] = (uint8_t)LV[b];
    G.st.generator = true;
    level_enter(MAP_SETTLEMENT, -1); ui_open(PN_HUD);
    Map& m = G.map; uint8_t zoom = G.set.zoom; G.set.zoom = 100;
    const struct { const char* n; float clock; } T[2] = {{"settlement_day", 11 * 60.0f}, {"settlement_dusk", 18 * 60 + 40.0f}};
    for (auto& t : T) {
        G.clock = t.clock; step(300);
        int W = m.w * 16, H = m.h * 16; px* img = (px*)malloc(W * H * sizeof(px)); static px vb[TOP_W * TOP_H];
        float cx0 = G.camX, cy0 = G.camY;
        for (int cy = 0; cy < H; cy += TOP_H) for (int cx = 0; cx < W; cx += TOP_W) {
            int ox = cx + TOP_W > W ? W - TOP_W : cx, oy = cy + TOP_H > H ? H - TOP_H : cy;
            G.camX = (float)ox; G.camY = (float)oy; Canvas v; v.init(vb, TOP_W, TOP_H); render_world(v, 1);
            for (int y = 0; y < TOP_H; y++) memcpy(img + (oy + y) * W + ox, vb + y * TOP_W, TOP_W * sizeof(px));
        }
        G.camX = cx0; G.camY = cy0;
        char p[512]; snprintf(p, sizeof p, "%s/%s.png", dir, t.n); gfx_writePNG(p, img, W, H); free(img); printf("  wrote %s\n", p);
    }
    G.set.zoom = zoom;
}

// combat showcase (zs_tool --combat DIR): a short scripted firefight in the police lobby by day and in the car park at
// night; writes the top screen at the first muzzle flash, mid-fight and after it (to judge flash, blood, casings, light)
static void combatShots(const char* dir) {
    const struct { const char* n; int tx, ty; float clock; } SC[2] = {{"combat_day", 22, 29, 11 * 60.0f}, {"combat_night", 24, 36, 22 * 60 + 30.0f}};
    for (auto& sc : SC) {
        game_newGame(); G.set.tutorialDone = true;
        G.squad[0] = 0; G.squad[1] = 1; G.squad[2] = -1; G.st.surv[0].onExpedition = G.st.surv[1].onExpedition = true;
        level_leave(); level_enter(MAP_POLICE, 0); ui_open(PN_HUD);
        G.clock = sc.clock; G.flashlight = sc.clock > 20 * 60; G.nz = 0;
        for (int k = 0; k < G.nu; k++) { G.u[k].x = G.u[k].px = sc.tx * 16 + 8.0f - k * 14; G.u[k].y = G.u[k].py = sc.ty * 16 + 8.0f + k * 6; }
        Unit& u = G.u[0]; u.fx = 1; u.fy = 0;
        for (int k = 0; k < 6; k++) {
            Zombie& z = G.z[G.nz++]; memset(&z, 0, sizeof z);
            z.x = z.px = u.x + 70 + (k % 3) * 26; z.y = z.py = u.y - 26 + (k / 3) * 44 + (k % 3) * 6; z.hp = 100; z.state = ZS_CHASE; z.spawnIdx = -1; z.speed = 0.5f; z.variant = (uint8_t)(k % ZOMB_VARIANTS); z.hx = -1;
            z.anim = Animator(); z.anim.play(AnimState::ZWalk, true);
        }
        G.camX = u.x - view_w() / 2 + 40; G.camY = u.y - view_h() / 2; step(20);
        bool gotFlash = false; char p[512];
        for (int f = 0; f < 330; f++) {
            Survivor& s = G.st.surv[u.sv]; if (s.equip >= 0) { s.pack.it[s.equip].mag = 12; s.pack.it[s.equip].dur = 300; } s.hp = (float)s.maxHp; s.infection = 0;
            int zi = -1; float zd = 1e9f;
            for (int k = 0; k < G.nz; k++) if (G.z[k].state != ZS_DEAD) { float d = len2(G.z[k].x - u.x, G.z[k].y - u.y); if (d < zd) { zd = d; zi = k; } }
            Input in; in.held = BTN_L;
            if (zi >= 0) { u.fx = (G.z[zi].x - u.x) / zd; u.fy = (G.z[zi].y - 4 - u.y) / zd; if ((f % 7) == 0) { in.down |= BTN_Y; in.held |= BTN_Y; } }
            step(1, in);
            if (!gotFlash && u.muzzleT > 0.03f) { gotFlash = true; snprintf(p, sizeof p, "%s/%s_flash.png", dir, sc.n); gfx_writePNG(p, topB, TOP_W, TOP_H); }
            if (f == 70) { snprintf(p, sizeof p, "%s/%s_mid.png", dir, sc.n); gfx_writePNG(p, topB, TOP_W, TOP_H); }
        }
        snprintf(p, sizeof p, "%s/%s_after.png", dir, sc.n); gfx_writePNG(p, topB, TOP_W, TOP_H); printf("  wrote %s\n", p);
    }
}

// zs_tool --bench-bake: how long drawing the static level layer takes per map (it happens once, on entering a level)
static void bakeBench() {
    game_newGame(); G.set.tutorialDone = true; G.squad[0] = 0; G.squad[1] = G.squad[2] = -1;
    for (int l = 0; l <= LOC_COUNT; l++) {
        int map = l < LOC_COUNT ? LOCATIONS[l].map : MAP_SETTLEMENT;
        map_load(G.map, map); map_refresh(G.map);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int k = 0; k < 20; k++) bake_level();
        double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / 20;
        printf("bake %-16s %dx%d tiles  %.2f ms\n", map_name(map), G.map.w, G.map.h, ms);
    }
}
