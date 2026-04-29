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
    // Create attributes for road rendering
    GA_RWHandleS primType(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));
    GA_RWHandleS roadTier(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "road_tier", 1));
    GA_RWHandleF pscale(myGdp->addFloatTuple(GA_ATTRIB_POINT, "pscale", 1));

    for (const auto& edge : graph.edges())
    {
        const RoadNode* a = graph.node(edge.fromNode);
        const RoadNode* b = graph.node(edge.toNode);
        if (!a || !b) continue;

        // Road segment as a two-point open polygon (line)
        GU_PrimPoly* line = GU_PrimPoly::build(myGdp, 2, GU_POLY_OPEN);
        myGdp->setPos3(line->getPointOffset(0), a->position);
        myGdp->setPos3(line->getPointOffset(1), b->position);

        // Set pscale on both points for Polywire SOP downstream
        if (pscale.isValid())
        {
            float ps = edge.width * 0.3f;
            pscale.set(line->getPointOffset(0), ps);
            pscale.set(line->getPointOffset(1), ps);
        }

        if (primType.isValid())
            primType.set(line->getMapOffset(), "road");

        // Tag road tier for material assignment
        if (roadTier.isValid())
        {
            const char* tier = "local";
            if (edge.width >= 2.5f)      tier = "arterial";
            else if (edge.width >= 1.5f) tier = "collector";
            roadTier.set(line->getMapOffset(), tier);
        }
    }
}

void GeometryObserver::writeBlocks(const std::vector<CityBlock>& blocks)
{
    GA_RWHandleS primType(myGdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));
    if (!primType.isValid())
        primType = GA_RWHandleS(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "type", 1));

    GA_RWHandleS zoneAttr(myGdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "zone", 1));
    GA_RWHandleI idAttr(myGdp->addIntTuple(GA_ATTRIB_PRIMITIVE, "id", 1));

    for (const auto& block : blocks)
    {
        if (block.boundary.size() < 3) continue;

        // Create a single closed polygon for the block boundary
        GU_PrimPoly* poly = GU_PrimPoly::build(myGdp, (GA_Size)block.boundary.size(), GU_POLY_CLOSED);
        for (size_t i = 0; i < block.boundary.size(); ++i)
        {
            myGdp->setPos3(poly->getPointOffset((GA_Size)i), block.boundary[i]);
        }

        // Tag primitive with block metadata
        GA_Offset offset = poly->getMapOffset();
        if (primType.isValid()) primType.set(offset, "block");
        if (zoneAttr.isValid()) zoneAttr.set(offset, zoneTypeName(block.zone).c_str());
        if (idAttr.isValid())   idAttr.set(offset, block.id);
    }
}

// 
// UIObserver
void UIObserver::onStateChanged(const SimulationState& state)
{
    if (!myNode) return;

    myNode->setChRefString(std::to_string(state.graph.nodeCount()),
                           CH_STRING_LITERAL, "info_nodes", 0, 0.0);

    myNode->setChRefString(std::to_string(state.graph.edgeCount()),
                           CH_STRING_LITERAL, "info_edges", 0, 0.0);

    myNode->setChRefString(std::to_string(state.blocks.size()),
                           CH_STRING_LITERAL, "info_blocks", 0, 0.0);
}
