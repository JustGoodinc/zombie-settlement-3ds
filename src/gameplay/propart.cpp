// propart.cpp - furniture and vehicles, drawn in the same three-quarter view as the walls: a lit top face,
// a darker front face and a dark outline. Most are 16 px wide and 24 tall, bottom-aligned on their tile, so
// they stand taller than a character's waist and overlap whoever walks behind them.
// Variants are picked from the prop's position and the map, so a supermarket shelf holds groceries, a hospital
// shelf medicine and a living-room shelf books, and no two neighbouring desks look quite the same.
#include "game.h"
#include "../core/art.h"
#include "../core/rng.h"
#include <stdlib.h>
#include <string.h>

namespace {

const px K = rgb(22, 22, 34), GOLD = rgb(236, 196, 72), RED = rgb(214, 52, 48), WHITE = rgb(240, 240, 232);
inline px dk(px c, int l) { return gfx_scale(c, l); }
inline px lt(px c, int a) { return gfx_mix(c, rgb(255, 255, 255), a); }

Sprite desk[4], filing[2], locker[2], crate[3], medcab, shelf[MAP_COUNT][3], shelfSparse[MAP_COUNT][2], bench[2], cot[2], chair, plant[3], cooler[2], barrel[4], car[6], wreck[2];

// a box seen from the front and slightly above: outline, top face (th rows), front face (fh rows)
void box(Sprite& s, int x, int y, int w, int th, int fh, px top, px front) {
    spr_rect(s, x, y, w, th + fh, K);
    spr_rect(s, x + 1, y + 1, w - 2, th - 1, top);
    spr_rect(s, x + 1, y + 1, w - 2, 1, lt(top, 56));
    spr_rect(s, x + 1, y + th, w - 2, fh - 1, front);
    spr_rect(s, x + 1, y + th, w - 2, 1, lt(front, 40));
    spr_rect(s, x + w - 2, y + th + 1, 1, fh - 2, dk(front, 208));
    spr_rect(s, x + 1, y + th + fh - 2, w - 2, 1, dk(front, 184));
}
// 1 px dark rim round everything drawn so far (inside the canvas)
void outline(Sprite& s) {
    Sprite c = spr_copy(s);
    for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
        if (spr_get(c, x, y) != KEYC) continue;
        bool edge = (x > 0 && spr_get(c, x - 1, y) != KEYC) || (x + 1 < s.w && spr_get(c, x + 1, y) != KEYC) || (y > 0 && spr_get(c, x, y - 1) != KEYC) || (y + 1 < s.h && spr_get(c, x, y + 1) != KEYC);
        if (edge) spr_set(s, x, y, K);
    }
    free(c.d);
}

// shelves: three bays of goods. `full` 0..100 = how much is left on them.
Sprite makeShelf(const px* goods, int ng, bool books, bool metal, int full, uint32_t seed) {
    Sprite s = spr_new(16, 24);
    px frame = metal ? rgb(170, 176, 190) : rgb(166, 122, 80), board = metal ? rgb(196, 200, 210) : rgb(190, 146, 98);
    box(s, 0, 0, 16, 3, 21, frame, rgb(74, 74, 98));
    spr_rect(s, 1, 3, 1, 20, dk(frame, 216)); spr_rect(s, 14, 3, 1, 20, dk(frame, 176));
    for (int bay = 0; bay < 3; bay++) {
        int floor = 8 + bay * 6;                                  // last row of goods in this bay
        spr_rect(s, 1, floor + 1, 14, 1, board);
        int x = 2;
        while (x < 14) {
            uint32_t h = hash32(seed * 131 + bay * 17 + x * 7);
            int w = books ? 1 + (h & 1) : 2 + (h & 1), hg = 4 + ((h >> 3) & 1);
            if (x + w > 14) w = 14 - x;
            if ((int)((h >> 8) % 100) < full) {
                px c = goods[(h >> 16) % ng];
                bool tipped = full < 60 && ((h >> 20) & 3) == 0 && !books;
                if (tipped) { spr_rect(s, x, floor - 1, w + 1 > 14 - x ? w : w + 1, 2, c); }
                else {
                    spr_rect(s, x, floor - hg + 1, w, hg, c);
                    spr_rect(s, x, floor - hg + 1, w, 1, lt(c, 70));
                    if (!books && hg >= 4 && w >= 2) spr_set(s, x, floor - hg + 3, lt(c, 120));      // a label
                    if (w >= 2) spr_rect(s, x + w - 1, floor - hg + 2, 1, hg - 1, dk(c, 200));
                }
            }
            x += w + ((h >> 5) & 1);
        }
    }
    return s;
}

// side-on car, 40x28, three-quarter view (same geometry as the original cruiser)
Sprite makeCar(px body, px bodyL, bool police, int damage) {
    Sprite in = spr_new(38, 26);
    px white = rgb(232, 232, 226), glass = rgb(84, 118, 146), glassL = rgb(150, 190, 212), dark = dk(body, 150);
    for (int y = 11; y < 22; y++) for (int x = 0; x < 38; x++) spr_set(in, x, y, y == 11 ? bodyL : (y >= 19 ? dark : body));
    if (police) { spr_rect(in, 9, 12, 20, 8, white); spr_rect(in, 9, 18, 20, 2, rgb(184, 184, 180)); spr_rect(in, 11, 14, 6, 1, rgb(40, 60, 120)); spr_rect(in, 21, 14, 6, 1, rgb(40, 60, 120)); }
    else { spr_rect(in, 2, 13, 34, 1, lt(body, 60)); spr_rect(in, 13, 16, 2, 1, K); spr_rect(in, 23, 16, 2, 1, K); }        // shoulder line, door handles
    spr_rect(in, 19, 12, 1, 8, dark);                                                                                         // door seam
    for (int y = 4; y < 11; y++) { int in0 = (11 - y) / 2; for (int x = 6 + in0; x < 32 - in0; x++) spr_set(in, x, y, y < 6 ? (police ? body : lt(body, 30)) : glass); }
    for (int y = 6; y < 11; y++) spr_set(in, 19, y, body);
    for (int y = 6; y < 9; y++) { spr_set(in, 10 + y - 6, y, glassL); spr_set(in, 22 + y - 6, y, glassL); }
    if (police) { spr_rect(in, 13, 1, 6, 3, rgb(214, 44, 40)); spr_rect(in, 19, 1, 6, 3, rgb(48, 88, 226)); spr_set(in, 14, 1, rgb(255, 150, 140)); spr_set(in, 20, 1, rgb(150, 180, 255)); }
    spr_rect(in, 34, 13, 4, 3, rgb(252, 242, 190)); spr_rect(in, 0, 13, 3, 3, rgb(206, 44, 40));
    spr_rect(in, 0, 21, 38, 1, rgb(96, 98, 110));
    for (int k = 0; k < 2; k++) { int wx = k ? 28 : 3; spr_rect(in, wx, 19, 7, 7, rgb(18, 18, 24)); spr_rect(in, wx + 2, 21, 3, 3, rgb(128, 130, 140)); spr_set(in, wx + 3, 22, rgb(60, 60, 70)); }
    if (damage) {   // burnt-out: scorched bonnet, smashed glass, rust blooms, a flat tyre
        for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { px v = spr_get(in, x, y); if (v == KEYC) continue; uint32_t h = hash32(x * 31 + y * 57 + damage * 7); if (h % 7 == 0) spr_set(in, x, y, gfx_mix(v, rgb(132, 78, 48), 170)); }
        spr_rect(in, 26, 11, 10, 6, rgb(36, 30, 32)); spr_rect(in, 28, 12, 6, 3, rgb(58, 44, 40));
        for (int y = 6; y < 11; y++) for (int x = 9; x < 30; x++) if (spr_get(in, x, y) == glass && hash32(x * 13 + y * 7 + damage) % 3) spr_set(in, x, y, rgb(26, 28, 38));
        spr_rect(in, 28, 23, 7, 3, rgb(18, 18, 24));
    }
    Sprite s = spr_new(40, 28);
    for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { px v = spr_get(in, x, y); if (v != KEYC) spr_set(s, x + 1, y + 1, v); }
    free(in.d);
    outline(s);
    return s;
}

Sprite makeTree(uint32_t seed, px c0, px c1, px c2, px c3) {
    Sprite s = spr_new(32, 32); Rng r(seed);
    spr_circle(s, 16, 17, 12, c0);
    for (int i = 0; i < 6; i++) spr_circle(s, 9 + r.range(15), 10 + r.range(13), 5 + r.range(2), c0);
    for (int i = 0; i < 7; i++) spr_circle(s, 8 + r.range(13), 8 + r.range(12), 4 + r.range(2), c1);     // mid tone, biased up-left (the light side)
    for (int i = 0; i < 6; i++) spr_circle(s, 8 + r.range(10), 7 + r.range(8), 2 + r.range(2), c2);
    for (int i = 0; i < 9; i++) { int x = 7 + r.range(10), y = 6 + r.range(8); spr_set(s, x, y, c3); spr_set(s, x + 1, y, c3); }
    outline(s);
    return s;
}

} // namespace

// tiny ASCII sprites: each letter indexes `keys` -> `cols`; '.' is transparent
static Sprite ascii(const char* const* rows, int h, const char* keys, const px* cols) {
    int w = (int)strlen(rows[0]); Sprite s = spr_new(w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w && rows[y][x]; x++) { const char* k = strchr(keys, rows[y][x]); if (k) spr_set(s, x, y, cols[k - keys]); }
    return s;
}

// ground clutter (8x8) and the small outdoor props, in the same outlined style
static void smallArt() {
    {   static const char* R[] = {"........", "........", "..l..l..", ".lg.lg..", ".g.lg.l.", "..gg.gg.", "...gg...", "........"};
        static const px C[] = {rgb(140, 184, 96), rgb(74, 126, 70)}; g_spr[S_DEC_TUFT] = ascii(R, 8, "lg", C); }
    {   static const char* R[] = {"........", "..w.....", ".wyw..p.", "..w..pyp", "..g...p.", "..g...g.", "........", "........"};
        static const px C[] = {rgb(242, 242, 232), rgb(246, 206, 72), rgb(238, 132, 152), rgb(70, 122, 66)}; g_spr[S_DEC_FLOWER] = ascii(R, 8, "wypg", C); }
    {   static const char* R[] = {"........", "........", "..hhh...", ".hmmmd..", ".mmmdd..", "..ddd...", "........", "........"};
        static const px C[] = {rgb(190, 188, 180), rgb(142, 140, 138), rgb(92, 90, 100)}; g_spr[S_DEC_ROCK] = ascii(R, 8, "hmd", C); }
    {   static const char* R[] = {"........", "..www...", "..wlww..", "..wwlws.", "..wwwws.", "...ssss.", "........", "........"};
        static const px C[] = {rgb(238, 236, 226), rgb(168, 170, 180), rgb(70, 70, 88)}; g_spr[S_DEC_PAPER] = ascii(R, 8, "wls", C); }
    {   static const char* R[] = {"........", ".d....m.", "...m....", "......d.", "..d.....", ".....m..", "...d....", "........"};
        static const px C[] = {rgb(62, 62, 76), rgb(104, 104, 118)}; g_spr[S_DEC_DEBRIS] = ascii(R, 8, "dm", C); }
    {   static const char* R[] = {"........", "..dddd..", ".ddvvdd.", ".dvtvdd.", ".ddvvdd.", "..dddd..", "........", "........"};
        static const px C[] = {rgb(40, 40, 54), rgb(84, 70, 124), rgb(64, 110, 130)}; g_spr[S_DEC_OIL] = ascii(R, 8, "dvt", C); }
    {   static const char* R[] = {"........", "......dd", "....dd..", "dd.d....", "..d.....", "..d.....", "...d....", "........"};
        static const px C[] = {rgb(44, 44, 60)}; g_spr[S_DEC_CRACK] = ascii(R, 8, "d", C); }
    {   static const char* R[] = {"........", "........", ".rr.....", "..rrrr..", "...rdrr.", ".....dd.", "........", "........"};
        static const px C[] = {rgb(132, 28, 34), rgb(96, 18, 26)}; g_spr[S_DEC_SMEAR] = ascii(R, 8, "rd", C); }
    {   static const char* R[] = {"........", ".oo.....", ".ob..y..", ".....yy.", "..b.....", ".bb..o..", ".....oo.", "........"};
        static const px C[] = {rgb(218, 142, 58), rgb(150, 96, 52), rgb(224, 194, 84)}; g_spr[S_DEC_LEAVES] = ascii(R, 8, "oby", C); }
    {   // traffic cone
        static const char* R[] = {"................", "................", ".......kk.......", "......kook......", "......kOok......", ".....kwwwwk.....", ".....kwwwwk.....", "....koOooodk....",
                                  "....kOoooodk....", "...kooOoooodk...", "...kOoooooodk...", "..kkkkkkkkkkkk..", "..kDDDDDDDDDDk..", "..kkkkkkkkkkkk..", "................", "................"};
        static const px C[] = {K, rgb(238, 122, 46), rgb(252, 166, 86), rgb(242, 240, 228), rgb(180, 78, 32), rgb(196, 90, 38)}; g_spr[S_CONE] = ascii(R, 16, "koOwdD", C); }
    {   // sandbags: two courses of stuffed sacks
        static const char* R[] = {"................", "................", "................", "................", "..kkkkkkkkkkkk..", ".khhhhhkhhhhhhk.", ".kttttskttttttk.", ".kttttskttttsdk.",
                                  "kkkkkkkkkkkkkkkk", "khhhkhhhhhhkhhhk", "kttskttttttkttsk", "kttskttttsdkttdk", "kddskddddddkdddk", "kkkkkkkkkkkkkkkk", "................", "................"};
        static const px C[] = {K, rgb(216, 196, 150), rgb(188, 166, 122), rgb(160, 138, 100), rgb(132, 112, 82)}; g_spr[S_SANDBAG] = ascii(R, 16, "khtsd", C); }
    {   // log pile: three cut logs, pale rings facing us
        static const char* R[] = {"................", "................", "................", ".....kkkkkk.....", "....krrrrrrk....", "....krmmmmrk....", "....krmddmrk....", "..kkkkrmmrkkkk..",
                                  ".krrrrkkkkrrrrk.", ".krmmmrkkrmmmrk.", ".krmddmrrmddmrk.", ".krmddmrrmddmrk.", ".krmmmrkkrmmmrk.", ".kbbbbbkkbbbbbk.", "..kkkkkkkkkkkk..", "................"};
        static const px C[] = {K, rgb(222, 190, 138), rgb(190, 152, 104), rgb(150, 112, 74), rgb(110, 78, 52)}; g_spr[S_LOGS] = ascii(R, 16, "krmdb", C); }
}

void propart_init() {
    const px WT = rgb(186, 136, 88), WF = rgb(136, 94, 60);
    // ---- desks
    {   Sprite s = spr_new(16, 24); box(s, 0, 8, 16, 6, 10, WT, WF);                       // office desk with a monitor
        spr_rect(s, 3, 1, 9, 8, K); spr_rect(s, 4, 2, 7, 6, rgb(44, 70, 100)); spr_set(s, 5, 3, rgb(160, 220, 240)); spr_set(s, 6, 3, rgb(160, 220, 240)); spr_set(s, 5, 4, rgb(96, 150, 190)); spr_rect(s, 6, 9, 3, 1, rgb(60, 62, 76));
        spr_rect(s, 4, 11, 6, 1, rgb(220, 220, 212)); spr_rect(s, 4, 12, 6, 1, rgb(150, 150, 152)); spr_rect(s, 12, 10, 3, 3, WHITE); spr_set(s, 13, 11, rgb(170, 170, 170));
        spr_rect(s, 2, 16, 5, 5, dk(WF, 220)); spr_rect(s, 9, 16, 5, 5, dk(WF, 220)); spr_set(s, 4, 18, GOLD); spr_set(s, 11, 18, GOLD);
        desk[0] = s; }
    {   Sprite s = spr_new(16, 24); box(s, 0, 8, 16, 6, 10, WT, WF);                       // desk with a lamp and paperwork
        spr_rect(s, 10, 3, 5, 3, K); spr_rect(s, 11, 4, 3, 1, rgb(70, 170, 110)); spr_set(s, 12, 6, K); spr_set(s, 12, 7, K); spr_set(s, 12, 8, K); spr_rect(s, 11, 9, 3, 1, rgb(60, 62, 76)); spr_set(s, 12, 5, rgb(250, 240, 170));
        spr_rect(s, 2, 10, 6, 3, WHITE); spr_rect(s, 3, 11, 4, 1, rgb(170, 170, 176)); spr_rect(s, 4, 9, 5, 2, rgb(226, 222, 204)); spr_set(s, 9, 12, RED);
        spr_rect(s, 2, 16, 12, 2, dk(WF, 220)); spr_rect(s, 2, 19, 12, 2, dk(WF, 220)); spr_set(s, 7, 16, GOLD); spr_set(s, 8, 16, GOLD); spr_set(s, 7, 19, GOLD); spr_set(s, 8, 19, GOLD);
        desk[1] = s; }
    {   Sprite s = spr_new(16, 24); px t = rgb(196, 160, 116), f = rgb(150, 112, 78); box(s, 0, 8, 16, 6, 10, t, f);   // home dresser: photo, vase
        spr_rect(s, 2, 3, 5, 6, K); spr_rect(s, 3, 4, 3, 4, rgb(150, 190, 214)); spr_set(s, 4, 6, rgb(236, 200, 160));
        spr_rect(s, 10, 6, 3, 4, K); spr_rect(s, 11, 7, 1, 2, rgb(96, 150, 200)); spr_set(s, 10, 4, rgb(96, 170, 96)); spr_set(s, 11, 3, rgb(232, 110, 130)); spr_set(s, 12, 4, rgb(96, 170, 96)); spr_set(s, 11, 5, rgb(70, 140, 80));
        for (int r = 0; r < 2; r++) { spr_rect(s, 2, 16 + r * 3, 12, 2, dk(f, 226)); spr_set(s, 5, 16 + r * 3, GOLD); spr_set(s, 10, 16 + r * 3, GOLD); }
        desk[2] = s; }
    {   Sprite s = spr_new(16, 24); box(s, 0, 8, 16, 6, 10, rgb(58, 60, 74), rgb(220, 208, 182));                        // checkout: belt + till
        for (int x = 2; x < 14; x += 2) spr_rect(s, x, 10, 1, 3, rgb(88, 90, 106));
        spr_rect(s, 1, 1, 8, 8, K); spr_rect(s, 2, 2, 6, 6, RED); spr_rect(s, 3, 3, 4, 2, rgb(160, 236, 180)); spr_rect(s, 3, 6, 4, 1, rgb(240, 200, 190));
        spr_rect(s, 1, 17, 14, 2, RED); spr_rect(s, 11, 5, 3, 4, K); spr_rect(s, 12, 6, 1, 2, rgb(240, 220, 120));
        desk[3] = s; }
    // ---- filing cabinets
    for (int v = 0; v < 2; v++) {
        px t = v ? rgb(204, 194, 166) : rgb(172, 180, 196), f = v ? rgb(160, 150, 124) : rgb(120, 132, 156);
        Sprite s = spr_new(16, 24); box(s, 2, 1, 12, 5, 18, t, f);
        for (int d = 0; d < 3; d++) { int y = 7 + d * 5; if (d) spr_rect(s, 3, y - 1, 10, 1, dk(f, 160)); spr_rect(s, 6, y + 2, 4, 1, lt(f, 110)); spr_rect(s, 6, y, 4, 1, WHITE); }
        filing[v] = s;
    }
    // ---- lockers
    for (int v = 0; v < 2; v++) {
        px t = v ? rgb(140, 152, 100) : rgb(104, 132, 178), f = v ? rgb(104, 118, 74) : rgb(70, 100, 152);
        Sprite s = spr_new(16, 24); box(s, 1, 0, 14, 4, 20, t, f);
        spr_rect(s, 8, 4, 1, 19, K);
        for (int k = 0; k < 2; k++) { int x = k ? 10 : 3; spr_rect(s, x, 6, 3, 1, dk(f, 150)); spr_rect(s, x, 8, 3, 1, dk(f, 150)); spr_rect(s, x, 11, 2, 2, rgb(232, 226, 200)); }
        spr_rect(s, 6, 13, 1, 3, lt(f, 130)); spr_rect(s, 10, 13, 1, 3, lt(f, 130));
        locker[v] = s;
    }
    // ---- crates: wooden, cardboard, military
    {   Sprite s = spr_new(16, 24); px t = rgb(212, 172, 118), f = rgb(172, 130, 86); box(s, 0, 8, 16, 5, 11, t, f);
        spr_rect(s, 5, 10, 1, 3, dk(t, 200)); spr_rect(s, 10, 10, 1, 3, dk(t, 200));
        spr_rect(s, 1, 14, 14, 1, lt(f, 50)); spr_rect(s, 1, 21, 14, 1, dk(f, 200)); spr_rect(s, 1, 14, 1, 8, lt(f, 30)); spr_rect(s, 14, 14, 1, 8, dk(f, 190));
        for (int i = 0; i < 7; i++) { spr_set(s, 2 + (i * 11) / 6, 15 + i, dk(f, 176)); spr_set(s, 13 - (i * 11) / 6, 15 + i, dk(f, 176)); }
        crate[0] = s; }
    {   Sprite s = spr_new(16, 24); px t = rgb(222, 192, 144), f = rgb(190, 156, 110); box(s, 1, 9, 14, 5, 10, t, f);
        spr_rect(s, 7, 10, 2, 13, rgb(238, 224, 190)); spr_rect(s, 3, 16, 3, 3, WHITE); spr_set(s, 4, 17, rgb(90, 90, 96));
        spr_set(s, 11, 17, dk(f, 170)); spr_set(s, 12, 16, dk(f, 170)); spr_set(s, 10, 16, dk(f, 170)); spr_set(s, 11, 18, dk(f, 170)); spr_set(s, 11, 19, dk(f, 170));
        crate[1] = s; }
    {   Sprite s = spr_new(16, 24); px t = rgb(124, 142, 92), f = rgb(92, 110, 68); box(s, 0, 8, 16, 5, 11, t, f);
        spr_rect(s, 1, 17, 14, 1, dk(f, 170)); spr_rect(s, 3, 15, 2, 2, rgb(60, 62, 70)); spr_rect(s, 11, 15, 2, 2, rgb(60, 62, 70));
        spr_rect(s, 5, 19, 6, 1, rgb(232, 206, 90)); spr_rect(s, 6, 20, 4, 1, rgb(232, 206, 90)); spr_rect(s, 4, 10, 8, 1, dk(t, 190));
        crate[2] = s; }
    // ---- medical cabinet
    {   Sprite s = spr_new(16, 24); box(s, 1, 1, 14, 4, 19, rgb(242, 244, 242), rgb(218, 224, 228));
        spr_rect(s, 7, 8, 2, 7, RED); spr_rect(s, 5, 10, 6, 3, RED); spr_rect(s, 7, 10, 2, 3, rgb(240, 96, 90));
        spr_rect(s, 3, 18, 10, 1, rgb(170, 178, 186)); spr_rect(s, 3, 20, 10, 1, rgb(170, 178, 186)); spr_set(s, 12, 13, rgb(120, 126, 136));
        medcab = s; }
    // ---- shelves, stocked for each kind of place
    static const px G_POLICE[] = {rgb(176, 138, 94), rgb(104, 122, 78), rgb(56, 58, 70), rgb(200, 190, 160), rgb(120, 96, 70)};
    static const px G_SETTLE[] = {rgb(176, 138, 94), rgb(150, 110, 70), rgb(104, 122, 78), rgb(200, 180, 130), rgb(120, 96, 70)};
    static const px G_HOSP[] = {rgb(240, 242, 240), rgb(124, 200, 218), rgb(74, 134, 196), rgb(210, 150, 64), rgb(236, 110, 110), rgb(160, 210, 160)};
    static const px G_MARKET[] = {rgb(220, 58, 50), rgb(244, 200, 62), rgb(62, 124, 212), rgb(88, 176, 88), rgb(240, 146, 52), rgb(240, 240, 230), rgb(206, 76, 150), rgb(120, 70, 160)};
    static const px G_HOMES[] = {rgb(160, 62, 62), rgb(62, 94, 156), rgb(72, 136, 92), rgb(208, 176, 92), rgb(116, 74, 136), rgb(220, 214, 196), rgb(200, 110, 60)};
    static const px G_YARD[] = {rgb(134, 140, 152), rgb(230, 134, 50), rgb(62, 104, 166), rgb(150, 96, 62), rgb(206, 190, 80)};
    static const struct { const px* g; int n; bool books, metal; } SH[MAP_COUNT] = {
        {G_POLICE, 5, false, true}, {G_SETTLE, 5, false, false}, {G_HOSP, 6, false, true}, {G_MARKET, 8, false, true}, {G_HOMES, 7, true, false}, {G_YARD, 5, false, true}};
    for (int m = 0; m < MAP_COUNT; m++) {
        for (int v = 0; v < 3; v++) shelf[m][v] = makeShelf(SH[m].g, SH[m].n, SH[m].books, SH[m].metal, 92, m * 100 + v);
        for (int v = 0; v < 2; v++) shelfSparse[m][v] = makeShelf(SH[m].g, SH[m].n, SH[m].books, SH[m].metal, 56, m * 100 + 50 + v);
    }
    // ---- benches: park bench, hospital waiting-room seats
    {   Sprite s = spr_new(16, 24); px w = rgb(176, 126, 80);
        spr_rect(s, 0, 9, 16, 5, K); spr_rect(s, 1, 10, 14, 1, lt(w, 60)); spr_rect(s, 1, 11, 14, 2, w);
        spr_rect(s, 0, 14, 16, 6, K); spr_rect(s, 1, 15, 14, 2, lt(w, 30)); spr_rect(s, 1, 17, 14, 2, dk(w, 214));
        spr_rect(s, 1, 20, 2, 4, K); spr_rect(s, 13, 20, 2, 4, K); spr_rect(s, 2, 13, 1, 2, rgb(60, 60, 72)); spr_rect(s, 13, 13, 1, 2, rgb(60, 60, 72));
        bench[0] = s; }
    {   Sprite s = spr_new(16, 24); px b = rgb(72, 132, 200);
        for (int k = 0; k < 3; k++) { int x = k * 5; spr_rect(s, x, 9, 6, 7, K); spr_rect(s, x + 1, 10, 4, 5, b); spr_rect(s, x + 1, 10, 4, 1, lt(b, 80)); spr_rect(s, x, 16, 6, 5, K); spr_rect(s, x + 1, 16, 4, 3, lt(b, 40)); spr_rect(s, x + 1, 19, 4, 1, dk(b, 190)); }
        spr_rect(s, 1, 21, 1, 3, rgb(150, 156, 168)); spr_rect(s, 14, 21, 1, 3, rgb(150, 156, 168)); spr_rect(s, 1, 21, 14, 1, rgb(110, 116, 130));
        bench[1] = s; }
    // ---- beds: hospital bed, cell cot
    for (int v = 0; v < 2; v++) {
        px bl = v ? rgb(128, 138, 120) : rgb(98, 174, 164), fr = v ? rgb(110, 114, 126) : rgb(196, 202, 212);
        Sprite s = spr_new(16, 24);
        spr_rect(s, 1, 1, 14, 22, K); spr_rect(s, 2, 2, 12, 20, fr); spr_rect(s, 3, 3, 10, 18, rgb(232, 234, 228));
        spr_rect(s, 4, 4, 8, 4, WHITE); spr_rect(s, 4, 7, 8, 1, rgb(196, 200, 204));
        spr_rect(s, 3, 10, 10, 11, bl); spr_rect(s, 3, 10, 10, 1, lt(bl, 90)); spr_rect(s, 3, 11, 10, 1, lt(bl, 40)); spr_rect(s, 3, 19, 10, 2, dk(bl, 200));
        spr_rect(s, 5, 14, 1, 3, dk(bl, 220)); spr_rect(s, 9, 13, 1, 2, dk(bl, 220));
        spr_rect(s, 2, 2, 12, 1, lt(fr, 70)); spr_rect(s, 1, 23, 2, 1, K); spr_rect(s, 13, 23, 2, 1, K);
        cot[v] = s;
    }
    // ---- office chair
    {   Sprite s = spr_new(16, 16); px c = rgb(80, 90, 120);
        spr_rect(s, 4, 1, 8, 6, K); spr_rect(s, 5, 2, 6, 4, c); spr_rect(s, 5, 2, 6, 1, lt(c, 70));
        spr_rect(s, 3, 7, 10, 6, K); spr_rect(s, 4, 8, 8, 3, lt(c, 34)); spr_rect(s, 4, 11, 8, 1, dk(c, 190));
        spr_rect(s, 7, 13, 2, 1, rgb(60, 62, 76)); spr_set(s, 4, 14, K); spr_set(s, 11, 14, K); spr_set(s, 7, 15, K); spr_set(s, 8, 15, K);
        chair = s; }
    // ---- potted plants
    for (int v = 0; v < 3; v++) {
        Sprite s = spr_new(16, 24); px pot = v == 2 ? rgb(232, 230, 222) : rgb(190, 106, 68), g0 = rgb(44, 104, 66), g1 = rgb(70, 142, 80), g2 = rgb(110, 178, 92);
        spr_rect(s, 5, 17, 6, 6, pot); spr_rect(s, 4, 16, 8, 2, lt(pot, 40)); spr_rect(s, 9, 18, 2, 5, dk(pot, 200)); spr_rect(s, 5, 22, 6, 1, dk(pot, 170));
        if (v == 1) { for (int k = -3; k <= 3; k++) for (int i = 0; i < 9; i++) { int x = 8 + (k * i) / 3, y = 15 - i - (k == 0 ? 2 : 0) + (abs(k) * i) / 8; spr_set(s, x, y, i > 5 ? g2 : (i > 2 ? g1 : g0)); } }   // fern fronds
        else { spr_circle(s, 8, 10, 5, g0); spr_circle(s, 6, 9, 3, g1); spr_circle(s, 10, 8, 3, g1); spr_circle(s, 7, 6, 2, g2); spr_circle(s, 10, 11, 2, g0); spr_set(s, 6, 7, lt(g2, 60)); spr_set(s, 9, 6, lt(g2, 60));
               if (v == 2) { spr_set(s, 5, 8, rgb(240, 120, 150)); spr_set(s, 9, 5, rgb(240, 120, 150)); spr_set(s, 11, 9, rgb(250, 220, 110)); spr_set(s, 7, 11, rgb(240, 120, 150)); } }
        outline(s); plant[v] = s;
    }
    // ---- water cooler, supermarket freezer
    {   Sprite s = spr_new(16, 24);
        spr_rect(s, 4, 10, 8, 14, K); spr_rect(s, 5, 11, 6, 12, rgb(230, 234, 236)); spr_rect(s, 9, 12, 2, 11, rgb(190, 196, 204)); spr_rect(s, 5, 17, 6, 1, rgb(120, 126, 140));
        spr_rect(s, 4, 1, 8, 10, K); spr_rect(s, 5, 2, 6, 8, rgb(96, 172, 224)); spr_rect(s, 6, 3, 1, 6, rgb(200, 232, 250)); spr_rect(s, 5, 2, 6, 1, rgb(150, 206, 240)); spr_rect(s, 9, 4, 2, 6, rgb(70, 140, 200));
        spr_set(s, 6, 14, RED); spr_set(s, 9, 14, rgb(60, 120, 220));
        cooler[0] = s; }
    {   Sprite s = spr_new(16, 24); box(s, 0, 0, 16, 3, 21, rgb(204, 210, 220), rgb(54, 66, 90));
        spr_rect(s, 1, 3, 14, 2, rgb(238, 100, 64)); spr_rect(s, 2, 6, 12, 15, rgb(150, 218, 234)); spr_rect(s, 2, 6, 12, 1, rgb(214, 244, 250));
        for (int r = 0; r < 3; r++) { spr_rect(s, 2, 10 + r * 5, 12, 1, rgb(96, 150, 176)); for (int i = 0; i < 4; i++) { uint32_t h = hash32(r * 7 + i * 13); static const px P[4] = {rgb(236, 90, 80), rgb(250, 214, 90), rgb(240, 240, 236), rgb(90, 160, 230)}; spr_rect(s, 3 + i * 3, 7 + r * 5, 2, 3, P[h & 3]); } }
        spr_rect(s, 7, 6, 1, 15, rgb(232, 240, 246)); spr_rect(s, 8, 6, 1, 15, rgb(70, 110, 140));
        cooler[1] = s; }
    // ---- oil drums
    static const px DRUM[4] = {rgb(64, 108, 170), rgb(184, 64, 54), rgb(150, 104, 66), rgb(232, 196, 64)};
    for (int v = 0; v < 4; v++) {
        Sprite s = spr_new(16, 24); px c = DRUM[v];
        spr_rect(s, 3, 9, 10, 14, c); spr_rect(s, 4, 9, 1, 14, lt(c, 70)); spr_rect(s, 10, 9, 2, 14, dk(c, 196)); spr_rect(s, 12, 9, 1, 14, dk(c, 150));
        spr_rect(s, 3, 13, 10, 1, dk(c, 160)); spr_rect(s, 3, 18, 10, 1, dk(c, 160));
        if (v == 3) spr_rect(s, 3, 15, 10, 2, K);
        spr_rect(s, 4, 6, 8, 1, lt(c, 40)); spr_rect(s, 3, 7, 10, 2, lt(c, 60)); spr_rect(s, 5, 7, 2, 1, K); spr_rect(s, 3, 8, 10, 1, dk(c, 220));
        outline(s); barrel[v] = s;
    }
    // ---- vehicles: one cruiser, five civilian paint jobs, two wrecks
    car[0] = makeCar(rgb(34, 36, 48), rgb(64, 68, 84), true, 0);
    static const px PAINT[5] = {rgb(186, 58, 52), rgb(64, 110, 184), rgb(226, 226, 218), rgb(226, 186, 70), rgb(70, 132, 96)};
    for (int v = 0; v < 5; v++) car[1 + v] = makeCar(PAINT[v], lt(PAINT[v], 70), false, 0);
    wreck[0] = makeCar(rgb(128, 86, 62), rgb(160, 112, 80), false, 1);
    wreck[1] = makeCar(rgb(70, 72, 84), rgb(104, 104, 114), false, 2);
    g_spr[S_CAR] = car[0]; g_spr[S_WRECK] = wreck[0];
    // ---- trees: broad-leaf, and a darker conifer-ish clump
    g_spr[S_TREE] = makeTree(77, rgb(36, 76, 58), rgb(58, 112, 64), rgb(92, 150, 76), rgb(140, 186, 96));
    g_spr[S_TREE2] = makeTree(131, rgb(40, 70, 50), rgb(72, 108, 56), rgb(112, 142, 66), rgb(160, 176, 88));
    smallArt();
}

// sprite and draw offset for a prop on its tile; nullptr = drawn by hand in the renderer (trees, fires, lamps, plots)
const Sprite* prop_sprite(const Map& m, const Prop& p, int& ox, int& oy) {
    uint32_t h = hash32(p.tx * 7349u + p.ty * 9151u + m.id * 31u);
    int id = m.id >= 0 && m.id < MAP_COUNT ? m.id : 0;
    ox = 0; oy = -8;
    switch (p.type) {
    case PR_DESK: return &desk[id == MAP_MARKET ? 3 : (id == MAP_HOMES ? 2 : (int)(h & 1))];
    case PR_FILING: return &filing[h & 1];
    case PR_LOCKER: return &locker[id == MAP_YARD ? 1 : 0];
    case PR_CRATE: return &crate[id == MAP_POLICE && (h % 3) == 0 ? 2 : 0];
    case PR_DECOR_CRATE: return &crate[1];
    case PR_MEDCAB: return &medcab;
    case PR_SHELF: return p.container >= 0 ? &shelf[id][h % 3] : &shelfSparse[id][h & 1];
    case PR_BENCH: return &bench[id == MAP_HOSPITAL ? 1 : 0];
    case PR_COT: return &cot[id == MAP_HOSPITAL ? 0 : 1];
    case PR_CHAIR: oy = 0; return &chair;
    case PR_PLANT: return &plant[h % 3];
    case PR_COOLER: return &cooler[id == MAP_MARKET ? 1 : 0];
    case PR_BARREL: return &barrel[h % 4];
    case PR_CAR: ox = -4; oy = -12; return &car[id == MAP_POLICE || (h % 6) == 0 ? 0 : 1 + (h >> 4) % 5];
    case PR_WRECK: ox = -4; oy = -12; return &wreck[h & 1];
    case PR_SHIPBOX: return &g_spr[S_SHIPBOX0 + (p.tx / 4 + p.ty / 2) % 4];
    case PR_BOXCAR: return &g_spr[S_BOXCAR];
    case PR_BED: oy = 0; return &g_spr[S_BED];
    case PR_SOFA: oy = 0; return &g_spr[S_SOFA];
    case PR_FRIDGE: oy = -6; return &g_spr[S_FRIDGE];
    case PR_TENT: ox = -7; oy = -24; return &g_spr[S_TENT];
    case PR_SANDBAG: oy = 0; return &g_spr[S_SANDBAG];
    case PR_LOGS: oy = 0; return &g_spr[S_LOGS];
    case PR_CONE: oy = 0; return &g_spr[S_CONE];
    case PR_WORKBENCH: oy = 0; return &g_spr[S_WORKBENCH];
    }
    return nullptr;
}

// which vehicles are still "alive": 1 = a cruiser whose lightbar is still turning, 2 = a wreck that is still burning
int prop_fx(const Map& m, const Prop& p) {
    uint32_t h = hash32(p.tx * 7349u + p.ty * 9151u + m.id * 31u);
    int id = m.id >= 0 && m.id < MAP_COUNT ? m.id : 0;
    if (p.type == PR_CAR && (id == MAP_POLICE || (h % 6) == 0)) return ((h >> 9) & 1) ? 1 : 0;
    if (p.type == PR_WRECK && ((h >> 7) % 3) == 0) return 2;
    return 0;
}
