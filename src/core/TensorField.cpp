#include "TensorField.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Lattice noise with bilinear interpolation and smoothstep
// ─────────────────────────────────────────────────────────────────────────────
float TensorField::smoothNoise(float x, float z, unsigned int seed) const
{
    // Integer cell corners
    int ix = (int)std::floor(x);
    int iz = (int)std::floor(z);

    // Fractional position within cell
    float fx = x - (float)ix;
    float fz = z - (float)iz;

    // Smoothstep interpolation weights: t*t*(3-2*t)
    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sz = fz * fz * (3.0f - 2.0f * fz);

    // Hash each corner to [0, 1]
    auto hash = [&](int cx, int cz) -> float {
        unsigned int h = (unsigned int)(cx * 1619 + cz * 31337 + seed * 1013904223);
        h = h & 0x7fffffff;
        return (float)h / (float)0x7fffffff;
    };

    float n00 = hash(ix,     iz);
    float n10 = hash(ix + 1, iz);
    float n01 = hash(ix,     iz + 1);
    float n11 = hash(ix + 1, iz + 1);

    // Bilinear interpolation
    float nx0 = n00 + sx * (n10 - n00);
    float nx1 = n01 + sx * (n11 - n01);
    return nx0 + sz * (nx1 - nx0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fractal Brownian Motion — 4 octaves of smoothNoise
// ─────────────────────────────────────────────────────────────────────────────
float TensorField::fbm(float x, float z, int octaves, unsigned int seed) const
{
    float value     = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float ampSum    = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        value     += amplitude * smoothNoise(x * frequency, z * frequency, seed);
        ampSum    += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / ampSum;
}

// ─────────────────────────────────────────────────────────────────────────────
// Water mask — straight river running north-south at riverX
// ─────────────────────────────────────────────────────────────────────────────
float TensorField::waterMask(float x, float /*z*/) const
{
    return (std::abs(x - riverX) < riverWidth) ? 1.0f : 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sample heading — blends FBM noise angle with radial (tangential) pull
// ─────────────────────────────────────────────────────────────────────────────
float TensorField::sampleHeading(float x, float z, unsigned int seed) const
{
    // Noise-based angle
    float noiseAngle = fbm(x * noiseScale, z * noiseScale, 4, seed)
                       * 2.0f * (float)M_PI;

    // Radial angle — tangent to circle around cityCenter
    float radialAngle = std::atan2(z - cityCenter.z(), x - cityCenter.x())
                        + (float)M_PI * 0.5f;

    // Blend
    return noiseAngle * (1.0f - radialWeight) + radialAngle * radialWeight;
}
