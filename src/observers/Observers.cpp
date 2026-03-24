#include "Observers.h"
#include <GU/GU_PrimPoly.h>
#include <GEO/GEO_PrimPoly.h>
#include <GA/GA_Handle.h>
#include <OP/OP_Node.h>

// GeometryObserver
void GeometryObserver::onStateChanged(const SimulationState& state)
{
    if (!myGdp) return;

    myGdp->clearAndDestroy();

    writeRoads(state.graph);
    writeBlocks(state.blocks);

    myGdp->bumpDataIdsForAddOrRemove(true, true, true);
}

void GeometryObserver::writeRoads(const RoadGraph& graph)
{
    // Create a string attribute to tag primitives by type
    GA_RWHandleS primType(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));

    for (const auto& edge : graph.edges())
    {
        const RoadNode* a = graph.node(edge.fromNode);
        const RoadNode* b = graph.node(edge.toNode);
        if (!a || !b) continue;

        // Road segment as a two-point open polygon (line)
        GU_PrimPoly* line = GU_PrimPoly::build(myGdp, 2, GU_POLY_OPEN);
        myGdp->setPos3(line->getPointOffset(0), a->position);
        myGdp->setPos3(line->getPointOffset(1), b->position);

        if (primType.isValid())
            primType.set(line->getMapOffset(), "road");
    }
}

void GeometryObserver::writeBlocks(const std::vector<CityBlock>& blocks)
{
    GA_RWHandleS primType(myGdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));
    if (!primType.isValid())
        primType = GA_RWHandleS(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));

    GA_RWHandleS zoneAttr(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "zone", 1));

    for (const auto& block : blocks)
    {
        if (block.boundary.size() < 3) continue;

        // Use BuildingFactory to get the right geometry strategy — no switch here
        auto buildingStrat = BuildingFactory::create(block.zone);
        if (buildingStrat)
        {
            buildingStrat->generate(myGdp, block.boundary, block.id);
        }

        // Tag last-added primitive with zone name for shading/selection
        GA_Offset lastPrim = myGdp->getPrimitiveMap().lastOffset();
        if (primType.isValid())  primType.set(lastPrim, "block");
        if (zoneAttr.isValid())  zoneAttr.set(lastPrim, zoneTypeName(block.zone).c_str());
    }
}

// 
// UIObserver
void UIObserver::onStateChanged(const SimulationState& state)
{
    if (!myNode) return;

    // Update read-only display parms on the HDA so the artist can see live stats.
    // These parm names must match what you define in the HDA Type Properties.
    myNode->setChRefString(std::to_string(state.tick),
                           CH_STRING_LITERAL, "info_tick", 0, 0.0);

    myNode->setChRefString(std::to_string(state.graph.nodeCount()),
                           CH_STRING_LITERAL, "info_nodes", 0, 0.0);

    myNode->setChRefString(std::to_string(state.graph.edgeCount()),
                           CH_STRING_LITERAL, "info_edges", 0, 0.0);

    myNode->setChRefString(std::to_string(state.blocks.size()),
                           CH_STRING_LITERAL, "info_blocks", 0, 0.0);
}
