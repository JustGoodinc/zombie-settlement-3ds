// All game art is original and generated here at load time: hand-drawn pixel
// templates (palette letters, see pal_char) plus procedural tiles/props.
// tools/zs_tool --gallery exports everything to assets/gallery/*.png for review.
#include "art.h"
#include "rng.h"
#include "../gameplay/content.h"
#include <stdlib.h>
#include <string.h>

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

// ---------------------------------------------------------------- characters (16x24 chibi, Try 2 style)
// Built from hand-drawn parts so every animation frame is drawn, not smeared:
// head (rows 0-13) + torso/arms (6 rows) + legs (4 rows).  Views: 0 = facing camera, 1 = side (right), 2 = back.
// Letters: k outline, h/H/j hair (base/light/dark), s/u/S skin (base/light/shade), e eyes, b/M/m shirt,
// B belt, n/p pants, x shoes, o/O backpack, y badge.
enum { CW = 16, CH = 24 };
ROWS(HD_FRONT,
"................", "....kkkkkkkk....", "..kkHHHHhhhhkk..", ".kHHHhhhhhhhhjk.", ".kHhhhhhhhhhhjk.", ".khhhhhhhhhhhjk.",
".khhjhhhhhjhhjk.", ".kjhhshhhhshhjk.", ".kjsjjssssjjsjk.", ".kSswessssweSSk.", ".kSseessssseeSk.", ".ksissssssssisk.",
"..kSsssSSsssSk..", "...kkSSSSSSkk...");   // big 2x2 eyes with a glint, brows, blush, jagged bangs
ROWS(HD_SIDE,
"................", "....kkkkkkkk....", "..kkHHHHhhhhkk..", ".kHHHhhhhhhhhhk.", ".kHhhhhhhhhhhhk.", ".khhhhhhhhhhhjk.",
".khhhhhjhhhhjjk.", ".kjhhhhshhhshjk.", ".kjhhhsjjssjjsk.", ".kjhSsswesswesk.", ".kjhSsseesseesk.", ".kSSssssssssisk.",
"..kSSssssSSssk..", "...kkSSSSSSkk...");   // 3/4 view: face turned right, back of the head on the left
ROWS(HD_BACK,
"................", "....kkkkkkkk....", "..kkhhhhhhhhkk..", ".kHHhhhhhhhhhjk.", ".kHhhhhhhhhhhjk.", ".khhhhhhhhhhhjk.",
".khhhhhhhhhhjjk.", ".kjhhhhhhhhhjjk.", ".kjhhhhhhhhhjjk.", ".kjjhhhhhhhjjjk.", ".kSjjjjjjjjjjSk.", ".kSjjjjjjjjjjSk.",
"..kSssssssssSk..", "...kkSSSSSSkk...");
// torso + arms, 6 rows (drawn at row 14): [view][arm state]
enum { A_IDLE, A_SWING_A, A_SWING_B, A_AIM, A_REACH, A_ZOMBIE, A_COUNT };
static const char* const TORSO[3][A_COUNT][6] = {
  { // front
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", ".kbMkbbbbbbkbmk.", ".kbmkbbbbbbkmmk.", ".kuskBBBYBBksSk.", ".kkkknnnnnnkkkk."},
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", ".kbMkbbbbbbkbmk.", ".kbmkbbbbbbksSk.", ".kuskBBBYBBkkkk.", ".kkkknnnnnnk...."},
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", ".kbMkbbbbbbkbmk.", ".kusbbbbbbbkmmk.", ".kkkkBBBYBBksSk.", "....knnnnnnkkkk."},
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", ".kbMmbbbbbbmbmk.", ".kbmkmbbbbmkbmk.", ".kbmkBuussBkbmk.", ".kkkknkSSknkkkk."},
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", ".kbMmbbbbbbmbmk.", ".kbmkmbbbbmkbmk.", ".kbmkBuussBkbmk.", ".kkkknkSSknkkkk."},
    {"..kkMMbssbbbkk..", ".kMMbbbmmbbbbmk.", "kuskbbbbbbbbkSuk", "kkkkbbbbbbbbkkkk", "...kBBBBBBBBk...", "...kknnnnnnkk..."},
  },
  { // side (3/4, facing right): body toward the camera, the near arm does the work (Try 2)
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbmk.", ".kbMkbbbbbbkbmk.", ".kbmkbbbbbbkbmk.", ".kuskBBBBBBksSk.", ".kkkknnnnnnkkkk."},
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbmk.", ".kbMkbbbbbbkmmk.", ".kbmkbbbbbbbkmmk", ".kuskBBBBBBk.kus", ".kkkknnnnnnk..kk"},
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbmk.", ".kbMkbbbbbbbbmk.", ".kbmkbbbbbbbbmk.", ".kuskBBBBBBBBk..", ".kkkknnnnnnkkk.."},
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbbkk", ".kbMkbbbbbbbmmus", ".kbmkbbbbbbbkkkk", ".kuskBBBBBBk....", ".kkkknnnnnnk...."},
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbmk.", ".kbMkbbbbbbbmbk.", ".kbmkbbbbbbbkmmk", ".kuskBBBBBBk.kus", ".kkkknnnnnnk..kk"},
    {"..kkMMbbbssbkk..", ".kMMbbbbbmmbbbkk", ".kbMbbbbbbbbmmus", ".kbmbbbbbbbbkkkk", ".kbmkBBBBBBkmmus", ".kkkknnnnnnkkkkk"},
  },
  { // back
    {"..kkbbbbbbbbkk..", ".kbbkoooooOkbmk.", ".kbbkoOOOOOkbmk.", ".kbmkoooooOkbmk.", ".kuskOOOOOOksSk.", ".kkkknnnnnnkkkk."},
    {"..kkbbbbbbbbkk..", ".kbbkoooooOkbmk.", ".kbbkoOOOOOkbmk.", ".kbmkoooooOksSk.", ".kuskOOOOOOkkkk.", ".kkkknnnnnnk...."},
    {"..kkbbbbbbbbkk..", ".kbbkoooooOkbmk.", ".kbbkoOOOOOkbmk.", ".kuskoooooOkbmk.", ".kkkkOOOOOOksSk.", "....knnnnnnkkkk."},
    {"..kkbbbbbbbbkk..", ".kbbkoooooOkbbk.", ".kbmkoOOOOOkmbk.", ".kbmkoooooOkmbk.", "..kkkOOOOOOkkk..", "....knnnnnnk...."},
    {"..kkbbbbbbbbkk..", ".kbbkoooooOkbbk.", ".kbmkoOOOOOkmbk.", ".kbmkoooooOkmbk.", "..kkkOOOOOOkkk..", "....knnnnnnk...."},
    {"..kkbbbbbbbbkk..", ".kbbbbbbbbbbbmk.", "kuskbbbbbbbbkSuk", "kkkkbbbbbbbbkkkk", "...kBBBBBBBBk...", "...kknnnnnnkk..."},
  },
};
// legs, 4 rows (drawn at row 20): [view][leg state]
enum { L_STAND, L_STEP_A, L_STEP_B, L_RUN_A, L_RUN_B, L_CROUCH, L_COUNT };
static const char* const LEGS[3][L_COUNT][4] = {
  { // front
    {"....knnnnnnk....", "....knpkknpk....", "....kxxkkxxk....", "....kkkkkkkk...."},
    {"....knnnnnnk....", "....knpkkxxk....", "....kxxkkkkk....", "....kkkk........"},
    {"....knnnnnnk....", "....kxxkknpk....", "....kkkkkxxk....", "........kkkk...."},
    {"....knnnnnnk....", "...knnpkkxxk....", "...kxxkkkkk.....", "...kkkk........."},
    {"....knnnnnnk....", "....kxxkknnpk...", ".....kkkkkxxk...", ".........kkkk..."},
    {"................", "................", "...knnnnnnnnk...", "...kxxkkkkxxk..."},
  },
  { // side
    {"....knnnnnnk....", "....knpkknpk....", "....kxxkkxxxk...", "....kkkkkkkkk..."},
    {"....knnnnnnk....", "...kpppk.knnnk..", "..kxxxk...kxxxk.", "..kkkkk...kkkkk."},
    {"....knnnnnnk....", "...knnnk.kpppk..", "..kxxxk...kxxxk.", "..kkkkk...kkkkk."},
    {"...knnnnnnnk....", "..kpppk..knnnk..", ".kxxxk.....kxxxk", ".kkkk.......kkkk"},
    {"....knnnnnnnk...", "...knnnk..kpppk.", "..kxxxk....kxxxk", "..kkkk......kkkk"},
    {"................", "................", "....knnnnnnnk...", "...kxxxkkkxxxk.."},
  },
  { // back
    {"....knnnnnnk....", "....kpnkkpnk....", "....kxxkkxxk....", "....kkkkkkkk...."},
    {"....knnnnnnk....", "....kpnkkxxk....", "....kxxkkkkk....", "....kkkk........"},
    {"....knnnnnnk....", "....kxxkkpnk....", "....kkkkkxxk....", "........kkkk...."},
    {"....knnnnnnk....", "...kpnnkkxxk....", "...kxxkkkkk.....", "...kkkk........."},
    {"....knnnnnnk....", "....kxxkkpnnk...", ".....kkkkkxxk...", ".........kkkk..."},
    {"................", "................", "...knnnnnnnnk...", "...kxxkkkkxxk..."},
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
            for (int x = 1; x < 15; x++) spr_set(h, x, 7, J);
            spr_set(h, 0, 7, k); spr_set(h, 15, 7, k);
            spr_set(h, 7, 3, pal_char('y')); spr_set(h, 8, 3, pal_char('y'));
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
        for (int y = 11; y <= 12; y++) for (int x = 6; x <= 9; x++) spr_set(s, x, y + drop, (x == 6 || x == 9) ? blood : mouth);
        for (int y = 3; y <= 6; y++) for (int x = 2; x <= 4 - (y > 4); x++) if (spr_get(s, x, y + drop) != pal_char('k')) spr_set(s, x, y + drop, ((x + y) & 1) ? blood : dried);   // smear on the scalp
    }
    if (view == 1) { spr_set(s, 9, 11 + drop, mouth); spr_set(s, 10, 11 + drop, mouth); spr_set(s, 9, 12 + drop, blood); spr_set(s, 10, 12 + drop, blood); spr_set(s, 10, 13 + drop, dried); }
    px shirt = p.shirt, shirtD = gfx_scale(p.shirt, 170), shirtL = gfx_mix(p.shirt, rgb(255, 255, 255), 60);
    for (int i = 0; i < s.w * s.h; i++) {
        px& v = s.d[i];
        if (v == gfx_mix(p.hair, rgb(255, 255, 255), 70)) v = p.skin;                      // balding: pale scalp, a few dark strands
        else if (v == p.hair) v = r.chance(0.07f) ? p.hair : gfx_mix(p.skin, p.skinS, 128);
        else if (v == gfx_scale(p.hair, 165)) v = r.chance(0.15f) ? p.hair : p.skinS;
        else if (v == shirt || v == shirtD || v == shirtL) { if (r.chance(0.10f)) v = p.skinS; else if (r.chance(0.18f)) v = r.chance(0.6f) ? blood : dried; }
        else if (v == p.pants && r.chance(0.08f)) v = dried;
    }
    if (view == 0) {

        spr_set(s, 10, 6 + drop, blood); spr_set(s, 10, 7 + drop, dried); spr_set(s, 11, 7 + drop, blood);   // scalp gash
    }
    if (view == 1) for (int y = 14; y <= 17; y++) if (spr_get(s, 10, y + drop) != KEYC) spr_set(s, 10, y + drop, blood);
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
                if (!z && cp.style == 1 && view != 2 && p != P_DEAD && p != P_LEAN_BACK && p != P_LEAN_FWD && p != P_HURT && p != P_FALL && p != P_RUN_A && p != P_RUN_B && p != P_REACH) spr_set(c, view == 0 ? 10 : 11, 15 + drop[p], pal_char('y'));   // chest badge
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
ROWS(PORTRAIT,
"........................", "........kkkkkkkk........", "......kkhhhhhhhhkk......", ".....khhhhhhhhhhhhk.....",
"....khhhhhhhhhhhhhhk....", "....khhhhhhhhhhhhhhk....", "....khsssssssssssshk....", "...khsssssssssssssshk...",
"...ksskkksssssskkkssk...", "...ksswesssssssswessk...", "...ksssssssSSsssssssk...", "...ksssssssSSsssssssk...",
"...kSssssssssssssssSk...", "...kssssssSkkSssssssk...", "...kSssssssssssssssSk...", "....kSssssssssssssSk....",
".....kkSssssssssSkk.....", ".......kssssssssk.......", ".....kkbbbssssbbbkk.....", "...kkbbbbbbssbbbbbbkk...",
"..kbbbbbbbbbbbbbbbbbbk..", ".kbbbbbbbbbbbbbbbbbbbbk.", ".kbbbbbbbbbbbbbbbbbbbbk.", ".kbbbbbbbbbbbbbbbbbbbbk.");

static void buildPortraits() {
    Sprite base = PARSE(PORTRAIT);
    for (int v = 0; v < PORTRAITS; v++) {
        Sprite s = spr_copy(base);
        if (v & 1) {  // long hair variant
            px h = pal_char('h'), k = pal_char('k');
            for (int y = 6; y <= 18; y++) {
                spr_set(s, 2, y, k); spr_set(s, 3, y, h); spr_set(s, 4, y, h);
                spr_set(s, 21, y, k); spr_set(s, 20, y, h); spr_set(s, 19, y, h);
            }
        }
        const CharPal& p = SURV_PAL[v];
        const px from[5] = {pal_char('h'), pal_char('s'), pal_char('S'), pal_char('b'), pal_char('n')};
        const px to[5] = {p.hair, p.skin, p.skinS, p.shirt, p.pants};
        spr_recolor(s, from, to, 5);
        // backdrop
        Sprite out = spr_new(24, 24);
        for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
            px v2 = spr_get(s, x, y);
            out.d[y * 24 + x] = v2 != KEYC ? v2 : gfx_mix(rgb(52, 50, 56), rgb(36, 34, 40), y * 10);
        }
        freeS(s);
        g_portrait[v] = out;
    }
    freeS(base);
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
        Sprite dt = tileNoise(rgb(92, 72, 52), 20, sd + 3);
        for (int i = 0; i < 3; i++) spr_set(dt, r.range(16), r.range(16), rgb(120, 110, 96));
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
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int log = x / 4; px b = (log & 1) ? rgb(104, 76, 48) : rgb(92, 66, 42);
            if ((x & 3) == 3) b = rgb(54, 38, 26);
            if (y < 3 && ((x & 3) == 0 || (x & 3) == 3) && y < 2) b = KEYC;  // sharpened tops
            pl.d[y * 16 + x] = b == KEYC ? rgb(70, 84, 44) : noisy(b, hash32(sd * 19 + y * 16 + x), 10);
        }
        for (int x = 0; x < 16; x++) spr_set(pl, x, 9, rgb(60, 44, 30));
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
    {   // police car (top-down, 32x16): white body, navy doors, light bar
        Sprite s = spr_new(32, 18);
        spr_rect(s, 1, 2, 30, 14, pal_char('k'));
        spr_rect(s, 2, 3, 28, 12, rgb(200, 200, 196));
        spr_rect(s, 10, 3, 12, 12, rgb(42, 58, 90));
        spr_rect(s, 7, 4, 3, 10, rgb(60, 84, 100)); spr_rect(s, 22, 4, 3, 10, rgb(60, 84, 100));  // windshields
        spr_rect(s, 14, 5, 4, 3, rgb(176, 48, 48)); spr_rect(s, 14, 10, 4, 3, rgb(60, 90, 200));  // light bar
        spr_rect(s, 3, 1, 4, 2, pal_char('k')); spr_rect(s, 25, 1, 4, 2, pal_char('k'));
        spr_rect(s, 3, 15, 4, 2, pal_char('k')); spr_rect(s, 25, 15, 4, 2, pal_char('k'));
        spr_set(s, 30, 5, rgb(230, 220, 160)); spr_set(s, 30, 12, rgb(230, 220, 160));
        g_spr[S_CAR] = s;
        Sprite w = spr_copy(s);  // wrecked civilian car: rusty
        for (int i = 0; i < w.w * w.h; i++) if (w.d[i] != KEYC && w.d[i] != pal_char('k')) w.d[i] = gfx_mix(w.d[i], rgb(110, 70, 50), 150);
        spr_rect(w, 14, 5, 4, 8, rgb(90, 60, 44));
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
    for (int b = 0; b < 3; b++) {  // blood decals
        Sprite s = spr_new(16, 16); Rng r(500 + b);
        for (int i = 0; i < 5 + b * 2; i++) spr_circle(s, 5 + r.range(6), 5 + r.range(6), 1 + r.range(2 + b), i & 1 ? rgb(100, 18, 18) : rgb(80, 14, 16));
        for (int i = 0; i < 6; i++) spr_set(s, r.range(16), r.range(16), rgb(110, 22, 20));
        g_spr[S_BLOOD0 + b] = s;
    }
    {   Sprite s = spr_new(16, 16);  // dropped backpack
        spr_rect(s, 3, 4, 10, 10, pal_char('k')); spr_rect(s, 4, 5, 8, 8, rgb(96, 110, 56)); spr_rect(s, 5, 9, 6, 3, rgb(70, 82, 40));
        spr_rect(s, 6, 2, 4, 3, pal_char('k'));
        g_spr[S_BAG] = s; }
    {   Sprite s = spr_new(16, 16);  // bedroll
        spr_rect(s, 1, 4, 14, 9, pal_char('k')); spr_rect(s, 2, 5, 12, 7, rgb(84, 92, 60)); spr_rect(s, 2, 5, 3, 7, rgb(170, 160, 130));
        g_spr[S_BEDROLL] = s; }
    {   // canvas ridge tent seen from above (32x24): two sloped panels, dark opening, patches
        Sprite s = spr_new(32, 24);
        px k = pal_char('k'), lit = rgb(128, 136, 80), dk = rgb(86, 94, 54), seam = rgb(64, 70, 40);
        for (int y = 2; y < 21; y++) {
            int inset = y < 6 ? 6 - y : 0;
            for (int x = 2 + inset; x < 30 - inset; x++) spr_set(s, x, y, y < 11 ? lit : dk);
            spr_set(s, 1 + inset, y, k); spr_set(s, 30 - inset, y, k);
        }
        for (int x = 2; x < 30; x++) { spr_set(s, x, 11, seam); spr_set(s, x, 21, k); }
        for (int x = 6; x < 26; x++) spr_set(s, x, 1, k);
        for (int y = 3; y < 21; y += 4) for (int x = 6; x < 26; x += 6) spr_set(s, x, y, y < 11 ? rgb(146, 152, 92) : seam);
        spr_rect(s, 13, 14, 6, 7, rgb(30, 28, 22)); spr_rect(s, 14, 13, 4, 1, rgb(30, 28, 22));
        spr_set(s, 0, 22, rgb(150, 140, 110)); spr_set(s, 1, 21, rgb(150, 140, 110)); spr_set(s, 31, 22, rgb(150, 140, 110)); spr_set(s, 30, 21, rgb(150, 140, 110));
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

// ---------------------------------------------------------------- settlement buildings
static void buildBuildings() {
    struct Style { px roof, roofDk, wall, trim; };
    static const Style ST[BLD_TYPES] = {
        {rgb(186, 182, 172), rgb(140, 136, 128), rgb(150, 140, 120), rgb(176, 48, 48)},  // infirmary
        {rgb(130, 96, 70), rgb(96, 70, 52), rgb(110, 100, 90), rgb(232, 192, 64)},       // workshop (rusty tin)
        {rgb(96, 110, 60), rgb(70, 82, 44), rgb(120, 96, 66), rgb(160, 140, 100)},       // storage (olive tarp)
        {rgb(120, 86, 56), rgb(90, 64, 42), rgb(140, 110, 76), rgb(110, 160, 70)},       // kitchen / garden
        {rgb(70, 80, 100), rgb(52, 60, 76), rgb(126, 112, 92), rgb(190, 176, 140)},      // living quarters
        {rgb(104, 76, 48), rgb(76, 54, 34), rgb(92, 66, 42), rgb(140, 142, 146)},        // defences / watchtower
    };
    for (int b = 0; b < BLD_TYPES; b++) for (int lv = 0; lv < BLD_LEVELS; lv++) {
        Sprite s = spr_new(48, 48);
        const Style& st = ST[b];
        if (lv == 0) {  // empty plot: staked dirt
            for (int y = 6; y < 46; y++) for (int x = 3; x < 45; x++) s.d[y * 48 + x] = noisy(rgb(92, 72, 52), hash32(b * 999 + y * 48 + x), 18);
            for (int i = 3; i < 45; i += 3) { spr_set(s, i, 6, rgb(200, 190, 160)); spr_set(s, i, 45, rgb(200, 190, 160)); }
            for (int i = 6; i < 46; i += 3) { spr_set(s, 3, i, rgb(200, 190, 160)); spr_set(s, 44, i, rgb(200, 190, 160)); }
            int cs[4][2] = {{2, 4}, {44, 4}, {2, 43}, {44, 43}};
            for (auto& c : cs) spr_rect(s, c[0], c[1], 2, 4, rgb(120, 90, 56));
            g_building[b][lv] = s; continue;
        }
        if (b == B_DEFENSES) {  // watchtower on stilts
            spr_rect(s, 12, 18, 3, 28, pal_char('k')); spr_rect(s, 33, 18, 3, 28, pal_char('k'));
            spr_rect(s, 13, 18, 1, 28, st.roof); spr_rect(s, 34, 18, 1, 28, st.roof);
            for (int i = 0; i < 20; i++) { spr_set(s, 14 + i, 22 + i, st.roofDk); spr_set(s, 33 - i, 22 + i, st.roofDk); }
            spr_rect(s, 8, 8, 32, 12, pal_char('k')); spr_rect(s, 9, 9, 30, 10, st.wall);
            for (int x = 10; x < 38; x += 4) spr_rect(s, x, 9, 1, 10, st.roofDk);
            spr_rect(s, 6, 2, 36, 7, pal_char('k')); spr_rect(s, 7, 3, 34, 5, st.roof);
            if (lv >= 2) { spr_rect(s, 4, 18, 40, 2, st.trim); }  // reinforced rail
            if (lv >= 3) { spr_rect(s, 22, 0, 1, 4, pal_char('k')); spr_rect(s, 23, 0, 5, 3, rgb(176, 48, 48)); spr_set(s, 40, 12, rgb(250, 230, 140)); }
            g_building[b][lv] = s; continue;
        }
        int x0 = 4, x1 = 44, roofTop = 6 - (lv >= 2 ? 2 : 0), roofBot = 26, wallBot = 44;
        // shadow
        spr_rect(s, x0 + 2, wallBot, x1 - x0, 3, rgb(30, 34, 24));
        // roof (3/4 view): slanted planes with ridge
        spr_rect(s, x0 - 1, roofTop - 1, x1 - x0 + 2, roofBot - roofTop + 2, pal_char('k'));
        for (int y = roofTop; y < roofBot; y++) for (int x = x0; x < x1; x++) {
            px c = y < (roofTop + roofBot) / 2 ? st.roof : st.roofDk;
            if (b == B_WORKSHOP && (x & 3) == 0) c = gfx_scale(c, 200);          // corrugated
            if (b == B_QUARTERS && ((y - roofTop) % 4 == 3)) c = gfx_scale(c, 210); // shingles
            s.d[y * 48 + x] = noisy(c, hash32(b * 7 + lv * 131 + y * 48 + x), 6);
        }
        spr_rect(s, x0, (roofTop + roofBot) / 2, x1 - x0, 1, gfx_scale(st.roof, 240));
        // front wall
        spr_rect(s, x0 - 1, roofBot, x1 - x0 + 2, wallBot - roofBot + 1, pal_char('k'));
        for (int y = roofBot + 1; y < wallBot; y++) for (int x = x0; x < x1; x++) {
            px c = st.wall;
            if (b == B_STORAGE || b == B_DEFENSES || b == B_KITCHEN) if ((x % 5) == 0) c = gfx_scale(c, 200);  // planks
            s.d[y * 48 + x] = noisy(c, hash32(b * 13 + y * 48 + x), 8);
        }
        // door + windows (lit)
        spr_rect(s, 20, 33, 8, 11, pal_char('k')); spr_rect(s, 21, 34, 6, 10, rgb(84, 60, 40)); spr_set(s, 25, 39, rgb(232, 192, 64));
        spr_rect(s, 8, 31, 8, 6, pal_char('k')); spr_rect(s, 9, 32, 6, 4, rgb(200, 170, 90));
        spr_rect(s, 32, 31, 8, 6, pal_char('k')); spr_rect(s, 33, 32, 6, 4, rgb(200, 170, 90));
        // type emblem on the roof
        int ex = 24, ey = (roofTop + roofBot) / 2 - 4;
        switch (b) {
        case B_INFIRMARY: spr_rect(s, ex - 1, ey - 3, 3, 9, st.trim); spr_rect(s, ex - 4, ey, 9, 3, st.trim); break;
        case B_WORKSHOP: spr_rect(s, ex - 4, ey + 1, 8, 2, st.trim); spr_rect(s, ex + 3, ey - 1, 3, 6, st.trim); break;
        case B_STORAGE: spr_rect(s, ex - 4, ey - 2, 8, 7, rgb(170, 134, 90)); spr_rect(s, ex - 4, ey + 1, 8, 1, pal_char('k')); break;
        case B_KITCHEN: spr_rect(s, ex + 8, roofTop - 4, 3, 6, rgb(70, 70, 70)); for (int i = 0; i < 3; i++) spr_set(s, ex - 2 + i * 2, ey, st.trim); break;
        case B_QUARTERS: spr_rect(s, ex - 5, ey, 10, 3, rgb(190, 176, 140)); spr_rect(s, ex - 5, ey - 1, 3, 2, rgb(220, 210, 190)); break;
        }
        if (b == B_KITCHEN) {  // garden beds in front
            for (int x = 2; x < 46; x += 2) { spr_set(s, x, 45, rgb(90, 150, 60)); spr_set(s, x + 1, 46, rgb(70, 120, 50)); }
        }
        if (lv >= 2) {  // annex / extension
            spr_rect(s, 40, 20, 8, 25, pal_char('k')); spr_rect(s, 41, 21, 6, 6, st.roofDk); spr_rect(s, 41, 27, 6, 17, st.wall);
        }
        if (lv >= 3) {  // reinforced trim + lantern + antenna
            spr_rect(s, x0, wallBot - 2, x1 - x0, 2, st.trim);
            spr_rect(s, 6, roofTop - 6, 1, 6, pal_char('k')); spr_set(s, 6, roofTop - 7, rgb(176, 48, 48));
            spr_set(s, 18, 30, rgb(250, 230, 140)); spr_set(s, 30, 30, rgb(250, 230, 140));
        }
        g_building[b][lv] = s;
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
