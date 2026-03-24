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

void CityBlock::assignZone(const UT_Vector3F& cityCenter, float commercialRadius)
{
    // Compute block centroid
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary)
        centroid += p;
    if (!boundary.empty())
        centroid /= (float)boundary.size();

    float dist = (centroid - cityCenter).length();

    if (area < 4.0f)
    {
        zone = ZoneType::Park;
    }
    else if (dist < commercialRadius)
    {
        zone = ZoneType::Commercial;
    }
    else if (dist < commercialRadius * 2.5f)
    {
        zone = ZoneType::Residential;
    }
    else
    {
        zone = ZoneType::Industrial;
    }
}
