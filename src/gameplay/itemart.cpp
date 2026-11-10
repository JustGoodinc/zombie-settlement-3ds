// itemart.cpp - inventory icons, 12 px per grid cell (ITEM_UNIT), in the same style as the world's furniture:
// a dark outline, light from the upper left, a few clean tones per material. In the starter backpack (24 px
// cells) they are drawn at 2x, the same pixel size as the world at default zoom; in upgraded bags at 1x.
#include "game.h"
#include "../core/art.h"
#include <stdlib.h>

namespace {
const px K = rgb(22, 22, 34), GOLD = rgb(236, 196, 72), RED = rgb(206, 52, 48), RED_H = rgb(240, 100, 90), RED_D = rgb(150, 34, 38), WHITE = rgb(240, 240, 232);
const px ST = rgb(150, 158, 176), ST_H = rgb(208, 214, 226), ST_D = rgb(98, 104, 124), GM = rgb(70, 74, 92), GM_H = rgb(116, 122, 142), GM_D = rgb(48, 50, 64);
const px WD = rgb(176, 124, 76), WD_H = rgb(210, 160, 108), WD_D = rgb(124, 84, 52);
inline px dk(px c, int l) { return gfx_scale(c, l); }
inline px lt(px c, int a) { return gfx_mix(c, rgb(255, 255, 255), a); }

Sprite* S = nullptr;
void R(int x, int y, int w, int h, px c) { spr_rect(*S, x, y, w, h, c); }
void P(int x, int y, px c) { spr_set(*S, x, y, c); }
void finish() {   // 1 px outline round the drawing
    Sprite c = spr_copy(*S);
    for (int y = 0; y < S->h; y++) for (int x = 0; x < S->w; x++) {
        if (spr_get(c, x, y) != KEYC) continue;
        if ((x > 0 && spr_get(c, x - 1, y) != KEYC) || (x + 1 < S->w && spr_get(c, x + 1, y) != KEYC) || (y > 0 && spr_get(c, x, y - 1) != KEYC) || (y + 1 < S->h && spr_get(c, x, y + 1) != KEYC)) spr_set(*S, x, y, K);
    }
    free(c.d);
}
void begin(int id, int cw, int ch) { g_item[id] = spr_new(cw * ITEM_UNIT, ch * ITEM_UNIT); S = &g_item[id]; }
} // namespace

void itemart_init() {
    // ---- firearms (muzzle to the right)
    begin(IT_PISTOL, 2, 1);
    R(4, 2, 16, 3, ST); R(4, 2, 16, 1, ST_H); R(13, 3, 3, 1, GM); P(20, 3, GM_D); R(5, 5, 12, 1, ST_D);
    for (int r = 0; r < 5; r++) { R(5 - r / 2, 6 + r, 5, 1, GM); P(5 - r / 2, 6 + r, GM_H); }
    P(11, 6, K); R(11, 7, 4, 1, ST_D); P(14, 6, ST_D); finish();
    begin(IT_REVOLVER, 2, 1);
    R(12, 2, 9, 2, ST); R(12, 2, 9, 1, ST_H); P(20, 1, ST_D); R(7, 2, 6, 4, rgb(126, 134, 152)); R(7, 2, 6, 1, ST_H); P(9, 3, ST_D); P(11, 3, ST_D); P(9, 5, ST_D); P(11, 5, ST_D);
    R(6, 4, 8, 2, ST_D); P(6, 1, ST_D); P(5, 2, ST_D);
    for (int r = 0; r < 5; r++) { R(4 - r / 3, 6 + r, 4, 1, WD); P(4 - r / 3, 6 + r, WD_H); P(7 - r / 3, 6 + r, WD_D); }
    P(11, 6, K); R(11, 7, 3, 1, ST_D); finish();
    begin(IT_SHOTGUN, 4, 1);
    R(16, 3, 30, 2, GM); R(16, 3, 30, 1, GM_H); R(18, 5, 24, 1, ST_D); R(23, 5, 10, 3, WD); R(23, 5, 10, 1, WD_H); P(25, 6, WD_D); P(27, 6, WD_D); P(29, 6, WD_D); P(31, 6, WD_D);
    R(11, 3, 7, 5, rgb(104, 110, 130)); R(11, 3, 7, 1, ST_H); R(2, 5, 9, 3, WD); R(1, 6, 2, 3, WD); R(4, 4, 7, 1, WD_H); R(2, 8, 7, 1, WD_D); R(13, 8, 3, 1, ST_D); P(14, 7, K); finish();
    begin(IT_RIFLE, 5, 1);
    R(28, 5, 30, 1, GM); R(28, 4, 18, 1, GM_H); R(24, 6, 14, 2, WD); R(24, 6, 14, 1, WD_H); R(17, 4, 11, 3, GM); R(17, 4, 11, 1, GM_H);
    R(18, 1, 13, 2, GM_D); P(18, 1, rgb(130, 190, 216)); P(30, 1, rgb(130, 190, 216)); P(21, 3, GM); P(27, 3, GM); P(24, 5, ST_H); P(25, 6, ST_H);
    R(4, 5, 13, 3, WD); R(2, 6, 3, 4, WD); R(6, 4, 11, 1, WD_H); R(4, 8, 9, 1, WD_D); R(19, 7, 3, 1, ST_D); P(20, 8, ST_D); finish();
    begin(IT_SMG, 3, 1);
    R(8, 2, 19, 4, GM); R(8, 2, 19, 1, GM_H); R(27, 3, 6, 2, GM_D); P(33, 3, ST_D); R(16, 6, 3, 5, GM_D); P(16, 6, GM); R(10, 6, 3, 4, rgb(86, 70, 60)); R(23, 6, 2, 3, GM_D);
    R(1, 3, 7, 1, ST_D); R(1, 3, 1, 4, ST_D); R(1, 6, 3, 1, ST_D); P(12, 1, ST_D); R(19, 3, 4, 1, GM_D); finish();
    // ---- melee
    begin(IT_KNIFE, 2, 1);
    R(10, 4, 9, 1, rgb(244, 246, 252)); R(10, 5, 12, 1, rgb(214, 218, 228)); R(10, 6, 10, 1, rgb(152, 158, 174)); R(9, 3, 1, 5, ST_D); R(3, 4, 6, 3, rgb(96, 68, 50)); R(3, 4, 6, 1, rgb(130, 96, 70)); P(5, 5, GOLD); P(7, 5, GOLD); P(2, 5, ST_D); finish();
    begin(IT_BAT, 4, 1);
    {   const px B = rgb(210, 174, 122), BH = rgb(236, 206, 156), BD = rgb(162, 126, 86);
        R(3, 5, 14, 2, B); R(1, 4, 2, 4, B); R(17, 4, 8, 4, B); R(25, 3, 20, 6, B); R(45, 4, 1, 4, B);
        R(3, 5, 14, 1, BH); R(17, 4, 8, 1, BH); R(25, 3, 20, 1, BH); R(25, 8, 20, 1, BD); R(17, 7, 8, 1, BD); R(3, 6, 14, 1, BD);
        for (int x = 5; x <= 11; x += 3) { P(x, 5, WHITE); P(x, 6, rgb(190, 190, 186)); }
        P(34, 5, BD); P(35, 5, BD); P(36, 6, BD); }
    finish();
    begin(IT_CROWBAR, 3, 1);
    {   const px C = rgb(74, 106, 168), CH = rgb(128, 158, 216), CD = rgb(48, 72, 124);
        R(5, 5, 24, 2, C); R(5, 5, 24, 1, CH); R(3, 3, 2, 3, C); P(4, 2, C); P(5, 2, C); P(6, 3, C); P(3, 3, CH);
        R(29, 5, 3, 2, C); R(31, 6, 2, 3, C); P(33, 8, C); P(32, 9, CD); R(5, 6, 24, 1, CD); }
    finish();
    begin(IT_MACHETE, 3, 1);
    R(11, 3, 20, 4, rgb(210, 216, 226)); R(11, 3, 20, 1, rgb(244, 246, 252)); R(22, 2, 10, 1, rgb(210, 216, 226)); R(31, 3, 2, 3, rgb(210, 216, 226)); P(33, 4, rgb(210, 216, 226)); R(11, 6, 21, 1, rgb(150, 156, 174));
    R(10, 2, 1, 6, ST_D); R(2, 4, 8, 3, rgb(72, 62, 60)); R(2, 4, 8, 1, rgb(108, 96, 92)); P(4, 5, GOLD); P(7, 5, GOLD); finish();
    // ---- ammunition: a carton with three rounds standing on it, colour-coded by calibre
    static const px AMMO[4] = {rgb(214, 170, 60), rgb(70, 150, 196), rgb(196, 60, 52), rgb(120, 140, 72)};
    for (int i = 0; i < 4; i++) {
        begin(IT_AMMO_9MM + i, 1, 1); px c = AMMO[i];
        R(2, 4, 8, 6, c); R(2, 4, 8, 1, lt(c, 90)); R(2, 9, 8, 1, dk(c, 180)); R(9, 5, 1, 5, dk(c, 200)); R(3, 6, 5, 2, WHITE); P(4, 7, rgb(100, 100, 110)); P(6, 7, rgb(100, 100, 110));
        for (int b = 0; b < 3; b++) { R(3 + b * 2, 2, 1, 2, GOLD); P(3 + b * 2, 1, rgb(206, 126, 76)); }
        finish();
    }
    // ---- medical
    begin(IT_BANDAGE, 1, 1);
    R(2, 4, 8, 5, rgb(238, 238, 230)); R(2, 4, 8, 1, rgb(255, 255, 250)); R(2, 8, 8, 1, rgb(196, 196, 190)); R(7, 5, 3, 3, rgb(214, 214, 206)); P(8, 6, rgb(236, 150, 150)); R(1, 9, 3, 1, rgb(238, 238, 230));
    R(4, 5, 1, 3, RED); R(3, 6, 3, 1, RED); finish();
    begin(IT_PILLS, 1, 1);
    R(3, 4, 6, 6, rgb(232, 138, 54)); R(3, 1, 6, 3, WHITE); R(3, 3, 6, 1, rgb(190, 190, 186)); R(3, 6, 6, 2, rgb(246, 242, 226)); P(4, 7, rgb(120, 120, 126)); P(6, 7, rgb(120, 120, 126)); P(4, 5, rgb(252, 186, 116)); R(8, 4, 1, 6, rgb(186, 100, 40)); finish();
    begin(IT_ANTIBIOTICS, 1, 1);
    R(2, 3, 8, 7, rgb(236, 240, 244)); R(2, 3, 8, 1, WHITE); R(2, 6, 8, 2, rgb(64, 130, 200)); R(9, 4, 1, 6, rgb(190, 196, 206)); R(4, 4, 4, 1, RED); R(3, 9, 3, 1, rgb(96, 190, 120)); R(7, 9, 2, 1, rgb(96, 190, 120)); finish();
    begin(IT_MEDKIT, 2, 2);
    R(2, 7, 20, 14, RED); R(2, 7, 20, 3, RED_H); R(2, 19, 20, 2, RED_D); R(20, 10, 2, 9, RED_D); R(8, 3, 8, 2, GM); R(8, 5, 2, 2, GM); R(14, 5, 2, 2, GM); R(8, 3, 8, 1, GM_H);
    R(10, 11, 4, 8, WHITE); R(8, 13, 8, 4, WHITE); R(10, 18, 4, 1, rgb(196, 196, 190)); P(5, 10, GOLD); P(18, 10, GOLD); finish();
    // ---- food
    begin(IT_CANNED, 1, 1);
    R(3, 3, 6, 7, rgb(190, 196, 206)); R(3, 2, 6, 2, rgb(228, 232, 238)); R(3, 5, 6, 3, rgb(204, 62, 52)); R(3, 6, 6, 1, rgb(246, 206, 80)); R(8, 3, 1, 7, rgb(140, 146, 160)); P(4, 2, WHITE); finish();
    begin(IT_MRE, 1, 2);
    R(2, 2, 8, 20, rgb(186, 160, 114)); R(2, 2, 8, 2, rgb(150, 126, 88)); R(3, 4, 1, 17, rgb(210, 186, 140)); R(9, 4, 1, 18, rgb(150, 126, 88));
    R(4, 8, 5, 1, rgb(96, 84, 60)); R(4, 10, 4, 1, rgb(96, 84, 60)); R(4, 12, 5, 1, rgb(96, 84, 60)); R(2, 15, 8, 3, rgb(106, 122, 58)); R(2, 15, 8, 1, rgb(140, 156, 84)); finish();
    // ---- materials
    begin(IT_SCRAP, 2, 1);
    spr_circle(*S, 6, 6, 3, rgb(140, 146, 162)); P(6, 6, K); P(5, 4, ST_H); P(6, 2, rgb(140, 146, 162)); P(6, 10, rgb(140, 146, 162)); P(2, 6, rgb(140, 146, 162)); P(10, 6, rgb(140, 146, 162));
    R(11, 7, 10, 2, ST); R(11, 7, 10, 1, ST_H); R(13, 3, 8, 3, rgb(122, 126, 142)); R(13, 3, 8, 1, ST_H); P(15, 4, rgb(168, 100, 62)); P(18, 5, rgb(168, 100, 62)); P(20, 8, rgb(168, 100, 62)); P(14, 8, ST_D); finish();
    begin(IT_PLANKS, 3, 1);
    R(2, 2, 32, 2, WD); R(1, 5, 33, 2, rgb(196, 146, 94)); R(3, 8, 31, 2, WD); R(2, 2, 32, 1, WD_H); R(1, 5, 33, 1, WD_H); R(3, 8, 31, 1, WD_H);
    for (int x = 6; x < 32; x += 7) { P(x, 3, WD_D); P(x + 3, 6, WD_D); P(x + 1, 9, WD_D); }
    P(32, 2, rgb(232, 204, 160)); P(33, 5, rgb(232, 204, 160)); P(33, 8, rgb(232, 204, 160)); P(4, 2, ST_D); P(30, 9, ST_D); finish();
    begin(IT_TOOLS, 2, 1);
    R(2, 4, 20, 7, RED); R(2, 4, 20, 1, RED_H); R(2, 6, 20, 1, RED_D); R(2, 10, 20, 1, RED_D); R(8, 1, 8, 2, GM); R(8, 1, 8, 1, GM_H); P(8, 3, GM); P(15, 3, GM); R(11, 6, 2, 2, GOLD); R(20, 5, 2, 5, RED_D); finish();
    begin(IT_FUEL, 2, 2);
    R(4, 5, 16, 17, RED); R(6, 3, 10, 2, RED); R(4, 5, 2, 17, RED_H); R(6, 3, 10, 1, RED_H); R(18, 6, 2, 16, RED_D); R(4, 20, 16, 2, RED_D);
    R(15, 1, 4, 3, GOLD); P(15, 1, lt(GOLD, 80)); R(7, 5, 6, 1, K);
    for (int i = 0; i < 10; i++) { P(7 + i, 9 + i, RED_D); P(16 - i, 9 + i, RED_D); }
    finish();
    // ---- keys
    begin(IT_KEYCARD, 1, 1);
    R(2, 3, 8, 6, rgb(236, 238, 242)); R(2, 4, 8, 1, rgb(64, 120, 200)); R(3, 6, 2, 2, GOLD); R(6, 7, 3, 1, rgb(150, 150, 164)); R(2, 8, 8, 1, rgb(190, 194, 204)); finish();
    begin(IT_CELLKEY, 1, 1);
    R(2, 3, 4, 1, GOLD); R(2, 6, 4, 1, rgb(176, 138, 46)); R(2, 4, 1, 2, GOLD); R(5, 4, 1, 2, rgb(176, 138, 46)); R(6, 5, 5, 1, GOLD); P(8, 6, rgb(176, 138, 46)); P(10, 6, rgb(176, 138, 46)); P(2, 3, lt(GOLD, 90)); finish();
    // ---- backpacks
    for (int v = 0; v < 2; v++) {
        begin(v ? IT_PACK_L : IT_PACK_M, 2, 2); px c = v ? rgb(152, 106, 66) : rgb(110, 128, 62), strap = v ? rgb(96, 64, 42) : rgb(84, 62, 44);
        R(5, 6, 14, 15, c); R(6, 4, 12, 3, lt(c, 50)); R(5, 6, 14, 2, lt(c, 50)); R(17, 8, 2, 13, dk(c, 196)); R(5, 19, 14, 2, dk(c, 180));
        R(8, 13, 8, 6, dk(c, 216)); R(8, 13, 8, 1, lt(c, 36)); P(11, 15, GOLD); P(12, 15, GOLD);
        R(3, 8, 2, 10, strap); R(19, 8, 2, 10, strap); R(10, 2, 4, 2, strap); P(11, 9, GOLD); P(12, 9, GOLD);
        if (v) { R(2, 12, 3, 7, dk(c, 216)); R(19, 12, 3, 7, dk(c, 200)); R(6, 1, 12, 3, rgb(124, 136, 156)); R(6, 1, 12, 1, rgb(170, 180, 198)); }
        finish();
    }
    // ---- settlement equipment
    begin(IT_GENERATOR, 3, 3);
    {   const px Y = rgb(228, 182, 62), YH = rgb(250, 220, 120), YD = rgb(168, 126, 40);
        R(3, 8, 30, 22, Y); R(3, 8, 30, 2, YH); R(3, 28, 30, 2, YD); R(31, 10, 2, 18, YD);
        R(6, 12, 12, 13, GM); for (int y = 13; y < 24; y += 2) R(7, y, 10, 1, GM_H); R(6, 12, 12, 1, GM_H);
        R(20, 11, 10, 6, RED); R(20, 11, 10, 1, RED_H); P(24, 9, GM); P(25, 9, GM); R(23, 10, 4, 1, GM);
        R(20, 19, 10, 8, rgb(38, 40, 54)); P(22, 21, rgb(154, 228, 64)); P(22, 24, rgb(240, 80, 70)); R(25, 21, 3, 2, WHITE); R(25, 24, 3, 2, rgb(120, 126, 140));
        R(3, 5, 30, 2, GM); R(3, 5, 30, 1, GM_H); R(4, 6, 2, 3, GM); R(30, 6, 2, 3, GM);
        spr_circle(*S, 9, 31, 3, rgb(30, 30, 40)); spr_circle(*S, 27, 31, 3, rgb(30, 30, 40)); P(9, 31, ST); P(27, 31, ST); }
    finish();
    begin(IT_MEDMACHINE, 3, 2);
    R(2, 3, 32, 18, rgb(210, 210, 202)); R(2, 3, 32, 2, WHITE); R(2, 19, 32, 2, rgb(160, 160, 156)); R(32, 5, 2, 14, rgb(170, 170, 166));
    R(5, 6, 17, 11, rgb(22, 48, 44)); R(5, 6, 17, 1, rgb(14, 30, 30));
    {   static const int W[15] = {0, 0, 0, -1, -4, 3, 1, 0, 0, 0, -1, -4, 3, 1, 0};
        for (int x = 0; x < 15; x++) { P(6 + x, 11 + W[x], rgb(126, 240, 146)); if (W[x] < -1) P(6 + x, 12 + W[x], rgb(70, 170, 100)); } }
    R(25, 6, 6, 3, RED); R(25, 6, 6, 1, RED_H); R(25, 11, 6, 3, rgb(64, 120, 200)); R(25, 11, 6, 1, rgb(120, 170, 236)); spr_circle(*S, 28, 17, 1, GM); R(6, 21, 3, 1, GM); R(27, 21, 3, 1, GM);
    finish();
}
