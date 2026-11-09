#include "anim.h"

static const AnimDef DEFS[] = {
#include "../../data/anims.inc"
};
static_assert(sizeof(DEFS) / sizeof(DEFS[0]) == (int)AnimState::Count, "anims.inc must define every AnimState in order");

static const char* NAMES[] = {"Idle", "Walk", "Run", "Aim", "Shoot", "Reload", "Melee", "Dodge", "Search", "Hurt", "Heal", "Death", "Dead",
                              "ZIdle", "ZWalk", "ZDetect", "ZAttack", "ZHurt", "ZStumble", "ZDeath", "ZLying", "ZRise", "Work", "Sleep", "Guard"};

const AnimDef& anim_def(AnimState s) { return DEFS[(int)s]; }
const char* anim_name(AnimState s) { return NAMES[(int)s]; }

bool Animator::busy() const { return !finished && !anim_def(state).interruptible && !anim_def(state).loop; }

bool Animator::play(AnimState s, bool force) {
    if (s == state && !finished) return false;  // keep cycle running
    const AnimDef& c = anim_def(state);
    const AnimDef& n = anim_def(s);
    if (!force && !finished) {
        if (state == AnimState::Dead || state == AnimState::Death || state == AnimState::ZDeath) return false;  // never revive
        if (!c.interruptible && !c.loop && n.priority <= c.priority) return false;
    }
    state = s; frame = 0; t = 0; finished = false;
    return true;
}

uint8_t Animator::update(float dt) {
    const AnimDef* d = &anim_def(state);
    uint8_t ev = 0;
    if (t == 0 && frame == 0 && !finished) ev |= d->f[0].event;  // first frame event
    t += dt * speed;
    for (int guard = 0; guard < 8; guard++) {
        float dur = d->f[frame].ms10 * 0.01f;
        if (t < dur) break;
        t -= dur;
        if (frame + 1 < d->n) { frame++; ev |= d->f[frame].event; continue; }
        if (d->loop) { frame = 0; ev |= d->f[0].event; continue; }
        // non-looping: hand over to next state
        finished = true;
        AnimState nx = d->next;
        if (nx != state) { state = nx; frame = 0; finished = false; d = &anim_def(state); ev |= d->f[0].event; }
        else { frame = d->n - 1; t = 0; break; }
    }
    if (t < 0.0001f && frame == 0) t = 0.0001f;  // mark first-frame event as fired
    return ev;
}

int dir8(float fx, float fy) {
    // octant from vector without atan2: compare against tan(22.5°) ~ 0.4142
    float ax = fx < 0 ? -fx : fx, ay = fy < 0 ? -fy : fy;
    if (ax < 1e-5f && ay < 1e-5f) return 2;
    if (ay < ax * 0.4142f) return fx > 0 ? 0 : 4;
    if (ax < ay * 0.4142f) return fy > 0 ? 2 : 6;
    if (fx > 0) return fy > 0 ? 1 : 7;
    return fy > 0 ? 3 : 5;
}
