#pragma once
#include "RoadGraph.h"
#include "CityBlock.h"
#include <vector>

// SimulationState — everything needed to reproduce a city at a given tick.
// This is what IPersistence saves and loads. Observers receive a const ref
// to this after every state change.
struct SimulationState
{
    RoadGraph               graph;
    std::vector<CityBlock>  blocks;
    int                     tick = 0;
    unsigned int            seed = 42;
    float                   commercialRadius = 10.0f;

    void reset(unsigned int newSeed)
    {
        graph.clear();
        blocks.clear();
        tick = 0;
        seed = newSeed;
    }
};
