#pragma once
#include "ZoneType.h"
#include <vector>
#include <UT/UT_Vector3.h>

struct CityBlock
{
    int id;
    std::vector<UT_Vector3F> boundary;
    ZoneType zone = ZoneType::Empty;
    float area = 0.0f;

    void computeArea();
    void assignZone(const UT_Vector3F& cityCenter, float commercialRadius);
    UT_Vector3F centroid() const;
};
