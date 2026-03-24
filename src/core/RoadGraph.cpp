#include "RoadGraph.h"

int RoadGraph::addNode(const UT_Vector3F& pos)
{
    int id = myNextNodeId++;
    myNodes.push_back({ id, pos });
    myAdjacency[id] = {};
    return id;
}

int RoadGraph::addEdge(int fromId, int toId, float width)
{
    int id = myNextEdgeId++;
    myEdges.push_back({ id, fromId, toId, width });
    myAdjacency[fromId].push_back(id);
    myAdjacency[toId].push_back(id);
    return id;
}

const RoadNode* RoadGraph::node(int id) const
{
    for (const auto& n : myNodes)
        if (n.id == id) return &n;
    return nullptr;
}

const RoadEdge* RoadGraph::edge(int id) const
{
    for (const auto& e : myEdges)
        if (e.id == id) return &e;
    return nullptr;
}

std::vector<int> RoadGraph::neighborEdges(int nodeId) const
{
    auto it = myAdjacency.find(nodeId);
    if (it != myAdjacency.end()) return it->second;
    return {};
}

void RoadGraph::clear()
{
    myNodes.clear();
    myEdges.clear();
    myAdjacency.clear();
    myNextNodeId = 0;
    myNextEdgeId = 0;
}
