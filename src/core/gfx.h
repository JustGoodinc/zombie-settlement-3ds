#pragma once
// Shared software renderer. Both platforms draw into RGB565 framebuffers of the
// native 3DS sizes (top 400x240, bottom 320x240) so output is pixel-identical.
#include <stdint.h>

typedef uint16_t px;
constexpr px rgb(int r, int g, int b) { return (px)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
constexpr px KEYC = 0xF81F;  // transparent colour key (magenta)

enum { TOP_W = 400, TOP_H = 240, BOT_W = 320, BOT_H = 240 };

struct Canvas {
    px* p; int w, h;
    int ox = 0, oy = 0;            // draw offset (screen shake / UI slide)
    int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;  // clip rect
    void init(px* buf, int W, int H) { p = buf; w = W; h = H; resetClip(); }
    void resetClip() { cx0 = 0; cy0 = 0; cx1 = w; cy1 = h; }
    void clip(int x, int y, int W, int H);
};

struct Sprite { int16_t w = 0, h = 0; px* d = nullptr; };

// Try 2 palette (see docs/TRY_2_VISUAL_SPECIFICATION.md)
namespace pal {
constexpr px black = rgb(20, 20, 24), ink = rgb(14, 14, 16);
constexpr px panel = rgb(38, 38, 43), panel2 = rgb(48, 48, 54), panelHi = rgb(66, 66, 72);
constexpr px border = rgb(92, 92, 100), borderLo = rgb(24, 24, 28);
constexpr px cream = rgb(226, 216, 196), dim = rgb(150, 144, 132), grey = rgb(110, 110, 116);
constexpr px gold = rgb(232, 192, 64), goldDk = rgb(150, 118, 34), goldBg = rgb(64, 56, 34);
constexpr px red = rgb(176, 48, 48), redDk = rgb(106, 24, 24), blood = rgb(120, 20, 20);
constexpr px green = rgb(98, 168, 64), lime = rgb(154, 228, 64), cyan = rgb(80, 170, 200);
constexpr px olive = rgb(106, 122, 58), brown = rgb(122, 90, 58), tan = rgb(180, 154, 122);
}

px gfx_mix(px a, px b, int t);       // t 0..256 (0 = a)
px gfx_scale(px c, int lvl);          // lvl 0..256
void gfx_clear(Canvas&, px c);
void gfx_fill(Canvas&, int x, int y, int w, int h, px c);
void gfx_fillA(Canvas&, int x, int y, int w, int h, px c, int alpha);  // blend
void gfx_rect(Canvas&, int x, int y, int w, int h, px c);
void gfx_line(Canvas&, int x0, int y0, int x1, int y1, px c);
void gfx_pset(Canvas&, int x, int y, px c);
void gfx_circle(Canvas&, int cx, int cy, int r, px c, bool filled);
void gfx_blit(Canvas&, const Sprite&, int x, int y, int scale = 1, bool flipX = false);
void gfx_blitEx(Canvas&, const Sprite&, int x, int y, bool flipX, int shade, px flash, int flashAmt);
void gfx_outline(Canvas&, const Sprite&, int x, int y, px c, bool flipX = false);
void gfx_shade(Canvas&, int x, int y, int w, int h, int lvl);
int  gfx_text(Canvas&, int x, int y, const char* s, px c, int scale = 1, bool shadow = true);
int  gfx_textW(const char* s, int scale = 1);
void gfx_textC(Canvas&, int cx, int y, const char* s, px c, int scale = 1);  // centred
int  gfx_wrap(Canvas&, int x, int y, int maxW, const char* s, px c);       // returns lines
int  gfx_textB(Canvas&, int x, int y, const char* s, px c);   // bold
int  gfx_textWB(const char* s);
void gfx_textCB(Canvas&, int cx, int y, const char* s, px c);

// sprite construction helpers (used by art.cpp and tools)
Sprite spr_new(int w, int h);
Sprite spr_parse(const char* const* rows, int h);
void spr_set(Sprite&, int x, int y, px c);
px   spr_get(const Sprite&, int x, int y);
void spr_rect(Sprite&, int x, int y, int w, int h, px c);
void spr_circle(Sprite&, int cx, int cy, int r, px c);
Sprite spr_copy(const Sprite&);
void spr_recolor(Sprite&, const px* from, const px* to, int n);
Sprite spr_rot90(const Sprite&);

bool gfx_writePNG(const char* path, const px* d, int w, int h);
px   pal_char(char ch);  // pixel-art palette letter -> colour
