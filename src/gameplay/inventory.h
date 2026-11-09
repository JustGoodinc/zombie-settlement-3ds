#pragma once
// Grid inventory (attaché-case style): items occupy w x h cells and can rotate 90 degrees.
#include <stdint.h>
#include "content.h"

struct Item {
    uint8_t id = IT_NONE;
    uint8_t x = 0, y = 0;
    uint8_t rot = 0;       // 1 = rotated 90 degrees
    uint16_t count = 1;
    uint16_t dur = 0;      // durability (weapons)
    uint8_t mag = 0;       // rounds loaded (firearms)
    uint8_t mod = 0;       // workshop upgrade level
};

Item makeItem(int id, int count = 1);
inline int itemW(const Item& it) { return it.rot ? idef(it.id).h : idef(it.id).w; }
inline int itemH(const Item& it) { return it.rot ? idef(it.id).w : idef(it.id).h; }

struct Grid {
    enum { MAX = 48 };
    uint8_t w = 6, h = 4, n = 0;
    Item it[MAX];

    int at(int x, int y) const;  // index of item covering cell, or -1
    bool fits(int id, int rot, int x, int y, int ignore = -1) const;
    bool findSpot(int id, int& rot, int& x, int& y) const;
    // Adds an item, merging stacks first. Returns how many units could NOT be added.
    int add(Item item);
    bool place(const Item& item);           // exact position, validated
    bool move(int i, int x, int y, int rot);  // validated move/rotate
    void remove(int i);
    int countOf(int id) const;
    int take(int id, int n);                // remove up to n units, returns removed
    bool sort();                            // repack largest-first; unchanged on failure
    bool resize(int nw, int nh);            // grow/shrink keeping all items, false if impossible
};

// Simple container (desk, cabinet, stash, dropped bag): an ordered list of items.
struct Container {
    enum { MAX = 32 };
    uint8_t n = 0, cap = 8;
    Item it[MAX];
    bool push(const Item& i) { if (n >= cap || n >= MAX) return false; it[n++] = i; return true; }
    void remove(int i) { for (int k = i; k + 1 < n; k++) it[k] = it[k + 1]; if (n) n--; }
};
