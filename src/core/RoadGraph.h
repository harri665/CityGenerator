#pragma once
#include <vector>
#include <unordered_map>
#include <UT/UT_Vector3.h>

// RoadNode — a single intersection or endpoint in the road network
struct RoadNode
{
    int         id;
    UT_Vector3F position;
};

// 
// RoadEdge — a road segment connecting two nodes
struct RoadEdge
{
    int id;
    int fromNode;
    int toNode;
    float width = 1.0f;
};

// 
// RoadGraph — the full road network, grown tick by tick
class RoadGraph
{
public:
    RoadGraph() = default;

    int         addNode(const UT_Vector3F& pos);
    int         addEdge(int fromId, int toId, float width = 1.0f);

    const RoadNode*             node(int id) const;
    const RoadEdge*             edge(int id) const;

    const std::vector<RoadNode>& nodes() const { return myNodes; }
    const std::vector<RoadEdge>& edges() const { return myEdges; }

    // Returns all edge IDs connected to a given node
    std::vector<int>            neighborEdges(int nodeId) const;

    void                        clear();
    int                         nodeCount() const { return (int)myNodes.size(); }
    int                         edgeCount() const { return (int)myEdges.size(); }

private:
    std::vector<RoadNode>                       myNodes;
    std::vector<RoadEdge>                       myEdges;
    std::unordered_map<int, std::vector<int>>   myAdjacency; // nodeId -> edge IDs
    int                                         myNextNodeId = 0;
    int                                         myNextEdgeId = 0;
};
