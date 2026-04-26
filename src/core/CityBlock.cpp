#include "CityBlock.h"
#include <cmath>

void CityBlock::computeArea()
{
    float a = 0.0f;
    int n = (int)boundary.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& p0 = boundary[i];
        const auto& p1 = boundary[(i + 1) % n];
        a += (p0.x() * p1.z()) - (p1.x() * p0.z());
    }
    area = std::abs(a * 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// File-local noise function for zone clustering (same hash pattern as
// TensorField::smoothNoise, avoids header changes)
// ─────────────────────────────────────────────────────────────────────────────
static float blockNoise(float x, float z)
{
    int ix = (int)std::floor(x);
    int iz = (int)std::floor(z);
    float fx = x - (float)ix;
    float fz = z - (float)iz;
    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sz = fz * fz * (3.0f - 2.0f * fz);

    auto hash = [](int cx, int cz) -> float {
        unsigned int h = (unsigned int)(cx * 1619 + cz * 31337 + 7919);
        h = h & 0x7fffffff;
        return (float)h / (float)0x7fffffff;
    };

    float n00 = hash(ix,     iz);
    float n10 = hash(ix + 1, iz);
    float n01 = hash(ix,     iz + 1);
    float n11 = hash(ix + 1, iz + 1);

    float nx0 = n00 + sx * (n10 - n00);
    float nx1 = n01 + sx * (n11 - n01);
    return nx0 + sz * (nx1 - nx0);
}

void CityBlock::assignZone(const UT_Vector3F& cityCenter, float commercialRadius)
{
    // Compute block centroid
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary)
        centroid += p;
    if (!boundary.empty())
        centroid /= (float)boundary.size();

    float dist = (centroid - cityCenter).length();

    // Sample noise at centroid — creates irregular zone boundaries
    float noise = blockNoise(centroid.x() * 0.15f, centroid.z() * 0.15f);

    // Parks: small blocks, or occasional green pockets in outer areas
    if (area < 4.0f ||
        (noise > 0.85f && dist > commercialRadius * 0.5f && area < 10.0f))
    {
        zone = ZoneType::Park;
    }
    // Commercial: CBD core, plus noise-driven corridors extending outward
    else if (dist < commercialRadius * 0.6f ||
             (noise > 0.5f && dist < commercialRadius * 1.5f))
    {
        zone = ZoneType::Commercial;
    }
    // Industrial: clusters at periphery only where noise is low
    else if (dist > commercialRadius * 2.0f && noise < 0.3f)
    {
        zone = ZoneType::Industrial;
    }
    // Residential: everything else
    else
    {
        zone = ZoneType::Residential;
    }
}
