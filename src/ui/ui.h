#pragma once
// Immediate-mode widgets for the bottom touchscreen (Try 2 visual language).
// Every widget works with the stylus/mouse AND with D-pad focus + A, so the game
// stays fully playable without touch (emulators, keyboard, controller).
#include "../core/gfx.h"
#include "../gameplay/game.h"

struct UiRect { int16_t x, y, w, h; bool enabled; };

struct Ui {
    Input in;
    int focus = 0, count = 0, prevCount = 0;
    UiRect rects[96], prev[96];
    int pressed = -1;          // widget under the stylus at touch-down
    bool consumed = false;     // an activation already happened this frame
    bool navMoved = false;
    int navDir = 0;            // this frame's direction (1 up, 2 down, 3 left, 4 right), with hold-repeat
    int navEdge = 0;           // direction pressed with no widget that way
    bool navLock = false, navLockNext = false;   // a grid owns the direction keys and A (set every frame it wants them)
};
extern Ui g_ui;

void ui_beginFrame(const Input& in);
void ui_endFrame();
void ui_resetFocus();

// drawing primitives
void ui_panel(Canvas& c, int x, int y, int w, int h, const char* title = nullptr);
void ui_bg(Canvas& c);
// button glyphs (style follows the active input device)
constexpr uint32_t BTN_STICK = 1u << 20, BTN_TOUCH = 1u << 21, BTN_DPAD_LR = BTN_LEFT | BTN_RIGHT;
extern int g_inputStyle;          // 0 = 3DS buttons, 1 = keyboard keycaps, 2 = Xbox pad
extern const char* g_keyName[12];  // keyboard labels per BTN bit (Windows fills from controls.ini)
extern const char* g_stickName;
int  ui_btn(Canvas& c, int x, int y, uint32_t btn);   // draws, returns width
int  ui_btnW(uint32_t btn);
int  ui_promptW(char b, const char* label);
void ui_glyph(Canvas& c, int x, int y, char b, bool hi = false);
void ui_prompt(Canvas& c, int x, int y, char b, const char* label);
void ui_bar(Canvas& c, int x, int y, int w, int h, float v, px col);
void ui_icon(Canvas& c, int x, int y, int sprId);

// interactive widgets: return true when activated (tap released inside, or A while focused)
enum { UB_NORMAL = 0, UB_SELECTED = 1, UB_GOLD = 2, UB_SMALL = 4, UB_LEFT = 8, UB_RAIL = 16 };
bool ui_button(Canvas& c, int x, int y, int w, int h, const char* label, int style = 0, bool enabled = true);
bool ui_tab(Canvas& c, int x, int y, int w, const char* label, bool active);
bool ui_hit(int x, int y, int w, int h, bool enabled = true);   // invisible focusable area
bool ui_focused(int x, int y, int w, int h);                    // is the next widget at this rect focused
int  ui_lastId();
bool ui_pressedB();          // back/cancel this frame
bool ui_shoulderL();
bool ui_shoulderR();
float ui_ease(float t);      // 0..1 ease-out for transitions
