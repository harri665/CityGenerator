#pragma once
#include "ZoneType.h"
#include <vector>
#include <UT/UT_Vector3.h>

// CityBlock — a closed polygon formed by the minimal road loop enclosing it
// Extracted from RoadGraph via planar face traversal
struct CityBlock
{
    int                     id;
    std::vector<UT_Vector3F> boundary;   // ordered loop of 2D corner positions
    ZoneType                zone = ZoneType::Empty;
    float                   area = 0.0f;

    // Compute signed area (shoelace) — also determines winding order
    void computeArea();

    // Assign zone based on distance from city center and block size
    void assignZone(const UT_Vector3F& cityCenter, float commercialRadius);
};
