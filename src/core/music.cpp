// The soundtrack: seven slow pieces for a small synthesiser, written as horror score rather than as songs. Each one is
// made of the same few things: a low drone that never quite holds still, string chords that slide into place and sit
// a semitone wrong, one sparse figure (a piano, a bell, a music box), a held note, and a sound that belongs to the
// place (a heartbeat, a cell door, a heart monitor, a shop chime, a tolling bell, a clock). Nothing keeps a beat.
// Every note is written out below as patterns and played by the sequencer as the game runs, so the music is a few
// kilobytes of text and no recorded sound. All seven are original.
#include "audio.h"
#include <math.h>
#include <string.h>

void (*music_trace)(int sect, int bar, int step, int voice, int note) = nullptr;
int music_voices = 255;

namespace {
const float RATE = (float)AUDIO_RATE;
float SINT[1024], HZ[128];
inline float sn(float ph) { return SINT[(int)(ph * 1024.0f) & 1023]; }
// band-limits the step in a saw or pulse wave (PolyBLEP), so high notes do not fold back as grit
inline float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// ---------------------------------------------------------------- how a piece is written
// A piece is a list of sections. A section lasts `bars` bars, names the voices that play (layers) and gives each one
// a pattern of steps, which repeats until the section ends. A step is a sixteenth note at the piece's tempo; the
// tempo only sets how fine that grid is, since nothing here plays on every step.
//   chords              one per bar, cycled: semitones above the key's root (six or more means that far below it
//                       instead), then the kind of chord, a letter from KINDS
//   bass, bell, lead    a note in semitones above the voice's key: 0-9, then a=10 ... n=23.
//                       In the bass, r is the root of the bar's chord, so one pattern follows the chords down
//   thump, slam, tick   x a hit, o a softer one
//   everywhere          . silence   - hold the note
// L_TREM is not a voice: it makes the strings shiver for the length of the section.
enum { L_BASS = 1, L_BELL = 2, L_PAD = 4, L_LEAD = 8, L_THUMP = 16, L_SLAM = 32, L_TICK = 64, L_TREM = 128 };
struct Sect { uint8_t bars, layers; const char* chords; const char* bass; const char* bell; const char* lead; const char* thump; const char* slam; const char* tick; };

// the four notes the strings play for each kind of chord, in semitones above its root
struct Kind { char id; int8_t pad[4]; };
const Kind KINDS[] = {
    {'m', {0, 0, 3, 7}},      // minor
    {'M', {0, 0, 4, 7}},      // major (only ever a semitone above the key, where it sounds like a threat)
    {'o', {0, 0, 7, 12}},     // a bare fifth: hollow, neither major nor minor
    {'b', {0, 3, 7, 8}},      // minor with a flat sixth grinding against the fifth
    {'j', {0, 3, 7, 11}},     // minor with a major seventh: the chord that does not resolve
    {'t', {0, 0, 6, 12}},     // a tritone and nothing else
    {'a', {0, 0, 4, 8}},      // augmented: no way home in it
    {'c', {0, 7, 12, 13}},    // a fifth with a flat ninth on top
};
const int NKIND = sizeof(KINDS) / sizeof(KINDS[0]);

// what a struck thing is made of: for each partial its pitch (times the note), its level, and how long it rings
// (times the piece's ring time)
struct Partial { float ratio, amp, ring; };
const Partial STRUCK[4][4] = {
    {{1, 1, 1}, {2, 0.45f, 0.6f}, {3, 0.22f, 0.4f}, {4, 0.10f, 0.28f}},            // 0: a piano string under a soft hammer: the upper partials die first
    {{1, 1, 1}, {2, 0.60f, 0.7f}, {3, 0.40f, 0.5f}, {5, 0.20f, 0.3f}},             // 1: a picked string: brighter, and it stays bright a little longer
    {{1, 1, 1}, {2.756f, 0.42f, 0.4f}, {5.404f, 0.16f, 0.15f}, {0, 0, 0}},         // 2: a struck bar, whose partials are not in tune: glass, a chime, a music box
    {{0.5f, 0.55f, 1.3f}, {1, 1, 1}, {1.2f, 0.5f, 0.6f}, {2, 0.3f, 0.35f}},        // 3: a big bell: hum, strike note, the minor third that makes it a bell, octave
};

struct Bass { uint8_t key; float cut, env, decay, sus, atk, glide; };   // filter: resting cutoff, how far a note opens it, how fast it closes (s); level it settles at; fade in (s); slide between notes (s)
struct Bell { uint8_t key, wave; float ring, level; };                  // which STRUCK row; how long the lowest partial rings (s); loudness
struct Pad  { uint8_t key; float cut, glide, warp; };                   // brightness; slide into each chord (s); how far two of the four strings drift off pitch
struct Lead { uint8_t key; float bright, atk, glide, vibHz, vib; };     // brightness; fade in (s); slide between notes (s); vibrato speed and depth
struct Piece {
    const char* name;
    uint8_t bpm, steps;                 // quarter notes a minute, steps in a bar
    Bass bass; Bell bell; Pad pad; Lead lead;
    uint8_t echoSteps; float echoFb;    // echo on everything but the drone and strings: delay in steps, feedback
    bool metal;                         // the slam is struck steel, not a door
    const Sect* sect; uint8_t nsect;
};

const char HOLD16[] = "r---------------";   // the drone: one long note a bar, following the chords

// ---- Dead Air (menus): A minor. A heartbeat under a drone, chords that sink from A minor to F minor and crawl up
// to B flat, a piano that keeps touching the note a semitone above the one it wants, and one held string that the
// chords turn sour underneath. The heart speeds up when the string comes in.
const char AIR_CH[] = "0m 0m 8m 8m 0m 0m 1M 1M";
const char AIR_HEART[] = "xo......xo......", AIR_RACE[] = "xo..xo..xo..xo..";
const char AIR_KEYS[] = "7.......8......." "7..............." "3.......4......." "3..............."
                        "7.......8......." "7.......c......." "d.......8......." "7...............";
const char AIR_LAST[] = "7.......8......." "7...............";
const char AIR_STRING[] = "7---------------" "----------------" "----------------" "----------------"
                          "................" "j---------------" "----------------" "------------....";
const Sect AIR[] = {
    {4, L_BASS | L_PAD | L_THUMP, "0o", HOLD16, nullptr, nullptr, AIR_HEART},
    {8, L_BASS | L_PAD | L_THUMP, AIR_CH, HOLD16, nullptr, nullptr, AIR_HEART},
    {8, L_BASS | L_PAD | L_THUMP | L_BELL, AIR_CH, HOLD16, AIR_KEYS, nullptr, AIR_HEART},
    {8, L_BASS | L_PAD | L_THUMP | L_BELL | L_LEAD | L_TREM, AIR_CH, HOLD16, AIR_KEYS, AIR_STRING, AIR_RACE},
    {4, L_BASS | L_BELL, "0m", HOLD16, AIR_LAST},
};
// ---- Embers (settlement): D minor. No pulse at all: a hollow fifth, chords that keep sinking by a third, somebody
// picking out a slow tune by the fire that stops on the wrong note, a voice that sighs up a semitone and back, and
// one thud from outside the walls.
const char EMB_CH[] = "0o 0m 8m 8m 5m 5m 0m 0j";
const char EMB_DRONE[] = "c---------------" "c---------------" "8---------------" "8---------------"     // D, B flat, G: down a third each time
                         "5---------------" "5---------------" "c---------------" "c---------------";
const char EMB_TUNE[] = "j.......f......." "e.........c....." "f.......b......." "8..............."
                        "h.....k........." "....j..........." "f.......e......." "....b...........";
const char EMB_END[] = "j.......f......." "e.........c....." "................" "....b...........";
const char EMB_VOICE[] = "7---------------" "----------------" "8---------------" "----------------"
                         "----------------" "----------------" "7---------------" "------------....";
const char EMB_THUD[] = "................" "................" "................" "................"
                        "................" "................" "................" "........o.......";
const Sect EMB[] = {
    {4, L_BASS | L_PAD, "0o 0o 0o 0m", EMB_DRONE},
    {8, L_BASS | L_PAD | L_BELL, EMB_CH, EMB_DRONE, EMB_TUNE},
    {8, L_BASS | L_PAD | L_BELL | L_LEAD | L_THUMP, EMB_CH, EMB_DRONE, EMB_TUNE, EMB_VOICE, EMB_THUD},
    {4, L_PAD | L_BELL, "0o 0o 0m 0j", nullptr, EMB_END},
};
// ---- Holding Cells (police station): E minor. Low notes struck twice and left to die, the second one sometimes a
// semitone up; a door slamming somewhere down the corridor; a high string that will not come off its note while the
// chords under it go bad, and then slides.
const char CELL_CH[] = "0m 0m 0b 0b 6t 6t 1M 1M";
const char CELL_STAB[] = "0---..0---......" "0---..1---......";
const char CELL_STAB2[] = "0---..0---..6---" "0---..1---..6---";
const char CELL_HELD[] = "j---------------";
const char CELL_LINE[] = "j---------------" "----------------" "k---------------" "----------------"
                         "i---------------" "----------------" "h---------------" "------------....";
const char CELL_DOOR[] = "................" "................" "................" "..........x.....";
const char CELL_DOORS[] = "................" "..........x....." "................" "....x.......o...";
const Sect CELL[] = {
    {4, L_BASS | L_PAD | L_SLAM, "0o", CELL_STAB, nullptr, nullptr, nullptr, CELL_DOOR},
    {8, L_BASS | L_PAD | L_SLAM, CELL_CH, CELL_STAB, nullptr, nullptr, nullptr, CELL_DOOR},
    {8, L_BASS | L_PAD | L_SLAM | L_LEAD, CELL_CH, CELL_STAB, nullptr, CELL_HELD, nullptr, CELL_DOOR},
    {8, L_BASS | L_PAD | L_SLAM | L_LEAD | L_TREM, CELL_CH, CELL_STAB2, nullptr, CELL_LINE, nullptr, CELL_DOORS},
    {4, L_PAD | L_SLAM, "0m 0m 0b 0b", nullptr, nullptr, nullptr, nullptr, CELL_DOOR},
};
// ---- Flatline (hospital): C sharp minor, on a fine grid so the monitor can be short. A heart monitor keeping time
// over a cold drone, glass dripping down a semitone at a time; the pulse races, stumbles and becomes one long note;
// then nothing.
const char WARD_CH[] = "0m 0m 0b 0b 0m 0m 6t 6t";
const char WARD_DRONE[] = "r-------------------------------";
const char WARD_BEEP[] = "j.......j.......j.......j.......";
const char WARD_LATE[] = "................................" "................................" "................................" "................................"
                         "j.......j.......j.......j......." "j.......j.......j.......j......." "j.......j.......j.......j......." "j.......j.......j.......j.......";
const char WARD_GLASS[] = "7...............6..............." "5.......................3......." "8...............7..............." "................................"
                          "7...............6..............." "5.......................3......." "6...............0..............." "................................";
const char WARD_PANIC[] = "8.......7.......8.......7......." "8...7...8...7...8..............." "6.......0.......6.......0......." "6...............................";
const char WARD_RACE[] = "j...j...j...j...j...j...j...j..." "j...j...j...j...j...j...j...j..." "j...j.......j...j...j.......j..." "j...........j...j...............";
const char WARD_FLAT[] = "j-------------------------------" "--------------------------------" "----------------................";
const Sect WARD[] = {
    {2, L_BASS | L_PAD | L_LEAD, "0o", WARD_DRONE, nullptr, WARD_BEEP},
    {8, L_BASS | L_PAD | L_BELL, WARD_CH, WARD_DRONE, WARD_GLASS},
    {8, L_BASS | L_PAD | L_BELL | L_LEAD, WARD_CH, WARD_DRONE, WARD_GLASS, WARD_LATE},
    {4, L_BASS | L_PAD | L_BELL | L_LEAD | L_TREM, "0b 0b 6t 6t", WARD_DRONE, WARD_PANIC, WARD_RACE},
    {3, L_PAD | L_LEAD | L_TREM, "0c", nullptr, nullptr, WARD_FLAT},
    {1, 0, "0m"},
};
// ---- Closing Time (supermarket): G minor. The shop's two-note chime still going off in the empty aisles, the
// chord under it turning minor after each one and the chime coming back flat, then as a tritone; and the tune the
// speakers used to play, slowed right down and warped.
const char SHOP_CH[] = "0o 0m 8m 8m 0o 0m 6t 1M";
const char SHOP_DRONE[] = "r---------------" "r---------------" "r---------------" "r---------------"
                          "r---------------" "r---------------" "0---------------" "r---------------";
const char SHOP_CHIME[] = "g..c............" "................" "f..b............" "................"
                          "g..c............" "................" "c..6............" "................";
const char SHOP_OPEN[] = "g..c............" "................";
const char SHOP_TAPE[] = "j---------------" "f---------------" "----------------" "b---------------"
                         "c---------------" "f---------------" "i---------------" "h-----------....";
const char SHOP_CRASH[] = "................" "................" "................" "........x......."
                          "................" "................" "................" "................";
const char SHOP_LOW[] = "0---------------" "0---------------" "r---------------" "r---------------";
const char SHOP_KNOCK[] = "................" "......x.o......." "................" "..x.o.....o.....";
const Sect SHOP[] = {
    {4, L_BASS | L_PAD | L_BELL, "0o", HOLD16, SHOP_OPEN},
    {8, L_BASS | L_PAD | L_BELL, SHOP_CH, SHOP_DRONE, SHOP_CHIME},
    {8, L_BASS | L_PAD | L_BELL | L_LEAD | L_SLAM, SHOP_CH, SHOP_DRONE, SHOP_CHIME, SHOP_TAPE, nullptr, SHOP_CRASH},
    {4, L_BASS | L_PAD | L_THUMP | L_TREM, "6t 6t 1M 1M", SHOP_LOW, nullptr, nullptr, SHOP_KNOCK},
    {2, L_BASS | L_BELL, "0o", HOLD16, SHOP_OPEN},
};
// ---- Dead Freight (train yard): B minor, seven to the bar so that it limps. A deep drone, something heavy that
// knocks twice a bar, struck steel ringing off into the dark, a bell that tolls three times and misses the fourth,
// and a horn a long way off that sags as it passes.
const char YARD_CH[] = "0m 0m 0m 0m 8m 8m 6t 6t";
const char HOLD14[] = "r-------------";
const char YARD_DRONE[] = "r-------------" "r-------------" "r-------------" "r-------------" "r-------------" "r-------------" "0-------------" "0-------------";
const char YARD_FALL[] = "r-------------" "r-------------" "0-------------" "0-------------";
const char YARD_LIMP[] = "x.....o.......";
const char YARD_STEEL[] = ".............." "..........x..." ".............." "....x.........";
const char YARD_STEEL2[] = "..........x..." "....x........." "..........x..." "....x.....o...";
const char YARD_TOLL[] = "...0.........." ".............." "...0.........." ".............." ".............." ".............." "...0.........." "..............";
const char YARD_TOLL2[] = "...0.........." "..............";
const char YARD_HORN[] = ".............." "7-------------" "6-------------" ".............." ".............." "f-------------" "e-------------" "..............";
const Sect YARD[] = {
    {4, L_BASS | L_PAD | L_THUMP, "0o", HOLD14, nullptr, nullptr, YARD_LIMP},
    {8, L_BASS | L_PAD | L_THUMP | L_SLAM, YARD_CH, YARD_DRONE, nullptr, nullptr, YARD_LIMP, YARD_STEEL},
    {8, L_BASS | L_PAD | L_THUMP | L_SLAM | L_BELL | L_LEAD, YARD_CH, YARD_DRONE, YARD_TOLL, YARD_HORN, YARD_LIMP, YARD_STEEL},
    {4, L_BASS | L_PAD | L_THUMP | L_SLAM | L_TREM, "8m 8m 6t 6t", YARD_FALL, nullptr, nullptr, YARD_LIMP, YARD_STEEL2},
    {4, L_BASS | L_BELL, "0o", HOLD14, YARD_TOLL2},
};
// ---- Maple Street (neighbourhood): F minor, three to the bar. A clock, a music box playing a lullaby, the chords
// under it going bad the second time round; then the box runs down before it reaches its last note and plays a
// wrong one instead, something moans, slow footsteps cross the floor overhead and stop, and only the clock is left.
const char MAPLE_CH[] = "0m 0j 0j 0m 5m 0b 6t 0j";
const char MAPLE_CH2[] = "0j 0j 6t 6t 0j 0a 0a 6t";
const char HOLD12[] = "0-----------";
const char MAPLE_CLOCK[] = "x...o...x...o...x...o...";
const char MAPLE_BOX[] = "j...f...c..." "e..........." "h...e...b..." "c..........." "k...h...c..." "f..........." "e...b...7..." "c...........";
const char MAPLE_RUNDOWN[] = "j...f...c..." "e..........." "h....e.....b" "............" "....d......." "............" "j......f...." "............";
const char MAPLE_MOAN[] = "c-----------" "------------" "b-----------" "------------" "a-----------" "------------" "9-----------" "--------....";
const char MAPLE_STEPS[] = "............" "............" "..o.....o..." "..o.....o..." "..o........." "............" "............" "........x...";
const Sect MAPLE[] = {
    {4, L_TICK | L_BASS | L_PAD, "0o", HOLD12, nullptr, nullptr, nullptr, nullptr, MAPLE_CLOCK},
    {8, L_TICK | L_BASS | L_PAD | L_BELL, "0o", HOLD12, MAPLE_BOX, nullptr, nullptr, nullptr, MAPLE_CLOCK},
    {8, L_TICK | L_BASS | L_PAD | L_BELL, MAPLE_CH, HOLD12, MAPLE_BOX, nullptr, nullptr, nullptr, MAPLE_CLOCK},
    {8, L_TICK | L_BASS | L_PAD | L_BELL | L_LEAD | L_THUMP | L_TREM, MAPLE_CH2, HOLD12, MAPLE_RUNDOWN, MAPLE_MOAN, MAPLE_STEPS, nullptr, MAPLE_CLOCK},
    {4, L_TICK | L_BASS, "0o", HOLD12, nullptr, nullptr, nullptr, nullptr, MAPLE_CLOCK},
};

#define SECTS(a) a, (uint8_t)(sizeof(a) / sizeof(a[0]))
const Piece PIECES[MUS_COUNT] = {
    //                 bpm steps   bass: key cut env decay sus fade slide          struck: key kind ring level   strings: key cut slide drift     lead: key bright fade slide vibrato     echo
    {"Dead Air",        52, 16, {45, 0.075f, 0.04f, 2.0f, 1.0f, 1.5f,   0.8f}, {69, 0, 2.0f, 0.14f}, {57, 0.20f, 0.6f, 0.004f}, {69, 0.20f, 1.5f,   0.0f, 5.0f, 0.005f}, 3, 0.50f, false, SECTS(AIR)},
    {"Embers",          46, 16, {38, 0.060f, 0.03f, 2.0f, 1.0f, 2.0f,   1.0f}, {50, 1, 2.4f, 0.13f}, {50, 0.17f, 0.9f, 0.006f}, {62, 0.14f, 1.5f,   0.5f, 4.6f, 0.005f}, 2, 0.45f, false, SECTS(EMB)},
    {"Holding Cells",   50, 16, {40, 0.060f, 0.30f, 1.0f, 0.0f, 0.003f, 0.0f}, {64, 0, 1.0f, 0.14f}, {52, 0.18f, 0.5f, 0.005f}, {64, 0.14f, 1.2f,   0.7f, 4.2f, 0.006f}, 3, 0.55f, false, SECTS(CELL)},
    {"Flatline",       100, 32, {49, 0.060f, 0.03f, 2.0f, 1.0f, 2.0f,   1.2f}, {85, 2, 1.8f, 0.09f}, {61, 0.16f, 0.8f, 0.003f}, {61, 0.12f, 0.004f, 0.0f, 5.0f, 0.000f}, 6, 0.45f, false, SECTS(WARD)},
    {"Closing Time",    50, 16, {43, 0.070f, 0.03f, 2.0f, 1.0f, 2.0f,   1.0f}, {55, 2, 2.6f, 0.18f}, {55, 0.18f, 1.2f, 0.012f}, {55, 0.18f, 0.8f,   0.6f, 0.7f, 0.012f}, 3, 0.50f, false, SECTS(SHOP)},
    {"Dead Freight",    48, 14, {47, 0.055f, 0.05f, 2.0f, 1.0f, 1.5f,   1.0f}, {59, 3, 3.2f, 0.24f}, {59, 0.20f, 0.8f, 0.005f}, {47, 0.13f, 0.5f,   1.2f, 5.0f, 0.000f}, 4, 0.50f, true,  SECTS(YARD)},
    {"Maple Street",    60, 12, {41, 0.060f, 0.03f, 2.0f, 1.0f, 2.0f,   0.0f}, {65, 2, 1.0f, 0.13f}, {53, 0.17f, 0.7f, 0.008f}, {41, 0.07f, 1.0f,   1.0f, 3.5f, 0.010f}, 3, 0.35f, false, SECTS(MAPLE)},
};

int semis(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'n' ? c - 'a' + 10 : 99); }
// chords: "<semitones><kind>" separated by spaces; 0 if one of them is not a kind of chord
int parseChords(const char* s, int8_t* root, uint8_t* kind) {
    int n = 0;
    while (*s && n < 8) {
        while (*s == ' ') s++;
        if (*s < '0' || *s > '9') break;
        int r = 0; while (*s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0');
        int k = 0; while (k < NKIND && KINDS[k].id != *s) k++;
        if (k == NKIND) return 0;
        root[n] = (int8_t)(r >= 6 ? r - 12 : r); kind[n] = (uint8_t)k; s++; n++;
    }
    return n;
}

// ---------------------------------------------------------------- the player
enum { ECHO = 32768 };
struct Struck { float ph[4], e[4], inc; };       // one ringing bell or string: where each partial is and how loud
struct State {
    int piece, sect, bar, step; float toStep, spstep;
    int8_t root[8]; uint8_t kind[8]; int nchord, len[6];
    float bFdec, bAtk, bRel, bGl, eDec[4], eAmp[4], eRatio[4], pGl, lAtk, lGl;   // per-piece constants, worked out once
    float bPh, bPh2, bSub, bInc, bCur, bLo, bBd, bFenv, bAmp; bool bGate;
    Struck bell[2]; int bellNext;
    float lPh, lInc, lCur, lAmp, lLp, lVib, lVibAmt; bool lGate;
    float pPh[4], pInc[4], pCur[4], pGain, pLp1, pLp2, trem, tremAmt; bool pOn, tremOn;
    float kPh, kEnv, kTgt, kPenv;
    float sE[3], sPh[3], sHit, sLp, hEnv, hPh, hInc, hLp; uint32_t noise;
    float slow, slowB, wA, wB;                   // very slow wobbles: the two filters, and the two strings that drift
    int ei, elen, etap; float eLp, echo[ECHO];
} M;
int want = MUS_NONE, cur = MUS_NONE;
float fade = 0;
const float STEEL[3] = {185.0f, 510.0f, 1000.0f};   // the first three ways a steel bar rings: not in tune with each other
const float KDEC = expf(-1 / (0.16f * RATE)), KPDEC = expf(-1 / (0.035f * RATE)), HITDEC = expf(-1 / (0.02f * RATE)), HDEC = expf(-1 / (0.010f * RATE)),
            DOORDEC[2] = {expf(-1 / (0.35f * RATE)), expf(-1 / (0.22f * RATE))},
            STEELDEC[3] = {expf(-1 / (1.1f * RATE)), expf(-1 / (0.5f * RATE)), expf(-1 / (0.22f * RATE))};

void loadSect() {
    const Sect& s = PIECES[M.piece].sect[M.sect];
    M.nchord = parseChords(s.chords, M.root, M.kind);
    const char* pat[6] = {s.bass, s.bell, s.lead, s.thump, s.slam, s.tick};
    for (int k = 0; k < 6; k++) M.len[k] = pat[k] ? (int)strlen(pat[k]) : 1;
}
void start(int piece) {
    memset(&M, 0, sizeof M);
    const Piece& p = PIECES[piece];
    M.piece = piece; M.step = -1; M.noise = 0x1234567u;
    M.spstep = RATE * 60.0f / p.bpm / 4.0f;
    M.elen = (int)(p.echoSteps * M.spstep); if (M.elen > ECHO - 1) M.elen = ECHO - 1; if (M.elen < 1) M.elen = 1;
    M.etap = (int)(M.elen * 0.382f);
    M.bFdec = expf(-1 / (p.bass.decay * RATE)); M.bAtk = 1 / (p.bass.atk * RATE); M.bRel = M.bAtk * 0.5f < 0.0004f ? M.bAtk * 0.5f : 0.0004f;
    M.bGl = p.bass.glide > 0 ? 1 / (p.bass.glide * RATE) : 1;
    for (int k = 0; k < 4; k++) {
        const Partial& q = STRUCK[p.bell.wave][k];
        M.eRatio[k] = q.ratio; M.eAmp[k] = q.amp; M.eDec[k] = q.amp > 0 ? expf(-1 / (p.bell.ring * q.ring * RATE)) : 0;
    }
    M.pGl = 1 / (p.pad.glide * RATE); M.lAtk = 1 / (p.lead.atk * RATE); M.lGl = p.lead.glide > 0 ? 1 / (p.lead.glide * RATE) : 1;
    loadSect();
}
inline char cell(const char* pat, int len, int pos, int on) { return pat && on ? pat[pos % len] : '.'; }
// the next step: move on (a step, a bar, a section) and start whatever falls on it
void advance() {
    const Piece& p = PIECES[M.piece];
    if (++M.step >= p.steps) {
        M.step = 0;
        if (++M.bar >= p.sect[M.sect].bars) { M.bar = 0; M.sect = (M.sect + 1) % p.nsect; loadSect(); }
    }
    const Sect& s = p.sect[M.sect];
    const int pos = M.bar * p.steps + M.step, ci = M.bar % M.nchord, root = M.root[ci];
    const int on = s.layers & music_voices;
#define TRACE(voice, note) if (music_trace) music_trace(M.sect, M.bar, M.step, voice, note)
    char c = cell(s.bass, M.len[0], pos, on & L_BASS);
    int n = c == 'r' ? root : semis(c);
    if (n != 99) {
        M.bInc = HZ[p.bass.key + n] / RATE; M.bFenv = 1;
        if (!M.bGate && M.bAmp < 0.02f) M.bCur = M.bInc;   // out of silence: start on the note, do not slide up to it
        M.bGate = true; TRACE(0, p.bass.key + n);
    } else if (c != '-') M.bGate = false;
    c = cell(s.bell, M.len[1], pos, on & L_BELL); n = semis(c);
    if (n != 99) {   // two can ring at once, so a note does not cut off the one before it
        Struck& v = M.bell[M.bellNext ^= 1];
        v.inc = HZ[p.bell.key + n] / RATE;
        for (int k = 0; k < 4; k++) { v.ph[k] = 0; v.e[k] = M.eAmp[k]; }
        TRACE(1, p.bell.key + n);
    }
    c = cell(s.lead, M.len[2], pos, on & L_LEAD); n = semis(c);
    if (n != 99) {
        M.lInc = HZ[p.lead.key + n] / RATE;
        if (!M.lGate) { M.lVibAmt = 0; if (M.lAmp < 0.02f) M.lCur = M.lInc; }
        M.lGate = true; TRACE(2, p.lead.key + n);
    } else if (c != '-') M.lGate = false;
    if (M.step == 0) {   // the strings take up the chord of the bar, sliding there from the last one
        const bool sounding = M.pOn || M.pGain > 0.01f;
        M.pOn = (on & L_PAD) != 0; M.tremOn = (on & L_TREM) != 0;
        if (M.pOn) {
            const int8_t* iv = KINDS[M.kind[ci]].pad; const int r = p.pad.key + root;
            for (int k = 0; k < 4; k++) {   // a doubled root sits a hair sharp and the top string a hair flat, so the chord never stands still
                M.pInc[k] = HZ[r + iv[k]] * (k == 1 && iv[1] == iv[0] ? 1.006f : (k == 3 ? 0.997f : 1.0f)) / RATE;
                if (!sounding) M.pCur[k] = M.pInc[k];
                TRACE(3, r + iv[k]);
            }
        }
    }
    c = cell(s.thump, M.len[3], pos, on & L_THUMP);
    if (c == 'x' || c == 'o') { M.kTgt = c == 'x' ? 1.0f : 0.6f; M.kPenv = 1; TRACE(4, c == 'x'); }
    c = cell(s.slam, M.len[4], pos, on & L_SLAM);
    if (c == 'x' || c == 'o') { M.sE[0] = M.sE[1] = M.sE[2] = M.sHit = c == 'x' ? 1.0f : 0.5f; M.sPh[0] = M.sPh[1] = M.sPh[2] = 0; TRACE(5, c == 'x'); }
    c = cell(s.tick, M.len[5], pos, on & L_TICK);
    if (c == 'x' || c == 'o') { M.hEnv = c == 'x' ? 1.0f : 0.75f; M.hInc = (c == 'x' ? 1700.0f : 1300.0f) / RATE; M.hPh = 0; TRACE(6, c == 'x'); }
#undef TRACE
}
}   // namespace

void music_init() {
    for (int i = 0; i < 1024; i++) SINT[i] = sinf(i * 6.2831853f / 1024.0f);
    for (int i = 0; i < 128; i++) HZ[i] = 440.0f * powf(2.0f, (i - 69) / 12.0f);
}
void audio_music(int track) { want = track >= 0 && track < MUS_COUNT ? track : MUS_NONE; }
int audio_musicTrack() { return want; }
void music_reset(int track) { want = cur = track >= 0 && track < MUS_COUNT ? track : MUS_NONE; fade = cur >= 0 ? 1.0f : 0.0f; if (cur >= 0) start(cur); }
const char* music_name(int track) { return track >= 0 && track < MUS_COUNT ? PIECES[track].name : ""; }
float music_loopSeconds(int track) {
    const Piece& p = PIECES[track]; int bars = 0;
    for (int i = 0; i < p.nsect; i++) bars += p.sect[i].bars;
    return bars * p.steps * 60.0f / p.bpm / 4.0f;
}
// every pattern must fill whole bars and use only known letters, every chord must be a known kind, every note must
// be on the keyboard and the echo must fit its buffer
const char* music_check() {
    for (const Piece& p : PIECES) {
        if (p.bass.key < 6 || p.pad.key < 6 || p.bass.key > 104 || p.bell.key > 104 || p.lead.key > 104 || p.pad.key > 108 || p.bell.wave > 3) return p.name;
        if (p.echoSteps * (RATE * 60.0f / p.bpm / 4.0f) >= ECHO) return p.name;
        for (int i = 0; i < p.nsect; i++) {
            const Sect& s = p.sect[i];
            int8_t root[8]; uint8_t kind[8];
            if (!s.bars || parseChords(s.chords, root, kind) < 1) return p.name;
            const char* pat[6] = {s.bass, s.bell, s.lead, s.thump, s.slam, s.tick};
            for (int k = 0; k < 6; k++) {
                if (!pat[k]) continue;
                if (!*pat[k] || strlen(pat[k]) % p.steps) return p.name;
                for (const char* c = pat[k]; *c; c++) {
                    if (*c == '.' || *c == '-') continue;
                    if (k < 3 ? semis(*c) == 99 && !(k == 0 && *c == 'r') : *c != 'x' && *c != 'o') return p.name;
                }
            }
        }
    }
    return nullptr;
}

// Adds `n` samples of music to `out`. A change of piece fades the old one out, starts the new one from the top and
// fades it in.
void music_render(float* out, int n) {
    while (n > 0) {
        if (cur != want && fade <= 0) { cur = want; if (cur >= 0) start(cur); }
        if (cur < 0) { fade = 0; return; }
        if (M.toStep <= 0) { advance(); M.toStep += M.spstep; }
        int m = (int)ceilf(M.toStep); if (m > n) m = n; if (m < 1) m = 1;
        const float target = cur == want ? 1.0f : 0.0f, slew = cur == want ? 1.0f / (1.2f * RATE) : 1.0f / (0.6f * RATE);
        const Piece& p = PIECES[M.piece];
        M.slow += m * 0.09f / RATE; if (M.slow >= 1) M.slow -= 1;
        M.slowB += m * 0.057f / RATE; if (M.slowB >= 1) M.slowB -= 1;
        M.wA += m * 0.11f / RATE; if (M.wA >= 1) M.wA -= 1;
        M.wB += m * 0.073f / RATE; if (M.wB >= 1) M.wB -= 1;
        const float padC = p.pad.cut * (1.0f + 0.3f * sn(M.slow)), bassC = p.bass.cut * (1.0f + 0.3f * sn(M.slowB));   // both filters breathe
        const float drift[4] = {1.0f, 1.0f, 1.0f + p.pad.warp * sn(M.wA), 1.0f - p.pad.warp * sn(M.wB)};
        for (int i = 0; i < m; i++) {
            float s = 0, send = 0;
            if (M.bGate || M.bAmp > 0.0005f) {   // bass: two saws a hair apart, so they beat, and a sine an octave down, through a resonant low-pass
                M.bCur += (M.bInc - M.bCur) * M.bGl;
                M.bPh += M.bCur; if (M.bPh >= 1) M.bPh -= 1;
                M.bPh2 += M.bCur * 1.003f; if (M.bPh2 >= 1) M.bPh2 -= 1;
                M.bSub += M.bCur * 0.5f; if (M.bSub >= 1) M.bSub -= 1;
                const float x = (2 * M.bPh - 1 - blep(M.bPh, M.bCur) + 2 * M.bPh2 - 1 - blep(M.bPh2, M.bCur)) * 0.36f + sn(M.bSub) * 0.3f;
                const float f = bassC + p.bass.env * M.bFenv; M.bFenv *= M.bFdec;
                M.bLo += f * M.bBd; const float hi = x - M.bLo - 0.7f * M.bBd; M.bBd += f * hi;
                M.bAmp += ((M.bGate ? p.bass.sus + (1 - p.bass.sus) * M.bFenv : 0.0f) - M.bAmp) * (M.bGate ? M.bAtk : M.bRel);
                s += M.bLo * M.bAmp * 0.30f;
            }
            if (M.bell[0].e[0] > 0.0008f || M.bell[1].e[0] > 0.0008f) {   // the struck voice: a few sine partials, each ringing down at its own rate
                float x = 0;
                for (Struck& v : M.bell) {
                    if (v.e[0] <= 0.0008f) continue;
                    const float inc = v.inc * drift[2];   // it wavers in pitch with the strings: nothing here is quite in tune
                    for (int k = 0; k < 4 && M.eAmp[k] > 0; k++) {
                        v.ph[k] += inc * M.eRatio[k]; if (v.ph[k] >= 1) v.ph[k] -= 1;
                        x += sn(v.ph[k]) * v.e[k]; v.e[k] *= M.eDec[k];
                    }
                }
                const float v = x * p.bell.level; s += v; send += v;
            }
            if (M.lGate || M.lAmp > 0.0005f) {   // lead: one soft saw that fades in, slides between notes and wavers
                M.lVib += p.lead.vibHz / RATE; if (M.lVib >= 1) M.lVib -= 1;
                if (M.lVibAmt < 1) M.lVibAmt += 1.0f / (0.9f * RATE);
                M.lCur += (M.lInc - M.lCur) * M.lGl;
                const float inc = M.lCur * (1 + sn(M.lVib) * p.lead.vib * M.lVibAmt);
                M.lPh += inc; if (M.lPh >= 1) M.lPh -= 1;
                M.lLp += p.lead.bright * (2 * M.lPh - 1 - blep(M.lPh, inc) - M.lLp);
                M.lAmp += ((M.lGate ? 1.0f : 0.0f) - M.lAmp) * (M.lGate ? M.lAtk : M.lAtk * 0.5f);
                const float v = M.lLp * M.lAmp * 0.24f; s += v; send += v * 0.8f;
            }
            if (M.pOn || M.pGain > 0.0005f) {    // strings: four saws (a chord) through a soft low-pass, swelling in and out
                float x = 0;
                for (int k = 0; k < 4; k++) {
                    M.pCur[k] += (M.pInc[k] - M.pCur[k]) * M.pGl;
                    M.pPh[k] += M.pCur[k] * drift[k]; if (M.pPh[k] >= 1) M.pPh[k] -= 1;
                    x += M.pPh[k];
                }
                x = x * 2 - 4;
                M.pLp1 += padC * (x - M.pLp1); M.pLp2 += padC * (M.pLp1 - M.pLp2);
                M.pGain += ((M.pOn ? 1.0f : 0.0f) - M.pGain) * 0.00006f;
                float g = M.pGain;
                if (M.tremOn || M.tremAmt > 0.001f) {   // the shiver
                    M.tremAmt += ((M.tremOn ? 1.0f : 0.0f) - M.tremAmt) * 0.00005f;
                    M.trem += 6.7f / RATE; if (M.trem >= 1) M.trem -= 1;
                    g *= 1 - 0.5f * M.tremAmt * (0.5f + 0.5f * sn(M.trem));
                }
                s += M.pLp2 * g * 0.12f;
            }
            if (M.kTgt > 0 || M.kEnv > 0.001f) {   // thump: a low sine that drops in pitch as it lands: a heartbeat, a footstep, something far off
                M.kPh += (45.0f + 110.0f * M.kPenv) / RATE; if (M.kPh >= 1) M.kPh -= 1;
                if (M.kTgt > 0) { M.kEnv += (M.kTgt - M.kEnv) * 0.04f; if (M.kEnv > M.kTgt * 0.97f) M.kTgt = 0; } else M.kEnv *= KDEC;
                s += (sn(M.kPh) + 0.35f * sn(M.kPh * 2)) * M.kEnv * 0.5f; M.kPenv *= KPDEC;
            }
            const bool slam = M.sE[0] > 0.001f || M.sE[1] > 0.001f, tick = M.hEnv > 0.001f;
            if (slam || tick) {
                M.noise = M.noise * 1664525u + 1013904223u;
                const float nz = (int32_t)M.noise * (1.0f / 2147483648.0f);
                if (slam) {
                    float v;
                    if (p.metal) {   // struck steel: three partials that ring down at different rates, and the hiss of the blow
                        float body = 0;
                        for (int k = 0; k < 3; k++) { M.sPh[k] += STEEL[k] / RATE; if (M.sPh[k] >= 1) M.sPh[k] -= 1; body += sn(M.sPh[k]) * M.sE[k]; M.sE[k] *= STEELDEC[k]; }
                        v = (body * 0.5f + nz * M.sHit * 0.5f) * 0.30f;
                    } else {         // a heavy door, a long way off: the thud, and the rumble after it
                        M.sLp += 0.12f * (nz - M.sLp);
                        M.sPh[0] += 58.0f / RATE; if (M.sPh[0] >= 1) M.sPh[0] -= 1;
                        v = (M.sLp * 3.0f * M.sE[0] + sn(M.sPh[0]) * M.sE[1] * 0.8f + nz * M.sHit * 0.2f) * 0.25f;
                        M.sE[0] *= DOORDEC[0]; M.sE[1] *= DOORDEC[1];
                    }
                    M.sHit *= HITDEC; s += v; send += v * 0.9f;
                }
                if (tick) {          // a clock: a short click with a little wood in it, pitched higher for the tick than the tock
                    M.hPh += M.hInc; if (M.hPh >= 1) M.hPh -= 1;
                    M.hLp += 0.5f * (nz - M.hLp);
                    const float v = (sn(M.hPh) * 0.6f + (nz - M.hLp) * 0.6f) * M.hEnv * 0.32f; s += v; send += v * 0.25f; M.hEnv *= HDEC;
                }
            }
            // echo: one long repeat that gets darker each time round, and an earlier, quieter one so it sounds like a room
            float e = M.echo[M.ei]; if (e > -1e-6f && e < 1e-6f) e = 0;
            int tap = M.ei + M.etap; if (tap >= M.elen) tap -= M.elen;
            M.eLp += 0.4f * (e - M.eLp);
            M.echo[M.ei] = send + M.eLp * p.echoFb; if (++M.ei >= M.elen) M.ei = 0;
            s += (e + M.echo[tap] * 0.5f) * 0.6f;
            if (fade < target) { fade += slew; if (fade > 1) fade = 1; } else if (fade > target) { fade -= slew; if (fade < 0) fade = 0; }
            out[i] += s * fade * 0.66f;   // leaves headroom: the loudest piece peaks under full scale at full volume
        }
        out += m; n -= m; M.toStep -= m;
    }
}
