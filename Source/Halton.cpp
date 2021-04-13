#include <PCH.h>
#include "Halton.h"

uint32_t ReverseBits32(uint32_t x)
{
    x = (x << 16) | (x >> 16);
    x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
    x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
    x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
    x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
    return x;
}

uint32_t CompactBits(uint32_t x)
{
    x &= 0x55555555;
    x = (x ^ (x >> 1)) & 0x33333333;
    x = (x ^ (x >> 2)) & 0x0F0F0F0F;
    x = (x ^ (x >> 4)) & 0x00FF00FF;
    x = (x ^ (x >> 8)) & 0x0000FFFF;
    return x;
}

uint32_t ReverseBits4(uint32_t x)
{
    x = ((x & 0x5) << 1) | ((x & 0xA) >> 1);
    x = ((x & 0x3) << 2) | ((x & 0xC) >> 2);
    return x;
}

uint32_t Bayer4x4(int2 samplePos, uint32_t tickIndex)
{
    uint32_t x = samplePos.x & 3;
    uint32_t y = samplePos.y & 3;
    uint32_t a = 2068378560 * (1 - (x >> 1)) + 1500172770 * (x >> 1);
    uint32_t b = (y + ((x & 1) << 2)) << 2;
    uint32_t sampleOffset = ReverseBits4(tickIndex);
    return ((a >> b) + sampleOffset) & 0xF;
}

float RadicalInverse(uint32_t idx, uint32_t base)
{
    float val = 0.0f;
    float rcpBase = 1.0f / (float)base;
    float rcpBi = rcpBase;
    while (idx > 0) {
        uint32_t d_i = idx % base;
        val += float(d_i) * rcpBi;
        idx = uint32_t(idx * rcpBase);
        rcpBi *= rcpBase;
    }
    return val;
}

float2 Halton2D(uint32_t idx)
{
    return float2(RadicalInverse(idx + 1, 3), ReverseBits32(idx + 1) * 2.3283064365386963e-10f);
}