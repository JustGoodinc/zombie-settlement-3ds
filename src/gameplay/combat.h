#pragma once
// Probability-based hit resolution. Tables are per-mille and always sum to 1000.
#include "../core/rng.h"

enum HitResult { HR_MISS, HR_BODY, HR_ARM, HR_LEG, HR_HEAD, HR_COUNT };

struct ShotParams {
    int skill = 3;          // shooting (or melee) skill 1..10; 3 = average survivor
    int weaponAcc = 0;      // weapon accuracy bonus in percent (+ upgrades)
    float dist = 32;        // px to target
    float range = 200;      // weapon range
    bool moving = false;
    bool injured = false;
    bool targetStill = false;   // idle or lying target
    bool targetDown = false;    // lying zombie: no limb/head distinction for legs
};

struct HitTable { int w[HR_COUNT]; };

HitTable hit_table(const ShotParams& p);
HitResult hit_roll(const HitTable& t, Rng& r);
// Roll conditioned on a hit (used when a stray bullet strikes something by collision).
HitResult hit_rollNoMiss(const HitTable& t, Rng& r);
int hit_damage(HitResult r, int baseDmg);   // HEAD returns a lethal value
const char* hit_name(HitResult r);
