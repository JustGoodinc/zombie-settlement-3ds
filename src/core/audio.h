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
// The soundtrack (music.cpp): one piece for the menus and one for each place, played by a small synth as the game runs.
enum MusicTrack { MUS_NONE = -1, MUS_MENU, MUS_SETTLEMENT, MUS_POLICE, MUS_HOSPITAL, MUS_MARKET, MUS_YARD, MUS_HOMES, MUS_COUNT };
void audio_music(int track);                   // the piece that should be playing; a change fades the old one out and the new one in
int  audio_musicTrack();                       // the piece last asked for
void music_init();
void music_reset(int track);                   // start a piece from the top at once, no fade (tools and tests)
void music_render(float* out, int frames);     // adds the music to out (about -1..1)
float music_loopSeconds(int track);
const char* music_name(int track);
const char* music_check();                     // nullptr when every pattern is well formed, else the name of the piece that is not
void audio_volumes(int master, int sfx, int music);  // 0..10
void audio_pause(bool paused);                       // dims ambience while menus pause the game
void audio_mix(int16_t* out, int frames);            // mono
int  audio_activeVoices();
int  audio_exportWavs(const char* dir);   // design handoff: writes every effect, the ambience beds and the soundtrack
int  audio_exportMusic(int track, const char* path);   // one full loop of a piece as a WAV; returns samples written
const char* audio_sfxName(int s);
