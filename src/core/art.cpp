// All game art is original and generated here at load time: hand-drawn pixel
// templates (palette letters, see pal_char) plus procedural tiles/props.
// tools/zs_tool --gallery exports everything to assets/gallery/*.png for review.
#include "art.h"
#include "rng.h"
#include "../gameplay/content.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Sprite g_tile[TA_COUNT][4];
Sprite g_spr[S_COUNT];
Sprite g_item[40];
Sprite g_portrait[PORTRAITS];
Sprite g_building[BLD_TYPES][BLD_LEVELS];

// [variant][storedDir 0..4][pose]
static Sprite survPose[SURV_VARIANTS][5][P_COUNT];
static Sprite zombPose[ZOMB_VARIANTS][5][P_COUNT];

#define ROWS(name, ...) static const char* const name[] = {__VA_ARGS__}
#define PARSE(name) spr_parse(name, (int)(sizeof(name) / sizeof(name[0])))
static void hline(Sprite& s, int x0, int x1, int y, px c);
static void dline(Sprite& s, int x0, int y0, int x1, int y1, px c);
static void outlineAll(Sprite& s);
static void vline(Sprite& s, int x, int y0, int y1, px c);

// ---------------------------------------------------------------- characters (16x24 chibi, Try 2 style)
// Built from hand-drawn parts so every animation frame is drawn, not smeared:
// head (rows 0-13) + torso/arms (6 rows) + legs (4 rows).  Views: 0 = facing camera, 1 = side (right), 2 = back.
// Letters: k outline, h/H/j hair (base/light/dark), s/u/S skin (base/light/shade), e eyes, b/M/m shirt,
// B belt, n/p pants, x shoes, o/O backpack, y badge.
enum { CW = 16, CH = 24 };
ROWS(HD_FRONT,
"................", "....kkkkkkkk....", "..kkHHHHhhhhkk..", ".kHHHhhhhhhhhjk.", ".kHhhhhhhhhhhjk.", ".khhhhhhhhhhhjk.",
".khhjhhhhhjhhjk.", ".kjhhshhhhshhjk.", ".kjsjjssssjjsjk.", ".kSseesssseeSSk.", ".kSseesssseeSSk.", ".kssssssSssssSk.",
"..kSsssSSsssSk..", "...kkSSSSSSkk...");   // dark 2x2 eyes under heavy brows, nose + jaw shading (Try 2)
ROWS(HD_SIDE,
"................", "....kkkkkkkk....", "..kkHHHHhhhhkk..", ".kHHHhhhhhhhhhk.", ".kHhhhhhhhhhhhk.", ".khhhhhhhhhhhjk.",
".khhhhhjhhhhjjk.", ".kjhhhhshhhshjk.", ".kjhhhsjjssjjsk.", ".kjhSsseesseeSk.", ".kjhSsseesseeSk.", ".kSSssssssSsssk.",
"..kSSssssSSssk..", "...kkSSSSSSkk...");   // 3/4 view: face turned right, back of the head on the left
ROWS(HD_BACK,
"................", "....kkkkkkkk....", "..kkhhhhhhhhkk..", ".kHHhhhhhhhhhjk.", ".kHhhhhhhhhhhjk.", ".khhhhhhhhhhhjk.",
".khhhhhhhhhhjjk.", ".kjhhhhhhhhhjjk.", ".kjhhhhhhhhhjjk.", ".kjjhhhhhhhjjjk.", ".kSjjjjjjjjjjSk.", ".kSjjjjjjjjjjSk.",
"..kSssssssssSk..", "...kkSSSSSSkk...");
// torso + arms, 6 rows (drawn at row 14): [view][arm state]. Stocky Try 2 build: shoulders as wide as the
// head, thick arms hanging slightly out, big hands at the hips.
enum { A_IDLE, A_SWING_A, A_SWING_B, A_AIM, A_REACH, A_ZOMBIE, A_COUNT };
static const char* const TORSO[3][A_COUNT][6] = {
  { // front
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kbMkbbbbbbbbkbmk", "kbmkbbbbbbbbkbmk", "kuskBBBBYBBBksSk", "kkkkknnnnnnkkkkk"},
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kbMkbbbbbbbbkbmk", "kbmkbbbbbbbbksSk", "kuskBBBBYBBBkkkk", "kkkkknnnnnnk...."},
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kbMkbbbbbbbbkbmk", "kuskbbbbbbbbkbmk", "kkkkBBBBYBBBksSk", "....knnnnnnkkkkk"},
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kbMmbbbbbbbbmbmk", "kbmkmbbbbbbmkbmk", "kbmkBBuussBBkbmk", "kkkkknkSSknkkkkk"},
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kbMmbbbbbbbbmbmk", "kbmkmbbbbbbmkbmk", "kbmkBBuussBBkbmk", "kkkkknkSSknkkkkk"},
    {".kkMMbbssbbbbkk.", "kMMbbbbmmbbbbbmk", "kuskbbbbbbbbkSuk", "kkkkbbbbbbbbkkkk", "...kBBBBYBBBk...", "...kknnnnnnkk..."},
  },
  { // side (3/4, facing right): body toward the camera, the near arm does the work
    {".kkMMbbbssbbbkk.", "kMMbbbbbmmbbbbmk", "kbMkbbbbbbbbkbmk", "kbmkbbbbbbbbkbmk", "kuskBBBBYBBBksSk", "kkkkknnnnnnkkkkk"},
    {".kkMMbbbssbbbkk.", "kMMbbbbbmmbbbbmk", "kbMkbbbbbbbbkmmk", "kbmkbbbbbbbbbkmk", "kuskBBBBYBBBBkus", "kkkkknnnnnnkk.kk"},
    {".kkMMbbbssbbbkk.", "kMMbbbbbmmbbbbmk", "kbMkbbbbbbbbkbmk", "kbmkbbbbbbbbkbbk", "kuskBBBBYBBBkkk.", "kkkkknnnnnnkk..."},
    {".kkMMbbbssbbbkk.", "kMMbbbbmmbbbbbkk", "kbMkbbbbbbbbmmus", "kbmkbbbbbbbbkkkk", "kuskBBBBYBBBk...", "kkkkknnnnnnk...."},
    {".kkMMbbbssbbbkk.", "kMMbbbbbmmbbbbmk", "kbMkbbbbbbbbkbmk", "kbmkbbbbbbbbkmmk", "kuskBBBBYBBBkkus", "kkkkknnnnnnkk.kk"},
    {".kkMMbbbssbbbkk.", "kMMbbbbmmbbbbbkk", "kbMbbbbbbbbbmmus", "kbmbbbbbbbbbkkkk", "kbmkBBBBYBBBmmus", "kkkkknnnnnnkkkkk"},
  },
  { // back
    {".kkbbbbbbbbbbkk.", "kbbbkoooooOkbbmk", "kbbbkoOOOOOkbbmk", "kbmbkoooooOkbmmk", "kuskkOOOOOOkksSk", "kkkkknnnnnnkkkkk"},
    {".kkbbbbbbbbbbkk.", "kbbbkoooooOkbbmk", "kbbbkoOOOOOkbbmk", "kbmbkoooooOksSmk", "kuskkOOOOOOkkkkk", "kkkkknnnnnnk...."},
    {".kkbbbbbbbbbbkk.", "kbbbkoooooOkbbmk", "kbbbkoOOOOOkbbmk", "kuSbkoooooOkbmmk", "kkkkkOOOOOOkksSk", "....knnnnnnkkkkk"},
    {".kkbbbbbbbbbbkk.", "kbbbkoooooOkbbbk", "kbmbkoOOOOOkbmbk", "kbmbkoooooOkbmbk", ".kkkkOOOOOOkkkk.", "....knnnnnnk...."},
    {".kkbbbbbbbbbbkk.", "kbbbkoooooOkbbbk", "kbmbkoOOOOOkbmbk", "kbmbkoooooOkbmbk", ".kkkkOOOOOOkkkk.", "....knnnnnnk...."},
    {".kkbbbbbbbbbbkk.", "kbbbbbbbbbbbbbmk", "kuskbbbbbbbbkSuk", "kkkkbbbbbbbbkkkk", "...kBBBBBBBBk...", "...kknnnnnnkk..."},
  },
};
// legs, 4 rows (drawn at row 20): [view][leg state]; short legs on big dark boots
enum { L_STAND, L_STEP_A, L_STEP_B, L_RUN_A, L_RUN_B, L_CROUCH, L_COUNT };
static const char* const LEGS[3][L_COUNT][4] = {
  { // front
    {"....knnnnnnk....", "....knnkknnk....", "...kxxxkkxxxk...", "...kkkkkkkkkk..."},
    {"....knnnnnnk....", "....knnkkxxxk...", "...kxxxkkkkk....", "...kkkkk........"},
    {"....knnnnnnk....", "...kxxxkknnk....", "....kkkkkxxxk...", "........kkkkk..."},
    {"....knnnnnnk....", "...knnpkkxxxk...", "..kxxxkkkkk.....", "..kkkkk........."},
    {"....knnnnnnk....", "...kxxxkknpnk...", ".....kkkkkxxxk..", ".........kkkkk.."},
    {"................", "................", "...knnnnnnnnk...", "..kxxxkkkkxxxk.."},
  },
  { // side
    {"....knnnnnnk....", "....knnkknnk....", "...kxxxkkxxxxk..", "...kkkkkkkkkkk.."},
    {"....knnnnnnk....", "...kpppk.knnnk..", "..kxxxk...kxxxxk", "..kkkkk...kkkkkk"},
    {"....knnnnnnk....", "...knnnk.kpppk..", "..kxxxk...kxxxxk", "..kkkkk...kkkkkk"},
    {"...knnnnnnnk....", "..kpppk..knnnk..", ".kxxxk.....kxxxk", ".kkkk.......kkkk"},
    {"....knnnnnnnk...", "...knnnk..kpppk.", "..kxxxk....kxxxk", "..kkkk......kkkk"},
    {"................", "................", "....knnnnnnnk...", "..kxxxxkkkxxxxk."},
  },
  { // back
    {"....knnnnnnk....", "....kpnkkpnk....", "...kxxxkkxxxk...", "...kkkkkkkkkk..."},
    {"....knnnnnnk....", "....kpnkkxxxk...", "...kxxxkkkkk....", "...kkkkk........"},
    {"....knnnnnnk....", "...kxxxkkpnk....", "....kkkkkxxxk...", "........kkkkk..."},
    {"....knnnnnnk....", "...kpnnkkxxxk...", "..kxxxkkkkk.....", "..kkkkk........."},
    {"....knnnnnnk....", "...kxxxkkpnnk...", ".....kkkkkxxxk..", ".........kkkkk.."},
    {"................", "................", "...knnnnnnnnk...", "..kxxxkkkkxxxk.."},
  },
};

static void stamp(Sprite& s, const char* const* rows, int n, int ox, int oy) {
    for (int y = 0; y < n; y++) for (int x = 0; rows[y][x]; x++) {
        px v = pal_char(rows[y][x]);
        if (v != KEYC) spr_set(s, x + ox, y + oy, v);
    }
}
static void freeS(Sprite& s) { if (s.d) { free(s.d); s.d = nullptr; } }

// one frame: legs, then torso and head lowered by `drop`, upper body shifted by `lean`, head by `turn`
static Sprite compose(const Sprite& head, int view, int arm, int leg, int drop, int lean, int turn) {
    Sprite s = spr_new(CW, CH);
    stamp(s, LEGS[view][leg], 4, 0, 20);
    stamp(s, TORSO[view][arm], 6, lean, 14 + drop);
    for (int y = 0; y < head.h; y++) for (int x = 0; x < head.w; x++) {
        px v = spr_get(head, x, y);
        if (v != KEYC) spr_set(s, x + lean + turn, y + drop, v);
    }
    return s;
}

struct CharPal { px hair, skin, skinS, shirt, pants; uint8_t style; };   // style: 0 short hair, 1 cap, 2 long hair
static const CharPal SURV_PAL[SURV_VARIANTS] = {
    {rgb(38, 46, 82), rgb(232, 182, 140), rgb(186, 128, 92), rgb(50, 66, 112), rgb(30, 36, 58), 1},     // police officer: navy cap + uniform
    {rgb(112, 62, 34), rgb(236, 190, 150), rgb(190, 136, 100), rgb(176, 64, 52), rgb(84, 76, 70), 0},   // brown hair, red shirt
    {rgb(64, 64, 70), rgb(232, 182, 140), rgb(186, 128, 92), rgb(70, 104, 62), rgb(36, 48, 78), 1},     // grey cap, green shirt
    {rgb(44, 78, 156), rgb(232, 182, 140), rgb(186, 128, 92), rgb(70, 104, 62), rgb(36, 48, 78), 1},    // blue cap, green shirt
    {rgb(214, 172, 86), rgb(236, 190, 150), rgb(190, 136, 100), rgb(68, 108, 160), rgb(96, 72, 50), 2}, // blonde, blue shirt
    {rgb(34, 28, 28), rgb(206, 154, 110), rgb(160, 108, 76), rgb(122, 44, 52), rgb(84, 94, 54), 0},
    {rgb(26, 22, 22), rgb(132, 88, 60), rgb(96, 62, 42), rgb(190, 156, 64), rgb(78, 78, 86), 0},
    {rgb(150, 72, 38), rgb(236, 190, 150), rgb(190, 136, 100), rgb(52, 120, 122), rgb(40, 50, 82), 2},
};
static const CharPal ZOMB_PAL[ZOMB_VARIANTS] = {   // pale, blood-soaked (Try 2)
    {rgb(58, 52, 50), rgb(204, 196, 186), rgb(150, 138, 134), rgb(176, 168, 160), rgb(64, 60, 66), 0},
    {rgb(40, 38, 38), rgb(186, 192, 172), rgb(132, 140, 122), rgb(118, 122, 112), rgb(70, 62, 54), 0},
    {rgb(90, 74, 60), rgb(214, 204, 196), rgb(160, 146, 142), rgb(150, 120, 108), rgb(52, 56, 70), 2},
    {rgb(30, 30, 32), rgb(196, 184, 180), rgb(142, 126, 124), rgb(98, 104, 122), rgb(60, 58, 54), 0},
};

static Sprite parseHead(const char* const* rows, int view, int style) {
    Sprite h = spr_parse(rows, 14);
    px H = pal_char('h'), J = pal_char('j'), L = pal_char('H'), k = pal_char('k');
    if (style == 1) {   // cap: crown in hair colours, brim across the forehead
        if (view == 0) {
            for (int x = 2; x < 16; x++) spr_set(h, x, 7, J);                 // brim juts out to the right (Try 2)
            for (int x = 3; x < 15; x++) spr_set(h, x, 8, spr_get(h, x, 8) == pal_char('s') ? pal_char('S') : spr_get(h, x, 8));   // shadow under the brim
            spr_set(h, 1, 7, k); spr_set(h, 15, 6, k); for (int x = 13; x < 16; x++) spr_set(h, x, 6, x == 15 ? k : J);
            spr_set(h, 4, 3, L); spr_set(h, 5, 3, L); spr_set(h, 4, 4, L);   // crown highlight
        }
        if (view == 1) { for (int x = 9; x < 15; x++) spr_set(h, x, 7, J); spr_set(h, 15, 7, k); spr_set(h, 15, 8, KEYC); for (int x = 9; x < 14; x++) spr_set(h, x, 6, H); }
        if (view == 2) for (int x = 2; x < 14; x++) spr_set(h, x, 9, J);
    } else if (style == 2) {   // long hair down the sides
        for (int y = 7; y <= 12; y++) {
            if (view == 0) { spr_set(h, 2, y, L); spr_set(h, 13, y, J); spr_set(h, 1, y, k); spr_set(h, 14, y, k); }
            if (view == 1) { spr_set(h, 3, y, H); spr_set(h, 4, y, J); spr_set(h, 2, y, k); }
            if (view == 2) for (int x = 2; x < 14; x++) if (spr_get(h, x, y) != k) spr_set(h, x, y, x < 5 ? L : (x > 11 ? J : H));
        }
    }
    return h;
}

static Sprite recolorChar(const Sprite& base, const CharPal& p, bool zombie) {
    Sprite s = spr_copy(base);
    const px from[11] = {pal_char('h'), pal_char('H'), pal_char('j'), pal_char('s'), pal_char('u'), pal_char('S'), pal_char('b'), pal_char('M'), pal_char('m'), pal_char('n'), pal_char('p')};
    px to[11] = {p.hair, gfx_mix(p.hair, rgb(255, 255, 255), 70), gfx_scale(p.hair, 165), p.skin, gfx_mix(p.skin, rgb(255, 255, 255), 80), p.skinS,
                 p.shirt, gfx_mix(p.shirt, rgb(255, 255, 255), 60), gfx_scale(p.shirt, 170), p.pants, gfx_scale(p.pants, 160)};
    spr_recolor(s, from, to, 11);
    { const px f2[3] = {pal_char('e'), pal_char('w'), pal_char('i')};
      const px t2[3] = {zombie ? rgb(30, 10, 12) : pal_char('e'), zombie ? rgb(150, 30, 28) : pal_char('w'), zombie ? p.skinS : gfx_mix(p.skin, rgb(232, 96, 90), 80)};
      spr_recolor(s, f2, t2, 3); }   // eyes: zombies get a dull red glint, survivors rosy cheeks
    return s;
}

// zombie wounds: gaping bloody mouth, blood down the chin and shirt, torn clothes, patchy hair
static void zombify(Sprite& s, const CharPal& p, int view, int drop, uint32_t seed) {
    Rng r(seed);
    px blood = rgb(168, 26, 26), dried = rgb(104, 18, 20), mouth = rgb(70, 8, 10);
    if (view == 0) {
        bool slit = (seed / 77) % 4 == 1;   // one variant has a black slit mouth instead of a bloody one (Try 2)
        for (int y = 11; y <= 12; y++) for (int x = 6; x <= 9; x++) spr_set(s, x, y + drop, slit ? (y == 12 ? rgb(24, 14, 14) : p.skinS) : ((x == 6 || x == 9) ? blood : mouth));
        for (int y = 3; y <= 6; y++) for (int x = 2; x <= 4 - (y > 4); x++) if (spr_get(s, x, y + drop) != pal_char('k')) spr_set(s, x, y + drop, (x == 2 || y == 3) ? dried : blood);   // solid smear on the scalp
    }
    if (view == 1) { spr_set(s, 9, 11 + drop, mouth); spr_set(s, 10, 11 + drop, mouth); spr_set(s, 9, 12 + drop, blood); spr_set(s, 10, 12 + drop, blood); spr_set(s, 10, 13 + drop, dried); }
    px shirt = p.shirt, shirtD = gfx_scale(p.shirt, 170), shirtL = gfx_mix(p.shirt, rgb(255, 255, 255), 60);
    for (int i = 0; i < s.w * s.h; i++) {
        px& v = s.d[i];
        if (v == gfx_mix(p.hair, rgb(255, 255, 255), 70)) v = p.skin;                      // balding: pale scalp, a few dark strands
        else if (v == p.hair) v = r.chance(0.07f) ? p.hair : gfx_mix(p.skin, p.skinS, 128);
        else if (v == gfx_scale(p.hair, 165)) v = r.chance(0.15f) ? p.hair : p.skinS;
        else if (v == shirt || v == shirtD || v == shirtL) { if (r.chance(0.05f)) v = p.skinS; else if (r.chance(0.03f)) v = dried; }   // a few tears
        else if (v == p.pants && r.chance(0.03f)) v = dried;
    }
    if (view == 0) {

        spr_set(s, 10, 6 + drop, blood); spr_set(s, 10, 7 + drop, dried); spr_set(s, 11, 7 + drop, blood);   // scalp gash
    }
    if (view == 1) for (int y = 14; y <= 17; y++) if (spr_get(s, 10, y + drop) != KEYC) spr_set(s, 10, y + drop, blood);
    for (int k = 0; k < 3; k++) {   // long blood streaks soaking down the clothes (Try 2), not speckle
        int sx = 4 + (int)(hash32(seed * 3 + k * 11) % 8), len = 3 + (int)(hash32(seed + k * 7) % 4);
        for (int y = 0; y < len; y++) { int yy = 14 + drop + y; px c = spr_get(s, sx, yy); if (c != KEYC && c != pal_char('k')) spr_set(s, sx, yy, y < len - 1 ? blood : dried); }
        px c2 = spr_get(s, sx + 1, 14 + drop); if (c2 != KEYC && c2 != pal_char('k')) spr_set(s, sx + 1, 14 + drop, dried);
    }
}

static void buildCharacters() {
    const char* const* HD[3] = {HD_FRONT, HD_SIDE, HD_BACK};
    static const int VIEW[5] = {0, 0, 1, 2, 2}, TURN[5] = {0, 1, 0, 1, 0};
    static const int FD[5] = {0, 1, 1, 1, 0};   // forward x (for leans / runs)
    for (int z = 0; z < 2; z++) {
        int nv = z ? ZOMB_VARIANTS : SURV_VARIANTS;
        for (int v = 0; v < nv; v++) for (int d = 0; d < 5; d++) {
            const CharPal& cp = z ? ZOMB_PAL[v] : SURV_PAL[v];
            int view = VIEW[d], fdx = FD[d], t = TURN[d], zd = z ? 1 : 0;   // zombies hunch: head + torso one row lower
            Sprite head = parseHead(HD[view], view, cp.style);
            int idle = (z && view == 1) ? A_ZOMBIE : A_IDLE;   // zombies reach forward in profile, arms hang when facing the camera
            Sprite raw[P_COUNT]; int drop[P_COUNT];
            struct Def { int p, arm, leg, drop, lean, turn; };
            const Def defs[] = {
                {P_STAND, idle, L_STAND, zd, 0, t}, {P_BREATHE, idle, L_STAND, zd + 1, 0, t},
                {P_WALK_A, (z && view == 1) ? A_ZOMBIE : A_SWING_A, L_STEP_A, zd, 0, t}, {P_WALK_B, (z && view == 1) ? A_ZOMBIE : A_SWING_B, L_STEP_B, zd, 0, t},
                {P_RUN_A, (z && view == 1) ? A_ZOMBIE : A_SWING_A, L_RUN_A, zd, fdx, t}, {P_RUN_B, (z && view == 1) ? A_ZOMBIE : A_SWING_B, L_RUN_B, zd, fdx, t},
                {P_CROUCH, idle, L_CROUCH, 2, 0, t}, {P_REACH, A_REACH, L_CROUCH, 2, fdx, t},
                {P_LEAN_BACK, idle, L_STAND, zd, -fdx, t + (view == 1 ? -1 : 0)}, {P_LEAN_FWD, idle, view == 1 ? L_STEP_A : L_STAND, zd + 1, fdx, t},
                {P_HURT, idle, L_STAND, zd, -fdx, t}, {P_FALL, idle, L_CROUCH, 4, -fdx, t},
                {P_DEAD, idle, L_STAND, 0, 0, 0}, {P_LOOK, idle, L_STAND, zd, 0, view == 1 ? -1 : (t ? 0 : 1)},
                {P_AIM, z ? idle : A_AIM, L_STAND, zd, 0, t},
            };
            for (const Def& df : defs) { raw[df.p] = compose(head, view, df.arm, df.leg, df.drop, df.lean, df.turn); drop[df.p] = df.drop; }
            freeS(head);
            Sprite* out = z ? zombPose[v][d] : survPose[v][d];
            for (int p = 0; p < P_COUNT; p++) {
                Sprite c = recolorChar(raw[p], cp, z != 0);
                if (!z && v == 0 && cp.style == 1 && view != 2 && p != P_DEAD && p != P_LEAN_BACK && p != P_LEAN_FWD && p != P_HURT && p != P_FALL && p != P_RUN_A && p != P_RUN_B && p != P_REACH) spr_set(c, view == 0 ? 10 : 11, 15 + drop[p], pal_char('y'));   // chest badge
                if (z) zombify(c, cp, view, drop[p], 1234 + v * 77 + d * 7);
                freeS(raw[p]);
                if (p == P_HURT) for (int i = 0; i < CW * CH; i++) if (c.d[i] != KEYC) c.d[i] = gfx_mix(c.d[i], pal::cream, 90);
                if (p == P_DEAD) {   // lying body (24x16), darkened
                    Sprite rr = spr_rot90(c); freeS(c); c = rr;
                    for (int i = 0; i < c.w * c.h; i++) if (c.d[i] != KEYC) c.d[i] = gfx_scale(c.d[i], z ? 190 : 210);
                }
                out[p] = c;
            }
        }
    }
}

const Sprite& art_char(bool zombie, int variant, int dir, Pose p, bool& flip) {
    static const int MAP[8] = {2, 1, 0, 1, 2, 3, 4, 3};
    flip = (dir == 3 || dir == 4 || dir == 5);
    int d = MAP[dir & 7];
    if (zombie) return zombPose[variant % ZOMB_VARIANTS][d][p];
    return survPose[variant % SURV_VARIANTS][d][p];
}

// ---------------------------------------------------------------- portraits
ROWS(PORTRAIT,   // 24x24 three-quarter bust facing right (Try 2 portraits): hair mass behind, low brows, shaded jaw, jacket + pack straps
"........................", "........kkkkkkkk........", "......kkhhhhhhhhkk......", ".....khhHHhhhhhhhhk.....",
"....khHHhhhhhhhhhhhk....", "....khhhhhhhhhhhhhhjk...", "...khhhhhhhhhhhhhhhjk...", "...khhhjhhhhhjhhhhhjjk..",
"...khhhhhshhsshhsshhjk..", "...khhhhjssjjsssssjjSk..", "...khhhhSsseessssseeSk..", "...khhhhSsseessssseeSk..",
"...khhhjSsseessSsseeSk..", "....kjjSsssssssSsssSk...", "....kSsssssssiisssSk....", ".....kSsssssssssssSk....",
"......kSssssssssssk.....", ".......kkSSSSSSSkk......", ".........kssssssk.......", "....kkOObbbMbsbbbOOkk...",
"..kkbbOObbMMbbbbbOObbkk.", ".kbbbbOObbbbbmbbbOObbbk.", ".kbMbbOObbbbbmbbbOObbmk.", ".kbbbbOObbbbbmbbbOObbbk.");

static void buildPortraits() {
    for (int v = 0; v < PORTRAITS; v++) {
        const CharPal& p = SURV_PAL[v];
        Sprite s = PARSE(PORTRAIT);
        px H = pal_char('h'), L = pal_char('H'), J = pal_char('j'), k = pal_char('k'), S = pal_char('S');
        if (p.style == 1) {   // cap pulled low, brim jutting out to the right over the eyes
            for (int y = 1; y <= 7; y++) for (int x = 0; x < 24; x++) if (spr_get(s, x, y) == pal_char('s')) spr_set(s, x, y, H);
            for (int x = 8; x <= 22; x++) spr_set(s, x, 8, L);                  // lit top of the brim
            for (int x = 9; x <= 23; x++) spr_set(s, x, 9, J);                  // dark underside, jutting past the head
            spr_set(s, 7, 8, k); spr_set(s, 23, 8, k); spr_set(s, 21, 7, k); spr_set(s, 22, 7, k); spr_set(s, 8, 9, k);
            for (int x = 9; x <= 19; x++) if (spr_get(s, x, 10) == pal_char('s')) spr_set(s, x, 10, S);   // shadow under the brim
            spr_set(s, 6, 3, L); spr_set(s, 7, 3, L); spr_set(s, 6, 4, L);
            if (v == 0 || v == 3) { spr_rect(s, 11, 4, 2, 2, rgb(196, 50, 44)); spr_set(s, 11, 4, rgb(240, 190, 180)); }   // cap emblem
        } else if (p.style == 2) {   // long hair tied at the side, strand framing the face
            for (int y = 9; y <= 18; y++) { spr_set(s, 2, y, k); spr_set(s, 3, y, J); spr_set(s, 4, y, H); spr_set(s, 5, y, L); }
            spr_set(s, 3, 19, k); spr_set(s, 4, 19, k); spr_set(s, 5, 19, k);
            spr_circle(s, 3, 10, 2, H); spr_set(s, 2, 9, L); spr_set(s, 0, 10, k); spr_set(s, 1, 8, k); spr_set(s, 1, 12, k);
            for (int y = 9; y <= 13; y++) spr_set(s, 20, y, J);
        }
        Sprite col = recolorChar(s, p, false);
        freeS(s);
        // backdrop: dark slate with a light rounded frame (Try 2 portrait cards)
        Sprite out = spr_new(24, 24);
        for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
            px v2 = spr_get(col, x, y);
            bool edge = x == 0 || y == 0 || x == 23 || y == 23, corner = (x == 0 || x == 23) && (y == 0 || y == 23);
            px bg = gfx_mix(rgb(62, 64, 82), rgb(36, 36, 48), y * 10);
            out.d[y * 24 + x] = corner ? rgb(30, 30, 38) : (edge ? rgb(120, 120, 136) : (v2 != KEYC ? v2 : bg));
        }
        freeS(col);
        g_portrait[v] = out;
    }
}

// ---------------------------------------------------------------- tiles
static px noisy(px base, uint32_t h, int amt) {
    int r = (int)(h % 100);
    if (r < amt) return gfx_scale(base, 220);
    if (r > 100 - amt) return gfx_mix(base, rgb(255, 255, 255), 24);
    return base;
}
static Sprite tileNoise(px base, int amt, uint32_t seed) {
    Sprite s = spr_new(16, 16);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) s.d[y * 16 + x] = noisy(base, hash32(seed * 977 + y * 16 + x), amt);
    return s;
}
static void dark(Sprite& s, int x, int y, int l) { px v = spr_get(s, x, y); if (v != KEYC) spr_set(s, x, y, gfx_scale(v, l)); }

static void buildTiles() {
    for (int v = 0; v < 4; v++) {
        uint32_t sd = 31 + v * 101;
        Rng r(sd);
        // grass: olive with darker clumps and blades
        Sprite g = tileNoise(rgb(70, 84, 44), 18, sd);
        for (int i = 0; i < 6; i++) { int x = r.range(15), y = 1 + r.range(14); spr_set(g, x, y, rgb(96, 114, 58)); spr_set(g, x, y - 1, rgb(84, 100, 50)); }
        for (int i = 0; i < 3; i++) { int x = r.range(14), y = r.range(14); spr_rect(g, x, y, 2, 2, rgb(58, 70, 38)); }
        if (v == 3) { spr_set(g, 6, 7, rgb(170, 150, 90)); spr_set(g, 11, 3, rgb(150, 80, 60)); }
        g_tile[TA_GRASS][v] = g;
        Sprite a = tileNoise(rgb(54, 54, 58), 14, sd + 1);
        if (v == 1) for (int i = 0; i < 9; i++) spr_set(a, 3 + i, 6 + (i * 3) / 5, rgb(36, 36, 40));
        g_tile[TA_ASPHALT][v] = a;
        Sprite ln = tileNoise(rgb(54, 54, 58), 14, sd + 2);
        for (int y = 0; y < 16; y++) if (hash32(y + v * 7) % 7) { spr_set(ln, 7, y, rgb(196, 170, 70)); spr_set(ln, 8, y, rgb(170, 146, 60)); }
        g_tile[TA_LINE][v] = ln;
        Sprite dt = tileNoise(rgb(134, 98, 68), 12, sd + 3);   // warm packed dirt (Try 2)
        for (int i = 0; i < 3; i++) { int x = r.range(13), y = r.range(13); spr_rect(dt, x, y, 3, 2, rgb(102, 78, 54)); }        // worn patches
        for (int i = 0; i < 2; i++) { int x = 1 + r.range(13), y = 1 + r.range(13); spr_set(dt, x, y, rgb(168, 154, 132)); spr_set(dt, x, y + 1, rgb(80, 62, 44)); }   // pebbles
        if (v == 2) { spr_set(dt, 4, 11, rgb(96, 130, 60)); spr_set(dt, 5, 10, rgb(110, 150, 66)); spr_set(dt, 6, 11, rgb(96, 130, 60)); }   // a stray tuft
        g_tile[TA_DIRT][v] = dt;
        // police lino checker with grout and grime
        Sprite t = spr_new(16, 16);
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            px b = (((x >> 3) + (y >> 3)) & 1) ? rgb(142, 138, 126) : rgb(122, 118, 108);
            if ((x & 7) == 0 || (y & 7) == 0) b = gfx_scale(b, 215);
            t.d[y * 16 + x] = noisy(b, hash32(sd * 31 + y * 16 + x), 8);
        }
        if (v == 2) for (int i = 0; i < 5; i++) dark(t, 9 + i % 3, 10 + i / 2, 190);
        g_tile[TA_TILE][v] = t;
        Sprite w = spr_new(16, 16);
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int plank = y / 4; int seam = (plank * 7 + v * 5) % 16;
            px b = (plank & 1) ? rgb(108, 78, 50) : rgb(98, 70, 46);
            if ((y & 3) == 3 || x == seam) b = rgb(66, 46, 30);
            w.d[y * 16 + x] = noisy(b, hash32(sd * 13 + y * 16 + x), 10);
        }
        g_tile[TA_WOOD][v] = w;
        Sprite c = spr_new(16, 16);
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) c.d[y * 16 + x] = noisy(((x + y) & 1) ? rgb(66, 72, 88) : rgb(60, 66, 80), hash32(sd * 7 + y * 16 + x), 10);
        g_tile[TA_CARPET][v] = c;
        Sprite cc = tileNoise(rgb(104, 104, 100), 14, sd + 5);
        if (v == 0) for (int i = 0; i < 7; i++) spr_set(cc, 2 + i, 12 - i / 2, rgb(76, 76, 74));
        g_tile[TA_CONCRETE][v] = cc;
        // wall top: dark charcoal mass with rim
        Sprite wt = tileNoise(rgb(46, 44, 50), 8, sd + 6);
        for (int i = 0; i < 16; i++) { spr_set(wt, i, 0, rgb(64, 62, 68)); }
        g_tile[TA_WALL_TOP][v] = wt;
        // wall front: cap, distressed plaster, baseboard
        Sprite wf = spr_new(16, 16);
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            px b = y < 4 ? rgb(52, 50, 56) : (y >= 14 ? rgb(64, 56, 48) : rgb(118, 108, 94));
            if (y == 4) b = rgb(84, 78, 70);
            if (y >= 5 && y < 14 && hash32(sd + x * 3) % 9 == 0 && y > 8) b = gfx_scale(b, 200);  // drip stains
            wf.d[y * 16 + x] = noisy(b, hash32(sd * 17 + y * 16 + x), 9);
        }
        if (v == 1) for (int i = 0; i < 5; i++) spr_set(wf, 10 + i % 2, 6 + i, rgb(80, 72, 62));
        g_tile[TA_WALL_FRONT][v] = wf;
        Sprite win = spr_copy(wf);
        spr_rect(win, 2, 5, 12, 8, rgb(44, 40, 38));
        spr_rect(win, 3, 6, 10, 6, rgb(58, 84, 98));
        spr_rect(win, 7, 6, 2, 6, rgb(44, 40, 38));
        spr_set(win, 4, 7, rgb(120, 150, 160)); spr_set(win, 5, 7, rgb(96, 128, 140)); spr_set(win, 10, 7, rgb(120, 150, 160));
        g_tile[TA_WINDOW][v] = win;
        Sprite wb = spr_copy(wf);
        spr_rect(wb, 2, 5, 12, 8, rgb(44, 40, 38));
        spr_rect(wb, 3, 6, 10, 6, rgb(18, 18, 22));
        spr_set(wb, 3, 6, rgb(96, 128, 140)); spr_set(wb, 4, 6, rgb(96, 128, 140)); spr_set(wb, 12, 11, rgb(96, 128, 140)); spr_set(wb, 11, 6, rgb(80, 110, 120));
        g_tile[TA_WINDOW_BROKEN][v] = wb;
        Sprite bars = spr_copy(g_tile[TA_TILE][v]);
        for (int x = 1; x < 16; x += 3) for (int y = 0; y < 16; y++) { spr_set(bars, x, y, rgb(90, 92, 98)); spr_set(bars, x + 1, y, rgb(46, 46, 52)); }
        spr_rect(bars, 0, 0, 16, 2, rgb(70, 72, 78));
        g_tile[TA_BARS][v] = bars;
        Sprite f = spr_copy(g_tile[TA_GRASS][v]);
        for (int y = 2; y < 14; y++) for (int x = 0; x < 16; x++) if (((x + y) % 4 == 0) || ((x - y + 16) % 4 == 0)) spr_set(f, x, y, rgb(120, 122, 126));
        spr_rect(f, 0, 1, 16, 2, rgb(140, 142, 146)); spr_rect(f, 0, 3, 16, 1, rgb(60, 60, 66));
        if (v == 2) spr_rect(f, 7, 1, 2, 14, rgb(90, 92, 98));
        g_tile[TA_FENCE][v] = f;
        Sprite e = spr_copy(g_tile[TA_ASPHALT][v]);
        for (int i = 0; i < 16; i++) {
            px c2 = ((i / 4) & 1) ? rgb(30, 30, 30) : rgb(220, 180, 50);
            spr_set(e, i, 0, c2); spr_set(e, i, 15, c2); spr_set(e, 0, i, c2); spr_set(e, 15, i, c2);
        }
        for (int i = 0; i < 4; i++) { spr_set(e, 5 + i, 6 + i, rgb(220, 180, 50)); spr_set(e, 10 - i, 6 + i, rgb(220, 180, 50)); }
        g_tile[TA_EXTRACT][v] = e;
        Sprite gd = spr_copy(g_tile[TA_DIRT][v]);
        for (int y = 2; y < 16; y += 5) for (int x = 1; x < 15; x++) spr_set(gd, x, y, rgb(66, 50, 36));
        for (int y = 1; y < 16; y += 5) for (int x = 2 + (v & 1); x < 15; x += 3) { spr_set(gd, x, y, rgb(90, 150, 60)); spr_set(gd, x, y - 1 < 0 ? 0 : y - 1, rgb(110, 170, 70)); }
        g_tile[TA_GARDEN][v] = gd;
        Sprite pl = spr_new(16, 16);
        // weathered grey picket boards (Try 2): dark gaps, uneven pointed tops, two rails, lit left edges
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int board = x / 4, bx = x & 3, top = (int)(hash32(sd * 7 + v * 5 + board) % 3);
            px b = (board & 1) ? rgb(74, 76, 84) : rgb(86, 88, 96);
            if (bx == 0) b = rgb(116, 118, 128);
            if (bx == 3) b = rgb(30, 30, 36);
            if (y < top || (y == top && (bx == 0 || bx == 3))) b = KEYC;
            else if (y == top || (y == top + 1 && bx == 1)) b = rgb(132, 134, 142);
            pl.d[y * 16 + x] = b == KEYC ? rgb(64, 78, 42) : noisy(b, hash32(sd * 19 + y * 16 + x), 8);
        }
        for (int x = 0; x < 16; x++) { if ((x & 3) != 3) { spr_set(pl, x, 6, rgb(56, 56, 62)); spr_set(pl, x, 12, rgb(56, 56, 62)); spr_set(pl, x, 5, rgb(100, 102, 110)); } }
        g_tile[TA_PALISADE][v] = pl;
    }
    for (int v = 0; v < 4; v++) {
        // doors: frame + wooden leaf + handle
        Sprite d = spr_copy(g_tile[TA_WALL_FRONT][v]);
        spr_rect(d, 2, 1, 12, 15, rgb(40, 34, 30));
        spr_rect(d, 3, 2, 10, 14, rgb(112, 80, 50));
        spr_rect(d, 4, 3, 8, 5, rgb(96, 68, 42)); spr_rect(d, 4, 9, 8, 6, rgb(96, 68, 42));
        spr_set(d, 11, 9, rgb(232, 192, 64)); spr_set(d, 11, 10, rgb(160, 128, 32));
        g_tile[TA_DOOR_CLOSED][v] = d;
        Sprite o = spr_copy(g_tile[TA_TILE][v]);
        spr_rect(o, 0, 0, 2, 16, rgb(40, 34, 30)); spr_rect(o, 14, 0, 2, 16, rgb(40, 34, 30));
        spr_rect(o, 2, 0, 2, 14, rgb(112, 80, 50)); spr_rect(o, 2, 14, 2, 2, rgb(70, 50, 32));
        g_tile[TA_DOOR_OPEN][v] = o;
        Sprite lk = spr_copy(d);
        spr_rect(lk, 9, 8, 4, 4, rgb(150, 40, 36)); spr_set(lk, 10, 9, rgb(232, 192, 64)); spr_set(lk, 11, 9, rgb(232, 192, 64));
        g_tile[TA_DOOR_LOCKED][v] = lk;
        Sprite br = spr_copy(g_tile[TA_TILE][v]);
        spr_rect(br, 0, 0, 2, 16, rgb(40, 34, 30)); spr_rect(br, 14, 0, 2, 16, rgb(40, 34, 30));
        for (int i = 0; i < 6; i++) spr_rect(br, 3 + i * 2, 11 + (i & 1) * 2, 2, 1, rgb(112, 80, 50));
        spr_rect(br, 2, 0, 3, 3, rgb(96, 68, 42)); spr_rect(br, 11, 0, 3, 2, rgb(96, 68, 42));
        g_tile[TA_DOOR_BROKEN][v] = br;
        Sprite cd = spr_copy(g_tile[TA_BARS][v]);
        spr_rect(cd, 5, 6, 6, 5, rgb(70, 72, 78)); spr_rect(cd, 6, 7, 4, 3, rgb(150, 40, 36)); spr_set(cd, 8, 8, rgb(232, 192, 64));
        g_tile[TA_CELLDOOR][v] = cd;
        Sprite gc = spr_copy(g_tile[TA_PALISADE][v]);
        spr_rect(gc, 0, 4, 16, 2, rgb(60, 44, 30)); spr_rect(gc, 0, 12, 16, 2, rgb(60, 44, 30));
        for (int i = 0; i < 16; i++) spr_set(gc, i, 4 + (i * 8) / 16, rgb(70, 50, 32));
        g_tile[TA_GATE_CLOSED][v] = gc;
        Sprite go = spr_copy(g_tile[TA_DIRT][v]);
        spr_rect(go, 0, 0, 2, 16, rgb(92, 66, 42)); spr_rect(go, 14, 0, 2, 16, rgb(92, 66, 42));
        g_tile[TA_GATE_OPEN][v] = go;
    }
    // crack overlays (transparent background)
    for (int v = 0; v < 4; v++) {
        Sprite c1 = spr_new(16, 16), c2 = spr_new(16, 16);
        static const int P1[][2] = {{8, 3}, {7, 4}, {7, 5}, {6, 6}, {6, 7}, {7, 8}, {8, 9}, {5, 7}, {4, 8}};
        for (auto& p : P1) spr_set(c1, p[0], p[1], rgb(28, 24, 22));
        for (auto& p : P1) spr_set(c2, p[0], p[1], rgb(28, 24, 22));
        static const int P2[][2] = {{9, 9}, {10, 10}, {10, 11}, {11, 12}, {9, 12}, {8, 13}, {12, 6}, {11, 5}, {4, 11}, {3, 12}};
        for (auto& p : P2) spr_set(c2, p[0], p[1], rgb(28, 24, 22));
        g_tile[TA_CRACK1][v] = c1; g_tile[TA_CRACK2][v] = c2;
    }
}

// ---------------------------------------------------------------- props
ROWS(DESK,
"................", "................", "kkkkkkkkkkkkkkkk", "kttttttttwwtttbk",
"ktttttwwtwwttttk", "kttttttttttttttk", "kbbbbbbbbbbbbbbk", "kBbbbbbBBbbbbbBk",
"kBbkkkbBBbkkkbBk", "kBbbbbbBBbbbbbBk", "kBbkkkbBBbkkkbBk", "kBbbbbbBBbbbbbBk",
"kBkkkkkkkkkkkkBk", "kBk..........kBk", "kkk..........kkk", "................");
ROWS(FILING,
"...kkkkkkkkkk...", "...kllllllllk...", "...kggggggggk...", "...kgllllllgk...",
"...kglkkkklgk...", "...kgllllllgk...", "...kggggggggk...", "...kgllllllgk...",
"...kglkkkklgk...", "...kgllllllgk...", "...kggggggggk...", "...kgllllllgk...",
"...kglkkkklgk...", "...kgllllllgk...", "...kkkkkkkkkk...", "................");

static void buildProps() {
    g_spr[S_DESK] = PARSE(DESK);
    g_spr[S_FILING] = PARSE(FILING);
    {   // locker: olive double door with vents and handles
        Sprite s = spr_new(16, 16);
        spr_rect(s, 2, 0, 12, 15, pal_char('k'));
        spr_rect(s, 3, 1, 10, 13, rgb(96, 110, 60));
        spr_rect(s, 7, 1, 2, 13, pal_char('k'));
        for (int y = 2; y < 5; y += 2) { spr_rect(s, 4, y, 2, 1, rgb(50, 60, 34)); spr_rect(s, 10, y, 2, 1, rgb(50, 60, 34)); }
        spr_set(s, 6, 8, rgb(170, 170, 170)); spr_set(s, 9, 8, rgb(170, 170, 170));
        spr_rect(s, 3, 13, 10, 1, rgb(60, 72, 40));
        g_spr[S_LOCKER] = s;
    }
    {   // crate: tan planks with X brace
        Sprite s = spr_new(16, 16);
        spr_rect(s, 1, 1, 14, 14, pal_char('k'));
        spr_rect(s, 2, 2, 12, 12, rgb(170, 134, 90));
        for (int i = 0; i < 12; i++) { spr_set(s, 2 + i, 2 + i, rgb(110, 80, 50)); spr_set(s, 13 - i, 2 + i, rgb(110, 80, 50)); }
        spr_rect(s, 2, 2, 12, 1, rgb(200, 170, 120)); spr_rect(s, 2, 13, 12, 1, rgb(110, 80, 50));
        g_spr[S_CRATE] = s;
    }
    {   // medical cabinet
        Sprite s = spr_new(16, 16);
        spr_rect(s, 2, 0, 12, 15, pal_char('k'));
        spr_rect(s, 3, 1, 10, 13, rgb(200, 198, 190));
        spr_rect(s, 7, 3, 2, 7, rgb(176, 48, 48)); spr_rect(s, 5, 5, 6, 2, rgb(176, 48, 48));
        spr_rect(s, 3, 11, 10, 1, rgb(150, 148, 140)); spr_set(s, 11, 12, rgb(90, 90, 90));
        g_spr[S_MEDCAB] = s;
    }
    {   // shelf with boxes
        Sprite s = spr_new(16, 16);
        spr_rect(s, 0, 1, 16, 14, pal_char('k'));
        spr_rect(s, 1, 2, 14, 12, rgb(66, 50, 36));
        for (int y = 2; y < 14; y += 4) spr_rect(s, 1, y + 3, 14, 1, rgb(110, 82, 52));
        static const px boxes[4] = {rgb(150, 120, 80), rgb(120, 130, 140), rgb(170, 150, 110), rgb(110, 120, 70)};
        for (int r = 0; r < 3; r++) for (int b = 0; b < 3; b++) if (hash32(r * 3 + b) % 4) spr_rect(s, 2 + b * 4 + (r & 1), 2 + r * 4, 3, 3, boxes[(r + b) & 3]);
        g_spr[S_SHELF] = s;
    }
    {   // Try 2 police car (40x28, 3/4 front view): black body, white door panel, light bar, chunky wheels
        Sprite in = spr_new(38, 26);
        px body = rgb(30, 32, 40), bodyL = rgb(58, 62, 74), white = rgb(226, 226, 220), whiteD = rgb(178, 178, 174), glass = rgb(70, 96, 118), glassL = rgb(130, 160, 180);
        // lower body
        for (int y = 11; y < 22; y++) for (int x = 0; x < 38; x++) spr_set(in, x, y, y == 11 ? bodyL : body);
        // white door panels + stripe
        spr_rect(in, 9, 12, 20, 8, white); spr_rect(in, 9, 18, 20, 2, whiteD); vline(in, 19, 12, 19, rgb(120, 120, 120));
        spr_rect(in, 11, 14, 6, 1, rgb(40, 60, 110)); spr_rect(in, 21, 14, 6, 1, rgb(40, 60, 110));   // POLICE lettering hint
        // cabin: roof + windows (narrower, set back)
        for (int y = 4; y < 11; y++) { int in0 = (11 - y) / 2; for (int x = 6 + in0; x < 32 - in0; x++) spr_set(in, x, y, y < 6 ? body : glass); }
        for (int y = 6; y < 11; y++) spr_set(in, 19, y, body);
        for (int y = 6; y < 9; y++) { spr_set(in, 10 + y - 6, y, glassL); spr_set(in, 22 + y - 6, y, glassL); }
        // light bar
        spr_rect(in, 13, 1, 6, 3, rgb(210, 40, 36)); spr_rect(in, 19, 1, 6, 3, rgb(44, 80, 220)); spr_set(in, 14, 1, rgb(255, 140, 130)); spr_set(in, 20, 1, rgb(140, 170, 255));
        // bumpers, headlights, grille
        spr_rect(in, 34, 13, 4, 3, rgb(250, 240, 190)); spr_rect(in, 0, 13, 3, 3, rgb(200, 40, 36));
        hline(in, 0, 37, 21, rgb(90, 92, 100));
        // wheels
        for (int k = 0; k < 2; k++) { int wx = k ? 28 : 3; spr_rect(in, wx, 20, 7, 6, rgb(16, 16, 18)); spr_rect(in, wx + 2, 22, 3, 2, rgb(110, 110, 116)); }
        Sprite s = spr_new(40, 28);
        for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { px v = spr_get(in, x, y); if (v != KEYC) spr_set(s, x + 1, y + 1, v); }
        free(in.d);
        outlineAll(s);
        g_spr[S_CAR] = s;
        Sprite w = spr_copy(s);  // wrecked civilian car: rusty, smashed bar, no lettering
        for (int i = 0; i < w.w * w.h; i++) if (w.d[i] != KEYC && w.d[i] != pal_char('k')) w.d[i] = gfx_mix(w.d[i], rgb(122, 74, 50), 160);
        spr_rect(w, 14, 2, 12, 3, rgb(40, 30, 26)); spr_rect(w, 22, 7, 6, 3, rgb(34, 28, 24));
        g_spr[S_WRECK] = w;
    }
    for (int t = 0; t < 2; t++) {   // trees: layered canopy (32x32)
        Sprite s = spr_new(32, 32);
        Rng r(77 + t);
        spr_circle(s, 16, 18, 13, rgb(30, 40, 26));
        for (int i = 0; i < 9; i++) spr_circle(s, 9 + r.range(14), 9 + r.range(14), 5 + r.range(3), t ? rgb(74, 82, 40) : rgb(58, 76, 40));
        for (int i = 0; i < 6; i++) spr_circle(s, 10 + r.range(12), 8 + r.range(10), 3, t ? rgb(98, 104, 52) : rgb(80, 100, 52));
        for (int i = 0; i < 14; i++) spr_set(s, 6 + r.range(20), 5 + r.range(20), t ? rgb(124, 122, 64) : rgb(104, 126, 64));
        g_spr[S_TREE + t] = s;
    }
    {   Sprite s = spr_new(16, 16);  // bench
        spr_rect(s, 1, 6, 14, 6, pal_char('k')); spr_rect(s, 2, 7, 12, 2, rgb(120, 88, 56)); spr_rect(s, 2, 10, 12, 1, rgb(96, 70, 44));
        spr_rect(s, 2, 12, 1, 3, pal_char('k')); spr_rect(s, 13, 12, 1, 3, pal_char('k'));
        g_spr[S_BENCH] = s; }
    {   Sprite s = spr_new(16, 16);  // cell cot
        spr_rect(s, 1, 2, 14, 12, pal_char('k')); spr_rect(s, 2, 3, 12, 10, rgb(100, 104, 96)); spr_rect(s, 2, 3, 4, 10, rgb(170, 166, 150));
        g_spr[S_COT] = s; }
    {   Sprite s = spr_new(16, 16);  // chair
        spr_rect(s, 4, 3, 8, 3, pal_char('k')); spr_rect(s, 5, 4, 6, 1, rgb(80, 80, 88));
        spr_rect(s, 4, 6, 8, 6, pal_char('k')); spr_rect(s, 5, 7, 6, 4, rgb(60, 60, 68));
        g_spr[S_CHAIR] = s; }
    {   Sprite s = spr_new(16, 16);  // potted plant
        spr_rect(s, 5, 10, 6, 5, pal_char('k')); spr_rect(s, 6, 11, 4, 3, rgb(140, 80, 50));
        spr_circle(s, 8, 6, 5, rgb(40, 60, 34)); spr_circle(s, 7, 5, 3, rgb(70, 100, 50)); spr_set(s, 9, 3, rgb(100, 130, 64));
        g_spr[S_PLANT] = s; }
    {   Sprite s = spr_new(16, 16);  // water cooler
        spr_rect(s, 4, 6, 8, 9, pal_char('k')); spr_rect(s, 5, 7, 6, 7, rgb(190, 190, 186));
        spr_rect(s, 5, 0, 6, 6, pal_char('k')); spr_rect(s, 6, 1, 4, 5, rgb(100, 150, 190));
        g_spr[S_COOLER] = s; }
    for (int f = 0; f < 3; f++) {  // campfire flame frames
        Sprite s = spr_new(16, 16);
        spr_rect(s, 2, 12, 12, 2, rgb(90, 90, 92)); spr_rect(s, 3, 11, 10, 1, rgb(60, 44, 30));
        for (int i = 0; i < 3; i++) spr_rect(s, 4 + i * 3, 10 - (i == 1 ? 0 : 1), 2, 3, rgb(90, 60, 36));
        int h = 6 + (f == 1 ? 2 : (f == 2 ? 1 : 0));
        for (int y = 0; y < h; y++) {
            int w = (h - y) / 2 + 1; int cx = 8 + ((f + y) % 3 == 0 ? 1 : 0) - (f == 2 && y > 3 ? 1 : 0);
            px c = y < 2 ? rgb(230, 120, 40) : (y < 4 ? rgb(240, 170, 60) : rgb(250, 230, 140));
            spr_rect(s, cx - w / 2 - (w > 1), 10 - y, w + (w > 1), 1, c);
        }
        g_spr[S_FIRE0 + f] = s;
    }
    {   Sprite s = spr_new(16, 16);  // radio / map table
        spr_rect(s, 0, 4, 16, 10, pal_char('k')); spr_rect(s, 1, 5, 14, 6, rgb(110, 82, 52));
        spr_rect(s, 2, 6, 7, 4, rgb(190, 176, 140)); spr_set(s, 4, 7, rgb(176, 48, 48)); spr_set(s, 6, 8, rgb(176, 48, 48));
        spr_rect(s, 10, 3, 4, 4, rgb(60, 64, 60)); spr_set(s, 11, 1, pal_char('k')); spr_set(s, 11, 2, pal_char('k')); spr_set(s, 12, 4, rgb(154, 228, 64));
        spr_rect(s, 1, 11, 1, 3, pal_char('k')); spr_rect(s, 14, 11, 1, 3, pal_char('k'));
        g_spr[S_RADIO] = s; }
    for (int f = 0; f < 2; f++) {  // generator prop (vibrates)
        Sprite s = spr_new(16, 16);
        spr_rect(s, 1, 4 + f, 14, 10, pal_char('k')); spr_rect(s, 2, 5 + f, 12, 8, rgb(170, 140, 50));
        for (int x = 3; x < 13; x += 2) spr_rect(s, x, 7 + f, 1, 4, rgb(80, 70, 40));
        spr_rect(s, 11, 1 + f, 2, 4, rgb(70, 70, 70));
        spr_set(s, 3, 6 + f, f ? rgb(154, 228, 64) : rgb(90, 140, 40));
        g_spr[S_GENPROP0 + f] = s;
    }
    {   Sprite s = spr_new(16, 16);  // workbench with vice and tools
        spr_rect(s, 0, 5, 16, 9, pal_char('k')); spr_rect(s, 1, 6, 14, 4, rgb(120, 92, 60)); spr_rect(s, 1, 10, 14, 3, rgb(80, 60, 40));
        spr_rect(s, 2, 3, 3, 3, rgb(90, 92, 98)); spr_rect(s, 9, 6, 5, 1, rgb(150, 150, 150)); spr_rect(s, 7, 7, 1, 2, rgb(176, 48, 48));
        g_spr[S_WORKBENCH] = s; }
    {   Sprite s = spr_new(16, 16);  // lamp post
        spr_rect(s, 7, 3, 2, 12, pal_char('k')); spr_rect(s, 5, 1, 6, 3, pal_char('k')); spr_rect(s, 6, 2, 4, 1, rgb(240, 220, 150));
        spr_rect(s, 5, 14, 6, 2, pal_char('k'));
        g_spr[S_LAMP] = s; }
    for (int b = 0; b < 3; b++) {  // blood decals: Try 2 splat with a central pool, radiating streaks, droplets
        Sprite s = spr_new(32, 24); Rng r(500 + b);
        px dk = rgb(92, 14, 16), md = rgb(124, 22, 22), lt = rgb(150, 30, 28);
        for (int i = 0; i < 4 + b; i++) spr_circle(s, 13 + r.range(6), 10 + r.range(4), 2 + r.range(2 + b), i & 1 ? md : dk);
        for (int k = 0; k < 7 + b * 2; k++) {
            float a = r.range(628) / 100.0f; int len = 5 + r.range(7 + b * 2);
            for (int t = 0; t < len; t++) {
                int x = 16 + (int)(cosf(a) * t), y = 12 + (int)(sinf(a) * t * 0.6f);
                spr_set(s, x, y, t < len / 2 ? md : dk); if (t < 3) spr_set(s, x, y + 1, md);
            }
            spr_circle(s, 16 + (int)(cosf(a) * (len + 2)), 12 + (int)(sinf(a) * (len + 2) * 0.6f), r.range(2), dk);
        }
        spr_set(s, 15, 10, lt); spr_set(s, 16, 10, lt); spr_set(s, 14, 11, lt);   // wet highlight
        g_spr[S_BLOOD0 + b] = s;
    }
    {   Sprite s = spr_new(16, 16);  // dropped backpack
        spr_rect(s, 3, 4, 10, 10, pal_char('k')); spr_rect(s, 4, 5, 8, 8, rgb(96, 110, 56)); spr_rect(s, 5, 9, 6, 3, rgb(70, 82, 40));
        spr_rect(s, 6, 2, 4, 3, pal_char('k'));
        g_spr[S_BAG] = s; }
    {   Sprite s = spr_new(16, 16);  // bedroll
        spr_rect(s, 1, 4, 14, 9, pal_char('k')); spr_rect(s, 2, 5, 12, 7, rgb(84, 92, 60)); spr_rect(s, 2, 5, 3, 7, rgb(170, 160, 130));
        g_spr[S_BEDROLL] = s; }
    {   // Try 2 army tent (46x40): big canvas roof with panel seams, darker front wall, warm lit doorway with crates
        Sprite in = spr_new(44, 38);
        px roof = rgb(112, 146, 82), roofD = rgb(84, 114, 60), wall = rgb(72, 100, 54), wallD = rgb(56, 80, 42);
        for (int y = 0; y < 22; y++) {
            int inset = (21 - y) / 4;
            for (int x = inset; x < 44 - inset; x++) {
                px c = noisy(y < 3 ? gfx_mix(roof, rgb(255, 255, 255), 40) : roof, hash32(x * 31 + y * 7 + 5), 8);
                if ((x - inset) % 9 == 0) c = roofD;                       // panel seams
                if (y == 21) c = roofD;
                spr_set(in, x, y, c);
            }
        }
        for (int k = 0; k < 3; k++) { int px0 = 6 + k * 13, py0 = 6 + (k & 1) * 6; spr_rect(in, px0, py0, 5, 4, rgb(124, 132, 92)); }   // patches
        for (int y = 22; y < 37; y++) for (int x = 1; x < 43; x++) spr_set(in, x, y, noisy((x % 7 == 0) ? wallD : wall, hash32(x * 13 + y), 8));
        // doorway: rolled-up flaps, warm lantern light, crates inside
        spr_rect(in, 15, 24, 14, 13, rgb(34, 26, 20));
        spr_rect(in, 16, 25, 12, 12, rgb(232, 164, 70)); spr_rect(in, 18, 26, 8, 4, rgb(252, 214, 120));
        spr_rect(in, 17, 31, 6, 5, rgb(140, 98, 54)); hline(in, 17, 22, 33, rgb(100, 70, 40));
        spr_rect(in, 23, 32, 4, 4, rgb(120, 84, 48));
        spr_rect(in, 13, 23, 3, 5, roofD); spr_rect(in, 28, 23, 3, 5, roofD);
        Sprite s = spr_new(46, 40);
        for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { px v = spr_get(in, x, y); if (v != KEYC) spr_set(s, x + 1, y + 1, v); }
        free(in.d);
        outlineAll(s);
        dline(s, 0, 39, 3, 24, rgb(170, 160, 120)); dline(s, 45, 39, 42, 24, rgb(170, 160, 120));   // guy ropes
        g_spr[S_TENT] = s;
    }
    {   // rusty oil drum
        Sprite s = spr_new(16, 16);
        spr_rect(s, 3, 2, 10, 13, pal_char('k'));
        spr_rect(s, 4, 3, 8, 11, rgb(70, 86, 110)); spr_rect(s, 4, 3, 3, 11, rgb(92, 110, 136));
        spr_rect(s, 4, 6, 8, 1, rgb(46, 56, 72)); spr_rect(s, 4, 10, 8, 1, rgb(46, 56, 72));
        spr_rect(s, 5, 3, 6, 2, rgb(40, 46, 56)); spr_set(s, 9, 4, rgb(120, 70, 40)); spr_set(s, 6, 12, rgb(130, 76, 40)); spr_set(s, 10, 8, rgb(120, 70, 40));
        g_spr[S_BARREL] = s;
    }
    {   // stacked sandbags
        Sprite s = spr_new(16, 16);
        for (int r = 0; r < 3; r++) for (int b = 0; b < ((r & 1) ? 2 : 3); b++) {
            int x = b * 5 + ((r & 1) ? 3 : 0), y = 4 + r * 4;
            spr_rect(s, x, y, 6, 4, pal_char('k')); spr_rect(s, x + 1, y + 1, 4, 2, rgb(170, 150, 104)); spr_set(s, x + 1, y + 1, rgb(196, 178, 130));
        }
        g_spr[S_SANDBAG] = s;
    }
    {   // log pile
        Sprite s = spr_new(16, 16);
        for (int i = 0; i < 3; i++) { spr_rect(s, 1, 5 + i * 3, 14, 3, pal_char('k')); spr_rect(s, 2, 6 + i * 3, 11, 1, rgb(120, 84, 50)); }
        for (int i = 0; i < 3; i++) { spr_rect(s, 12, 5 + i * 3, 3, 3, rgb(196, 160, 110)); spr_set(s, 13, 6 + i * 3, rgb(140, 100, 64)); }
        g_spr[S_LOGS] = s;
    }
    for (int f = 0; f < 2; f++) {   // torch post with a flickering flame
        Sprite s = spr_new(16, 16);
        spr_rect(s, 7, 6, 2, 10, pal_char('k')); spr_rect(s, 7, 7, 1, 8, rgb(110, 80, 50));
        spr_rect(s, 6, 4, 4, 3, rgb(70, 60, 50));
        spr_rect(s, 6 + f, 1, 3, 3, rgb(240, 150, 50)); spr_set(s, 7 + f, 0, rgb(250, 220, 120)); spr_set(s, 7, 2, rgb(255, 240, 170));
        g_spr[S_TORCH0 + f] = s;
    }
    {   // traffic cone
        Sprite s = spr_new(16, 16);
        spr_rect(s, 3, 12, 10, 3, pal_char('k')); spr_rect(s, 4, 13, 8, 1, rgb(200, 80, 30));
        for (int y = 3; y < 12; y++) { int w = (y - 2) / 2 + 1; spr_rect(s, 8 - w, y, w * 2, 1, (y == 6 || y == 7) ? rgb(220, 220, 210) : rgb(220, 96, 36)); }
        g_spr[S_CONE] = s;
    }
    // ground decals (8x8, transparent)
    auto dec = [](int id, uint32_t seed, int kind) {
        Sprite s = spr_new(8, 8); Rng r(seed);
        switch (kind) {
        case 0: for (int i = 0; i < 6; i++) { int x = 1 + r.range(6); spr_set(s, x, 6, rgb(64, 84, 40)); spr_set(s, x + (i & 1), 5 - (i % 3), rgb(104, 128, 60)); } break;
        case 1: for (int i = 0; i < 3; i++) { int x = 1 + r.range(6), y = 1 + r.range(5); spr_set(s, x, y, i == 0 ? rgb(230, 210, 90) : rgb(220, 220, 210)); spr_set(s, x, y + 1, rgb(70, 96, 46)); } break;
        case 2: spr_rect(s, 2, 3, 4, 3, rgb(96, 94, 90)); spr_rect(s, 2, 3, 2, 1, rgb(140, 138, 132)); spr_rect(s, 2, 6, 4, 1, rgb(50, 48, 46)); break;
        case 3: spr_rect(s, 1, 2, 5, 4, rgb(200, 196, 180)); spr_rect(s, 2, 3, 3, 1, rgb(150, 150, 150)); spr_set(s, 5, 5, rgb(160, 156, 140)); break;
        case 4: for (int i = 0; i < 6; i++) spr_set(s, r.range(8), r.range(8), (i & 1) ? rgb(90, 80, 70) : rgb(60, 54, 48)); break;
        case 5: spr_circle(s, 4, 4, 2, rgb(44, 44, 48)); spr_set(s, 3, 3, rgb(58, 56, 62)); break;
        case 6: for (int i = 0; i < 7; i++) spr_set(s, i, 3 + (i % 3 == 0 ? 1 : 0) + (i > 4 ? 1 : 0), rgb(36, 36, 40)); break;
        case 7: for (int i = 0; i < 6; i++) { spr_set(s, i + 1, 3 + (i & 1), rgb(96, 16, 16)); spr_set(s, i + 1, 4, rgb(76, 12, 14)); } break;
        case 8: for (int i = 0; i < 4; i++) { int x = r.range(7), y = r.range(7); spr_set(s, x, y, rgb(150, 100, 40)); spr_set(s, x + 1, y, rgb(120, 80, 30)); } break;
        }
        g_spr[id] = s;
    };
    dec(S_DEC_TUFT, 1, 0); dec(S_DEC_FLOWER, 2, 1); dec(S_DEC_ROCK, 3, 2); dec(S_DEC_PAPER, 4, 3); dec(S_DEC_DEBRIS, 5, 4);
    dec(S_DEC_OIL, 6, 5); dec(S_DEC_CRACK, 7, 6); dec(S_DEC_SMEAR, 8, 7); dec(S_DEC_LEAVES, 9, 8);
}

// ---------------------------------------------------------------- icons (8x8 UI)
static void buildIcons() {
    auto mk = [](SprId id, const char* const* rows) { g_spr[id] = spr_parse(rows, 8); };
    static const char* const FOOD[] = {".kkkkkk.", "kllllllk", "krrrrrrk", "krwwwwrk", "krrrrrrk", "kllllllk", ".kkkkkk.", "........"};
    static const char* const MED[] = {"..kkkk..", "..krrk..", "kkkrrkkk", "krrrrrrk", "krrrrrrk", "kkkrrkkk", "..krrk..", "..kkkk.."};
    static const char* const AMMO[] = {"..k..k..", ".kyk.kyk", ".kyk.kyk", ".kbk.kbk", ".kbk.kbk", ".kbk.kbk", ".kkk.kkk", "........"};
    static const char* const MAT[] = {"........", "kkkkkkk.", "ktttbttk", "kkkkkkkk", "kttbtttk", "kkkkkkkk", "........", "........"};
    static const char* const FUEL[] = {"...kk...", "..krrk..", ".krrrrk.", ".krrrrk.", "krrrrrrk", "krwrrrrk", ".krrrrk.", "..kkkk.."};
    static const char* const SHIELD[] = {"kkkkkkkk", "klllllgk", "kllyylgk", "klyyyygk", ".kllllk.", ".klllgk.", "..kllk..", "...kk..."};
    static const char* const SKULL[] = {".kkkkkk.", "kwwwwwwk", "kwkwwkwk", "kwkwwkwk", "kwwwwwwk", ".kwkwkk.", ".kwwwwk.", "..kkkk.."};
    static const char* const BIO[] = {"..rrrr..", ".r....r.", "r..rr..r", "r.rrrr.r", "r..rr..r", ".rr..rr.", "..r..r..", ".rr..rr."};
    static const char* const PEOPLE[] = {"..kk.kk.", ".kssksk.", ".kssksk.", "..kk.kk.", ".kbbkbbk", "kbbbkbbk", "kbbbkbbk", "kkkkkkkk"};
    static const char* const CLOCK[] = {".kkkkkk.", "kwwkwwwk", "kwwkwwwk", "kwwkkkwk", "kwwwwwwk", "kwwwwwwk", ".kkkkkk.", "........"};
    mk(S_ICON_FOOD, FOOD); mk(S_ICON_MED, MED); mk(S_ICON_AMMO, AMMO); mk(S_ICON_MAT, MAT); mk(S_ICON_FUEL, FUEL);
    mk(S_ICON_SHIELD, SHIELD); mk(S_ICON_SKULL, SKULL); mk(S_ICON_BIO, BIO); mk(S_ICON_PEOPLE, PEOPLE); mk(S_ICON_CLOCK, CLOCK);
    static const char* const HOUSE[] = {"...kk...", "..kbbk..", ".kbbbbk.", "kbbbbbbk", ".kwwtwk.", ".kwwtwk.", ".kwwtwk.", ".kkkkkk."};
    static const char* const TREE[] = {"..kkkk..", ".kOooOk.", "kOoooOOk", "kOooOOOk", ".kOOOOk.", "..kbbk..", "...bb...", "..kkkk.."};
    static const char* const TRAIN[] = {"kkkkkkk.", "klllllkk", "kNNlNNkk", "kllllllk", "kggggggk", "kkkkkkkk", ".kk..kk.", "........"};
    mk(S_ICON_HOUSE, HOUSE); mk(S_ICON_TREE, TREE); mk(S_ICON_TRAIN, TRAIN);
}

// ---------------------------------------------------------------- inventory item icons
ROWS(I_PISTOL, "................", "..kkkkkkkkkkkk..", "..klllllllllgk..", "..kgggggggggkk..", "..kgggkkkkkkk...", "..kBBBk.k.......", "..kBBBkk........", "..kkkkk.........");
ROWS(I_REVOLVER, "................", "....kkkkkkkkkkk.", "...klllllllllllk", "...kgggggkkkkkk.", "..kkgggk.k......", "..kbbbkkk.......", ".kbbbk..........", ".kkkk...........");
ROWS(I_SHOTGUN, "................................", "................................", "..........kkkkkkkkkkkkkkkkkkkkk.",
     "kkkk....kkllllllllllllllllllllk.", "kbbbkkkkkgggggggbbbbbbbgggggggk.", "kbbbbbbkkkkkkkkkkbbbbbbbkkkkkkk.",
     ".kkbbbbk......kk.kkkkkkk........", "...kkkk.........................");
ROWS(I_RIFLE, "........................................", "..............kkkkkkkkk.................", "..............kgggggggk.................",
     "kkk..........kkkkkkkkkkkkkkkkkkkkkkkkkk.", "kbbbkkkkkkkkkbbbbbbbbbbbblllllllllllllk.", "kbbbbbbbbbbbbbbbbbbbbbbbkkkkkkkkkkkkkkk.",
     ".kkbbbbbkk...kkk.kkkkkkk................", "...kkkkk................................");
ROWS(I_SMG, "........................", "....kkkkkkkkkkkkkkkk....", "....kggggggggggggglllkk.", "kkkkkggggggggggggggkk...",
     "kgggkkkkgggkkkkkkkk.....", "kkkk...kgggk.kgk........", ".......kgggk.kgk........", ".......kkkkk.kkk........");
ROWS(I_KNIFE, "................", "................", "........kkkkkk..", "kkkkkkkkwwwwwwk.", "kbbbbbkklllllk..", "kkkkkkkkkkkkk...", "................", "................");
ROWS(I_BAT, "................................", "....................kkkkkkkkkk..", "..........kkkkkkkkkkttttttttttk.", "kkkkkkkkkktttttttttttttttttttttk",
     "kBBBBBkttttttttttttttttttttttttk", "kkkkkkkkkkbbbbbbbbbbbbbbbbbbbbk.", "..........kkkkkkkkkkkkkkkkkkkk..", "................................");
ROWS(I_CROWBAR, "......................kk", ".....................krk", "kk..................krk.", "rk.................krk..",
     "rrkkkkkkkkkkkkkkkkkrk...", ".rrrrrrrrrrrrrrrrrrk....", "..kkkkkkkkkkkkkkkkk.....", "........................");
ROWS(I_MACHETE, "........................", "........kkkkkkkkkkkkk...", "kkkkkkkklllllllllllllkk.", "kBBBBBBklllllllllllllllk",
     "kkkkkkkklllllllllllllkk.", "........kkkkkkkkkkkkk...", "........................", "........................");
ROWS(I_AMMO, "........", ".kkkkkk.", "kyyyyyyk", "kbbbbbbk", "kbwbwbwk", "kbbbbbbk", "kkkkkkkk", "........");
ROWS(I_BANDAGE, "........", "..kkkk..", ".kwwwwk.", "kwwlwwwk", "kwwwwlwk", ".kwwwwk.", "..kkkk..", "........");
ROWS(I_PILLS, "..kkkk..", "..kwwk..", ".kkkkkk.", ".kooook.", ".kwwwwk.", ".kwrrwk.", ".kooook.", "..kkkk..");
ROWS(I_ANTI, "...kk...", "..kllk..", "..kkkk..", ".kNNNNk.", ".kNwwNk.", ".kNNNNk.", ".kNNNNk.", "..kkkk..");
ROWS(I_MEDKIT, "................", ".....kkkkkk.....", ".....k....k.....", ".kkkkkkkkkkkkkk.", ".kwwwwwwwwwwwwk.", ".kwwwwwrrwwwwwk.", ".kwwwwwrrwwwwwk.", ".kwwwrrrrrrwwwk.",
     ".kwwwrrrrrrwwwk.", ".kwwwwwrrwwwwwk.", ".kwwwwwrrwwwwwk.", ".kwwwwwwwwwwwwk.", ".kllllllllllllk.", ".kkkkkkkkkkkkkk.", "................", "................");
ROWS(I_CANNED, ".kkkkkk.", "kllllllk", "krrrrrrk", "krwwwwrk", "krrrrrrk", "kllllllk", ".kkkkkk.", "........");
ROWS(I_MRE, "........", ".kkkkkk.", ".kbbbbk.", ".kbbbbk.", ".kbyybk.", ".kbyybk.", ".kbbbbk.", ".kbbbbk.", ".kbbbbk.", ".kbwwbk.", ".kbbbbk.", ".kbbbbk.", ".kbbbbk.", ".kbbbbk.", ".kkkkkk.", "........");
ROWS(I_SCRAP, "................", "......kkk.......", "..kkk.klk.kkkk..", ".kgllkkglkkllgk.", "kggglgkkgglggggk", "kgllggglgggllglk", "kkkkkkkkkkkkkkkk", "................");
ROWS(I_PLANKS, "........................", "kkkkkkkkkkkkkkkkkkkkkkk.", "kttttttttbttttttttttttk.", "kkkkkkkkkkkkkkkkkkkkkkkk", "ktttttbtttttttttttttbttk", "kkkkkkkkkkkkkkkkkkkkkkkk", "........................", "........................");
ROWS(I_TOOLS, "................", ".....kkkkkk.....", ".....k....k.....", "kkkkkkkkkkkkkkkk", "krrrrrrrrrrrrrrk", "krrrrrkyykrrrrrk", "kRRRRRRRRRRRRRRk", "kkkkkkkkkkkkkkkk");
ROWS(I_FUEL, "................", "..........kkk...", "...kkkkk.kllk...", "...k...kkkkk....", "..kkkkkkkkkkkk..", "..krrrrrrrrrrk..", "..krRRrrrrRRrk..", "..krrRRrrRRrrk..",
     "..krrrRRRRrrrk..", "..krrrRRRRrrrk..", "..krrRRrrRRrrk..", "..krRRrrrrRRrk..", "..krrrrrrrrrrk..", "..kkkkkkkkkkkk..", "................", "................");
ROWS(I_KEY, "........", ".kkk....", "kyYyk...", "kY.Ykkkk", "kyYyyyyk", ".kkkkykk", ".....k..", "........");
ROWS(I_CARD, "........", "kkkkkkkk", "kNNNNNNk", "kNwwNNNk", "kNNNNyyk", "kNNNNNNk", "kkkkkkkk", "........");
ROWS(I_PACK, "................", "......kkkk......", ".....k....k.....", "...kkkkkkkkkk...", "..kooooooooook..", "..koOOOOOOOOok..", "..koOkkkkkkOok..", "..koOkooookOok..",
     "..koOkooookOok..", "..koOkkkkkkOok..", "..kooooooooook..", "..kooooooooook..", "..kOOOOOOOOOOk..", "...kkkkkkkkkk...", "................", "................");

static void buildItems() {
    auto P = [](const char* const* r, int h) { return spr_parse(r, h); };
#define IT(id, arr) g_item[id] = P(arr, (int)(sizeof(arr) / sizeof(arr[0])))
    IT(IT_PISTOL, I_PISTOL); IT(IT_REVOLVER, I_REVOLVER); IT(IT_SHOTGUN, I_SHOTGUN); IT(IT_RIFLE, I_RIFLE); IT(IT_SMG, I_SMG);
    IT(IT_KNIFE, I_KNIFE); IT(IT_BAT, I_BAT); IT(IT_CROWBAR, I_CROWBAR); IT(IT_MACHETE, I_MACHETE);
    IT(IT_BANDAGE, I_BANDAGE); IT(IT_PILLS, I_PILLS); IT(IT_ANTIBIOTICS, I_ANTI); IT(IT_MEDKIT, I_MEDKIT);
    IT(IT_CANNED, I_CANNED); IT(IT_MRE, I_MRE); IT(IT_SCRAP, I_SCRAP); IT(IT_PLANKS, I_PLANKS); IT(IT_TOOLS, I_TOOLS);
    IT(IT_FUEL, I_FUEL); IT(IT_CELLKEY, I_KEY); IT(IT_KEYCARD, I_CARD); IT(IT_PACK_M, I_PACK);
    g_item[IT_PACK_L] = spr_copy(g_item[IT_PACK_M]);
    { const px f[2] = {pal_char('o'), pal_char('O')}, t[2] = {rgb(122, 90, 58), rgb(80, 58, 38)}; spr_recolor(g_item[IT_PACK_L], f, t, 2); }
    static const px AMMO_COL[4] = {rgb(232, 192, 64), rgb(80, 170, 200), rgb(196, 60, 50), rgb(130, 150, 70)};
    for (int i = 0; i < 4; i++) {
        g_item[IT_AMMO_9MM + i] = P(I_AMMO, 8);
        const px f[1] = {pal_char('y')}; spr_recolor(g_item[IT_AMMO_9MM + i], f, &AMMO_COL[i], 1);
    }
    {   // generator 24x24
        Sprite s = spr_new(24, 24);
        spr_rect(s, 1, 4, 22, 17, pal_char('k')); spr_rect(s, 2, 5, 20, 15, rgb(170, 140, 50));
        spr_rect(s, 4, 8, 9, 9, rgb(60, 60, 60)); for (int y = 9; y < 16; y += 2) spr_rect(s, 5, y, 7, 1, rgb(110, 110, 110));
        spr_rect(s, 15, 8, 5, 3, rgb(40, 40, 40)); spr_set(s, 16, 9, rgb(154, 228, 64)); spr_set(s, 18, 9, rgb(176, 48, 48));
        spr_rect(s, 2, 1, 20, 3, pal_char('k')); spr_rect(s, 3, 2, 18, 1, rgb(90, 90, 90));
        spr_circle(s, 5, 21, 2, pal_char('k')); spr_circle(s, 18, 21, 2, pal_char('k'));
        g_item[IT_GENERATOR] = s;
    }
    {   // medical machine 24x16
        Sprite s = spr_new(24, 16);
        spr_rect(s, 1, 1, 22, 14, pal_char('k')); spr_rect(s, 2, 2, 20, 12, rgb(196, 196, 190));
        spr_rect(s, 4, 4, 10, 6, rgb(30, 50, 40));
        for (int x = 0; x < 9; x++) spr_set(s, 5 + x, 7 - ((x == 3) ? 2 : (x == 4 ? -1 : 0)), rgb(120, 230, 120));
        spr_rect(s, 16, 4, 4, 2, rgb(176, 48, 48)); spr_rect(s, 16, 7, 4, 2, rgb(80, 120, 190));
        g_item[IT_MEDMACHINE] = s;
    }
#undef IT
}

// ---------------------------------------------------------------- settlement buildings (Try 2 style)
// Each facility has its own silhouette and grows with its level. World art may be wider than the
// 3x3-tile plot and taller (drawn bottom-centred on the plot); g_building holds 48x48 menu thumbnails.
Sprite g_bldWorld[BLD_TYPES][BLD_LEVELS];
BldFx g_bldFx[BLD_TYPES][BLD_LEVELS];

static void texRect(Sprite& s, int x, int y, int w, int h, px c, int amt, uint32_t seed) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) spr_set(s, i, j, noisy(c, hash32(seed + j * 977 + i * 31), amt));
}
static void hline(Sprite& s, int x0, int x1, int y, px c) { for (int x = x0; x <= x1; x++) spr_set(s, x, y, c); }
static void vline(Sprite& s, int x, int y0, int y1, px c) { for (int y = y0; y <= y1; y++) spr_set(s, x, y, c); }
static void dline(Sprite& s, int x0, int y0, int x1, int y1, px c) {
    int n = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0); if (!n) n = 1;
    for (int i = 0; i <= n; i++) spr_set(s, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, c);
}
// dark outline around every opaque shape (like the character sprites)
static void outlineAll(Sprite& s) {
    px k = pal_char('k');
    Sprite o = spr_copy(s);
    for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) {
        if (spr_get(o, x, y) != KEYC) continue;
        if (spr_get(o, x - 1, y) != KEYC || spr_get(o, x + 1, y) != KEYC || spr_get(o, x, y - 1) != KEYC || spr_get(o, x, y + 1) != KEYC) spr_set(s, x, y, k);
    }
    free(o.d);
}
static void litWindow(Sprite& s, BldFx& fx, int x, int y, int w, int h, bool cross = true) {
    px frame = rgb(58, 40, 30), glass = rgb(248, 196, 86), hi = rgb(255, 238, 170);
    spr_rect(s, x, y, w, h, frame);
    spr_rect(s, x + 1, y + 1, w - 2, h - 2, glass);
    spr_rect(s, x + 1, y + 1, (w - 2) / 2, 1, hi);
    if (cross) { vline(s, x + w / 2, y + 1, y + h - 2, frame); if (h > 5) hline(s, x + 1, x + w - 2, y + h / 2, frame); }
    if (fx.nwin < 4) { int8_t* r = fx.win[fx.nwin++]; r[0] = (int8_t)x; r[1] = (int8_t)y; r[2] = (int8_t)w; r[3] = (int8_t)h; }
}
// sloped roof seen from the front, wider at the eaves: rows of shingles or corrugated ribs
static void roofTrap(Sprite& s, int x, int y, int w, int h, int inset, px base, px line, int style, uint32_t seed) {
    for (int r = 0; r < h; r++) {
        int in = inset * (h - 1 - r) / (h > 1 ? h - 1 : 1), x0 = x + in, x1 = x + w - 1 - in;
        for (int i = x0; i <= x1; i++) {
            px c = noisy(base, hash32(seed + r * 131 + i), 7);
            if (style == 0) { if (r % 3 == 2) c = line; else if (((i + (r / 3) * 2) % 4) == 0) c = gfx_mix(c, line, 140); }   // shingles
            if (style == 1) { if ((i & 1) == 0) c = gfx_scale(c, 215); if (hash32(seed + i * 7 + r / 4) % 9 == 0) c = gfx_mix(c, rgb(150, 82, 40), 150); }   // corrugated + rust
            if (style == 2) { if (r % 4 == 3) c = line; }   // canvas / planks
            if (r == 0) c = gfx_scale(c, 180);
            spr_set(s, i, y + r, c);
        }
    }
    hline(s, x, x + w - 1, y + h, gfx_scale(line, 140));   // eave shadow line
}

static void bldQuarters(Sprite& s, BldFx& fx, int lv, int W, int H) {
    int wallH = 22, roofH = H - wallH - 2, wy = H - wallH - 1;
    // wall: stone footing + timber planks
    texRect(s, 4, wy, W - 8, wallH, rgb(132, 106, 78), 10, 11);
    for (int x = 6; x < W - 4; x += 5) vline(s, x, wy, H - 6, rgb(100, 80, 58));
    for (int x = 4; x < W - 4; x++) for (int y = H - 5; y < H - 1; y++) spr_set(s, x, y, noisy(((x / 4 + y) & 1) ? rgb(118, 116, 110) : rgb(98, 96, 92), hash32(x * 13 + y), 10));
    // roof (slate)
    roofTrap(s, 1, 2, W - 2, roofH, 6, rgb(96, 104, 128), rgb(62, 68, 88), 0, 77 + lv);
    if (lv >= 2) {   // dormers with lit windows
        for (int k = 0; k < 2; k++) {
            int dx = k ? W - 20 : 12, dy = 6;
            texRect(s, dx, dy + 3, 9, 7, rgb(132, 106, 78), 8, 5 + k);
            for (int r = 0; r < 4; r++) hline(s, dx - 1 + r, dx + 9 - r, dy + 3 - r, rgb(80, 88, 110));
            litWindow(s, fx, dx + 2, dy + 4, 5, 5, false);
        }
    }
    if (lv >= 3) { texRect(s, W - 14, 0, 5, 8, rgb(120, 116, 110), 14, 9); hline(s, W - 15, W - 9, 0, rgb(80, 78, 74)); fx.smokeX = (int8_t)(W - 12); fx.smokeY = 0; }
    // doors + windows
    int dw = lv >= 2 ? 12 : 8, dx = W / 2 - dw / 2;
    spr_rect(s, dx - 1, H - 17, dw + 2, 16, rgb(52, 36, 28));
    texRect(s, dx, H - 16, dw, 15, rgb(84, 60, 44), 8, 21);
    if (lv >= 2) vline(s, dx + dw / 2, H - 16, H - 2, rgb(52, 36, 28));
    spr_set(s, dx + dw / 2 - 2, H - 9, rgb(232, 192, 64)); spr_set(s, dx + dw / 2 + 1, H - 9, rgb(232, 192, 64));
    litWindow(s, fx, 8, wy + 5, 10, 8); litWindow(s, fx, W - 18, wy + 5, 10, 8);
    texRect(s, dx - 2, H - 2, dw + 4, 2, rgb(128, 126, 120), 10, 3);   // stone step
    if (lv >= 3) { spr_rect(s, dx - 5, wy + 4, 2, 3, rgb(60, 50, 40)); spr_set(s, dx - 5, wy + 7, rgb(255, 220, 120)); spr_set(s, dx - 4, wy + 7, rgb(255, 220, 120)); }
}

static void bldTower(Sprite& s, BldFx& fx, int lv, int W, int H) {
    int cabTop = 7, cabBot = 33, deck = cabBot + 1;
    px wood = rgb(124, 88, 56), woodD = rgb(86, 60, 38), woodL = rgb(156, 116, 76);
    // legs (back pair darker), braces, ladder
    texRect(s, 9, deck, 3, H - deck - 1, woodD, 10, 1); texRect(s, W - 12, deck, 3, H - deck - 1, woodD, 10, 2);
    texRect(s, 4, deck, 4, H - deck, wood, 10, 3); texRect(s, W - 8, deck, 4, H - deck, wood, 10, 4);
    for (int k = 0; k < 1; k++) {
        int y0 = deck + 3, y1 = H - 4; (void)k;   // one heavy X brace
        dline(s, 7, y0, W - 8, y1, woodL); dline(s, W - 8, y0, 7, y1, woodL);
    }
    vline(s, W / 2 - 3, deck, H - 1, woodD); vline(s, W / 2 + 2, deck, H - 1, woodD);
    for (int y = deck + 3; y < H - 1; y += 4) hline(s, W / 2 - 3, W / 2 + 2, y, woodL);
    // deck + cabin
    texRect(s, 1, deck - 1, W - 2, 4, woodL, 10, 5); hline(s, 1, W - 2, deck + 2, woodD);
    texRect(s, 4, cabTop + 6, W - 8, cabBot - cabTop - 6, wood, 10, 6);
    for (int x = 6; x < W - 4; x += 4) vline(s, x, cabTop + 6, cabBot - 1, woodD);
    litWindow(s, fx, 5, cabTop + 8, 13, 12); litWindow(s, fx, W - 18, cabTop + 8, 13, 12);
    roofTrap(s, 0, cabTop - 4, W, 10, 5, rgb(92, 66, 44), rgb(60, 42, 28), 2, 9);
    if (lv >= 2) {   // railing + sandbags at the base
        for (int x = 2; x < W - 2; x += 5) vline(s, x, deck - 5, deck - 1, woodL);
        hline(s, 1, W - 2, deck - 5, woodL);
        for (int k = 0; k < 4; k++) { int bx = 1 + k * (W - 6) / 3; texRect(s, bx, H - 5, 6, 4, rgb(176, 156, 112), 12, 40 + k); hline(s, bx, bx + 5, H - 2, rgb(130, 112, 78)); }
    }
    if (lv >= 3) { vline(s, W - 4, 0, cabTop - 2, rgb(70, 70, 74)); fx.flagX = (int8_t)(W - 3); fx.flagY = 0; spr_rect(s, 3, cabTop - 1, 4, 3, rgb(200, 200, 190)); spr_set(s, 2, cabTop, rgb(255, 250, 200)); }
}

static void bldInfirmary(Sprite& s, BldFx& fx, int lv, int W, int H) {
    px canvas = rgb(218, 214, 200), canvasD = rgb(180, 176, 162), red = rgb(196, 44, 40);
    int eave = H - 16;
    roofTrap(s, 2, 4, W - 4, eave - 4, 10, canvas, canvasD, 2, 33 + lv);
    hline(s, 12, W - 13, 4, gfx_scale(canvas, 230));   // ridge pole
    texRect(s, 4, eave + 1, W - 8, H - eave - 2, canvasD, 6, 7);
    // door flap tied back
    int cx = W / 2;
    for (int r = 0; r < 13; r++) hline(s, cx - r / 2 - 1, cx + r / 2, eave + 1 + r, rgb(52, 46, 44));
    dline(s, cx - 1, eave + 1, cx - 8, H - 3, canvas); dline(s, cx, eave + 1, cx + 7, H - 3, canvas);
    // red crosses: roof + wall
    spr_rect(s, cx - 2, 10, 4, 12, red); spr_rect(s, cx - 6, 14, 12, 4, red);
    spr_rect(s, 8, eave + 4, 2, 6, red); spr_rect(s, 6, eave + 6, 6, 2, red);
    // guy ropes + pegs
    dline(s, 2, eave, 0, H - 1, rgb(160, 150, 120)); dline(s, W - 3, eave, W - 1, H - 1, rgb(160, 150, 120));
    if (lv >= 2) {   // supply crate with a cross + lantern glow inside
        texRect(s, W - 14, H - 9, 9, 8, rgb(150, 116, 74), 10, 12); spr_rect(s, W - 10, H - 7, 1, 4, red); spr_rect(s, W - 12, H - 6, 5, 1, red);
        fx.win[fx.nwin][0] = (int8_t)(cx - 3); fx.win[fx.nwin][1] = (int8_t)(eave + 6); fx.win[fx.nwin][2] = 6; fx.win[fx.nwin][3] = 6; fx.nwin++;
    }
    if (lv >= 3) { vline(s, 5, H - 18, H - 2, rgb(150, 150, 156)); spr_rect(s, 4, H - 19, 4, 4, rgb(170, 210, 230)); }   // IV stand
}

static void bldWorkshop(Sprite& s, BldFx& fx, int lv, int W, int H) {
    int roofB = 20;
    px post = rgb(112, 80, 52);
    // back wall in shadow + pegboard with tools
    texRect(s, 3, roofB, W - 6, H - roofB - 3, rgb(58, 46, 40), 10, 3);
    for (int x = 5; x < W - 4; x += 5) vline(s, x, roofB, H - 4, rgb(48, 38, 34));
    texRect(s, 10, roofB + 3, W - 20, 10, rgb(150, 120, 84), 8, 4);
    px tool = rgb(176, 178, 184);
    vline(s, 14, roofB + 4, roofB + 11, tool); spr_rect(s, 13, roofB + 4, 3, 2, tool);           // wrench
    vline(s, 20, roofB + 6, roofB + 11, rgb(120, 84, 50)); spr_rect(s, 18, roofB + 4, 5, 2, tool); // hammer
    for (int i = 0; i < 8; i++) spr_set(s, 26 + i, roofB + 5 + i / 3, tool);                        // saw
    // corrugated tin lean-to on posts
    roofTrap(s, 0, 3, W, roofB - 3, 3, rgb(150, 154, 160), rgb(100, 104, 110), 1, 51 + lv);
    for (int k = 0; k < 3; k++) { int x = k == 0 ? 2 : (k == 1 ? W / 2 - 1 : W - 5); texRect(s, x, roofB + 1, 3, H - roofB - 2, post, 10, 60 + k); }
    // workbench with a vice
    texRect(s, 8, H - 12, W - 16, 4, rgb(150, 108, 66), 8, 70); hline(s, 8, W - 9, H - 9, rgb(100, 70, 44));
    vline(s, 10, H - 8, H - 2, rgb(100, 70, 44)); vline(s, W - 11, H - 8, H - 2, rgb(100, 70, 44));
    spr_rect(s, W - 20, H - 15, 6, 3, rgb(90, 92, 98)); spr_rect(s, 16, H - 14, 8, 2, rgb(170, 60, 40));   // vice, red toolbox
    if (lv >= 2) { spr_rect(s, W / 2 + 4, H - 6, 7, 4, rgb(70, 70, 76)); spr_rect(s, W / 2 + 3, H - 7, 9, 2, rgb(96, 96, 104)); }   // anvil
    if (lv >= 3) { vline(s, W / 2 - 8, roofB + 1, roofB + 4, rgb(60, 60, 60)); spr_rect(s, W / 2 - 10, roofB + 5, 5, 3, rgb(255, 214, 110));
                   fx.win[fx.nwin][0] = (int8_t)(W / 2 - 12); fx.win[fx.nwin][1] = (int8_t)(roofB + 4); fx.win[fx.nwin][2] = 9; fx.win[fx.nwin][3] = 6; fx.nwin++; }
}

static void container(Sprite& s, int x, int y, int w, int h, px base, uint32_t seed) {
    texRect(s, x, y, w, 4, gfx_mix(base, rgb(255, 255, 255), 50), 8, seed);   // top face
    for (int i = x; i < x + w; i++) for (int j = y + 4; j < y + h; j++) {
        px c = noisy(base, hash32(seed + i * 17 + j), 8);
        if ((i - x) % 3 == 0) c = gfx_scale(c, 200); else if ((i - x) % 3 == 1) c = gfx_mix(c, rgb(255, 255, 255), 24);
        spr_set(s, i, j, c);
    }
    hline(s, x, x + w - 1, y + 4, gfx_scale(base, 150));
    for (int k = 0; k < 4; k++) vline(s, x + w - 3 - k * 3, y + 6, y + h - 2, rgb(190, 190, 186));   // door lock rods
    spr_rect(s, x + 4, y + 8, 8, 2, rgb(230, 230, 220));   // stencil
}
static void bldStorage(Sprite& s, BldFx& fx, int lv, int W, int H) {
    (void)fx;
    int ch = 22;
    container(s, 1, H - ch - 1, W - 2 - (lv >= 3 ? 12 : 0), ch, rgb(156, 66, 46), 5);
    if (lv >= 2) container(s, 6, H - 2 * ch + 2, W - 18, ch - 2, rgb(52, 112, 114), 9);
    if (lv >= 3) for (int k = 0; k < 2; k++) { int bx = W - 12, by = H - 8 - k * 7; texRect(s, bx, by, 10, 7, rgb(160, 124, 80), 10, 30 + k); hline(s, bx, bx + 9, by + 3, rgb(110, 84, 54)); }
}

static void bldKitchen(Sprite& s, BldFx& fx, int lv, int W, int H) {
    px frame = rgb(200, 204, 200), glass = rgb(150, 196, 188), leaf = rgb(84, 150, 64), leafD = rgb(52, 104, 46);
    int gw = W - 14, roofB = 18;
    // stove-pipe cookhouse at the right
    texRect(s, gw, roofB + 2, W - gw - 1, H - roofB - 3, rgb(128, 104, 76), 10, 2);
    roofTrap(s, gw - 1, roofB - 4, W - gw + 1, 6, 2, rgb(110, 80, 56), rgb(80, 58, 40), 2, 3);
    vline(s, W - 5, 2, roofB - 3, rgb(40, 40, 44)); vline(s, W - 4, 2, roofB - 3, rgb(70, 70, 76));
    fx.smokeX = (int8_t)(W - 5); fx.smokeY = 1;
    litWindow(s, fx, gw + 2, roofB + 6, 8, 6);
    // greenhouse: glass gable roof + walls full of plants
    for (int r = 0; r < roofB - 2; r++) {
        int in = (roofB - 3 - r) / 2;
        for (int i = 1 + in; i < gw - 1 - in; i++) spr_set(s, i, 2 + r, ((i + r) % 7 == 0) ? rgb(220, 240, 236) : gfx_mix(glass, leaf, (hash32(i * 3 + r) % 3) ? 60 : 140));
    }
    for (int i = 1; i < gw - 1; i++) for (int j = roofB; j < H - 2; j++) spr_set(s, i, j, (hash32(i * 31 + j * 7 + lv) % 4) ? gfx_mix(glass, leaf, 170) : leafD);
    for (int x = 1; x < gw; x += 6) vline(s, x, roofB, H - 2, frame);
    hline(s, 1, gw - 2, roofB, frame); hline(s, 1, gw - 2, H - 2, frame);
    for (int r = 0; r < 4; r++) for (int x = 3; x < gw - 2; x += 4) spr_set(s, x, H - 6 - r * 3, r & 1 ? rgb(220, 70, 50) : rgb(240, 170, 40));   // tomatoes / squash
    spr_rect(s, gw / 2 - 2, H - 10, 5, 8, rgb(80, 120, 110));   // door
    if (lv >= 2) { texRect(s, W - 9, H - 9, 7, 8, rgb(70, 92, 120), 10, 7); hline(s, W - 9, W - 3, H - 9, rgb(110, 140, 170)); }   // rain barrel
}

static void buildBuildings() {
    static const int SZ[BLD_TYPES][BLD_LEVELS][2] = {   // world sprite size per level (w, h)
        {{48, 48}, {48, 46}, {56, 50}, {58, 52}},   // infirmary tent
        {{48, 48}, {52, 50}, {58, 52}, {62, 54}},   // workshop shed
        {{48, 48}, {50, 26}, {50, 46}, {60, 46}},   // storage containers
        {{48, 48}, {48, 50}, {54, 52}, {58, 54}},   // greenhouse + cookhouse
        {{48, 48}, {50, 52}, {60, 62}, {62, 66}},   // living quarters house
        {{48, 48}, {42, 64}, {44, 68}, {46, 72}},   // watchtower: plank cabin on stout legs
    };
    for (int b = 0; b < BLD_TYPES; b++) for (int lv = 0; lv < BLD_LEVELS; lv++) {
        BldFx& fx = g_bldFx[b][lv]; fx = BldFx{}; fx.smokeX = fx.flagX = -1;
        int W = SZ[b][lv][0], H = SZ[b][lv][1];
        Sprite s = spr_new(W, H);
        if (lv == 0) {  // empty plot: staked dirt
            for (int y = 6; y < 46; y++) for (int x = 3; x < 45; x++) s.d[y * 48 + x] = noisy(rgb(92, 72, 52), hash32(b * 999 + y * 48 + x), 18);
            for (int i = 3; i < 45; i += 3) { spr_set(s, i, 6, rgb(200, 190, 160)); spr_set(s, i, 45, rgb(200, 190, 160)); }
            for (int i = 6; i < 46; i += 3) { spr_set(s, 3, i, rgb(200, 190, 160)); spr_set(s, 44, i, rgb(200, 190, 160)); }
            int cs[4][2] = {{2, 4}, {44, 4}, {2, 43}, {44, 43}};
            for (auto& c : cs) spr_rect(s, c[0], c[1], 2, 4, rgb(120, 90, 56));
            g_bldWorld[b][lv] = s; g_building[b][lv] = spr_copy(s); continue;
        }
        // draw inside a 1px margin so the outline fits
        Sprite in = spr_new(W - 2, H - 2);
        switch (b) {
        case B_QUARTERS: bldQuarters(in, fx, lv, W - 2, H - 2); break;
        case B_DEFENSES: bldTower(in, fx, lv, W - 2, H - 2); break;
        case B_INFIRMARY: bldInfirmary(in, fx, lv, W - 2, H - 2); break;
        case B_WORKSHOP: bldWorkshop(in, fx, lv, W - 2, H - 2); break;
        case B_STORAGE: bldStorage(in, fx, lv, W - 2, H - 2); break;
        default: bldKitchen(in, fx, lv, W - 2, H - 2); break;
        }
        for (int y = 0; y < in.h; y++) for (int x = 0; x < in.w; x++) { px v = spr_get(in, x, y); if (v != KEYC) spr_set(s, x + 1, y + 1, v); }
        free(in.d);
        outlineAll(s);
        for (int k = 0; k < fx.nwin; k++) { fx.win[k][0]++; fx.win[k][1]++; }
        if (fx.smokeX >= 0) { fx.smokeX++; fx.smokeY++; }
        if (fx.flagX >= 0) { fx.flagX++; fx.flagY++; }
        g_bldWorld[b][lv] = s;
        // 48x48 menu thumbnail: nearest-neighbour fit, bottom-centred
        Sprite t = spr_new(48, 48);
        float k = W > H ? 48.0f / W : 48.0f / H; if (k > 1) k = 1;
        int tw = (int)(W * k), th = (int)(H * k), ox = (48 - tw) / 2, oy = 48 - th;
        for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) spr_set(t, ox + x, oy + y, spr_get(s, (int)(x / k), (int)(y / k)));
        g_building[b][lv] = t;
    }
}


// ---------------------------------------------------------------- held weapons (side view, muzzle pointing right)
// grip = pixel under the hand, muzzle = where the flash starts
Sprite g_held[40];
int8_t g_heldGrip[40][2], g_heldMuzzle[40][2];
ROWS(H_PISTOL,   "kkkkkkkk", "kllllllk", "kggggggk", "kddkkkk.", "kddk....", "kkkk....");
ROWS(H_REVOLVER, "kkkkkkkkk", "kdklllllk", "kddgggggk", "kBBkkkkk.", "kBBk.....", "kkkk.....");
ROWS(H_SMG,      ".kkkkkkkkkk", "kdddglllldk", "kddddddgkkk", "kkdkkdk....", "..kkkdk....", "....kkk....");
ROWS(H_SHOTGUN,  "......kkkkkkkkkk", "kkkkkkddddddddlk", "kbbbbkdkBBBBBkkk", "kbbkkkkkkkkkk...", "kkk.............");
ROWS(H_RIFLE,    "......kkkk......", "kkkkkkkllkkkkkkk", "kbbbbbkddddddddk", "kbBkkbbbbbbbbkk.", "kkk..kkkkkkkkk..");
ROWS(H_KNIFE,    "...kkkk..", "kBBklllwk", "kkkkkkkk.");
ROWS(H_BAT,      ".......kkkkk.", "kkkkkkkbbbbbk", "kBbbbbbbbbbbk", "kkkkkkkkkkkk.");
ROWS(H_CROWBAR,  "kkkkkkkkkkk.", "kgllllllllgk", "kkkkkkkkkkgk", "..........kk");
ROWS(H_MACHETE,  "....kkkkkkk.", "kkkkklllllwk", "kBBkkggggkk.", "kkkk.kkkk...");
static void buildHeld() {
    struct H { int id; const char* const* r; int n, gx, gy, mx, my; };
#define HW(id, a, gx, gy, mx, my) {id, a, (int)(sizeof(a) / sizeof(a[0])), gx, gy, mx, my}
    const H hs[] = {
        HW(IT_PISTOL, H_PISTOL, 2, 3, 8, 1), HW(IT_REVOLVER, H_REVOLVER, 2, 3, 9, 1), HW(IT_SMG, H_SMG, 3, 3, 11, 1),
        HW(IT_SHOTGUN, H_SHOTGUN, 6, 3, 16, 1), HW(IT_RIFLE, H_RIFLE, 6, 3, 16, 2), HW(IT_KNIFE, H_KNIFE, 1, 1, 9, 1),
        HW(IT_BAT, H_BAT, 2, 2, 13, 1), HW(IT_CROWBAR, H_CROWBAR, 1, 1, 12, 1), HW(IT_MACHETE, H_MACHETE, 1, 2, 12, 1),
    };
#undef HW
    for (const H& h : hs) {
        g_held[h.id] = spr_parse(h.r, h.n);
        g_heldGrip[h.id][0] = (int8_t)h.gx; g_heldGrip[h.id][1] = (int8_t)h.gy;
        g_heldMuzzle[h.id][0] = (int8_t)h.mx; g_heldMuzzle[h.id][1] = (int8_t)h.my;
    }
}

void art_init() {
    static bool done = false;
    if (done) return;
    done = true;
    buildTiles();
    buildCharacters();
    buildHeld();
    buildPortraits();
    buildProps();
    buildIcons();
    buildItems();
    buildBuildings();
    // logo zombie head for title (scaled portrait-like)
    bool f;
    g_spr[S_LOGO_ZOMBIE] = spr_copy(art_char(true, 0, 2, P_STAND, f));
}
