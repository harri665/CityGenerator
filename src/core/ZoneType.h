#pragma once
#include <string>

// ZoneType — land use classification for each city block
// Treated polymorphically via BuildingFactory — no switch statements on this
enum class ZoneType
{
    Residential,
    Commercial,
    Industrial,
    Park,
    Empty
};

inline std::string zoneTypeName(ZoneType z)
{
    switch (z)
    {
        case ZoneType::Residential: return "residential";
        case ZoneType::Commercial:  return "commercial";
        case ZoneType::Industrial:  return "industrial";
        case ZoneType::Park:        return "park";
        default:                    return "empty";
    }
}
