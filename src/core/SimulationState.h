#pragma once
#include "RoadGraph.h"
#include "CityBlock.h"
#include "StreamlineGenerator.h"
#include <vector>

struct FieldConfig
{
    float gridTheta  = 0.0f;
    float gridSize   = 200.0f;
    float gridDecay  = 1.0f;
    float radialCx   = 0.0f;
    float radialCz   = 0.0f;
    float radialSize = 80.0f;
    float radialDecay= 1.0f;
};

struct SimulationState
{
    RoadGraph graph;
    std::vector<CityBlock> blocks;
    StreamlineParams params;
    FieldConfig field;
    float commercialRadius = 10.0f;

    void reset()
    {
        graph.clear();
        blocks.clear();
    }
};
