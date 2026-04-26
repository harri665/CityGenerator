#include "GrowthStrategies.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// GridGrowthStrategy
// ─────────────────────────────────────────────────────────────────────────────
bool GridGrowthStrategy::grow(SimulationState& state)
{
    RoadGraph& g = state.graph;

    // Seed the graph on first tick
    if (g.nodeCount() == 0)
    {
        int origin = g.addNode(UT_Vector3F(0, 0, 0));
        myFrontier.clear();
        myFrontier.push_back(origin);
        return true;
    }

    // Iterate over a copy of the frontier, clear and rebuild
    std::vector<int> oldFrontier = myFrontier;
    myFrontier.clear();

    float snapDist = myBlockSize * 0.4f;

    // Cardinal directions: +X, -X, +Z, -Z
    UT_Vector3F dirs[4] = {
        UT_Vector3F(myBlockSize, 0, 0),
        UT_Vector3F(-myBlockSize, 0, 0),
        UT_Vector3F(0, 0, myBlockSize),
        UT_Vector3F(0, 0, -myBlockSize)
    };

    for (int frontId : oldFrontier)
    {
        const RoadNode* frontNode = g.node(frontId);
        if (!frontNode) continue;

        for (int d = 0; d < 4; ++d)
        {
            UT_Vector3F candidate = frontNode->position + dirs[d];

            // Snap check: skip if any existing node is within snapDist
            bool tooClose = false;
            for (const auto& n : g.nodes())
            {
                float dx = n.position.x() - candidate.x();
                float dz = n.position.z() - candidate.z();
                if (std::sqrt(dx * dx + dz * dz) < snapDist)
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            int newId = g.addNode(candidate);
            g.addEdge(frontId, newId);
            myFrontier.push_back(newId);
        }
    }

    return !myFrontier.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// OrganicGrowthStrategy
// ─────────────────────────────────────────────────────────────────────────────
bool OrganicGrowthStrategy::grow(SimulationState& state)
{
    RoadGraph& g = state.graph;

    // Tick 0: seed the graph and one arterial tip
    if (g.nodeCount() == 0)
    {
        int origin = g.addNode(UT_Vector3F(0, 0, 0));
        myTips.clear();
        float heading = myField.sampleHeading(0, 0, state.seed);
        myTips.push_back({ origin, heading, 0 });
        return true;
    }

    // Tier parameters: segLen, snapRadius, jitter, branchChance, branchTier, maxTips
    struct TierParams {
        float segLen;
        float snapRadius;
        float jitter;
        float branchChance;
        int   branchTier;
        int   maxTips;
    };

    TierParams tierParams[3];
    // Tier 0 (arterial)
    tierParams[0].segLen      = mySegmentLen * 3.0f;
    tierParams[0].snapRadius  = tierParams[0].segLen * 0.4f;
    tierParams[0].jitter      = myAngleJitter * 0.3f;
    tierParams[0].branchChance = 0.08f;
    tierParams[0].branchTier  = 1;
    tierParams[0].maxTips     = 8;
    // Tier 1 (collector)
    tierParams[1].segLen      = mySegmentLen * 1.5f;
    tierParams[1].snapRadius  = tierParams[1].segLen * 0.4f;
    tierParams[1].jitter      = myAngleJitter * 0.7f;
    tierParams[1].branchChance = 0.15f;
    tierParams[1].branchTier  = 2;
    tierParams[1].maxTips     = 24;
    // Tier 2 (local)
    tierParams[2].segLen      = mySegmentLen;
    tierParams[2].snapRadius  = tierParams[2].segLen * 0.4f;
    tierParams[2].jitter      = myAngleJitter;
    tierParams[2].branchChance = 0.0f;
    tierParams[2].branchTier  = -1;
    tierParams[2].maxTips     = 64;

    // Work on a copy; rebuild myTips
    std::vector<GrowthTip> oldTips = myTips;
    myTips.clear();
    bool addedGeometry = false;

    // Sort tips by tier so arterials process first, then collectors, then locals
    std::sort(oldTips.begin(), oldTips.end(),
              [](const GrowthTip& a, const GrowthTip& b) { return a.tier < b.tier; });

    for (int tipIdx = 0; tipIdx < (int)oldTips.size(); ++tipIdx)
    {
        GrowthTip tip = oldTips[tipIdx];
        if (tip.tier < 0 || tip.tier > 2) continue;

        const TierParams& tp = tierParams[tip.tier];
        const RoadNode* tipNode = g.node(tip.nodeId);
        if (!tipNode) continue;

        UT_Vector3F pos = tipNode->position;

        // Density gradient: blocks shrink near center, grow at edges
        float distFromCenter = std::sqrt(pos.x() * pos.x() + pos.z() * pos.z());
        float densityFactor = 0.5f + 0.5f * std::min(1.0f,
            distFromCenter / (state.commercialRadius * 4.0f));
        float segLen    = tp.segLen    * densityFactor;
        float snapRadius = tp.snapRadius * densityFactor;

        // Blend heading with tensor field
        float heading = tip.heading * 0.6f
            + myField.sampleHeading(pos.x(), pos.z(),
                                    state.seed + state.tick * 7 + tipIdx) * 0.4f;

        // Apply jitter
        srand(state.seed + state.tick * 100 + tipIdx);
        float randFloat = (float)rand() / (float)RAND_MAX;
        heading += (randFloat - 0.5f) * 2.0f * tp.jitter;

        // Compute candidate position
        float newX = pos.x() + std::cos(heading) * segLen;
        float newZ = pos.z() + std::sin(heading) * segLen;

        // Water check
        if (myField.waterMask(newX, newZ) > 0.5f)
        {
            // Try reflecting heading
            heading += (float)M_PI * 0.5f;
            newX = pos.x() + std::cos(heading) * segLen;
            newZ = pos.z() + std::sin(heading) * segLen;

            if (myField.waterMask(newX, newZ) > 0.5f)
            {
                // Terminate this tip
                continue;
            }
        }

        // Snap check: find closest existing node within snapRadius
        int snapNodeId = -1;
        float bestDist = snapRadius;
        for (const auto& n : g.nodes())
        {
            float dx = n.position.x() - newX;
            float dz = n.position.z() - newZ;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < bestDist)
            {
                bestDist = dist;
                snapNodeId = n.id;
            }
        }

        // Road width by tier: arterials wide, locals narrow
        static const float tierWidth[3] = { 3.0f, 1.8f, 1.0f };
        float edgeWidth = tierWidth[tip.tier];

        if (snapNodeId >= 0)
        {
            // Connect to existing node and terminate this tip
            g.addEdge(tip.nodeId, snapNodeId, edgeWidth);
            addedGeometry = true;
            continue;
        }

        // Add new node and edge
        int newId = g.addNode(UT_Vector3F(newX, 0, newZ));
        g.addEdge(tip.nodeId, newId, edgeWidth);
        addedGeometry = true;

        // Re-add tip with updated position and heading
        myTips.push_back({ newId, heading, tip.tier });

        // Branching
        if (tp.branchTier >= 0 && tp.branchChance > 0.0f)
        {
            float branchRand = (float)rand() / (float)RAND_MAX;
            if (branchRand < tp.branchChance)
            {
                // Count tips of branchTier
                int tierCount = 0;
                for (const auto& t : myTips)
                    if (t.tier == tp.branchTier) tierCount++;

                if (tierCount < tierParams[tp.branchTier].maxTips)
                {
                    float branchHeadingRand = (float)rand() / (float)RAND_MAX;
                    float branchHeading = heading + (float)M_PI * 0.5f
                                          + (branchHeadingRand - 0.5f) * 0.4f;
                    myTips.push_back({ tip.nodeId, branchHeading, tp.branchTier });
                }
            }
        }
    }

    return addedGeometry;
}

// ─────────────────────────────────────────────────────────────────────────────
// RadialGrowthStrategy
// ─────────────────────────────────────────────────────────────────────────────
bool RadialGrowthStrategy::grow(SimulationState& state)
{
    RoadGraph& g = state.graph;

    // Tick 0: seed origin and initialize spoke node IDs
    if (g.nodeCount() == 0)
    {
        int origin = g.addNode(UT_Vector3F(0, 0, 0));
        mySpokeNodeIds.clear();
        mySpokeNodeIds.resize(mySpokesPerRing, origin);
        myCurrentRing = 0;
        return true;
    }

    float snapDist = myRingSpacing * 0.4f;

    // Even ticks (after tick 0): extend spokes outward
    if (state.tick % 2 == 0)
    {
        bool addedAny = false;
        for (int i = 0; i < mySpokesPerRing; ++i)
        {
            float angle = (float)i * (2.0f * (float)M_PI / (float)mySpokesPerRing);
            float r     = myRingSpacing * (float)(myCurrentRing + 1);
            UT_Vector3F candidate(std::cos(angle) * r, 0, std::sin(angle) * r);

            // Snap check
            bool tooClose = false;
            for (const auto& n : g.nodes())
            {
                float dx = n.position.x() - candidate.x();
                float dz = n.position.z() - candidate.z();
                if (std::sqrt(dx * dx + dz * dz) < snapDist)
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) continue;

            int newId = g.addNode(candidate);
            g.addEdge(mySpokeNodeIds[i], newId);
            mySpokeNodeIds[i] = newId;
            addedAny = true;
        }
        return addedAny;
    }
    else
    {
        // Odd ticks: connect ring between adjacent spoke tips
        for (int i = 0; i < mySpokesPerRing; ++i)
        {
            int a = mySpokeNodeIds[i];
            int b = mySpokeNodeIds[(i + 1) % mySpokesPerRing];

            // Check if edge already exists
            bool exists = false;
            for (const auto& e : g.edges())
            {
                if ((e.fromNode == a && e.toNode == b) ||
                    (e.fromNode == b && e.toNode == a))
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                g.addEdge(a, b);
            }
        }
        myCurrentRing++;
        return true;
    }
}
