#pragma once
// Data-driven animation controller. Definitions live in data/anims.inc.
// Animators advance on simulation time (never rendered frames), so animation
// speed is identical at 30 FPS on 3DS and 60+ FPS on PC.
#include <stdint.h>

enum class AnimState : uint8_t {
    Idle, Walk, Run, Aim, Shoot, Reload, Melee, Dodge, Search, Hurt, Heal, Death, Dead,
    // zombie-specific
    ZIdle, ZWalk, ZDetect, ZAttack, ZHurt, ZStumble, ZDeath, ZLying, ZRise,
    // settlement job loops
    Work, Sleep, Guard,
    Count
};

// Poses are generated once at load (art.cpp) for every character variant and
// each of 5 stored directions (S, SE, E, NE, N; west-facing ones are mirrored).
enum Pose : uint8_t {
    P_STAND, P_BREATHE, P_WALK_A, P_WALK_B, P_RUN_A, P_RUN_B, P_CROUCH, P_REACH,
    P_LEAN_BACK, P_LEAN_FWD, P_HURT, P_FALL, P_DEAD, P_LOOK, P_COUNT
};

enum AnimEvent : uint8_t {
    EV_NONE = 0, EV_FOOTSTEP = 1, EV_MUZZLE = 2, EV_IMPACT = 4, EV_RELOAD = 8,
    EV_SWING = 16, EV_BODYFALL = 32, EV_GROAN = 64, EV_DONE = 128
};

struct AnimFrame {
    uint8_t pose;
    uint8_t ms10;     // duration in 10 ms units
    int8_t kick;      // render offset along facing (+ forward, - recoil back)
    int8_t bob;       // vertical render offset
    uint8_t event;    // AnimEvent bits fired when the frame starts
};

struct AnimDef {
    AnimState state;
    bool loop;
    uint8_t priority;      // higher priority can interrupt lower
    bool interruptible;    // false = must finish (unless a higher priority 'force' play)
    AnimState next;        // state after a non-looping animation ends
    uint8_t n;
    AnimFrame f[6];
};

const AnimDef& anim_def(AnimState s);
const char* anim_name(AnimState s);

struct Animator {
    AnimState state = AnimState::Idle;
    uint8_t frame = 0;
    float t = 0;          // time inside current frame (seconds)
    float speed = 1.0f;   // per-instance timing variation (zombies desync)
    bool finished = false;

    // Returns true if the state changed. Same-state calls keep the current
    // frame so rapid direction changes never reset the walk cycle.
    bool play(AnimState s, bool force = false);
    // Advance by simulation dt, returns AnimEvent bits that fired.
    uint8_t update(float dt);
    const AnimFrame& cur() const { return anim_def(state).f[frame]; }
    bool busy() const;    // a non-interruptible action is running
};

// 8-way direction from a facing vector: 0=E,1=SE,2=S,3=SW,4=W,5=NW,6=N,7=NE
int dir8(float fx, float fy);
