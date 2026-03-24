#include "Factories.h"
#include "../strategies/GrowthStrategies.h"
#include <GU/GU_PrimPoly.h>
#include <cstdlib>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// StrategyFactory
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IGrowthStrategy>
StrategyFactory::create(const std::string& key, float param1, float param2)
{
    if (key == "grid")    return std::make_unique<GridGrowthStrategy>(param1);
    if (key == "organic") return std::make_unique<OrganicGrowthStrategy>(param1, param2, 0.4f, 25.0f);
    if (key == "radial")  return std::make_unique<RadialGrowthStrategy>((int)param1, param2);

    // Default fallback — grid
    return std::make_unique<GridGrowthStrategy>(param1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Building strategy implementations
// Each generates extruded polygon geometry for its zone type.
// Heights and footprint inset are parameterised per zone.
// ─────────────────────────────────────────────────────────────────────────────

static void extrudeBlock(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& boundary,
                         float inset, float height, int seed)
{
    if (boundary.size() < 3) return;

    int n = (int)boundary.size();

    // Compute centroid
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary) centroid += p;
    centroid /= (float)n;

    // Deterministic height variation using seed
    unsigned int h = (unsigned int)seed * 1664525u + 1013904223u;
    float heightVar = 0.5f + 1.0f * (float)(h % 1000u) / 1000.0f;
    float actualHeight = height * heightVar;

    // Landmark check: 1 in 20 buildings is a tower
    unsigned int h2 = (unsigned int)seed * 22695477u + 1u;
    if (h2 % 20u == 0u)
    {
        actualHeight *= 2.5f;
    }

    // Inset boundary points toward centroid
    std::vector<UT_Vector3F> base(n);
    std::vector<UT_Vector3F> top(n);
    for (int i = 0; i < n; ++i)
    {
        UT_Vector3F pt = boundary[i] + (centroid - boundary[i]) * inset;
        base[i] = UT_Vector3F(pt.x(), 0.0f, pt.z());
        top[i]  = UT_Vector3F(pt.x(), actualHeight, pt.z());
    }

    // Base polygon (Y=0)
    GU_PrimPoly* basePoly = GU_PrimPoly::build(gdp, n, GU_POLY_CLOSED);
    for (int i = 0; i < n; ++i)
        gdp->setPos3(basePoly->getPointOffset(i), base[i]);

    // Top polygon (Y=actualHeight)
    GU_PrimPoly* topPoly = GU_PrimPoly::build(gdp, n, GU_POLY_CLOSED);
    for (int i = 0; i < n; ++i)
        gdp->setPos3(topPoly->getPointOffset(i), top[i]);

    // Side quads — wound so normals face outward
    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;
        GU_PrimPoly* side = GU_PrimPoly::build(gdp, 4, GU_POLY_CLOSED);
        gdp->setPos3(side->getPointOffset(0), base[i]);
        gdp->setPos3(side->getPointOffset(1), base[j]);
        gdp->setPos3(side->getPointOffset(2), top[j]);
        gdp->setPos3(side->getPointOffset(3), top[i]);
    }
}

void ResidentialBuildingStrategy::generate(GU_Detail* gdp,
                                            const std::vector<UT_Vector3F>& boundary,
                                            int seed)
{
    // Low-density: height 2-4 units
    extrudeBlock(gdp, boundary, 0.3f, 3.0f, seed);
}

void CommercialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Dense: height 6-12 units
    extrudeBlock(gdp, boundary, 0.1f, 9.0f, seed);
}

void IndustrialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Wide, low warehouse-style: height 2-3 units
    extrudeBlock(gdp, boundary, 0.05f, 2.5f, seed);
}

void ParkBuildingStrategy::generate(GU_Detail* gdp,
                                     const std::vector<UT_Vector3F>& boundary,
                                     int seed)
{
    // Scatter 5 tree approximations (8-sided circles, radius 0.5)
    if (boundary.size() < 3) return;

    // Compute centroid
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary) centroid += p;
    centroid /= (float)boundary.size();

    srand((unsigned int)seed);
    for (int t = 0; t < 5; ++t)
    {
        float ox = ((float)rand() / (float)RAND_MAX - 0.5f) * 4.0f;
        float oz = ((float)rand() / (float)RAND_MAX - 0.5f) * 4.0f;
        UT_Vector3F center(centroid.x() + ox, 0.0f, centroid.z() + oz);

        const int sides = 8;
        const float radius = 0.5f;
        GU_PrimPoly* tree = GU_PrimPoly::build(gdp, sides, GU_POLY_CLOSED);
        for (int i = 0; i < sides; ++i)
        {
            float angle = (float)i * 2.0f * (float)M_PI / (float)sides;
            UT_Vector3F pt(center.x() + std::cos(angle) * radius,
                           0.0f,
                           center.z() + std::sin(angle) * radius);
            gdp->setPos3(tree->getPointOffset(i), pt);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildingFactory
// ─────────────────────────────────────────────────────────────────────────────
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
