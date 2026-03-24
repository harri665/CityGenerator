#pragma once
#include "ISimObserver.h"
#include "../factory/Factories.h"
#include <GU/GU_Detail.h>
#include <OP/OP_Node.h>

// GeometryObserver
// Rebuilds the SOP's output geometry whenever simulation state changes.
// Writes road edges as line primitives and building footprints as polygons.
class GeometryObserver : public ISimObserver
{
public:
    // gdp is the GU_Detail owned by the SOP's cook output
    explicit GeometryObserver(GU_Detail* gdp) : myGdp(gdp) {}

    void onStateChanged(const SimulationState& state) override;

private:
    void writeRoads(const RoadGraph& graph);
    void writeBlocks(const std::vector<CityBlock>& blocks);

    GU_Detail* myGdp = nullptr;
};

// UIObserver
// Updates HDA parameter display values (tick counter, node/edge counts)
// after each state change so the UI stays in sync without polling.
class UIObserver : public ISimObserver
{
public:
    explicit UIObserver(OP_Node* node) : myNode(node) {}

    void onStateChanged(const SimulationState& state) override;

private:
    OP_Node* myNode = nullptr;
};
