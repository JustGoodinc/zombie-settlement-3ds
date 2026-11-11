// zs_tool --music DIR: every piece of the soundtrack as a WAV (one full loop), a spectrogram of it as a PNG, and a
// table of what was measured: length, level, how the energy is split between low, middle and high, how each part of
// the piece compares, and what the synth costs to run. This is how the music is checked without ears.

static void fft(float* re, float* im, int n) {   // in place, n a power of two
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -6.2831853f / len, wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = a + len / 2;
                float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

// The score as the sequencer plays it, one line a bar: every note each voice starts, by name. This is how the
// writing is checked against what was meant (the patterns in music.cpp are letters, not note names).
static FILE* scoreOut; static int scoreSect, scoreBar, scoreVoice;
static void scoreNote(int sect, int bar, int step, int voice, int note) {
    static const char* NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* VOICE[7] = {"bass", "bell", "lead", "strings", "thump", "slam", "tick"};
    if (sect != scoreSect || bar != scoreBar) { fprintf(scoreOut, "\n  %d.%-2d", sect + 1, bar + 1); scoreSect = sect; scoreBar = bar; scoreVoice = -1; }
    if (voice != scoreVoice) { fprintf(scoreOut, "  %s", VOICE[voice]); scoreVoice = voice; }
    if (voice == 3) fprintf(scoreOut, " %s%d", NAMES[note % 12], note / 12 - 1);
    else if (voice < 3) fprintf(scoreOut, " %d:%s%d", step, NAMES[note % 12], note / 12 - 1);
    else fprintf(scoreOut, " %d%s", step, note ? "" : "(soft)");
}
static void musicScore(const char* dir) {
    char path[512]; snprintf(path, sizeof path, "%s/score.txt", dir);
    scoreOut = fopen(path, "w"); if (!scoreOut) return;
    static float sink[1024];
    for (int t = 0; t < MUS_COUNT; t++) {
        fprintf(scoreOut, "%s%s (%.1f s)   section.bar, then each voice's notes as step:note", t ? "\n\n" : "", music_name(t), music_loopSeconds(t));
        scoreSect = scoreBar = -1;
        music_trace = scoreNote; music_reset(t);
        const int total = (int)(music_loopSeconds(t) * AUDIO_RATE) - 64;   // stop just short of the loop point
        for (int at = 0; at < total; at += 1024) music_render(sink, total - at < 1024 ? total - at : 1024);
        music_trace = nullptr;
    }
    fprintf(scoreOut, "\n"); fclose(scoreOut);
}
// Each voice on its own: how loud it is over the whole loop and at its loudest moment (dB, sliders at full), how
// many notes or hits a second the piece starts, and the highest peak over three times round (the second and third
// loops are not sample-for-sample the first: the slow wobbles and the echo carry on).
static int rateNotes;
static void rateNote(int, int, int, int voice, int) { if (voice != 3) rateNotes++; }
static void musicVoices() {
    static float blk[1024];
    static const int BIT[7] = {1, 2, 8, 4, 16, 32, 64};   // bass, bell, lead, strings, thump, slam, tick
    printf("\n%-14s  %-11s %-11s %-11s %-11s %-11s %-11s %-11s  %7s %7s\n", "each voice", "bass", "bell", "lead", "strings", "thump", "slam", "tick", "notes/s", "peak x3");
    for (int t = 0; t < MUS_COUNT; t++) {
        const int total = (int)(music_loopSeconds(t) * AUDIO_RATE);
        printf("%-14s ", music_name(t));
        for (int v = 0; v < 7; v++) {
            music_voices = BIT[v] | 128; music_reset(t);
            double sum = 0; float peak = 0;
            for (int at = 0; at < total; at += 1024) {
                const int m = total - at < 1024 ? total - at : 1024;
                memset(blk, 0, sizeof blk); music_render(blk, m);
                for (int i = 0; i < m; i++) { sum += blk[i] * blk[i]; if (fabsf(blk[i]) > peak) peak = fabsf(blk[i]); }
            }
            if (peak < 1e-4f) printf(" %-11s", "-"); else printf(" %5.1f/%-5.1f", 10 * log10(sum / total + 1e-12), 20 * log10f(peak));
        }
        music_voices = 255; music_reset(t); rateNotes = 0; music_trace = rateNote;
        float peak = 0;
        for (int at = 0; at < total * 3; at += 1024) {
            if (at >= total) music_trace = nullptr;
            memset(blk, 0, sizeof blk); music_render(blk, 1024);
            for (int i = 0; i < 1024; i++) if (fabsf(blk[i]) > peak) peak = fabsf(blk[i]);
        }
        music_trace = nullptr;
        printf("  %7.2f %7.2f\n", rateNotes / music_loopSeconds(t), peak);
    }
}

static void musicTool(const char* dir) {
    enum { N = 2048, W = 1000, H = 256 };
    static float win[N], re[N], im[N];
    musicScore(dir);
    for (int i = 0; i < N; i++) win[i] = 0.5f - 0.5f * cosf(6.2831853f * i / N);
    printf("%-14s %6s %6s %7s %6s  %5s %5s %5s  %s\n", "piece", "loop s", "peak", "rms dB", "crest", "low%", "mid%", "high%", "loudness by eighths of the loop (dB)");
    for (int t = 0; t < MUS_COUNT; t++) {
        const int total = (int)(music_loopSeconds(t) * AUDIO_RATE);
        float* pcm = (float*)calloc(total + N, sizeof(float));
        music_reset(t);
        for (int at = 0; at < total; at += 1024) music_render(pcm + at, total - at < 1024 ? total - at : 1024);
        for (int i = 0; i < total; i++) pcm[i] *= 0.6f * 0.8f;   // the level it has in the game at the default music and master volume (6 and 8 of 10)
        char path[512], name[40]; snprintf(name, sizeof name, "%s", music_name(t)); for (char* c = name; *c; c++) if (*c == ' ') *c = '_';
        snprintf(path, sizeof path, "%s/music_%d_%s.wav", dir, t, name); audio_exportMusic(t, path);
        // level
        double sum = 0; float peak = 0; double part[8] = {0};
        for (int i = 0; i < total; i++) { float a = fabsf(pcm[i]); if (a > peak) peak = a; sum += pcm[i] * pcm[i]; part[(int)((long long)i * 8 / total)] += pcm[i] * pcm[i]; }
        float rms = (float)sqrt(sum / total);
        // spectrogram: time left to right, 40 Hz at the bottom to 11 kHz at the top (log), brighter = louder
        px* img = (px*)malloc(W * H * sizeof(px));
        double band[3] = {0, 0, 0};
        for (int x = 0; x < W; x++) {
            int at = (int)((long long)x * (total - N) / (W - 1)); if (at < 0) at = 0;
            for (int i = 0; i < N; i++) { re[i] = pcm[at + i] * win[i]; im[i] = 0; }
            fft(re, im, N);
            for (int k = 1; k < N / 2; k++) { float hz = k * (float)AUDIO_RATE / N, e = re[k] * re[k] + im[k] * im[k]; band[hz < 250 ? 0 : (hz < 2000 ? 1 : 2)] += e; }
            for (int y = 0; y < H; y++) {
                float f0 = 40.0f * powf(11025.0f / 40.0f, (float)(H - 1 - y) / H), f1 = 40.0f * powf(11025.0f / 40.0f, (float)(H - y) / H);
                int k0 = (int)(f0 * N / AUDIO_RATE), k1 = (int)(f1 * N / AUDIO_RATE); if (k0 < 1) k0 = 1; if (k1 <= k0) k1 = k0 + 1;
                float mx = 0; for (int k = k0; k < k1 && k < N / 2; k++) { float e = re[k] * re[k] + im[k] * im[k]; if (e > mx) mx = e; }
                float db = 10 * log10f(mx / (N * N / 16.0f) + 1e-12f), v = (db + 72) / 60; v = v < 0 ? 0 : (v > 1 ? 1 : v);   // -72 dB .. -12 dB
                static const px RAMP[5] = {rgb(14, 14, 34), rgb(60, 30, 110), rgb(190, 50, 120), rgb(250, 150, 60), rgb(255, 244, 170)};
                int seg = (int)(v * 4); if (seg > 3) seg = 3;
                img[y * W + x] = gfx_mix(RAMP[seg], RAMP[seg + 1], (int)((v * 4 - seg) * 256));
            }
        }
        snprintf(path, sizeof path, "%s/music_%d_%s.png", dir, t, name); gfx_writePNG(path, img, W, H); free(img);
        double all = band[0] + band[1] + band[2] + 1e-12;
        printf("%-14s %6.1f %6.2f %7.1f %6.1f  %5.0f %5.0f %5.0f ", music_name(t), music_loopSeconds(t), peak, 20 * log10f(rms + 1e-9f), 20 * log10f(peak / (rms + 1e-9f)),
               band[0] * 100 / all, band[1] * 100 / all, band[2] * 100 / all);
        for (int k = 0; k < 8; k++) printf(" %5.1f", 10 * log10(part[k] / (total / 8) + 1e-12));
        printf("\n");
        // which notes are sounding: the strongest spectral peaks a few seconds in, named (and how many cents off pitch)
        {
            enum { BIG = 8192 }; static float br[BIG], bi[BIG], mag[BIG / 2];
            int at = (int)(3.0f * AUDIO_RATE); if (at + BIG > total) at = 0;
            for (int i = 0; i < BIG; i++) { br[i] = pcm[at + i] * (0.5f - 0.5f * cosf(6.2831853f * i / BIG)); bi[i] = 0; }
            fft(br, bi, BIG);
            for (int k = 0; k < BIG / 2; k++) mag[k] = sqrtf(br[k] * br[k] + bi[k] * bi[k]);
            printf("    notes sounding at 3 s:");
            static const char* NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            for (int n = 0; n < 7; n++) {
                int best = -1; float bm = 0;
                for (int k = 6; k < BIG / 2 - 1; k++) if (mag[k] > mag[k - 1] && mag[k] >= mag[k + 1] && mag[k] > bm) { bm = mag[k]; best = k; }
                if (best < 0 || bm < 2.0f) break;
                float a = mag[best - 1], b = mag[best], c = mag[best + 1], off = 0.5f * (a - c) / (a - 2 * b + c);
                float hz = (best + off) * (float)AUDIO_RATE / BIG, midi = 69 + 12 * log2f(hz / 440.0f); int m = (int)floorf(midi + 0.5f);
                printf("  %s%d %.0fHz(%+.0fc)", NAMES[m % 12], m / 12 - 1, hz, (midi - m) * 100);
                for (int k = best - 3; k <= best + 3; k++) if (k >= 0 && k < BIG / 2) mag[k] = 0;   // take the peak out and look for the next
            }
            printf("\n");
        }
        free(pcm);
    }
    // cost: seconds of processor time for a second of sound, music alone and the whole mix, against the four
    // sine calls a sample that the old drone and crickets made (the part of the old mixer that is gone)
    static int16_t out[1024]; static float blk[1024];
    const int SECS = 120;
    double mus = 0; int worst = 0;   // the dearest piece, over one whole loop of each
    for (int t = 0; t < MUS_COUNT; t++) {
        const int blocks = (int)(music_loopSeconds(t) * AUDIO_RATE) / 1024;
        music_reset(t);
        auto a = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < blocks; i++) { memset(blk, 0, sizeof blk); music_render(blk, 1024); }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - a).count() / (blocks * 1024.0 / AUDIO_RATE);
        if (ms > mus) { mus = ms; worst = t; }
    }
    musicVoices();
    printf("dearest piece to play: %s\n", music_name(worst));
    music_reset(worst);
    audio_volumes(8, 8, 6); audio_ambience(true, false, false, false);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < AUDIO_RATE * SECS / 1024; i++) audio_mix(out, 1024);
    double mix = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / SECS;
    volatile float sink = 0; float ph = 0;
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < AUDIO_RATE * SECS; i++) { ph += 0.01f; sink = sink + sinf(ph) + sinf(ph * 1.3f) + sinf(ph * 1.7f) + sinf(ph * 2.1f); }
    double old = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / SECS;
    printf("cost per second of sound on this PC: music alone %.3f ms, whole mix (effects idle, wind, crickets, music) %.3f ms; four sinf a sample (what the old drone cost) %.3f ms\n", mus, mix, old);
    music_reset(MUS_NONE);
}
