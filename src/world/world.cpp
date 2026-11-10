#include "world.h"
#include <string.h>
#include <math.h>

extern const char* const* MAP_DATA[];
extern const int MAP_HEIGHT[];

const char* map_name(int which) {
    static const char* N[MAP_COUNT] = {"Police Station", "Settlement", "Hospital", "Supermarket", "Neighborhood", "Rail & Ship Yard"};
    return which >= 0 && which < MAP_COUNT ? N[which] : "";
}

static bool isFloor(uint8_t t) { return t == T_GRASS || t == T_ASPHALT || t == T_LINE || t == T_DIRT || t == T_TILE || t == T_WOOD || t == T_CARPET || t == T_CONCRETE || t == T_GARDEN || t == T_EXTRACT || t == T_RAIL; }

static int tileOf(char c) {
    switch (c) {
    case '"': return T_GRASS; case '=': return T_ASPHALT; case '^': return T_LINE; case ':': return T_DIRT;
    case '.': return T_TILE; case ',': return T_WOOD; case '~': return T_CARPET; case '_': return T_CONCRETE;
    case '#': return T_WALL; case '%': return T_BARS; case 'F': return T_FENCE; case 'E': return T_EXTRACT;
    case 'g': return T_GARDEN; case 'X': return T_PALISADE; case 'H': return T_RAIL; case '$': return T_WATER;
    case 'W': case 'D': case 'O': case 'K': case 'C': case 'G': return T_DOOR;
    default: return -1;  // entity: floor resolved from neighbours
    }
}

static void addProp(Map& m, int type, int x, int y, int tw, int th, bool container, bool solid, int special = 0) {
    if (m.nprop >= Map::MAXPROP || (container && m.ncontainers >= MAX_MAP_CONT)) { m.overflow++; return; }
    Prop& p = m.prop[m.nprop];
    p.type = (uint8_t)type; p.tx = (uint8_t)x; p.ty = (uint8_t)y; p.tw = (uint8_t)tw; p.th = (uint8_t)th;
    p.container = container ? (int8_t)m.ncontainers++ : (int8_t)-1;
    p.special = (uint8_t)special; p.solid = solid;
    for (int j = 0; j < th; j++) for (int i = 0; i < tw; i++) if (m.in(x + i, y + j)) m.propAt[m.idx(x + i, y + j)] = (int16_t)m.nprop;
    m.nprop++;
}

void map_load(Map& m, int which) {
    const char* const* rows = MAP_DATA[which];
    int H = MAP_HEIGHT[which], W = 0;
    for (int j = 0; j < H; j++) { int l = (int)strlen(rows[j]); if (l > W) W = l; }
    if (W > Map::MAXW) W = Map::MAXW;
    if (H > Map::MAXH) H = Map::MAXH;
    m.w = W; m.h = H; m.id = which; m.nbar = m.nprop = m.nspawn = m.ncontainers = m.overflow = 0;
    memset(m.barAt, -1, sizeof(m.barAt));
    memset(m.propAt, -1, sizeof(m.propAt));
    uint8_t defFloor = which == MAP_SETTLEMENT ? T_DIRT : (which == MAP_HOMES || which == MAP_YARD) ? T_GRASS : T_TILE;
    auto ch = [&](int x, int y) -> char { if (y < 0 || y >= H) return '"'; int l = (int)strlen(rows[y]); return x >= 0 && x < l ? rows[y][x] : '"'; };
    // pass 1: tiles
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int t = tileOf(ch(x, y));
        m.t[m.idx(x, y)] = t < 0 ? 255 : (uint8_t)t;
    }
    // pass 2: resolve entity floors from neighbours
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        uint8_t& t = m.t[m.idx(x, y)];
        if (t != 255) continue;
        static const int N[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-2, 0}, {2, 0}, {0, -2}, {0, 2}};
        t = defFloor;
        for (auto& n : N) { int nt = tileOf(ch(x + n[0], y + n[1])); if (nt >= 0 && isFloor((uint8_t)nt)) { t = (uint8_t)nt; break; } }
    }
    // pass 3: barriers, props, spawns
    int sumEx = 0, nEx = 0, sumEy = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        char c = ch(x, y);
        auto bar = [&](int kind, int state, int hp) {
            if (m.nbar >= Map::MAXBAR) { m.overflow++; return; }
            Barrier& b = m.bar[m.nbar];
            b.x = (uint8_t)x; b.y = (uint8_t)y; b.kind = (uint8_t)kind; b.state = (uint8_t)state; b.hp = b.maxHp = (int16_t)hp;
            b.anim = state == BS_OPEN ? 1.0f : 0.0f; b.shake = 0;
            m.barAt[m.idx(x, y)] = (int8_t)m.nbar++;
        };
        auto spawn = [&](char k) { if (m.nspawn < Map::MAXSPAWN) m.spawn[m.nspawn++] = Spawn{k, (uint8_t)x, (uint8_t)y}; else m.overflow++; };
        switch (c) {
        case 'W': bar(BK_WINDOW, BS_CLOSED, 30); break;
        case 'D': bar(BK_DOOR, BS_CLOSED, 80); break;
        case 'O': bar(BK_DOOR, BS_OPEN, 80); break;
        case 'K': bar(BK_KEYDOOR, BS_LOCKED, 160); break;
        case 'C': bar(BK_CELLDOOR, BS_LOCKED, 200); break;
        case 'G': bar(BK_GATE, BS_CLOSED, 250); break;
        case 's': addProp(m, PR_SHELF, x, y, 1, 1, true, true); break;
        case 'j': addProp(m, PR_SHELF, x, y, 1, 1, false, true); break;   // picked-over shelf (decor)
        case 'x': addProp(m, PR_CRATE, x, y, 1, 1, true, true); break;
        case 'd': addProp(m, PR_DESK, x, y, 1, 1, true, true); break;
        case 'Q': addProp(m, PR_DESK, x, y, 1, 1, true, true, 1); break;
        case 'U': addProp(m, PR_DESK, x, y, 1, 1, true, true, 2); break;
        case 'f': addProp(m, PR_FILING, x, y, 1, 1, true, true); break;
        case 'l': addProp(m, PR_LOCKER, x, y, 1, 1, true, true); break;
        case 'm': addProp(m, PR_MEDCAB, x, y, 1, 1, true, true); break;
        case 'v': addProp(m, PR_CAR, x, y, 2, 1, true, true); break;
        case 'u': addProp(m, PR_WRECK, x, y, 2, 1, true, true); break;
        case 'T': addProp(m, PR_TREE, x, y, 1, 1, false, true); break;
        case 'b': addProp(m, PR_BENCH, x, y, 1, 1, false, true); break;
        case 'k': addProp(m, PR_COT, x, y, 1, 1, false, true); break;
        case 'c': addProp(m, PR_CHAIR, x, y, 1, 1, false, false); break;
        case 'p': addProp(m, PR_PLANT, x, y, 1, 1, false, true); break;
        case 'w': addProp(m, PR_COOLER, x, y, 1, 1, false, true); break;
        case 'y': addProp(m, PR_DECOR_CRATE, x, y, 1, 1, false, true); break;
        case 'A': addProp(m, PR_TENT, x, y, 2, 1, false, true); break;
        case 'o': addProp(m, PR_BARREL, x, y, 1, 1, false, true); break;
        case 'S': addProp(m, PR_SANDBAG, x, y, 1, 1, false, true); break;
        case 'Y': addProp(m, PR_LOGS, x, y, 1, 1, false, true); break;
        case '!': addProp(m, PR_TORCH, x, y, 1, 1, false, true); break;
        case 'i': addProp(m, PR_CONE, x, y, 1, 1, false, false); break;
        case 'L': addProp(m, PR_LAMP, x, y, 1, 1, false, true); break;
        case '*': addProp(m, PR_FIRE, x, y, 1, 1, false, true); break;
        case 'r': addProp(m, PR_RADIO, x, y, 1, 1, false, true); break;
        case '&': addProp(m, PR_WORKBENCH, x, y, 1, 1, false, true); break;
        case 'n': addProp(m, PR_GENERATOR, x, y, 1, 1, false, false); break;
        case 'N': addProp(m, PR_SHIPBOX, x, y, 4, 2, true, true); break;   // paint colour picked from position in the renderer
        case 'B': addProp(m, PR_BOXCAR, x, y, 5, 2, true, true); break;
        case 'h': addProp(m, PR_BED, x, y, 1, 2, false, true); break;
        case 'a': addProp(m, PR_SOFA, x, y, 2, 1, false, true); break;
        case 'e': addProp(m, PR_FRIDGE, x, y, 1, 1, true, true); break;
        case '1': case '2': case '3': case '4': case '5': case '6':
            addProp(m, PR_PLOT, x, y, 3, 3, false, false, c - '1'); break;
        case 'Z': case 'z': case 'q': case 'R': case 'V': spawn(c); break;
        case 'P': m.spawnX = x; m.spawnY = y; break;
        }
        if (c == 'E' || c == 'G') { sumEx += x; sumEy += y; nEx++; }
    }
    if (nEx) { m.exitX = sumEx / nEx; m.exitY = sumEy / nEx; }
    map_refresh(m);
}

// indoor = floor that can't be reached from the map edge without passing a wall, door or window
// (doors count as walls whatever their state, so opening one never turns a room into outdoors)
static void markOutside(const Map& m, uint8_t* out) {
    static uint16_t q[Map::MAXW * Map::MAXH];
    int n = m.w * m.h, head = 0, tail = 0;
    memset(out, 0, n);
    auto stops = [&](int i) { uint8_t t = m.t[i]; return t == T_WALL || t == T_DOOR || t == T_BARS || t == T_FENCE || t == T_PALISADE; };
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++)
        if ((x == 0 || y == 0 || x == m.w - 1 || y == m.h - 1) && !stops(m.idx(x, y))) { out[m.idx(x, y)] = 1; q[tail++] = (uint16_t)m.idx(x, y); }
    while (head < tail) {
        int c = q[head++], cx = c % m.w, cy = c / m.w;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& d : D) {
            int nx = cx + d[0], ny = cy + d[1];
            if (!m.in(nx, ny)) continue;
            int ni = m.idx(nx, ny);
            if (out[ni] || stops(ni)) continue;
            out[ni] = 1; q[tail++] = (uint16_t)ni;
        }
    }
}

void map_refresh(Map& m) {
    static uint8_t outside[Map::MAXW * Map::MAXH];
    markOutside(m, outside);
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
        int i = m.idx(x, y); uint8_t t = m.t[i], f = 0;
        if (t == T_WALL || t == T_PALISADE) f = F_BLOCK | F_OPAQUE;
        else if (t == T_BARS || t == T_FENCE || t == T_WATER) f = F_BLOCK;
        if (m.id != MAP_SETTLEMENT && isFloor(t) && !outside[i]) f |= F_INDOOR;
        if (m.barAt[i] >= 0) {
            const Barrier& b = m.bar[m.barAt[i]];
            f |= F_BARRIER;
            if (b.state != BS_OPEN && b.state != BS_BROKEN) { f |= F_BLOCK; if (b.kind != BK_WINDOW && b.kind != BK_CELLDOOR) f |= F_OPAQUE; }
            if (b.kind == BK_WINDOW && b.state == BS_BROKEN) f &= ~F_BLOCK;
            if (b.kind != BK_GATE) f |= F_INDOOR;
        }
        if (m.propAt[i] >= 0) {
            const Prop& p = m.prop[m.propAt[i]];
            if (p.solid) f |= F_BLOCK;
            if (p.type == PR_PLOT) f |= F_BLOCK * 0;  // plots: solidity applied by settlement when built
        }
        m.flags[i] = f;
    }
}

bool map_blocked(const Map& m, int tx, int ty) { return !m.in(tx, ty) || (m.flags[m.idx(tx, ty)] & F_BLOCK); }
bool map_opaque(const Map& m, int tx, int ty) { return !m.in(tx, ty) || (m.flags[m.idx(tx, ty)] & F_OPAQUE); }

bool map_los(const Map& m, float x0, float y0, float x1, float y1) {
    int ax = (int)(x0 / 16), ay = (int)(y0 / 16), bx = (int)(x1 / 16), by = (int)(y1 / 16);
    int dx = bx > ax ? bx - ax : ax - bx, sx = ax < bx ? 1 : -1, dy = -(by > ay ? by - ay : ay - by), sy = ay < by ? 1 : -1, e = dx + dy;
    for (int n = 0; n < 128; n++) {
        if (ax == bx && ay == by) return true;
        if (n > 0 && map_opaque(m, ax, ay)) return false;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; ax += sx; }
        if (e2 <= dx) { e += dx; ay += sy; }
    }
    return true;
}

static bool circleHits(const Map& m, float x, float y, float r, int& barrier) {
    int x0 = (int)floorf((x - r) / 16), x1 = (int)floorf((x + r) / 16), y0 = (int)floorf((y - r) / 16), y1 = (int)floorf((y + r) / 16);
    for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
        if (!map_blocked(m, tx, ty)) continue;
        // closest point of tile to circle centre
        float cx = x < tx * 16 ? tx * 16 : (x > tx * 16 + 16 ? tx * 16 + 16 : x);
        float cy = y < ty * 16 ? ty * 16 : (y > ty * 16 + 16 ? ty * 16 + 16 : y);
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) {
            if (m.in(tx, ty) && m.barAt[m.idx(tx, ty)] >= 0) barrier = m.barAt[m.idx(tx, ty)];
            return true;
        }
    }
    return false;
}

int map_move(const Map& m, float& x, float& y, float dx, float dy, float r) {
    int barrier = -1;
    // sub-step so fast movers (dodge) never tunnel through walls
    int steps = (int)((fabsf(dx) + fabsf(dy)) / 4.0f) + 1;
    float sx = dx / steps, sy = dy / steps;
    int dummy;
    for (int i = 0; i < steps; i++) {
        // blocked mostly head-on (a door frame, a desk corner): slip sideways around the corner instead of sticking
        if (!circleHits(m, x + sx, y, r, barrier)) x += sx;
        else if (sx != 0 && fabsf(sy) < fabsf(sx) * 0.5f) {
            for (int k = 1; k <= 7; k++) {
                float n = fabsf(sx) < k ? fabsf(sx) : (float)k;
                if (!circleHits(m, x + sx, y - k, r, dummy) && !circleHits(m, x, y - n, r, dummy)) { y -= n; break; }
                if (!circleHits(m, x + sx, y + k, r, dummy) && !circleHits(m, x, y + n, r, dummy)) { y += n; break; }
            }
        }
        if (!circleHits(m, x, y + sy, r, barrier)) y += sy;
        else if (sy != 0 && fabsf(sx) < fabsf(sy) * 0.5f) {
            for (int k = 1; k <= 7; k++) {
                float n = fabsf(sy) < k ? fabsf(sy) : (float)k;
                if (!circleHits(m, x - k, y + sy, r, dummy) && !circleHits(m, x - n, y, r, dummy)) { x -= n; break; }
                if (!circleHits(m, x + k, y + sy, r, dummy) && !circleHits(m, x + n, y, r, dummy)) { x += n; break; }
            }
        }
    }
    return barrier;
}

void map_flow(const Map& m, int tx, int ty, uint16_t* dist, bool through) {
    int n = m.w * m.h;
    for (int i = 0; i < n; i++) dist[i] = 0xFFFF;
    if (!m.in(tx, ty)) return;
    static uint16_t q[Map::MAXW * Map::MAXH];
    int head = 0, tail = 0;
    dist[m.idx(tx, ty)] = 0; q[tail++] = (uint16_t)m.idx(tx, ty);
    while (head < tail) {
        int c = q[head++], cx = c % m.w, cy = c / m.w;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& d : D) {
            int nx = cx + d[0], ny = cy + d[1];
            if (!m.in(nx, ny)) continue;
            int ni = m.idx(nx, ny);
            if (dist[ni] != 0xFFFF) continue;
            uint8_t f = m.flags[ni];
            bool pass = !(f & F_BLOCK);
            if (!pass && through && (f & F_BARRIER)) {
                const Barrier& b = m.bar[m.barAt[ni]];
                pass = b.kind == BK_DOOR || b.kind == BK_WINDOW || b.kind == BK_GATE;
            }
            if (!pass) continue;
            dist[ni] = (uint16_t)(dist[c] + 1 + ((f & F_BARRIER) && (f & F_BLOCK) ? 4 : 0));
            q[tail++] = (uint16_t)ni;
        }
    }
}

bool map_flowStep(const Map& m, const uint16_t* dist, int tx, int ty, int& nx, int& ny) {
    if (!m.in(tx, ty)) return false;
    uint16_t best = dist[m.idx(tx, ty)];
    bool found = false;
    static const int D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int k = 0; k < 8; k++) {
        int x = tx + D[k][0], y = ty + D[k][1];
        if (!m.in(x, y)) continue;
        if (k >= 4 && (map_blocked(m, tx + D[k][0], ty) || map_blocked(m, tx, ty + D[k][1]))) continue;  // no corner cutting
        uint16_t d = dist[m.idx(x, y)];
        if (d < best) { best = d; nx = x; ny = y; found = true; }
    }
    return found;
}
