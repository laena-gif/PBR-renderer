#pragma once

uint32_t ReverseBits32(uint32_t x);
uint32_t CompactBits(uint32_t x);
uint32_t ReverseBits4(uint32_t x);
uint32_t Bayer4x4(int2 samplePos, uint32_t tickIndex);
float RadicalInverse(uint32_t idx, uint32_t base);
float2 Halton2D(uint32_t idx);