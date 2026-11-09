#pragma once
#include <stdint.h>

// Deterministic xorshift RNG: same seed -> same sequence on PC and 3DS.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 0x9E3779B9u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int range(int n) { return n > 0 ? (int)(next() % (uint32_t)n) : 0; }
    int between(int a, int b) { return a + range(b - a + 1); }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }
    bool chance(float p) { return f() < p; }
};

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
