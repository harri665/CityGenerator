#include "CitySimulator.h"
#include <algorithm>

CitySimulator::CitySimulator(std::unique_ptr<IGrowthStrategy> growthStrategy,
                              std::unique_ptr<IPersistence>    persistence,
                              std::vector<ISimObserver*>        observers)
    : myGrowthStrategy(std::move(growthStrategy))
    , myPersistence(std::move(persistence))
    , myObservers(std::move(observers))
{
}

bool CitySimulator::step()
{
    if (!myGrowthStrategy) return false;

    bool grew = myGrowthStrategy->grow(myState);

    if (grew)
    {
        ++myState.tick;
        rebuildBlocks();
        notifyObservers();
    }

    return grew;
}

void CitySimulator::run(int steps)
{
    for (int i = 0; i < steps; ++i)
    {
        if (!step()) break; // stop early if fully grown
    }
}

void CitySimulator::reset(unsigned int seed)
{
    myState.reset(seed);
    notifyObservers();
}

bool CitySimulator::save(const std::string& path)
{
    if (!myPersistence) return false;
    return myPersistence->save(myState, path);
}

bool CitySimulator::load(const std::string& path)
{
    if (!myPersistence) return false;
    bool ok = myPersistence->load(myState, path);
    if (ok) notifyObservers();
    return ok;
}

void CitySimulator::setGrowthStrategy(std::unique_ptr<IGrowthStrategy> strategy)
{
    myGrowthStrategy = std::move(strategy);
}

void CitySimulator::addObserver(ISimObserver* obs)
{
    myObservers.push_back(obs);
}

void CitySimulator::removeObserver(ISimObserver* obs)
{
    myObservers.erase(
        std::remove(myObservers.begin(), myObservers.end(), obs),
        myObservers.end());
}

void CitySimulator::notifyObservers()
{
    // Polymorphic dispatch — each observer handles its own concerns
    for (ISimObserver* obs : myObservers)
        obs->onStateChanged(myState);
}

void CitySimulator::rebuildBlocks()
{
    // TODO: Traverse the road graph to extract minimal face loops (city blocks).
    //
    // Algorithm outline (half-edge / twin-edge face traversal):
    //   1. For each directed edge (u→v), find the next edge by turning
    //      clockwise at v among all edges incident to v.
    //   2. Follow the chain until we return to the start — this traces one face.
    //   3. Filter out the unbounded outer face (largest area or winding check).
    //   4. Convert each face loop to a CityBlock boundary.
    //   5. Call block.computeArea() and block.assignZone().
    //
    // Reference: "Extracting polygons from a planar straight-line graph"
    // or see CGAL's Arrangement_2 for a production-quality version.

    myState.blocks.clear();

    // Placeholder: create one block per edge pair as a stub so the rest of
    // the pipeline (building generation, persistence) can be tested end-to-end.
    int blockId = 0;
    const auto& edges = myState.graph.edges();
    for (int i = 0; i + 1 < (int)edges.size(); i += 2)
    {
        const RoadNode* a = myState.graph.node(edges[i].fromNode);
        const RoadNode* b = myState.graph.node(edges[i].toNode);
        const RoadNode* c = myState.graph.node(edges[i+1].toNode);
        if (!a || !b || !c) continue;

        CityBlock block;
        block.id = blockId++;
        block.boundary = { a->position, b->position, c->position };
        block.computeArea();
        block.assignZone(UT_Vector3F(0, 0, 0), myState.commercialRadius);
        myState.blocks.push_back(std::move(block));
    }
}
