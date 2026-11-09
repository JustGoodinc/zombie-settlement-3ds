#include "inventory.h"

Item makeItem(int id, int count) {
    Item i; i.id = (uint8_t)id; i.count = (uint16_t)count;
    i.dur = idef(id).wpn.dur;
    if (idef(id).cat == IC_FIREARM) i.mag = idef(id).wpn.mag;
    return i;
}

int Grid::at(int x, int y) const {
    for (int i = 0; i < n; i++) {
        const Item& a = it[i];
        if (x >= a.x && y >= a.y && x < a.x + itemW(a) && y < a.y + itemH(a)) return i;
    }
    return -1;
}

bool Grid::fits(int id, int rot, int x, int y, int ignore) const {
    int iw = rot ? idef(id).h : idef(id).w, ih = rot ? idef(id).w : idef(id).h;
    if (x < 0 || y < 0 || x + iw > w || y + ih > h) return false;
    for (int i = 0; i < n; i++) {
        if (i == ignore) continue;
        const Item& a = it[i];
        if (x < a.x + itemW(a) && a.x < x + iw && y < a.y + itemH(a) && a.y < y + ih) return false;
    }
    return true;
}

bool Grid::findSpot(int id, int& rot, int& x, int& y) const {
    for (int r = 0; r < 2; r++) {
        if (r == 1 && idef(id).w == idef(id).h) break;
        for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++)
            if (fits(id, r, xx, yy)) { rot = r; x = xx; y = yy; return true; }
    }
    return false;
}

int Grid::add(Item item) {
    const ItemDef& d = idef(item.id);
    if (d.maxStack > 1) {
        for (int i = 0; i < n && item.count; i++) {
            if (it[i].id != item.id || it[i].count >= d.maxStack) continue;
            int room = d.maxStack - it[i].count, m = item.count < room ? item.count : room;
            it[i].count += m; item.count -= m;
        }
    }
    while (item.count) {
        if (n >= MAX) return item.count;
        int r, x, y;
        if (!findSpot(item.id, r, x, y)) return item.count;
        Item p = item; p.rot = (uint8_t)r; p.x = (uint8_t)x; p.y = (uint8_t)y;
        p.count = item.count > d.maxStack ? d.maxStack : item.count;
        it[n++] = p; item.count -= p.count;
    }
    return 0;
}

bool Grid::place(const Item& item) {
    if (n >= MAX || !fits(item.id, item.rot, item.x, item.y)) return false;
    it[n++] = item;
    return true;
}

bool Grid::move(int i, int x, int y, int rot) {
    if (i < 0 || i >= n || !fits(it[i].id, rot, x, y, i)) return false;
    it[i].x = (uint8_t)x; it[i].y = (uint8_t)y; it[i].rot = (uint8_t)rot;
    return true;
}

void Grid::remove(int i) {
    if (i < 0 || i >= n) return;
    for (int k = i; k + 1 < n; k++) it[k] = it[k + 1];
    n--;
}

int Grid::countOf(int id) const { int c = 0; for (int i = 0; i < n; i++) if (it[i].id == id) c += it[i].count; return c; }

int Grid::take(int id, int want) {
    int got = 0;
    for (int i = n - 1; i >= 0 && got < want; i--) {
        if (it[i].id != id) continue;
        int m = want - got < it[i].count ? want - got : it[i].count;
        it[i].count -= m; got += m;
        if (!it[i].count) remove(i);
    }
    return got;
}

static int area(const Item& a) { return idef(a.id).w * idef(a.id).h; }

bool Grid::sort() {
    Grid g; g.w = w; g.h = h;
    Item tmp[MAX]; int m = n;
    for (int i = 0; i < n; i++) tmp[i] = it[i];
    // insertion sort: larger first, then by id so stacks group together
    for (int i = 1; i < m; i++) {
        Item k = tmp[i]; int j = i - 1;
        while (j >= 0 && (area(tmp[j]) < area(k) || (area(tmp[j]) == area(k) && tmp[j].id > k.id))) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = k;
    }
    for (int i = 0; i < m; i++) {
        int r, x, y;
        if (!g.findSpot(tmp[i].id, r, x, y)) return false;
        Item p = tmp[i]; p.rot = (uint8_t)r; p.x = (uint8_t)x; p.y = (uint8_t)y;
        g.it[g.n++] = p;
    }
    *this = g;
    return true;
}

bool Grid::resize(int nw, int nh) {
    Grid g = *this; g.w = (uint8_t)nw; g.h = (uint8_t)nh;
    bool ok = true;
    for (int i = 0; i < n; i++) if (it[i].x + itemW(it[i]) > nw || it[i].y + itemH(it[i]) > nh) ok = false;
    if (!ok && !g.sort()) return false;
    *this = g;
    return true;
}
