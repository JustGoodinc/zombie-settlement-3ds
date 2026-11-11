// The soundtrack: seven short pieces for a small synthesiser (bass, arpeggio, lead, string pad, drum machine, echo),
// written in the manner of late-70s / early-80s horror film scores: a steady pulsing bass, one minimal figure that
// keeps repeating, slow minor chords, sparse drums. Every note is written out below as patterns and played by the
// sequencer as the game runs, so the music is a few kilobytes of text and no recorded sound. All seven are original.
#include "audio.h"
#include <math.h>
#include <string.h>

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
// A piece is a list of sections. A section lasts `bars` bars, names the voices that play (layers) and gives one bar
// (or several) of steps for each. A step is a sixteenth note.
//   chords  one per bar, cycled: semitones above the key's root, then m (minor) or M (major), e.g. "0m 8M 10M"
//   bass, arp   notes relative to the bar's chord:  r root  u 2nd  t third  w 4th  f fifth  v sixth  s flat 7th
//               o octave  U 9th  T third above the octave  W  F fifth above  S  O two octaves  l d D below the root
//   lead    notes in semitones above the key's root: 0-9 then a=10 ... n=23
//   kick, snare, hat   x hit, o soft hit
//   everywhere:  . silence   - hold the note
enum { L_BASS = 1, L_ARP = 2, L_PAD = 4, L_LEAD = 8, L_KICK = 16, L_SNARE = 32, L_HAT = 64, L_ALL = 127 };
struct Sect { uint8_t bars, layers; const char* chords; const char* bass; const char* arp; const char* lead; const char* kick; const char* snare; const char* hat; };
struct Piece {
    const char* name;
    uint8_t bpm, steps;                          // quarter notes a minute; steps in a bar (16 = 4/4, 12 = 6/8, 14 = 7/8)
    uint8_t bassKey, arpKey, padKey, leadKey;    // MIDI note of the key's root in each voice's register
    float bassCut, bassEnv, bassDecay;           // bass filter: resting cutoff, how far each note opens it, how fast it closes (s)
    float arpDuty, arpDecay, arpBright;          // arpeggio: pulse width, how long a note rings (s), brightness
    float padCut;                                // pad brightness
    uint8_t echoSteps; float echoFb;             // echo on the arpeggio and lead: delay in steps, feedback
    bool metal;                                  // the snare is a clank of struck steel
    const Sect* sect; uint8_t nsect;
};

// ---- Main Title (menu): A minor, a heartbeat in the bass, a slow falling line
const char MENU_CH[] = "0m 0m 8M 8M 10M 10M 7m 7m";
const char MENU_HEART[] = "r.r.....r.r.....", MENU_DRIVE[] = "r.r.r.r.r.r.o.r.", MENU_ARP[] = "r.f.o.T.o.f.T.o.";
const char MENU_LEAD[] = "j---------------" "h-------f-------" "f---------------" "c-------f-------"
                         "e---------------" "h-------e-------" "e-------c-------" "a---------------";
const Sect MENU[] = {
    {8, L_BASS | L_PAD, MENU_CH, MENU_HEART},
    {8, L_BASS | L_PAD | L_ARP, MENU_CH, MENU_HEART, MENU_ARP},
    {8, L_BASS | L_PAD | L_ARP | L_LEAD | L_KICK, MENU_CH, MENU_HEART, MENU_ARP, MENU_LEAD, "x.......x......."},
    {8, L_ALL, MENU_CH, MENU_DRIVE, MENU_ARP, MENU_LEAD, "x.......x.......", "............x...", "..x...x...x...x."},
    {4, L_PAD | L_ARP, "0m 0m 8M 10M", nullptr, MENU_ARP},
};
// ---- Home Fire (settlement): D minor, no drums, warm and slow
const char HOME_CH[] = "0m 0m 8M 8M 3M 3M 10M 10M";
const char HOME_BASS[] = "r-----------f---", HOME_ARP[] = "r.f.o.T.....o.f.";
const char HOME_LEAD[] = "7-------a-------" "c---------------" "c-------f-------" "c---------------"
                         "a-------7-------" "7---------------" "e-------h-------" "e---------------";
const Sect HOME[] = {
    {8, L_PAD | L_BASS, HOME_CH, HOME_BASS},
    {8, L_PAD | L_BASS | L_ARP, HOME_CH, HOME_BASS, HOME_ARP},
    {8, L_PAD | L_BASS | L_ARP | L_LEAD, HOME_CH, HOME_BASS, HOME_ARP, HOME_LEAD},
    {8, L_PAD | L_ARP, HOME_CH, nullptr, HOME_ARP},
};
// ---- Precinct Nine (police station): E minor, a driving bass and a drum machine
const char COP_CH[] = "0m 0m 0m 0m 8M 8M 10M 10M";
const char COP_STEADY[] = "r.r.r.r.r.r.o.r.", COP_GALLOP[] = "r.rr.r.or.rr.s.o", COP_SEQ[] = "f.v.f.v.f.v.f.o.";
const char COP_LEAD[] = "c-------a---7---" "8-----------7---" "c-------a---7---" "3---------------"
                        "3-------7-------" "8---------------" "a-------5-------" "2---------------";
const char COP_KICK[] = "x.......x.x.....", COP_SNARE[] = "....x.......x...", COP_HAT[] = "x.x.x.x.x.x.x.x.";
const Sect COP[] = {
    {4, L_BASS | L_HAT, "0m", COP_STEADY, nullptr, nullptr, nullptr, nullptr, COP_HAT},
    {8, L_BASS | L_KICK | L_HAT | L_ARP, COP_CH, COP_STEADY, COP_SEQ, nullptr, COP_KICK, nullptr, COP_HAT},
    {8, L_BASS | L_KICK | L_SNARE | L_HAT | L_ARP | L_PAD, COP_CH, COP_GALLOP, COP_SEQ, nullptr, COP_KICK, COP_SNARE, COP_HAT},
    {8, L_ALL, COP_CH, COP_GALLOP, COP_SEQ, COP_LEAD, COP_KICK, COP_SNARE, COP_HAT},
    {4, L_PAD | L_ARP | L_HAT, "0m 0m 8M 10M", nullptr, COP_SEQ, nullptr, nullptr, nullptr, COP_HAT},
};
// ---- Ward Four (hospital): C sharp minor in 6/8, glass bells over a drone, a slow heartbeat
const char WARD_CH[] = "0m 0m 8M 8M 0m 0m 5m 7M";
const char WARD_DRONE[] = "r-----------", WARD_PULSE[] = "r-----r-----", WARD_BELLS[] = "o.f.T.f.U.f.", WARD_BEAT[] = "x.....o.....";
const char WARD_LEAD[] = "j-----h-----" "f-----------" "f-----c-----" "8-----------" "7-----c-----" "e-----------" "c-----8-----" "b-----------";
const Sect WARD[] = {
    {4, L_PAD | L_BASS, "0m", WARD_DRONE},
    {8, L_PAD | L_BASS | L_ARP, WARD_CH, WARD_DRONE, WARD_BELLS},
    {8, L_PAD | L_BASS | L_ARP | L_KICK, WARD_CH, WARD_PULSE, WARD_BELLS, nullptr, WARD_BEAT},
    {8, L_PAD | L_BASS | L_ARP | L_KICK | L_LEAD, WARD_CH, WARD_PULSE, WARD_BELLS, WARD_LEAD, WARD_BEAT},
    {4, L_PAD | L_ARP, "0m 0m 8M 7M", nullptr, WARD_BELLS},
};
// ---- Aisle Nine (supermarket): G minor, a bubbling sequencer over an octave bass and a four-on-the-floor kick
const char SHOP_CH[] = "0m 0m 8M 8M 10M 10M 5m 7M";
const char SHOP_BASS[] = "r.o.r.o.r.o.r.o.", SHOP_SEQ[] = "rfTfofTfrfTfofTf";
const char SHOP_LEAD[] = "7---a---c-------" "a-------7-------" "8---a---c-------" "f-------c-------"
                         "a---c---e-------" "c-------a-------" "8-------7---5---" "7-------b-------";
const char SHOP_KICK[] = "x...x...x...x...", SHOP_CLAP[] = "....x.......x...", SHOP_HAT[] = "..x...x...x...x.";
const Sect SHOP[] = {
    {4, L_ARP | L_HAT, "0m", nullptr, SHOP_SEQ, nullptr, nullptr, nullptr, SHOP_HAT},
    {8, L_ARP | L_HAT | L_BASS | L_KICK, SHOP_CH, SHOP_BASS, SHOP_SEQ, nullptr, SHOP_KICK, nullptr, SHOP_HAT},
    {8, L_ARP | L_HAT | L_BASS | L_KICK | L_SNARE | L_PAD, SHOP_CH, SHOP_BASS, SHOP_SEQ, nullptr, SHOP_KICK, SHOP_CLAP, SHOP_HAT},
    {8, L_ALL, SHOP_CH, SHOP_BASS, SHOP_SEQ, SHOP_LEAD, SHOP_KICK, SHOP_CLAP, SHOP_HAT},
    {4, L_BASS | L_KICK | L_PAD, "0m 0m 8M 10M", SHOP_BASS, nullptr, nullptr, SHOP_KICK},
};
// ---- Rolling Stock (train yard): B minor in 7/8, a heavy bass, hats like wheels on rails, struck steel
const char YARD_CH[] = "0m 0m 0m 0m 10M 10M 8M 8M";
const char YARD_BASS[] = "r...r...r..r..", YARD_BASS2[] = "r...r...o..r..", YARD_ARP[] = "f.....o.......";
const char YARD_LEAD[] = "7-------------" "8-------7-----" "5-------------" "3-----2-------" "2-------------" "5-------7-----" "a-------8-----" "7-------------";
const char YARD_KICK[] = "x.......x.....", YARD_CLANK[] = "........x...x.", YARD_HAT[] = "x.xxx.xxx.xxx.";
const Sect YARD[] = {
    {4, L_HAT | L_BASS, "0m", YARD_BASS, nullptr, nullptr, nullptr, nullptr, YARD_HAT},
    {8, L_HAT | L_BASS | L_KICK | L_SNARE | L_ARP, YARD_CH, YARD_BASS, YARD_ARP, nullptr, YARD_KICK, YARD_CLANK, YARD_HAT},
    {8, L_HAT | L_BASS | L_KICK | L_SNARE | L_ARP | L_PAD, YARD_CH, YARD_BASS2, YARD_ARP, nullptr, YARD_KICK, YARD_CLANK, YARD_HAT},
    {8, L_ALL, YARD_CH, YARD_BASS2, YARD_ARP, YARD_LEAD, YARD_KICK, YARD_CLANK, YARD_HAT},
    {4, L_PAD | L_HAT, "0m 0m 10M 8M", nullptr, nullptr, nullptr, nullptr, nullptr, YARD_HAT},
};
// ---- Maple Street (neighbourhood): F minor, a music box in threes and twos, a clock ticking, a low slow line
const char MAPLE_CH[] = "0m 0m 8M 8M 5m 5m 7M 7M";
const char MAPLE_BOX[] = "r.t.f.r.t.f.o.f.", MAPLE_LONG[] = "r---------------", MAPLE_WALK[] = "r...r...r...r...";
const char MAPLE_LEAD[] = "c---------------" "a-------8-------" "8-------7-------" "3---------------"
                          "5-------8-------" "c---------------" "b-------7-------" "2---------------";
const char MAPLE_TICK[] = "x...o...o...o...", MAPLE_KICK[] = "x...............";
const Sect MAPLE[] = {
    {8, L_ARP | L_HAT, MAPLE_CH, nullptr, MAPLE_BOX, nullptr, nullptr, nullptr, MAPLE_TICK},
    {8, L_ARP | L_HAT | L_BASS, MAPLE_CH, MAPLE_LONG, MAPLE_BOX, nullptr, nullptr, nullptr, MAPLE_TICK},
    {8, L_ARP | L_HAT | L_BASS | L_PAD | L_KICK, MAPLE_CH, MAPLE_WALK, MAPLE_BOX, nullptr, MAPLE_KICK, nullptr, MAPLE_TICK},
    {8, L_ARP | L_HAT | L_BASS | L_PAD | L_KICK | L_LEAD, MAPLE_CH, MAPLE_WALK, MAPLE_BOX, MAPLE_LEAD, MAPLE_KICK, nullptr, MAPLE_TICK},
    {4, L_ARP, "0m 0m 8M 7M", nullptr, MAPLE_BOX},
};

#define SECTS(a) a, (uint8_t)(sizeof(a) / sizeof(a[0]))
const Piece PIECES[MUS_COUNT] = {
    //  name             bpm steps  bass arp pad lead   bassCut env decay   duty  ring  bright  pad    echo
    {"Main Title",       100, 16,   45, 57, 57, 57,     0.08f, 0.30f, 0.16f, 0.30f, 0.35f, 0.50f, 0.28f, 3, 0.38f, false, SECTS(MENU)},
    {"Home Fire",         76, 16,   50, 62, 50, 62,     0.08f, 0.12f, 0.40f, 0.50f, 0.50f, 0.38f, 0.24f, 3, 0.35f, false, SECTS(HOME)},
    {"Precinct Nine",    112, 16,   40, 64, 52, 64,     0.09f, 0.38f, 0.10f, 0.20f, 0.14f, 0.55f, 0.30f, 3, 0.30f, false, SECTS(COP)},
    {"Ward Four",         84, 12,   49, 61, 61, 61,     0.07f, 0.08f, 0.60f, 0.50f, 0.60f, 0.42f, 0.22f, 4, 0.45f, false, SECTS(WARD)},
    {"Aisle Nine",       118, 16,   43, 67, 55, 67,     0.10f, 0.32f, 0.09f, 0.25f, 0.09f, 0.55f, 0.30f, 3, 0.25f, false, SECTS(SHOP)},
    {"Rolling Stock",     92, 14,   47, 59, 59, 71,     0.07f, 0.42f, 0.22f, 0.40f, 0.50f, 0.45f, 0.22f, 4, 0.40f, true,  SECTS(YARD)},
    {"Maple Street",     126, 16,   41, 65, 53, 53,     0.07f, 0.16f, 0.30f, 0.50f, 0.30f, 0.50f, 0.24f, 3, 0.30f, false, SECTS(MAPLE)},
};

int degree(char c, bool major) {
    switch (c) {
    case 'r': return 0;  case 'u': return 2;  case 't': return major ? 4 : 3;   case 'w': return 5;  case 'f': return 7;  case 'v': return major ? 9 : 8;
    case 's': return 10; case 'o': return 12; case 'U': return 14; case 'T': return major ? 16 : 15; case 'W': return 17; case 'F': return 19;
    case 'S': return 22; case 'O': return 24; case 'l': return -2; case 'd': return -5; case 'D': return -12;
    }
    return 99;
}
int semis(char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'n' ? c - 'a' + 10 : 99); }
// chords: "<semitones><m|M>" separated by spaces; roots above a tritone are played below the key's root instead
int parseChords(const char* s, int8_t* root, bool* major) {
    int n = 0;
    while (*s && n < 8) {
        while (*s == ' ') s++;
        if (*s < '0' || *s > '9') break;
        int r = 0; while (*s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0');
        root[n] = (int8_t)(r > 6 ? r - 12 : r); major[n] = *s == 'M'; if (*s) s++;
        n++;
    }
    return n;
}

// ---------------------------------------------------------------- the player
struct State {
    int piece, sect, bar, step; float toStep, spstep;
    int8_t root[8]; bool major[8]; int nchord, len[6];
    float bFdec, aDec, aBright, duty, bCut, bEnvAmt, padC, fb;   // per-piece constants, worked out once
    float bPh, bSub, bInc, bLo, bBd, bFenv, bAmp; bool bGate;
    float aPh, aInc, aEnv, aLp;
    float lPh, lInc, lAmp, lLp, lVib, lVibAmt; bool lGate;
    float pPh[4], pInc[4], pGain, pLp1, pLp2; bool pOn;
    float kPh, kEnv, kPenv, sEnv, sTenv, sLp, sPh, sPh2, hEnv, hLp; uint32_t noise;
    float slow;                                  // a very slow wobble for the pad filter
    int ei, elen; float echo[16384];
} M;
int want = MUS_NONE, cur = MUS_NONE;
float fade = 0;
const float KDEC = expf(-1 / (0.085f * RATE)), KPDEC = expf(-1 / (0.018f * RATE)), SDEC = expf(-1 / (0.075f * RATE)), STDEC = expf(-1 / (0.035f * RATE)),
            MDEC = expf(-1 / (0.16f * RATE)), HDEC = expf(-1 / (0.018f * RATE));

void loadSect() {
    const Sect& s = PIECES[M.piece].sect[M.sect];
    M.nchord = parseChords(s.chords, M.root, M.major);
    const char* pat[6] = {s.bass, s.arp, s.lead, s.kick, s.snare, s.hat};
    for (int k = 0; k < 6; k++) M.len[k] = pat[k] ? (int)strlen(pat[k]) : 1;
}
void start(int piece) {
    memset(&M, 0, sizeof M);
    const Piece& p = PIECES[piece];
    M.piece = piece; M.step = -1; M.noise = 0x1234567u;
    M.spstep = RATE * 60.0f / p.bpm / 4.0f;
    M.elen = (int)(p.echoSteps * M.spstep); if (M.elen > 16383) M.elen = 16383; if (M.elen < 1) M.elen = 1;
    M.bFdec = expf(-1 / (p.bassDecay * RATE)); M.aDec = expf(-1 / (p.arpDecay * RATE));
    M.aBright = p.arpBright; M.duty = p.arpDuty; M.bCut = p.bassCut; M.bEnvAmt = p.bassEnv; M.padC = p.padCut; M.fb = p.echoFb;
    loadSect();
}
// the next sixteenth: move on a step (and bar, and section) and start whatever notes fall on it
void advance() {
    const Piece& p = PIECES[M.piece];
    if (++M.step >= p.steps) {
        M.step = 0;
        if (++M.bar >= p.sect[M.sect].bars) { M.bar = 0; M.sect = (M.sect + 1) % p.nsect; loadSect(); }
    }
    const Sect& s = p.sect[M.sect];
    const int pos = M.bar * p.steps + M.step, root = M.root[M.bar % M.nchord]; const bool major = M.major[M.bar % M.nchord];
    char c = s.bass && (s.layers & L_BASS) ? s.bass[pos % M.len[0]] : '.';
    int d = degree(c, major);
    if (d != 99) { M.bInc = HZ[p.bassKey + root + d] / RATE; M.bFenv = 1; M.bGate = true; } else if (c != '-') M.bGate = false;
    c = s.arp && (s.layers & L_ARP) ? s.arp[pos % M.len[1]] : '.';
    d = degree(c, major);
    if (d != 99) { M.aInc = HZ[p.arpKey + root + d] / RATE; M.aEnv = 1; }
    c = s.lead && (s.layers & L_LEAD) ? s.lead[pos % M.len[2]] : '.';
    d = semis(c);
    if (d != 99) { M.lInc = HZ[p.leadKey + d] / RATE; if (!M.lGate) M.lVibAmt = 0; M.lGate = true; } else if (c != '-') M.lGate = false;
    if (M.step == 0) {   // the pad follows the chord of the bar: root (twice, a hair apart), third, fifth
        M.pOn = (s.layers & L_PAD) != 0;
        if (M.pOn) {
            const int r = p.padKey + root;
            M.pInc[0] = HZ[r] / RATE; M.pInc[1] = HZ[r] * 1.006f / RATE; M.pInc[2] = HZ[r + (major ? 4 : 3)] / RATE; M.pInc[3] = HZ[r + 7] * 0.997f / RATE;
        }
    }
    c = s.kick && (s.layers & L_KICK) ? s.kick[pos % M.len[3]] : '.';
    if (c == 'x' || c == 'o') { M.kEnv = c == 'x' ? 1.0f : 0.55f; M.kPenv = 1; M.kPh = 0; }
    c = s.snare && (s.layers & L_SNARE) ? s.snare[pos % M.len[4]] : '.';
    if (c == 'x' || c == 'o') { M.sEnv = c == 'x' ? 1.0f : 0.5f; M.sTenv = M.sEnv; }
    c = s.hat && (s.layers & L_HAT) ? s.hat[pos % M.len[5]] : '.';
    if (c == 'x' || c == 'o') M.hEnv = c == 'x' ? 1.0f : 0.45f;
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
// every pattern must fill whole bars and use only known letters, and every note must be on the keyboard
const char* music_check() {
    for (const Piece& p : PIECES) for (int i = 0; i < p.nsect; i++) {
        const Sect& s = p.sect[i];
        int8_t root[8]; bool major[8]; int n = parseChords(s.chords, root, major);
        if (n < 1 || !s.bars) return p.name;
        const char* pat[6] = {s.bass, s.arp, s.lead, s.kick, s.snare, s.hat};
        for (int k = 0; k < 6; k++) {
            if (!pat[k]) continue;
            if (strlen(pat[k]) % p.steps) return p.name;
            for (const char* c = pat[k]; *c; c++) {
                if (*c == '.' || *c == '-') continue;
                if (k < 2) { int d = degree(*c, false), key = k ? p.arpKey : p.bassKey; if (d == 99 || key - 5 + d < 0 || key + 6 + d + 1 > 127) return p.name; }
                else if (k == 2) { if (semis(*c) == 99) return p.name; }
                else if (*c != 'x' && *c != 'o') return p.name;
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
        const bool metal = PIECES[M.piece].metal;
        M.slow += m * 0.09f / RATE; if (M.slow >= 1) M.slow -= 1;
        const float padC = M.padC * (1.0f + 0.3f * sn(M.slow));   // the pad's filter drifts a little
        for (int i = 0; i < m; i++) {
            float s = 0, send = 0;
            if (M.bGate || M.bAmp > 0.0005f) {   // bass: saw and a sine an octave down through a resonant low-pass that each note opens
                M.bPh += M.bInc; if (M.bPh >= 1) M.bPh -= 1;
                M.bSub += M.bInc * 0.5f; if (M.bSub >= 1) M.bSub -= 1;
                const float x = (2 * M.bPh - 1 - blep(M.bPh, M.bInc)) * 0.7f + sn(M.bSub) * 0.3f;
                const float f = M.bCut + M.bEnvAmt * M.bFenv; M.bFenv *= M.bFdec;
                M.bLo += f * M.bBd; const float hi = x - M.bLo - 0.7f * M.bBd; M.bBd += f * hi;
                M.bAmp += ((M.bGate ? 0.6f + 0.4f * M.bFenv : 0.0f) - M.bAmp) * (M.bGate ? 0.02f : 0.006f);
                s += M.bLo * M.bAmp * 0.46f;
            }
            if (M.aEnv > 0.0008f) {              // arpeggio: a plucked pulse wave
                M.aPh += M.aInc; if (M.aPh >= 1) M.aPh -= 1;
                float t2 = M.aPh + M.duty; if (t2 >= 1) t2 -= 1;
                const float x = (2 * M.aPh - blep(M.aPh, M.aInc)) - (2 * t2 - blep(t2, M.aInc));
                M.aLp += M.aBright * (x * M.aEnv - M.aLp); M.aEnv *= M.aDec;
                const float v = M.aLp * 0.26f; s += v; send += v;
            }
            if (M.lGate || M.lAmp > 0.0005f) {   // lead: a soft saw with slow attack and a vibrato that comes in late
                M.lVib += 5.6f / RATE; if (M.lVib >= 4) M.lVib -= 4;
                if (M.lVibAmt < 1) M.lVibAmt += 1.0f / (0.5f * RATE);
                const float inc = M.lInc * (1 + sn(M.lVib) * 0.0045f * M.lVibAmt);
                M.lPh += inc; if (M.lPh >= 1) M.lPh -= 1;
                M.lLp += 0.32f * (2 * M.lPh - 1 - blep(M.lPh, inc) - M.lLp);
                M.lAmp += ((M.lGate ? 1.0f : 0.0f) - M.lAmp) * (M.lGate ? 0.0025f : 0.0008f);
                const float v = M.lLp * M.lAmp * 0.24f; s += v; send += v * 0.8f;
            }
            if (M.pOn || M.pGain > 0.0005f) {    // pad: four saws (a chord) through a soft low-pass, swelling in and out
                float x = 0;
                for (int k = 0; k < 4; k++) { M.pPh[k] += M.pInc[k]; if (M.pPh[k] >= 1) M.pPh[k] -= 1; x += M.pPh[k]; }
                x = x * 2 - 4;
                M.pLp1 += padC * (x - M.pLp1); M.pLp2 += padC * (M.pLp1 - M.pLp2);
                M.pGain += ((M.pOn ? 1.0f : 0.0f) - M.pGain) * 0.00012f;
                s += M.pLp2 * M.pGain * 0.095f;
            }
            if (M.kEnv > 0.001f) {               // kick: a sine dropping from about 200 Hz to 46
                M.kPh += (46.0f + 170.0f * M.kPenv) / RATE; if (M.kPh >= 1) M.kPh -= 1;
                s += sn(M.kPh) * M.kEnv * 0.56f; M.kEnv *= KDEC; M.kPenv *= KPDEC;
            }
            if (M.sEnv > 0.001f || M.hEnv > 0.001f) {
                M.noise = M.noise * 1664525u + 1013904223u;
                const float nz = (int32_t)M.noise * (1.0f / 2147483648.0f);
                if (M.sEnv > 0.001f) {
                    if (metal) {                 // struck steel: two square waves that do not agree, and a little hiss
                        M.sPh += 587.0f / RATE; if (M.sPh >= 1) M.sPh -= 1;
                        M.sPh2 += 845.0f / RATE; if (M.sPh2 >= 1) M.sPh2 -= 1;
                        const float body = ((M.sPh < 0.5f ? 0.5f : -0.5f) + (M.sPh2 < 0.5f ? 0.5f : -0.5f)) + nz * 0.25f;
                        M.sLp += 0.4f * (body - M.sLp);
                        const float v = (body - M.sLp) * M.sEnv * 0.22f; s += v; send += v * 0.5f; M.sEnv *= MDEC;
                    } else {                     // snare: bright noise over a short low thump
                        M.sLp += 0.30f * (nz - M.sLp);
                        M.sPh += 185.0f / RATE; if (M.sPh >= 1) M.sPh -= 1;
                        s += ((nz - M.sLp) * 0.6f * M.sEnv + sn(M.sPh) * 0.45f * M.sTenv) * 0.46f; M.sEnv *= SDEC; M.sTenv *= STDEC;
                    }
                }
                if (M.hEnv > 0.001f) { M.hLp += 0.55f * (nz - M.hLp); s += (nz - M.hLp) * M.hEnv * 0.17f; M.hEnv *= HDEC; }
            }
            float e = M.echo[M.ei]; if (e > -1e-6f && e < 1e-6f) e = 0;   // echo
            M.echo[M.ei] = send + e * M.fb; if (++M.ei >= M.elen) M.ei = 0;
            s += e * 0.55f;
            if (fade < target) { fade += slew; if (fade > 1) fade = 1; } else if (fade > target) { fade -= slew; if (fade < 0) fade = 0; }
            out[i] += s * fade * 0.55f;   // leaves headroom: the loudest piece peaks just under full scale at full volume
        }
        out += m; n -= m; M.toStep -= m;
    }
}
