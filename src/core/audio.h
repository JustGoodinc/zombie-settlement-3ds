#pragma once
// Original procedurally synthesised sound effects + software mixer (mono, 22050 Hz).
// Platforms only pull mixed samples: Windows waveOut, 3DS NDSP.
#include <stdint.h>

enum Sfx {
    SFX_PISTOL, SFX_REVOLVER, SFX_SHOTGUN, SFX_RIFLE, SFX_SMG, SFX_EMPTY, SFX_RELOAD, SFX_SWING, SFX_HIT,
    SFX_GROAN, SFX_ZSTEP, SFX_STEP, SFX_DOOR, SFX_GLASS, SFX_BANG, SFX_SEARCH, SFX_PICKUP, SFX_INV,
    SFX_MENU_MOVE, SFX_MENU_OK, SFX_MENU_BACK, SFX_BUILD, SFX_HEAL, SFX_DEATH, SFX_ALARM, SFX_HURT,
    SFX_ZDIE, SFX_CASING, SFX_BITE, SFX_COUNT
};

enum { AUDIO_RATE = 22050, AUDIO_VOICES = 8 };

void audio_init();
void audio_play(Sfx s, float vol = 1.0f, float pitch = 1.0f);
void audio_playAt(Sfx s, float x, float y, float vol = 1.0f, float pitch = 1.0f);  // world-positioned
void audio_listener(float x, float y);
void audio_ambience(bool night, bool indoors, bool settlement, bool title);
void audio_volumes(int master, int sfx, int music);  // 0..10
void audio_pause(bool paused);                       // dims ambience while menus pause the game
void audio_mix(int16_t* out, int frames);            // mono
int  audio_activeVoices();
