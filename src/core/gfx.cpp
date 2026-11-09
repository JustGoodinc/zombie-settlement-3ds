#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Canvas::clip(int x, int y, int W, int H) {
    cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
    cx1 = x + W > w ? w : x + W; cy1 = y + H > h ? h : y + H;
}

px gfx_mix(px a, px b, int t) {
    int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (((br - ar) * t) >> 8), g = ag + (((bg - ag) * t) >> 8), bl = ab + (((bb - ab) * t) >> 8);
    return (px)((r << 11) | (g << 5) | bl);
}
px gfx_scale(px c, int l) {
    int r = ((c >> 11) * l) >> 8, g = (((c >> 5) & 63) * l) >> 8, b = ((c & 31) * l) >> 8;
    return (px)((r << 11) | (g << 5) | b);
}

void gfx_clear(Canvas& c, px col) { for (int i = 0; i < c.w * c.h; i++) c.p[i] = col; }

void gfx_fill(Canvas& c, int x, int y, int w, int h, px col) {
    x += c.ox; y += c.oy;
    int x0 = x < c.cx0 ? c.cx0 : x, y0 = y < c.cy0 ? c.cy0 : y;
    int x1 = x + w > c.cx1 ? c.cx1 : x + w, y1 = y + h > c.cy1 ? c.cy1 : y + h;
    for (int j = y0; j < y1; j++) { px* r = c.p + j * c.w; for (int i = x0; i < x1; i++) r[i] = col; }
}
void gfx_fillA(Canvas& c, int x, int y, int w, int h, px col, int a) {
    x += c.ox; y += c.oy;
    int x0 = x < c.cx0 ? c.cx0 : x, y0 = y < c.cy0 ? c.cy0 : y;
    int x1 = x + w > c.cx1 ? c.cx1 : x + w, y1 = y + h > c.cy1 ? c.cy1 : y + h;
    for (int j = y0; j < y1; j++) { px* r = c.p + j * c.w; for (int i = x0; i < x1; i++) r[i] = gfx_mix(r[i], col, a); }
}
void gfx_shade(Canvas& c, int x, int y, int w, int h, int lvl) {
    if (lvl >= 256) return;
    x += c.ox; y += c.oy;
    int x0 = x < c.cx0 ? c.cx0 : x, y0 = y < c.cy0 ? c.cy0 : y;
    int x1 = x + w > c.cx1 ? c.cx1 : x + w, y1 = y + h > c.cy1 ? c.cy1 : y + h;
    for (int j = y0; j < y1; j++) { px* r = c.p + j * c.w; for (int i = x0; i < x1; i++) r[i] = gfx_scale(r[i], lvl); }
}
void gfx_pset(Canvas& c, int x, int y, px col) {
    x += c.ox; y += c.oy;
    if (x >= c.cx0 && y >= c.cy0 && x < c.cx1 && y < c.cy1) c.p[y * c.w + x] = col;
}
void gfx_rect(Canvas& c, int x, int y, int w, int h, px col) {
    gfx_fill(c, x, y, w, 1, col); gfx_fill(c, x, y + h - 1, w, 1, col);
    gfx_fill(c, x, y, 1, h, col); gfx_fill(c, x + w - 1, y, 1, h, col);
}
void gfx_line(Canvas& c, int x0, int y0, int x1, int y1, px col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (int n = 0; n < 2000; n++) {
        gfx_pset(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
void gfx_circle(Canvas& c, int cx, int cy, int r, px col, bool filled) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) {
        int d = x * x + y * y;
        if (d <= r * r + r && (filled || d >= (r - 1) * (r - 1) + (r - 1))) gfx_pset(c, cx + x, cy + y, col);
    }
}

void gfx_blit(Canvas& c, const Sprite& s, int x, int y, int sc, bool flip) {
    if (!s.d) return;
    x += c.ox; y += c.oy;
    if (x >= c.cx1 || y >= c.cy1 || x + s.w * sc <= c.cx0 || y + s.h * sc <= c.cy0) return;
    for (int j = 0; j < s.h * sc; j++) {
        int py = y + j; if (py < c.cy0 || py >= c.cy1) continue;
        const px* row = s.d + (j / sc) * s.w; px* dst = c.p + py * c.w;
        for (int i = 0; i < s.w * sc; i++) {
            int pxx = x + i; if (pxx < c.cx0 || pxx >= c.cx1) continue;
            int sx = i / sc; px v = row[flip ? s.w - 1 - sx : sx];
            if (v != KEYC) dst[pxx] = v;
        }
    }
}
void gfx_blitEx(Canvas& c, const Sprite& s, int x, int y, bool flip, int shade, px flash, int fa) {
    if (!s.d) return;
    x += c.ox; y += c.oy;
    if (x >= c.cx1 || y >= c.cy1 || x + s.w <= c.cx0 || y + s.h <= c.cy0) return;
    for (int j = 0; j < s.h; j++) {
        int py = y + j; if (py < c.cy0 || py >= c.cy1) continue;
        const px* row = s.d + j * s.w; px* dst = c.p + py * c.w;
        for (int i = 0; i < s.w; i++) {
            int pxx = x + i; if (pxx < c.cx0 || pxx >= c.cx1) continue;
            px v = row[flip ? s.w - 1 - i : i]; if (v == KEYC) continue;
            if (fa) v = gfx_mix(v, flash, fa);
            if (shade < 256) v = gfx_scale(v, shade);
            dst[pxx] = v;
        }
    }
}
void gfx_outline(Canvas& c, const Sprite& s, int x, int y, px col, bool flip) {
    auto op = [&](int i, int j) {
        if (i < 0 || j < 0 || i >= s.w || j >= s.h) return false;
        return s.d[j * s.w + (flip ? s.w - 1 - i : i)] != KEYC;
    };
    for (int j = -1; j <= s.h; j++) for (int i = -1; i <= s.w; i++) {
        if (op(i, j)) continue;
        if (op(i - 1, j) || op(i + 1, j) || op(i, j - 1) || op(i, j + 1)) gfx_pset(c, x + i, y + j, col);
    }
}

// ---- 5x8 bitmap font (ASCII 32..126), column-major, bit0 = top row ----
static const uint8_t FONT[95][5] = {
{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},
{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0,5,3,0,0},{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},
{0x2A,0x1C,0x7F,0x1C,0x2A},{8,8,0x3E,8,8},{0,0xA0,0x60,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},
{0x20,0x10,8,4,2},{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,9,7},{0x36,0x49,0x49,0x49,0x36},
{0x46,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{0,8,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
{0,0x41,0x22,0x14,8},{2,1,0x59,9,6},{0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},
{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x41,0x51,0x73},
{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
{0x7F,2,0x1C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},
{0x7F,9,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},{3,1,0x7F,1,3},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,8,0x14,0x63},{3,4,0x78,4,3},{0x61,0x59,0x49,0x4D,0x43},{0,0x7F,0x41,0x41,0x41},
{2,4,8,0x10,0x20},{0,0x41,0x41,0x41,0x7F},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},{0,3,7,8,0},
{0x20,0x54,0x54,0x78,0x40},{0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x28,0x7F},{0x38,0x54,0x54,0x54,0x18},
{0,8,0x7E,9,2},{0x18,0xA4,0xA4,0x9C,0x78},{0x7F,8,4,4,0x78},{0,0x44,0x7D,0x40,0},{0x20,0x40,0x40,0x3D,0},
{0x7F,0x10,0x28,0x44,0},{0,0x41,0x7F,0x40,0},{0x7C,4,0x78,4,0x78},{0x7C,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
{0xFC,0x18,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},{0x7C,8,4,4,8},{0x48,0x54,0x54,0x54,0x24},{4,4,0x3F,0x44,0x24},
{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},
{0x44,0x64,0x54,0x4C,0x44},{0,8,0x36,0x41,0},{0,0,0x77,0,0},{0,0x41,0x36,8,0},{2,1,2,4,2}};

static void glyph(Canvas& c, int x, int y, char ch, px col, int sc) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t* g = FONT[ch - 32];
    for (int i = 0; i < 5; i++) for (int j = 0; j < 8; j++)
        if (g[i] & (1 << j)) {
            if (sc == 1) gfx_pset(c, x + i, y + j, col);
            else gfx_fill(c, x + i * sc, y + j * sc, sc, sc, col);
        }
}
int gfx_textW(const char* s, int sc) {
    int w = 0, best = 0;
    for (; *s; s++) { if (*s == '\n') { if (w > best) best = w; w = 0; } else w += 6 * sc; }
    if (w > best) best = w;
    return best ? best - sc : 0;
}
int gfx_text(Canvas& c, int x, int y, const char* s, px col, int sc, bool shadow) {
    int cx = x;
    for (; *s; s++) {
        if (*s == '\n') { cx = x; y += 10 * sc; continue; }
        if (shadow) glyph(c, cx + sc, y + sc, *s, pal::ink, sc);
        glyph(c, cx, y, *s, col, sc);
        cx += 6 * sc;
    }
    return cx - x;
}
void gfx_textC(Canvas& c, int cx, int y, const char* s, px col, int sc) { gfx_text(c, cx - gfx_textW(s, sc) / 2, y, s, col, sc); }

int gfx_wrap(Canvas& c, int x, int y, int maxW, const char* s, px col) {
    char line[96]; int lines = 0; const char* p = s;
    while (*p) {
        int n = 0, lastSp = -1;
        while (p[n] && p[n] != '\n' && (n + 1) * 6 <= maxW + 1) { if (p[n] == ' ') lastSp = n; n++; }
        if (p[n] && p[n] != '\n' && lastSp > 0) n = lastSp;
        if (n > 95) n = 95;
        memcpy(line, p, n); line[n] = 0;
        gfx_text(c, x, y + lines * 10, line, col);
        lines++; p += n;
        while (*p == ' ' || *p == '\n') p++;
    }
    return lines;
}

// ---- sprites ----
Sprite spr_new(int w, int h) {
    Sprite s; s.w = (int16_t)w; s.h = (int16_t)h; s.d = (px*)malloc(sizeof(px) * w * h);
    for (int i = 0; i < w * h; i++) s.d[i] = KEYC;
    return s;
}
void spr_set(Sprite& s, int x, int y, px c) { if (x >= 0 && y >= 0 && x < s.w && y < s.h) s.d[y * s.w + x] = c; }
px spr_get(const Sprite& s, int x, int y) { return (x >= 0 && y >= 0 && x < s.w && y < s.h) ? s.d[y * s.w + x] : KEYC; }
void spr_rect(Sprite& s, int x, int y, int w, int h, px c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) spr_set(s, i, j, c); }
void spr_circle(Sprite& s, int cx, int cy, int r, px c) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r) spr_set(s, cx + x, cy + y, c);
}
Sprite spr_copy(const Sprite& a) { Sprite s = spr_new(a.w, a.h); memcpy(s.d, a.d, sizeof(px) * a.w * a.h); return s; }
void spr_recolor(Sprite& s, const px* from, const px* to, int n) {
    for (int i = 0; i < s.w * s.h; i++) for (int k = 0; k < n; k++) if (s.d[i] == from[k]) { s.d[i] = to[k]; break; }
}
Sprite spr_rot90(const Sprite& a) {
    Sprite s = spr_new(a.h, a.w);
    for (int y = 0; y < a.h; y++) for (int x = 0; x < a.w; x++) spr_set(s, a.h - 1 - y, x, a.d[y * a.w + x]);
    return s;
}

px pal_char(char ch) {
    switch (ch) {
    case 'k': return rgb(22, 20, 24);    case 'e': return rgb(22, 20, 24);
    case 'd': return rgb(42, 42, 48);    case 'g': return rgb(74, 74, 82);
    case 'l': return rgb(138, 138, 144); case 'w': return rgb(216, 208, 192);
    case 't': return rgb(180, 154, 122); case 's': return rgb(216, 168, 128);
    case 'S': return rgb(160, 112, 80);  case 'b': return rgb(122, 90, 58);
    case 'B': return rgb(74, 53, 40);    case 'o': return rgb(106, 122, 58);
    case 'O': return rgb(59, 74, 42);    case 'n': return rgb(42, 58, 90);
    case 'N': return rgb(74, 106, 154);  case 'r': return rgb(176, 48, 48);
    case 'R': return rgb(106, 24, 24);   case 'y': return rgb(232, 192, 64);
    case 'Y': return rgb(160, 128, 32);  case 'z': return rgb(122, 150, 106);
    case 'Z': return rgb(74, 106, 74);   case 'h': return rgb(58, 42, 26);
    case 'c': return rgb(80, 160, 192);  case 'G': return rgb(154, 228, 64);
    default: return KEYC;
    }
}
Sprite spr_parse(const char* const* rows, int h) {
    int w = 0;
    for (int j = 0; j < h; j++) { int l = (int)strlen(rows[j]); if (l > w) w = l; }
    Sprite s = spr_new(w, h);
    for (int j = 0; j < h; j++) for (int i = 0; rows[j][i]; i++) s.d[j * w + i] = pal_char(rows[j][i]);
    return s;
}

// ---- minimal PNG writer (stored deflate) for screenshots and the asset gallery ----
static uint32_t crcTab[256];
static uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crcTab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t k = i; for (int j = 0; j < 8; j++) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1; crcTab[i] = k; }
    for (size_t i = 0; i < n; i++) c = crcTab[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}
static void be32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE* f, const char* type, const uint8_t* d, uint32_t n) {
    uint8_t h[8]; be32(h, n); memcpy(h + 4, type, 4); fwrite(h, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    uint32_t c = crc((const uint8_t*)type, 4); c = crc(d, n, c) ^ 0xFFFFFFFFu;
    uint8_t t[4]; be32(t, c); fwrite(t, 1, 4, f);
}
bool gfx_writePNG(const char* path, const px* d, int w, int h) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    uint8_t ihdr[13]; be32(ihdr, w); be32(ihdr + 4, h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t raw = (size_t)h * (w * 3 + 1);
    uint8_t* r = (uint8_t*)malloc(raw); size_t k = 0;
    for (int y = 0; y < h; y++) {
        r[k++] = 0;
        for (int x = 0; x < w; x++) {
            px c = d[y * w + x];
            r[k++] = (uint8_t)(((c >> 11) * 255) / 31); r[k++] = (uint8_t)((((c >> 5) & 63) * 255) / 63); r[k++] = (uint8_t)(((c & 31) * 255) / 31);
        }
    }
    size_t blocks = (raw + 65534) / 65535, zn = 2 + raw + blocks * 5 + 4;
    uint8_t* z = (uint8_t*)malloc(zn); size_t o = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    for (size_t p = 0; p < raw; p += 65535) {
        size_t n = raw - p > 65535 ? 65535 : raw - p;
        z[o++] = (p + n >= raw) ? 1 : 0; z[o++] = n & 255; z[o++] = n >> 8; z[o++] = ~n & 255; z[o++] = (~n >> 8) & 255;
        memcpy(z + o, r + p, n); o += n;
    }
    be32(z + o, (b << 16) | a); o += 4;
    chunk(f, "IDAT", z, (uint32_t)o); chunk(f, "IEND", nullptr, 0);
    free(r); free(z); fclose(f);
    return true;
}
