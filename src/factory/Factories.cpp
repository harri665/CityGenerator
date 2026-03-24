#include "Factories.h"
#include "../strategies/GrowthStrategies.h"
#include <GU/GU_PrimPoly.h>
#include <cstdlib>

// StrategyFactory
std::unique_ptr<IGrowthStrategy>
StrategyFactory::create(const std::string& key, float param1, float param2)
{
    if (key == "grid")    return std::make_unique<GridGrowthStrategy>(param1);
    if (key == "organic") return std::make_unique<OrganicGrowthStrategy>(param1, param2);
    if (key == "radial")  return std::make_unique<RadialGrowthStrategy>((int)param1, param2);

    // Default fallback — grid
    return std::make_unique<GridGrowthStrategy>(param1);
}

// Building strategy implementations
// Each generates extruded polygon geometry for its zone type.
// Heights and footprint inset are parameterised per zone.

static void extrudeBlock(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& boundary,
                         float inset, float height, int seed)
{
    if (boundary.size() < 3) return;

    // Simple inset: shrink each point toward the centroid
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary) centroid += p;
    centroid /= (float)boundary.size();

    // Build base polygon
    GU_PrimPoly* poly = GU_PrimPoly::build(gdp, (int)boundary.size(), GU_POLY_CLOSED);
    for (int i = 0; i < (int)boundary.size(); ++i)
    {
        UT_Vector3F pt = boundary[i] + (centroid - boundary[i]) * inset;
        gdp->setPos3(poly->getPointOffset(i), pt);
    }

    // TODO: Use GU_Detail extrude or manually build top + side polygons
    // For now the base polygon acts as a footprint placeholder.
    // Replace with proper extrusion using GEO_PrimPoly and bridge topology.
    (void)height;
    (void)seed;
}

void ResidentialBuildingStrategy::generate(GU_Detail* gdp,
                                            const std::vector<UT_Vector3F>& boundary,
                                            int seed)
{
    // Low-density: multiple small buildings, height 2–4 units
    extrudeBlock(gdp, boundary, 0.3f, 3.0f, seed);
}

void CommercialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Dense: single larger building filling most of the block, height 6–12 units
    extrudeBlock(gdp, boundary, 0.1f, 9.0f, seed);
}

void IndustrialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Wide, low warehouse-style: height 2–3 units, large footprint
    extrudeBlock(gdp, boundary, 0.05f, 2.5f, seed);
}

void ParkBuildingStrategy::generate(GU_Detail* gdp,
                                     const std::vector<UT_Vector3F>& boundary,
                                     int seed)
{
    // No extrusion — just mark the block boundary as an open polygon
    if (boundary.size() < 3) return;
    GU_PrimPoly* poly = GU_PrimPoly::build(gdp, (int)boundary.size(), GU_POLY_CLOSED);
    for (int i = 0; i < (int)boundary.size(); ++i)
        gdp->setPos3(poly->getPointOffset(i), boundary[i]);
    (void)seed;
}

// BuildingFactory
std::unique_ptr<IBuildingStrategy> BuildingFactory::create(ZoneType zone)
{
    switch (zone)
    {
        case ZoneType::Residential: return std::make_unique<ResidentialBuildingStrategy>();
        case ZoneType::Commercial:  return std::make_unique<CommercialBuildingStrategy>();
        case ZoneType::Industrial:  return std::make_unique<IndustrialBuildingStrategy>();
        case ZoneType::Park:        return std::make_unique<ParkBuildingStrategy>();
        default:                    return nullptr;
    }
}
