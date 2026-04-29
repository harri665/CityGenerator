#pragma once
#include "../core/RoadGraph.h"
#include "../core/CityBlock.h"
#include <UT/UT_Vector3.h>
#include <vector>

// Half-edge face traversal in the XZ plane, parcel subdivision and
// distance-based zoning. Produces the parcel-level CityBlocks that
// BuildingFactory consumes. Independent of CitySimulator/observers so
// it can be called from any SOP that has a RoadGraph.
namespace BlockExtraction
{
    std::vector<CityBlock> rebuild(const RoadGraph& graph,
                                   float commercialRadius,
                                   const UT_Vector3F& cityCenter);
}
