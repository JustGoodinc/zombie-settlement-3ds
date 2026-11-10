// Windows platform layer: Win32 window simulating the 3DS dual-screen layout,
// GDI presentation (nearest-neighbour), keyboard/mouse/XInput, waveOut audio.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include "../../gameplay/game.h"
#include "../../core/audio.h"
#include "../../ui/ui.h"

extern bool g_canQuit;
extern const char* g_controlsText;

enum { GAP = 8, FB_W = TOP_W, FB_H = TOP_H + GAP + BOT_H };
static px topBuf[TOP_W * TOP_H], botBuf[BOT_W * BOT_H];
static uint32_t frame32[FB_W * FB_H];
static HWND hwnd;
static bool keys[256];
static bool mouseDown;
static int mouseX, mouseY;
static RECT dst;  // where the composed frame is drawn inside the client area
static bool isFullscreen;
static WINDOWPLACEMENT prevPlacement = {sizeof(prevPlacement)};
static char baseDir[MAX_PATH];

// ---- key bindings (editable in controls.ini next to the saves) ----
struct Bind { const char* name; uint32_t btn; int vk[3]; };
static Bind binds[] = {
    {"A", BTN_A, {'E', 'K', 0}}, {"B", BTN_B, {VK_SHIFT, VK_BACK, 0}}, {"X", BTN_X, {'R', 0, 0}}, {"Y", BTN_Y, {VK_SPACE, 'J', 0}},
    {"L", BTN_L, {'Q', 0, 0}}, {"R", BTN_R, {'F', 0, 0}}, {"START", BTN_START, {VK_ESCAPE, VK_RETURN, 0}}, {"SELECT", BTN_SELECT, {VK_TAB, 'I', 0}},
    {"UP", BTN_UP, {VK_UP, 0, 0}}, {"DOWN", BTN_DOWN, {VK_DOWN, 0, 0}}, {"LEFT", BTN_LEFT, {VK_LEFT, 0, 0}}, {"RIGHT", BTN_RIGHT, {VK_RIGHT, 0, 0}},
};
static int stickVk[4] = {'W', 'S', 'A', 'D'};

static int parseKey(const char* s) {
    if (!s[0]) return 0;
    if (!s[1]) return (s[0] >= 'a' && s[0] <= 'z') ? s[0] - 32 : s[0];
    struct { const char* n; int vk; } names[] = {{"SPACE", VK_SPACE}, {"SHIFT", VK_SHIFT}, {"CTRL", VK_CONTROL}, {"ENTER", VK_RETURN}, {"ESC", VK_ESCAPE},
        {"TAB", VK_TAB}, {"BACKSPACE", VK_BACK}, {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT}, {"ALT", VK_MENU}};
    for (auto& n : names) if (!_stricmp(s, n.n)) return n.vk;
    return 0;
}
static void loadBindings() {
    char p[MAX_PATH]; snprintf(p, sizeof p, "%scontrols.ini", baseDir);
    FILE* f = fopen(p, "r");
    if (!f) {
        f = fopen(p, "w");
        if (f) {
            fputs("# Staying Alive key bindings. Format: BUTTON=KEY[,KEY]. Keys: letters, SPACE, SHIFT, CTRL, ENTER, ESC, TAB, BACKSPACE, arrows.\n"
                  "MOVE_UP=W\nMOVE_DOWN=S\nMOVE_LEFT=A\nMOVE_RIGHT=D\nA=E,K\nB=SHIFT,BACKSPACE\nX=R\nY=SPACE,J\nL=Q\nR=F\nSTART=ESC,ENTER\nSELECT=TAB,I\n"
                  "UP=UP\nDOWN=DOWN\nLEFT=LEFT\nRIGHT=RIGHT\n", f);
            fclose(f);
        }
        return;
    }
    char line[128];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; char* val = eq + 1; val[strcspn(val, "\r\n")] = 0;
        int vks[3] = {0, 0, 0}, n = 0;
        for (char* tok = strtok(val, ","); tok && n < 3; tok = strtok(nullptr, ",")) { int vk = parseKey(tok); if (vk) vks[n++] = vk; }
        if (!n) continue;
        if (!_stricmp(line, "MOVE_UP")) stickVk[0] = vks[0]; else if (!_stricmp(line, "MOVE_DOWN")) stickVk[1] = vks[0];
        else if (!_stricmp(line, "MOVE_LEFT")) stickVk[2] = vks[0]; else if (!_stricmp(line, "MOVE_RIGHT")) stickVk[3] = vks[0];
        else for (auto& b : binds) if (!_stricmp(line, b.name)) memcpy(b.vk, vks, sizeof vks);
    }
    fclose(f);
}

// keycap labels for on-screen prompts, taken from the active bindings
static const char* vkName(int vk) {
    static char buf[16][8]; static int n = 0;
    switch (vk) {
    case VK_SPACE: return "Space"; case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl"; case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Esc"; case VK_TAB: return "Tab"; case VK_BACK: return "Bksp"; case VK_MENU: return "Alt";
    case VK_UP: return "^"; case VK_DOWN: return "v"; case VK_LEFT: return "<"; case VK_RIGHT: return ">";
    }
    char* b = buf[n++ & 15]; b[0] = (char)vk; b[1] = 0; return b;
}
static void buildKeyNames() {
    for (auto& b : binds) for (int i = 0; i < 12; i++) if (b.btn == (1u << i)) g_keyName[i] = vkName(b.vk[0]);
    static char stick[8];
    bool letters = true; for (int i = 0; i < 4; i++) letters &= stickVk[i] >= 'A' && stickVk[i] <= 'Z';
    if (letters) { stick[0] = (char)stickVk[0]; stick[1] = (char)stickVk[2]; stick[2] = (char)stickVk[1]; stick[3] = (char)stickVk[3]; stick[4] = 0; g_stickName = stick; }
}

// ---- XInput (loaded dynamically so the game runs without it) ----
struct XPAD { WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
struct XSTATE { DWORD packet; XPAD pad; };
typedef DWORD(WINAPI* XGetState)(DWORD, XSTATE*);
static XGetState xget;

// ---- display ----
static void layout() {
    RECT r; GetClientRect(hwnd, &r);
    int cw = r.right, ch = r.bottom;
    int sc = cw / FB_W < ch / FB_H ? cw / FB_W : ch / FB_H;
    if (sc < 1) sc = 1;
    int w = FB_W * sc, h = FB_H * sc;
    // allow fractional fit only if integer scale would waste too much space
    if (sc * FB_W < cw * 0.6 && sc * FB_H < ch * 0.6) { float f = (float)cw / FB_W < (float)ch / FB_H ? (float)cw / FB_W : (float)ch / FB_H; w = (int)(FB_W * f); h = (int)(FB_H * f); }
    dst.left = (cw - w) / 2; dst.top = (ch - h) / 2; dst.right = dst.left + w; dst.bottom = dst.top + h;
}
static void sizeWindow() {
    if (isFullscreen) return;
    RECT r = {0, 0, FB_W * G.set.winScale, FB_H * G.set.winScale};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
}
static void setFullscreen(bool on) {
    if (on == isFullscreen) return;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (on) {
        MONITORINFO mi = {sizeof mi};
        GetWindowPlacement(hwnd, &prevPlacement);
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &prevPlacement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    isFullscreen = on;
    layout();
}
static inline uint32_t to32(px c) {
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}
static void present(HDC dc) {
    for (int i = 0; i < FB_W * FB_H; i++) frame32[i] = 0x101014;
    for (int y = 0; y < TOP_H; y++) for (int x = 0; x < TOP_W; x++) frame32[y * FB_W + x] = to32(topBuf[y * TOP_W + x]);
    int bx = (TOP_W - BOT_W) / 2;
    for (int y = 0; y < BOT_H; y++) for (int x = 0; x < BOT_W; x++) frame32[(TOP_H + GAP + y) * FB_W + bx + x] = to32(botBuf[y * BOT_W + x]);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = FB_W; bi.bmiHeader.biHeight = -FB_H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    RECT r; GetClientRect(hwnd, &r);
    HBRUSH br = CreateSolidBrush(RGB(12, 12, 14));
    RECT bands[4] = {{0, 0, r.right, dst.top}, {0, dst.bottom, r.right, r.bottom}, {0, dst.top, dst.left, dst.bottom}, {dst.right, dst.top, r.right, dst.bottom}};
    for (auto& b : bands) FillRect(dc, &b, br);
    DeleteObject(br);
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top, 0, 0, FB_W, FB_H, frame32, &bi, DIB_RGB_COLORS, SRCCOPY);
}
static bool mouseToTouch(int mx, int my, int& tx, int& ty) {
    float sx = (float)(dst.right - dst.left) / FB_W, sy = (float)(dst.bottom - dst.top) / FB_H;
    float fx = (mx - dst.left) / sx - (TOP_W - BOT_W) / 2, fy = (my - dst.top) / sy - (TOP_H + GAP);
    tx = (int)fx; ty = (int)fy;
    return fx >= 0 && fy >= 0 && fx < BOT_W && fy < BOT_H;
}

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CLOSE: G.quitRequested = true; return 0;
    case WM_SIZE: layout(); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (w < 256) keys[w] = true;
        if (w == VK_F11) { G.set.fullscreen = !G.set.fullscreen; }
        if (w == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) G.quitRequested = true;
        return 0;
    case WM_KEYUP: case WM_SYSKEYUP: if (w < 256) keys[w] = false; return 0;
    case WM_KILLFOCUS: memset(keys, 0, sizeof keys); mouseDown = false; return 0;
    case WM_LBUTTONDOWN: mouseDown = true; SetCapture(h); mouseX = (short)LOWORD(l); mouseY = (short)HIWORD(l); return 0;
    case WM_LBUTTONUP: mouseDown = false; ReleaseCapture(); return 0;
    case WM_MOUSEMOVE: mouseX = (short)LOWORD(l); mouseY = (short)HIWORD(l); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); present(dc); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(h, m, w, l);
}

// ---- audio (waveOut, 4 x 1024-sample buffers) ----
static HWAVEOUT wo;
static WAVEHDR hdr[4];
static int16_t abuf[4][1024];
static void audioInit() {
    WAVEFORMATEX f = {}; f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = AUDIO_RATE; f.wBitsPerSample = 16;
    f.nBlockAlign = 2; f.nAvgBytesPerSec = AUDIO_RATE * 2;
    if (waveOutOpen(&wo, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { wo = nullptr; return; }
    for (int i = 0; i < 4; i++) {
        hdr[i] = {}; hdr[i].lpData = (LPSTR)abuf[i]; hdr[i].dwBufferLength = sizeof abuf[i];
        waveOutPrepareHeader(wo, &hdr[i], sizeof(WAVEHDR));
        audio_mix(abuf[i], 1024); waveOutWrite(wo, &hdr[i], sizeof(WAVEHDR));
    }
}
static void audioPump() {
    if (!wo) return;
    for (int i = 0; i < 4; i++) if (hdr[i].dwFlags & WHDR_DONE) { audio_mix(abuf[i], 1024); waveOutWrite(wo, &hdr[i], sizeof(WAVEHDR)); }
}

static void screenshot() {
    char p[MAX_PATH]; CreateDirectoryA("screenshots", nullptr);
    static int n = 0; SYSTEMTIME t; GetLocalTime(&t);
    snprintf(p, sizeof p, "screenshots/zs_%04d%02d%02d_%02d%02d%02d_%d_top.png", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, n);
    gfx_writePNG(p, topBuf, TOP_W, TOP_H);
    snprintf(p, sizeof p, "screenshots/zs_%04d%02d%02d_%02d%02d%02d_%d_bottom.png", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, n++);
    gfx_writePNG(p, botBuf, BOT_W, BOT_H);
    game_toast("Screenshot saved.");
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    // saves + settings in %APPDATA%\ZombieSettlement\ (portable fallback: ./saves/)
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        snprintf(baseDir, sizeof baseDir, "%s\\ZombieSettlement\\", appdata);
        CreateDirectoryA(baseDir, nullptr);
    } else { snprintf(baseDir, sizeof baseDir, "saves\\"); CreateDirectoryA(baseDir, nullptr); }
    save_setDir(baseDir);
    loadBindings();
    buildKeyNames();
    g_inputStyle = 1;   // keyboard keycaps until a gamepad is used
    g_canQuit = true;
    g_controlsText =
        "Move/aim: WASD (facing follows movement)\nFire (Y): Space or J   Reload (X): R\nInteract/Search (A): E or K\n"
        "Sprint (hold B) / Dodge (tap B): Shift\nLock facing (L): Q   Flashlight (R): F\nWeapons: Left/Right  Heal: Up  Squad: Down\n"
        "Inventory (Select): Tab or I   Pause (Start): Esc\nTouchscreen: click the bottom screen\nGamepad: stick, A/B, X=Fire, Y=Reload\nF11 fullscreen  F12 screenshot  controls.ini rebinds";

    WNDCLASSA wc = {}; wc.lpfnWndProc = wndProc; wc.hInstance = inst; wc.lpszClassName = "StayingAlive";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
    RegisterClassA(&wc);
    game_init();
    settings_load();
    RECT r = {0, 0, FB_W * G.set.winScale, FB_H * G.set.winScale};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowA("StayingAlive", "Staying Alive", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    layout();
    HMODULE xi = LoadLibraryA("xinput1_4.dll"); if (!xi) xi = LoadLibraryA("xinput9_1_0.dll");
    if (xi) xget = (XGetState)GetProcAddress(xi, "XInputGetState");
    audioInit();
    timeBeginPeriod(1);

    LARGE_INTEGER freq, last, now; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
    uint32_t prevHeld = 0; bool prevTouch = false; int lastScale = G.set.winScale; bool lastFs = false;
    float fpsAcc = 0; int fpsN = 0;
    if (G.set.fullscreen) setFullscreen(true);
    while (!G.quitRequested) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        if (G.quitRequested) break;
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart; last = now;
        fpsAcc += dt; fpsN++; if (fpsAcc >= 0.5f) { G.fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0; }

        Input in;
        uint32_t held = 0;
        bool focus = GetForegroundWindow() == hwnd;
        for (auto& b : binds) for (int k = 0; k < 3; k++) if (b.vk[k] && focus && (GetAsyncKeyState(b.vk[k]) & 0x8000)) held |= b.btn;
        if (held) g_inputStyle = 1;
        float ax = 0, ay = 0;
        if (focus) {
            if (GetAsyncKeyState(stickVk[0]) & 0x8000) ay -= 1; if (GetAsyncKeyState(stickVk[1]) & 0x8000) ay += 1;
            if (GetAsyncKeyState(stickVk[2]) & 0x8000) ax -= 1; if (GetAsyncKeyState(stickVk[3]) & 0x8000) ax += 1;
            if (ax || ay) g_inputStyle = 1;
        }
        if (ax && ay) { ax *= 0.7071f; ay *= 0.7071f; }
        XSTATE xs;
        if (xget && xget(0, &xs) == 0) {
            const XPAD& p = xs.pad;
            float lx = p.lx / 32767.0f, ly = -p.ly / 32767.0f;
            if (lx * lx + ly * ly > 0.06f) { ax = lx; ay = ly; g_inputStyle = 2; }
            if (p.buttons || p.rt > 100) g_inputStyle = 2;
            if (p.buttons & 0x1000) held |= BTN_A; if (p.buttons & 0x2000) held |= BTN_B; if (p.buttons & 0x4000) held |= BTN_Y; if (p.buttons & 0x8000) held |= BTN_X;
            if (p.rt > 100) held |= BTN_Y;
            if (p.buttons & 0x0100) held |= BTN_L; if (p.buttons & 0x0200) held |= BTN_R;
            if (p.buttons & 0x0010) held |= BTN_START; if (p.buttons & 0x0020) held |= BTN_SELECT;
            if (p.buttons & 0x0001) held |= BTN_UP; if (p.buttons & 0x0002) held |= BTN_DOWN; if (p.buttons & 0x0004) held |= BTN_LEFT; if (p.buttons & 0x0008) held |= BTN_RIGHT;
        }
        in.held = held; in.down = held & ~prevHeld; in.up = prevHeld & ~held; prevHeld = held;
        in.ax = ax; in.ay = ay;
        int tx, ty; bool onBottom = mouseToTouch(mouseX, mouseY, tx, ty);
        bool touching = mouseDown && (onBottom || prevTouch);
        in.touching = touching; in.touchDown = touching && !prevTouch; in.touchUp = !touching && prevTouch;
        in.tx = tx < 0 ? 0 : (tx >= BOT_W ? BOT_W - 1 : tx); in.ty = ty < 0 ? 0 : (ty >= BOT_H ? BOT_H - 1 : ty);
        prevTouch = touching;
        if (focus && (GetAsyncKeyState(VK_F12) & 1)) screenshot();

        game_frame(in, dt);
        Canvas top, bot; top.init(topBuf, TOP_W, TOP_H); bot.init(botBuf, BOT_W, BOT_H);
        extern float g_renderAlpha;
        game_render(top, bot, g_renderAlpha);
        audioPump();
        HDC dc = GetDC(hwnd); present(dc); ReleaseDC(hwnd, dc);

        if (G.set.winScale != lastScale) { lastScale = G.set.winScale; sizeWindow(); settings_save(); }
        if (G.set.fullscreen != lastFs) { lastFs = G.set.fullscreen; setFullscreen(lastFs); settings_save(); }
        // ~60 FPS render cap; simulation is fixed-step and independent of this
        QueryPerformanceCounter(&now);
        float spent = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        if (spent < 1.0f / 60) Sleep((DWORD)((1.0f / 60 - spent) * 1000));
    }
    settings_save();
    if (wo) { waveOutReset(wo); for (int i = 0; i < 4; i++) waveOutUnprepareHeader(wo, &hdr[i], sizeof(WAVEHDR)); waveOutClose(wo); }
    timeEndPeriod(1);
    return 0;
}
