#include "Factories.h"
#include <GU/GU_PrimPoly.h>
#include <cstdlib>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Building strategy implementations
// Each generates extruded polygon geometry for its zone type.
// Heights and footprint inset are parameterised per zone.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// extrudeBlock — builds base, top, and side quads with per-vertex setback
// variation to break up cookie-cutter appearance. No tower logic here —
// tower multiplier is applied per-zone in generate() methods.
// ─────────────────────────────────────────────────────────────────────────────
static void extrudeBlock(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& boundary,
                         float inset, float height, int seed,
                         float setbackVariation = 0.0f)
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

    // Inset boundary points toward centroid with per-vertex setback jitter
    std::vector<UT_Vector3F> base(n);
    std::vector<UT_Vector3F> top(n);
    for (int i = 0; i < n; ++i)
    {
        // Per-vertex inset variation breaks up the roofline
        unsigned int vh = (unsigned int)(seed + i * 2654435761u);
        float vertexInset = inset + setbackVariation * 0.15f
            * ((float)(vh % 100u) / 100.0f - 0.5f);
        vertexInset = std::max(0.01f, std::min(0.9f, vertexInset));

        UT_Vector3F pt = boundary[i] + (centroid - boundary[i]) * vertexInset;
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

// ─────────────────────────────────────────────────────────────────────────────
// extrudeL — L-shaped footprint for residential corner lots. Clips off one
// corner quadrant from a roughly square parcel, then extrudes the L shape.
// ─────────────────────────────────────────────────────────────────────────────
static void extrudeL(GU_Detail* gdp,
                     const std::vector<UT_Vector3F>& boundary,
                     float inset, float height, int seed)
{
    if (boundary.size() < 3) return;

    // Compute centroid and bounding box in XZ
    UT_Vector3F centroid(0, 0, 0);
    float minX = 1e18f, maxX = -1e18f, minZ = 1e18f, maxZ = -1e18f;
    for (const auto& p : boundary)
    {
        centroid += p;
        minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x());
        minZ = std::min(minZ, p.z()); maxZ = std::max(maxZ, p.z());
    }
    centroid /= (float)boundary.size();

    // Pick which corner to clip based on seed
    float midX = (minX + maxX) * 0.5f;
    float midZ = (minZ + maxZ) * 0.5f;
    int corner = seed % 4;

    // Define clip point and normal to remove one quadrant
    UT_Vector3F clipPt, clipNorm;
    switch (corner)
    {
        case 0: clipPt = UT_Vector3F(midX, 0, midZ); clipNorm = UT_Vector3F(-1, 0, -1); break;
        case 1: clipPt = UT_Vector3F(midX, 0, midZ); clipNorm = UT_Vector3F( 1, 0, -1); break;
        case 2: clipPt = UT_Vector3F(midX, 0, midZ); clipNorm = UT_Vector3F( 1, 0,  1); break;
        default:clipPt = UT_Vector3F(midX, 0, midZ); clipNorm = UT_Vector3F(-1, 0,  1); break;
    }

    // Normalize the clip normal
    float nl = std::sqrt(clipNorm.x() * clipNorm.x() + clipNorm.z() * clipNorm.z());
    clipNorm.x() /= nl;
    clipNorm.z() /= nl;

    // Build L shape by keeping vertices on the "inside" of the clip plane
    std::vector<UT_Vector3F> lShape;
    int n = (int)boundary.size();
    for (int i = 0; i < n; ++i)
    {
        const UT_Vector3F& cur  = boundary[i];
        const UT_Vector3F& prev = boundary[(i + n - 1) % n];
        float dCur  = (cur.x()  - clipPt.x()) * clipNorm.x()
                    + (cur.z()  - clipPt.z()) * clipNorm.z();
        float dPrev = (prev.x() - clipPt.x()) * clipNorm.x()
                    + (prev.z() - clipPt.z()) * clipNorm.z();
        bool curIn  = dCur  >= 0.0f;
        bool prevIn = dPrev >= 0.0f;
        if (curIn != prevIn)
        {
            float t = dPrev / (dPrev - dCur);
            lShape.push_back(UT_Vector3F(
                prev.x() + t * (cur.x() - prev.x()), 0,
                prev.z() + t * (cur.z() - prev.z())));
        }
        if (curIn) lShape.push_back(cur);
    }

    if (lShape.size() >= 3)
        extrudeBlock(gdp, lShape, inset, height, seed, 0.3f);
}

void ResidentialBuildingStrategy::generate(GU_Detail* gdp,
                                            const std::vector<UT_Vector3F>& boundary,
                                            int seed)
{
    // L-shaped footprint for roughly square parcels (1-in-3 chance)
    if ((int)boundary.size() == 4)
    {
        // Check aspect ratio
        float minE = 1e18f, maxE = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            UT_Vector3F d = boundary[(i + 1) % 4] - boundary[i];
            float len = std::sqrt(d.x() * d.x() + d.z() * d.z());
            minE = std::min(minE, len);
            maxE = std::max(maxE, len);
        }
        float aspect = (maxE > 1e-6f) ? minE / maxE : 0.0f;
        unsigned int lh = (unsigned int)seed * 2654435761u;
        if (aspect > 0.8f && aspect < 1.3f && (lh % 3u == 0u))
        {
            extrudeL(gdp, boundary, 0.25f, 2.5f, seed);
            return;
        }
    }

    // Low-density residential: height 2.5, setback variation 0.5, no towers
    extrudeBlock(gdp, boundary, 0.25f, 2.5f, seed, 0.5f);
}

void CommercialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Compute distance from origin to determine CBD vs suburban commercial
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary) centroid += p;
    centroid /= (float)boundary.size();
    float dist = std::sqrt(centroid.x() * centroid.x() + centroid.z() * centroid.z());

    float height, inset;
    if (dist < 15.0f)
    {
        // CBD core: tall buildings, tower chance 1-in-5
        height = 12.0f;
        inset  = 0.08f;
        unsigned int th = (unsigned int)seed * 22695477u + 1u;
        if (th % 5u == 0u) height *= 3.0f;
    }
    else
    {
        // Suburban commercial: lower, wider
        height = 6.0f;
        inset  = 0.12f;
    }

    extrudeBlock(gdp, boundary, inset, height, seed, 0.2f);
}

void IndustrialBuildingStrategy::generate(GU_Detail* gdp,
                                           const std::vector<UT_Vector3F>& boundary,
                                           int seed)
{
    // Wide, low warehouse-style: minimal setback, narrow height variation
    extrudeBlock(gdp, boundary, 0.04f, 3.0f, seed, 0.0f);
}

void ParkBuildingStrategy::generate(GU_Detail* gdp,
                                     const std::vector<UT_Vector3F>& boundary,
                                     int seed)
{
    if (boundary.size() < 3) return;

    // Compute centroid and area for tree count
    UT_Vector3F centroid(0, 0, 0);
    for (const auto& p : boundary) centroid += p;
    centroid /= (float)boundary.size();

    float area = 0.0f;
    int bn = (int)boundary.size();
    for (int i = 0; i < bn; ++i)
    {
        const auto& p0 = boundary[i];
        const auto& p1 = boundary[(i + 1) % bn];
        area += (p0.x() * p1.z()) - (p1.x() * p0.z());
    }
    area = std::abs(area * 0.5f);

    // Flat green ground polygon at Y=0.01
    GU_PrimPoly* ground = GU_PrimPoly::build(gdp, bn, GU_POLY_CLOSED);
    for (int i = 0; i < bn; ++i)
    {
        UT_Vector3F pt = boundary[i] + (centroid - boundary[i]) * 0.05f;
        gdp->setPos3(ground->getPointOffset(i), UT_Vector3F(pt.x(), 0.01f, pt.z()));
    }

    // Tree count scales with area, capped at 12
    int treeCount = std::min(12, (int)(3.0f + area * 0.5f));

    srand((unsigned int)seed);
    for (int t = 0; t < treeCount; ++t)
    {
        float ox = ((float)rand() / (float)RAND_MAX - 0.5f) * 4.0f;
        float oz = ((float)rand() / (float)RAND_MAX - 0.5f) * 4.0f;
        UT_Vector3F center(centroid.x() + ox, 0.0f, centroid.z() + oz);

        // Vary tree radius by hash
        unsigned int rh = (unsigned int)(seed + t * 2654435761u);
        float radius = 0.3f + 0.5f * (float)(rh % 100u) / 100.0f;

        const int sides = 8;
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
