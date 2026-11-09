#include "combat.h"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

HitTable hit_table(const ShotParams& p) {
    // Baseline (skill 3, no bonuses, <=64px): miss 30, body 40, arm 15, leg 10, head 5.
    float missF = 1.0f - 0.07f * (p.skill - 3) - p.weaponAcc / 100.0f;
    float near = 64.0f;
    float far = p.dist > near ? (p.dist - near) / (p.range > near ? p.range - near : 1.0f) : 0.0f;
    missF += clampf(far, 0, 1.5f) * 0.9f;
    if (p.moving) missF += 0.25f;
    if (p.injured) missF += 0.30f;
    if (p.targetStill) missF -= 0.10f;
    missF = clampf(missF, 0.25f, 2.6f);
    float headF = (1.0f + 0.12f * (p.skill - 3) + p.weaponAcc / 200.0f) * clampf(1.0f - far, 0.3f, 1.0f);
    if (p.moving) headF *= 0.7f;
    headF = clampf(headF, 0.1f, 3.0f);

    float raw[HR_COUNT] = {300 * missF, 400, 150, p.targetDown ? 60.0f : 100.0f, 50 * headF};
    float sum = 0;
    for (float v : raw) sum += v;
    HitTable t; int total = 0;
    for (int i = 0; i < HR_COUNT; i++) { t.w[i] = (int)(raw[i] * 1000.0f / sum + 0.5f); if (t.w[i] < 0) t.w[i] = 0; total += t.w[i]; }
    t.w[HR_BODY] += 1000 - total;   // rounding remainder -> body, keeps sum exact
    return t;
}

HitResult hit_roll(const HitTable& t, Rng& r) {
    int v = r.range(1000);
    for (int i = 0; i < HR_COUNT; i++) { if (v < t.w[i]) return (HitResult)i; v -= t.w[i]; }
    return HR_BODY;
}

HitResult hit_rollNoMiss(const HitTable& t, Rng& r) {
    int tot = 1000 - t.w[HR_MISS];
    if (tot <= 0) return HR_BODY;
    int v = r.range(tot);
    for (int i = 1; i < HR_COUNT; i++) { if (v < t.w[i]) return (HitResult)i; v -= t.w[i]; }
    return HR_BODY;
}

int hit_damage(HitResult r, int d) {
    switch (r) {
    case HR_BODY: return d;
    case HR_ARM: return d * 55 / 100;
    case HR_LEG: return d * 60 / 100;
    case HR_HEAD: return 9999;  // ordinary zombies die instantly
    default: return 0;
    }
}

const char* hit_name(HitResult r) {
    static const char* N[] = {"MISS", "BODY", "ARM", "LEG", "HEADSHOT"};
    return N[r];
}
