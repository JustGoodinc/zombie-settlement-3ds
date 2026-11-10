#include "audio.h"
#include "rng.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const float PI2 = 6.2831853f;
struct Buf { int16_t* d = nullptr; int n = 0; uint8_t prio = 1; };
static Buf sfx[SFX_COUNT];
struct Voice { int s = -1; float pos = 0, step = 1, vol = 0; };
static Voice voices[AUDIO_VOICES];
static float lx, ly, masterV = 0.8f, sfxV = 0.8f, musicV = 0.6f;
static bool ambNight, ambIndoor, ambSettle, ambTitle, paused;
static Rng arng(99);

static Buf mk(float sec) { Buf b; b.n = (int)(sec * AUDIO_RATE); b.d = (int16_t*)calloc(b.n, sizeof(int16_t)); return b; }
static void put(Buf& b, int i, float v) { if (i < 0 || i >= b.n) return; float o = b.d[i] / 32767.0f + v; if (o > 1) o = 1; if (o < -1) o = -1; b.d[i] = (int16_t)(o * 32767); }

// noise burst with decay and one-pole low-pass
static void noise(Buf& b, int start, float sec, float amp, float decay, float lp, uint32_t seed) {
    Rng r(seed); float y = 0; int n = (int)(sec * AUDIO_RATE);
    for (int i = 0; i < n; i++) {
        float t = (float)i / AUDIO_RATE, x = r.f() * 2 - 1;
        y += lp * (x - y);
        put(b, start + i, y * amp * expf(-t * decay));
    }
}
static void tone(Buf& b, int start, float sec, float f0, float f1, float amp, float decay, int wave) {
    float ph = 0; int n = (int)(sec * AUDIO_RATE);
    for (int i = 0; i < n; i++) {
        float t = (float)i / AUDIO_RATE, f = f0 + (f1 - f0) * (t / sec);
        ph += f / AUDIO_RATE; ph -= floorf(ph);
        float v = wave == 0 ? sinf(ph * PI2) : (wave == 1 ? (ph * 2 - 1) : (ph < 0.5f ? 1.0f : -1.0f));
        float env = expf(-t * decay) * (t < 0.004f ? t / 0.004f : 1.0f);
        put(b, start + i, v * amp * env);
    }
}
static int S(float sec) { return (int)(sec * AUDIO_RATE); }

void audio_init() {
    // firearms: crack (noise) + body thump (sine) + tail
    sfx[SFX_PISTOL] = mk(0.35f); noise(sfx[SFX_PISTOL], 0, 0.3f, 0.9f, 22, 0.6f, 1); tone(sfx[SFX_PISTOL], 0, 0.12f, 140, 60, 0.6f, 30, 0); sfx[SFX_PISTOL].prio = 3;
    sfx[SFX_REVOLVER] = mk(0.45f); noise(sfx[SFX_REVOLVER], 0, 0.4f, 1.0f, 14, 0.5f, 2); tone(sfx[SFX_REVOLVER], 0, 0.16f, 120, 50, 0.7f, 22, 0); sfx[SFX_REVOLVER].prio = 3;
    sfx[SFX_SHOTGUN] = mk(0.6f); noise(sfx[SFX_SHOTGUN], 0, 0.55f, 1.0f, 9, 0.35f, 3); tone(sfx[SFX_SHOTGUN], 0, 0.2f, 90, 40, 0.8f, 16, 0);
    noise(sfx[SFX_SHOTGUN], S(0.38f), 0.05f, 0.4f, 60, 0.8f, 33); noise(sfx[SFX_SHOTGUN], S(0.46f), 0.05f, 0.4f, 60, 0.8f, 34); sfx[SFX_SHOTGUN].prio = 3;
    sfx[SFX_RIFLE] = mk(0.7f); noise(sfx[SFX_RIFLE], 0, 0.65f, 1.0f, 8, 0.45f, 4); tone(sfx[SFX_RIFLE], 0, 0.22f, 110, 45, 0.8f, 14, 0); sfx[SFX_RIFLE].prio = 3;
    sfx[SFX_SMG] = mk(0.16f); noise(sfx[SFX_SMG], 0, 0.15f, 0.8f, 35, 0.6f, 5); tone(sfx[SFX_SMG], 0, 0.06f, 160, 80, 0.4f, 50, 0); sfx[SFX_SMG].prio = 3;
    sfx[SFX_EMPTY] = mk(0.08f); tone(sfx[SFX_EMPTY], 0, 0.02f, 1800, 1200, 0.5f, 200, 2); noise(sfx[SFX_EMPTY], S(0.03f), 0.03f, 0.3f, 150, 0.9f, 6);
    sfx[SFX_RELOAD] = mk(0.3f); noise(sfx[SFX_RELOAD], 0, 0.05f, 0.5f, 90, 0.9f, 7); tone(sfx[SFX_RELOAD], S(0.12f), 0.04f, 900, 600, 0.4f, 90, 2); noise(sfx[SFX_RELOAD], S(0.2f), 0.06f, 0.6f, 80, 0.85f, 8);
    sfx[SFX_SWING] = mk(0.2f); { Rng r(9); float y = 0; for (int i = 0; i < S(0.2f); i++) { float t = (float)i / AUDIO_RATE; y += (0.05f + t * 2) * ((r.f() * 2 - 1) - y); put(sfx[SFX_SWING], i, y * 0.6f * sinf(t / 0.2f * 3.14159f)); } }
    sfx[SFX_HIT] = mk(0.18f); noise(sfx[SFX_HIT], 0, 0.15f, 0.9f, 30, 0.2f, 10); tone(sfx[SFX_HIT], 0, 0.1f, 110, 60, 0.6f, 35, 0); sfx[SFX_HIT].prio = 2;
    sfx[SFX_GROAN] = mk(1.0f); {
        float ph = 0, y = 0; Rng r(11);
        for (int i = 0; i < sfx[SFX_GROAN].n; i++) {
            float t = (float)i / AUDIO_RATE, f = 95 + 15 * sinf(t * 7) - 20 * t + r.f() * 4;
            ph += f / AUDIO_RATE; ph -= floorf(ph);
            float v = (ph * 2 - 1) * 0.7f + (r.f() * 2 - 1) * 0.3f; y += 0.12f * (v - y);
            float env = sinf(fminf(t, 1.0f) * 3.14159f);
            put(sfx[SFX_GROAN], i, y * 0.8f * env);
        }
    }
    sfx[SFX_ZSTEP] = mk(0.12f); noise(sfx[SFX_ZSTEP], 0, 0.1f, 0.4f, 40, 0.1f, 12);
    sfx[SFX_STEP] = mk(0.06f); noise(sfx[SFX_STEP], 0, 0.05f, 0.35f, 70, 0.25f, 13); sfx[SFX_STEP].prio = 0;
    sfx[SFX_DOOR] = mk(0.45f); { float ph = 0; Rng r(14); for (int i = 0; i < S(0.4f); i++) { float t = (float)i / AUDIO_RATE, f = 220 - t * 180 + r.f() * 30; ph += f / AUDIO_RATE; ph -= floorf(ph); put(sfx[SFX_DOOR], i, (ph * 2 - 1) * 0.25f * (1 - t / 0.4f)); } }
    noise(sfx[SFX_DOOR], S(0.38f), 0.06f, 0.5f, 50, 0.3f, 15);
    sfx[SFX_GLASS] = mk(0.5f); noise(sfx[SFX_GLASS], 0, 0.3f, 0.7f, 18, 0.95f, 16);
    for (int k = 0; k < 6; k++) tone(sfx[SFX_GLASS], S(0.02f * k + 0.03f), 0.25f, 2400.0f + k * 530, 2300.0f + k * 500, 0.2f, 18, 0);
    sfx[SFX_BANG] = mk(0.3f); noise(sfx[SFX_BANG], 0, 0.25f, 1.0f, 18, 0.15f, 17); tone(sfx[SFX_BANG], 0, 0.15f, 80, 40, 0.7f, 25, 0); sfx[SFX_BANG].prio = 2;
    sfx[SFX_SEARCH] = mk(0.6f); { Rng r(18); float y = 0; for (int i = 0; i < sfx[SFX_SEARCH].n; i++) { float t = (float)i / AUDIO_RATE; y += 0.4f * ((r.f() * 2 - 1) - y); put(sfx[SFX_SEARCH], i, y * 0.25f * (0.5f + 0.5f * sinf(t * 40))); } }
    sfx[SFX_PICKUP] = mk(0.18f); tone(sfx[SFX_PICKUP], 0, 0.07f, 660, 660, 0.35f, 20, 0); tone(sfx[SFX_PICKUP], S(0.07f), 0.1f, 990, 990, 0.35f, 20, 0);
    sfx[SFX_INV] = mk(0.06f); noise(sfx[SFX_INV], 0, 0.05f, 0.4f, 80, 0.5f, 19); tone(sfx[SFX_INV], 0, 0.03f, 300, 200, 0.3f, 80, 0);
    sfx[SFX_MENU_MOVE] = mk(0.04f); tone(sfx[SFX_MENU_MOVE], 0, 0.035f, 880, 880, 0.18f, 60, 2);
    sfx[SFX_MENU_OK] = mk(0.12f); tone(sfx[SFX_MENU_OK], 0, 0.05f, 660, 660, 0.22f, 30, 2); tone(sfx[SFX_MENU_OK], S(0.05f), 0.07f, 990, 990, 0.22f, 30, 2);
    sfx[SFX_MENU_BACK] = mk(0.1f); tone(sfx[SFX_MENU_BACK], 0, 0.09f, 600, 380, 0.22f, 25, 2);
    sfx[SFX_BUILD] = mk(0.7f); for (int k = 0; k < 3; k++) { noise(sfx[SFX_BUILD], S(0.22f * k), 0.12f, 0.8f, 45, 0.3f, 20 + k); tone(sfx[SFX_BUILD], S(0.22f * k), 0.08f, 300, 200, 0.4f, 50, 0); }
    sfx[SFX_HEAL] = mk(0.5f); for (int k = 0; k < 3; k++) tone(sfx[SFX_HEAL], S(0.1f * k), 0.3f, 523.0f * (1 + k * 0.25f), 523.0f * (1 + k * 0.25f), 0.25f, 8, 0);
    sfx[SFX_DEATH] = mk(1.2f); tone(sfx[SFX_DEATH], 0, 1.2f, 220, 70, 0.5f, 2.5f, 1); tone(sfx[SFX_DEATH], 0, 1.2f, 233, 74, 0.3f, 2.5f, 0); sfx[SFX_DEATH].prio = 4;
    sfx[SFX_ALARM] = mk(1.2f); for (int k = 0; k < 4; k++) tone(sfx[SFX_ALARM], S(0.3f * k), 0.28f, k & 1 ? 520.0f : 700.0f, k & 1 ? 520.0f : 700.0f, 0.3f, 2, 2); sfx[SFX_ALARM].prio = 4;
    sfx[SFX_HURT] = mk(0.25f); tone(sfx[SFX_HURT], 0, 0.2f, 320, 180, 0.4f, 14, 1); noise(sfx[SFX_HURT], 0, 0.1f, 0.4f, 40, 0.3f, 23); sfx[SFX_HURT].prio = 3;
    sfx[SFX_ZDIE] = mk(0.6f); { float ph = 0; for (int i = 0; i < S(0.55f); i++) { float t = (float)i / AUDIO_RATE, f = 120 - t * 120; ph += f / AUDIO_RATE; ph -= floorf(ph); put(sfx[SFX_ZDIE], i, (ph * 2 - 1) * 0.5f * (1 - t / 0.55f)); } }
    noise(sfx[SFX_ZDIE], S(0.35f), 0.2f, 0.6f, 25, 0.12f, 24); sfx[SFX_ZDIE].prio = 2;
    sfx[SFX_CASING] = mk(0.1f); tone(sfx[SFX_CASING], 0, 0.04f, 3200, 3000, 0.12f, 60, 0); tone(sfx[SFX_CASING], S(0.05f), 0.04f, 3400, 3300, 0.08f, 70, 0); sfx[SFX_CASING].prio = 0;
    sfx[SFX_BITE] = mk(0.3f); noise(sfx[SFX_BITE], 0, 0.25f, 0.9f, 16, 0.25f, 25); tone(sfx[SFX_BITE], 0, 0.15f, 180, 90, 0.5f, 20, 1); sfx[SFX_BITE].prio = 3;
}

void audio_listener(float x, float y) { lx = x; ly = y; }
void audio_volumes(int m, int s, int mu) { masterV = m / 10.0f; sfxV = s / 10.0f; musicV = mu / 10.0f; }
void audio_ambience(bool night, bool indoors, bool settlement, bool title) { ambNight = night; ambIndoor = indoors; ambSettle = settlement; ambTitle = title; }
void audio_pause(bool p) { paused = p; }

void audio_play(Sfx s, float vol, float pitch) {
    if (vol <= 0.01f || !sfx[s].d) return;
    // voice stealing: free voice, else quietest lower-or-equal priority voice
    int best = -1; float bestScore = 1e9f;
    for (int i = 0; i < AUDIO_VOICES; i++) {
        if (voices[i].s < 0) { best = i; break; }
        float score = sfx[voices[i].s].prio * 10 + voices[i].vol;
        if (sfx[voices[i].s].prio <= sfx[s].prio && score < bestScore) { bestScore = score; best = i; }
    }
    if (best < 0) return;
    voices[best].s = s; voices[best].pos = 0; voices[best].step = pitch; voices[best].vol = vol;
}

void audio_playAt(Sfx s, float x, float y, float vol, float pitch) {
    float d = sqrtf((x - lx) * (x - lx) + (y - ly) * (y - ly));
    float att = d < 60 ? 1.0f : 60.0f / d;  // inverse distance: offscreen threats stay audible
    if (d > 420) return;
    audio_play(s, vol * att, pitch);
}

int audio_activeVoices() { int n = 0; for (auto& v : voices) n += v.s >= 0; return n; }

// procedural ambience + music drone, generated per sample (cheap oscillators)
static float windY, windMod, cricketPh, droneP[3], musicT;
void audio_mix(int16_t* out, int frames) {
    static const float CH[4][3] = {{110.0f, 130.8f, 164.8f}, {98.0f, 116.5f, 146.8f}, {87.3f, 110.0f, 130.8f}, {98.0f, 123.5f, 146.8f}};
    for (int i = 0; i < frames; i++) {
        float mix = 0;
        for (auto& v : voices) {
            if (v.s < 0) continue;
            const Buf& b = sfx[v.s]; int p = (int)v.pos;
            if (p >= b.n) { v.s = -1; continue; }
            mix += b.d[p] / 32767.0f * v.vol * sfxV;
            v.pos += v.step;
        }
        // ambience: wind (filtered noise, slowly modulated), crickets at night outdoors
        musicT += 1.0f / AUDIO_RATE;
        windMod = 0.5f + 0.5f * sinf(musicT * 0.21f) * sinf(musicT * 0.07f + 1.3f);
        windY += 0.02f * ((arng.f() * 2 - 1) - windY);
        float amb = windY * (ambIndoor ? 0.10f : 0.22f) * (0.4f + windMod);
        if (ambNight && !ambIndoor) {
            cricketPh += 4200.0f / AUDIO_RATE; cricketPh -= floorf(cricketPh);
            float gate = fmodf(musicT * 3.1f, 1.0f) < 0.12f && fmodf(musicT * 0.37f, 1.0f) < 0.6f ? 1.0f : 0.0f;
            amb += sinf(cricketPh * PI2) * 0.025f * gate;
        }
        // music: slow detuned drone chords (atmospheric, never intense)
        int chord = ((int)(musicT / 8.0f)) & 3; float fade = fmodf(musicT, 8.0f); float env = fade < 2 ? fade / 2 : (fade > 6 ? (8 - fade) / 2 : 1);
        float mus = 0;
        for (int k = 0; k < 3; k++) {
            droneP[k] += CH[chord][k] * (ambTitle ? 1.0f : 0.5f) / AUDIO_RATE; droneP[k] -= floorf(droneP[k]);
            mus += sinf(droneP[k] * PI2) * 0.05f;
        }
        mus *= env * (ambSettle || ambTitle ? 1.0f : 0.55f);
        float total = (mix + amb * sfxV + mus * musicV * (paused ? 0.6f : 1.0f)) * masterV;
        if (total > 1) total = 1; if (total < -1) total = -1;
        out[i] = (int16_t)(total * 32000);
    }
}

// Design-handoff export: every effect as a 16-bit mono WAV (22050 Hz).
static void writeWav(const char* path, const int16_t* d, int n) {
    FILE* f = fopen(path, "wb"); if (!f) return;
    uint32_t dsz = n * 2, rsz = 36 + dsz, sr = AUDIO_RATE, br = AUDIO_RATE * 2, fl = 16; uint16_t fmt[2] = {1, 1}, ba[2] = {2, 16};
    fwrite("RIFF", 1, 4, f); fwrite(&rsz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fl, 4, 1, f);
    fwrite(fmt, 2, 2, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(ba, 2, 2, f);
    fwrite("data", 1, 4, f); fwrite(&dsz, 4, 1, f); fwrite(d, 2, n, f); fclose(f);
}
static const char* SFX_NAMES[SFX_COUNT] = {"pistol", "revolver", "shotgun", "rifle", "smg", "empty_click", "reload", "melee_swing", "melee_hit",
    "zombie_groan", "zombie_step", "footstep", "door", "glass_break", "door_bang", "search", "pickup", "inventory_move", "menu_move", "menu_ok",
    "menu_back", "build", "heal", "death", "alarm", "hurt", "zombie_die", "casing", "bite"};
const char* audio_sfxName(int s) { return SFX_NAMES[s]; }
int audio_exportWavs(const char* dir) {
    char p[512]; int n = 0;
    for (int s = 0; s < SFX_COUNT; s++) if (sfx[s].d) { snprintf(p, sizeof p, "%s/sfx_%02d_%s.wav", dir, s, SFX_NAMES[s]); writeWav(p, sfx[s].d, sfx[s].n); n++; }
    // ambience + music beds, rendered through the real mixer
    struct Bed { const char* name; bool night, indoor, settle, title; } beds[4] = {
        {"bed_title_music", false, false, false, true}, {"bed_settlement_day", false, false, true, false},
        {"bed_expedition_night_outdoor", true, false, false, false}, {"bed_expedition_indoor", false, true, false, false}};
    static int16_t buf[AUDIO_RATE * 16];
    for (auto& b : beds) {
        for (auto& v : voices) v.s = -1;
        audio_ambience(b.night, b.indoor, b.settle, b.title); audio_volumes(10, 8, 8); audio_pause(false);
        audio_mix(buf, AUDIO_RATE * 16);
        snprintf(p, sizeof p, "%s/%s.wav", dir, b.name); writeWav(p, buf, AUDIO_RATE * 16); n++;
    }
    return n;
}
