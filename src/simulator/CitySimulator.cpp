#include "CitySimulator.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// ─────────────────────────────────────────────────────────────────────────────
// Sutherland-Hodgman polygon clipping against a single line in XZ plane.
// The line is defined by a point on the line and a normal vector. Points on
// the side where dot(pos - linePoint, normal) >= 0 are "inside".
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<UT_Vector3F> clipPolygonByLine(
    const std::vector<UT_Vector3F>& poly,
    const UT_Vector3F& linePoint,
    const UT_Vector3F& normal)
{
    if (poly.empty()) return {};

    std::vector<UT_Vector3F> output;
    int n = (int)poly.size();

    for (int i = 0; i < n; ++i)
    {
        const UT_Vector3F& cur  = poly[i];
        const UT_Vector3F& prev = poly[(i + n - 1) % n];

        float dCur  = (cur.x()  - linePoint.x()) * normal.x()
                    + (cur.z()  - linePoint.z()) * normal.z();
        float dPrev = (prev.x() - linePoint.x()) * normal.x()
                    + (prev.z() - linePoint.z()) * normal.z();

        bool curInside  = dCur  >= 0.0f;
        bool prevInside = dPrev >= 0.0f;

        if (curInside != prevInside)
        {
            // Intersection
            float t = dPrev / (dPrev - dCur);
            UT_Vector3F inter;
            inter.x() = prev.x() + t * (cur.x() - prev.x());
            inter.y() = 0.0f;
            inter.z() = prev.z() + t * (cur.z() - prev.z());
            output.push_back(inter);
        }

        if (curInside)
        {
            output.push_back(cur);
        }
    }

    return output;
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildBlocks — planar face extraction via half-edge traversal, then
// parcel subdivision via Sutherland-Hodgman clipping
// ─────────────────────────────────────────────────────────────────────────────
void CitySimulator::rebuildBlocks()
{
    myState.blocks.clear();

    const RoadGraph& g = myState.graph;
    if (g.edgeCount() == 0) return;

    // ── Phase 1: Planar face extraction (half-edge traversal) ───────────

    // Build directed half-edge structures
    struct HalfEdge {
        int from, to;
    };

    std::vector<HalfEdge> halfEdges;
    for (const auto& e : g.edges())
    {
        halfEdges.push_back({ e.fromNode, e.toNode });
        halfEdges.push_back({ e.toNode, e.fromNode });
    }

    // Build outgoing half-edge index: node -> list of half-edge indices
    std::unordered_map<int, std::vector<int>> outgoing;
    for (int i = 0; i < (int)halfEdges.size(); ++i)
    {
        outgoing[halfEdges[i].from].push_back(i);
    }

    // For each half-edge, find "next": at node 'to', pick the outgoing
    // half-edge with the smallest clockwise turn from the incoming direction.
    std::vector<int> next(halfEdges.size(), -1);

    for (int i = 0; i < (int)halfEdges.size(); ++i)
    {
        int fromId = halfEdges[i].from;
        int toId   = halfEdges[i].to;

        const RoadNode* fromNode = g.node(fromId);
        const RoadNode* toNode   = g.node(toId);
        if (!fromNode || !toNode) continue;

        // Incoming direction vector
        float dxIn = toNode->position.x() - fromNode->position.x();
        float dzIn = toNode->position.z() - fromNode->position.z();
        float inAngle = std::atan2(dzIn, dxIn);

        const auto& candidates = outgoing[toId];
        int bestIdx = -1;
        float bestAngle = 1e9f;

        for (int cIdx : candidates)
        {
            int candTo = halfEdges[cIdx].to;
            if (candTo == fromId) continue; // skip U-turn

            const RoadNode* candNode = g.node(candTo);
            if (!candNode) continue;

            float dxOut = candNode->position.x() - toNode->position.x();
            float dzOut = candNode->position.z() - toNode->position.z();
            float outAngle = std::atan2(dzOut, dxOut);

            // Signed angle from incoming to outgoing (clockwise = negative)
            float diff = outAngle - inAngle;
            // Normalize to (-PI, PI]
            while (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
            while (diff <= -(float)M_PI) diff += 2.0f * (float)M_PI;

            // We want the most clockwise turn (most negative angle)
            // which corresponds to the smallest value of diff
            if (bestIdx < 0 || diff < bestAngle)
            {
                bestAngle = diff;
                bestIdx   = cIdx;
            }
        }

        next[i] = bestIdx;
    }

    // Traverse chains to extract face loops
    std::vector<bool> visited(halfEdges.size(), false);
    std::vector<CityBlock> rawBlocks;
    int blockId = 0;

    for (int startHe = 0; startHe < (int)halfEdges.size(); ++startHe)
    {
        if (visited[startHe]) continue;
        if (next[startHe] < 0) continue;

        // Trace the face
        std::vector<int> chain;
        int cur = startHe;
        bool valid = true;

        while (true)
        {
            if (cur < 0) { valid = false; break; }
            if (visited[cur])
            {
                // Check if we closed back to start
                if (cur == startHe) break;
                valid = false;
                break;
            }

            visited[cur] = true;
            chain.push_back(cur);

            if ((int)chain.size() > 64) { valid = false; break; }

            cur = next[cur];
        }

        if (!valid || (int)chain.size() < 3) continue;

        // Extract node positions for the face
        std::vector<UT_Vector3F> boundary;
        for (int heIdx : chain)
        {
            const RoadNode* n = g.node(halfEdges[heIdx].from);
            if (n) boundary.push_back(n->position);
        }

        if ((int)boundary.size() < 3) continue;

        // Compute signed area (shoelace in XZ)
        float signedArea = 0.0f;
        int bn = (int)boundary.size();
        for (int i = 0; i < bn; ++i)
        {
            const auto& p0 = boundary[i];
            const auto& p1 = boundary[(i + 1) % bn];
            signedArea += (p0.x() * p1.z()) - (p1.x() * p0.z());
        }
        signedArea *= 0.5f;

        // Skip outer face (positive area) and degenerate faces
        if (signedArea >= 0.0f) continue;
        if (std::abs(signedArea) < 1.0f) continue;

        CityBlock block;
        block.id = blockId++;
        block.boundary = boundary;
        block.computeArea();
        block.assignZone(UT_Vector3F(0, 0, 0), myState.commercialRadius);
        rawBlocks.push_back(std::move(block));
    }

    // ── Phase 2: Parcel subdivision ─────────────────────────────────────

    int parcelId = 0;

    for (const auto& block : rawBlocks)
    {
        // Parks: no subdivision, keep whole block
        if (block.zone == ZoneType::Park)
        {
            CityBlock parcel = block;
            parcel.id = parcelId++;
            myState.blocks.push_back(std::move(parcel));
            continue;
        }

        // Determine lot width by zone
        float lotWidth = 4.0f; // Residential default
        if (block.zone == ZoneType::Commercial)  lotWidth = 8.0f;
        if (block.zone == ZoneType::Industrial)  lotWidth = 12.0f;

        // Find the longest edge to determine primary axis
        int bn = (int)block.boundary.size();
        float bestLen = 0.0f;
        UT_Vector3F primaryAxis(1, 0, 0);

        for (int i = 0; i < bn; ++i)
        {
            UT_Vector3F diff = block.boundary[(i + 1) % bn] - block.boundary[i];
            float len = std::sqrt(diff.x() * diff.x() + diff.z() * diff.z());
            if (len > bestLen)
            {
                bestLen = len;
                primaryAxis = UT_Vector3F(diff.x(), 0, diff.z());
            }
        }

        // Normalize primary axis
        float axisLen = std::sqrt(primaryAxis.x() * primaryAxis.x()
                                + primaryAxis.z() * primaryAxis.z());
        if (axisLen < 1e-6f)
        {
            // Degenerate — keep as-is
            CityBlock parcel = block;
            parcel.id = parcelId++;
            myState.blocks.push_back(std::move(parcel));
            continue;
        }
        primaryAxis.x() /= axisLen;
        primaryAxis.z() /= axisLen;

        // Normal perpendicular to primary axis (in XZ plane)
        UT_Vector3F clipNormal(-primaryAxis.z(), 0, primaryAxis.x());

        // Project boundary onto primary axis to get span
        float minProj =  1e18f;
        float maxProj = -1e18f;
        for (const auto& p : block.boundary)
        {
            float proj = p.x() * primaryAxis.x() + p.z() * primaryAxis.z();
            minProj = std::min(minProj, proj);
            maxProj = std::max(maxProj, proj);
        }

        float span = maxProj - minProj;
        int numLots = std::max(1, (int)std::floor(span / lotWidth));

        if (numLots <= 1)
        {
            // Too small to subdivide
            CityBlock parcel = block;
            parcel.id = parcelId++;
            myState.blocks.push_back(std::move(parcel));
            continue;
        }

        float actualLotWidth = span / (float)numLots;

        // Clip block polygon into lots using Sutherland-Hodgman
        for (int lot = 0; lot < numLots; ++lot)
        {
            float lotMin = minProj + lot * actualLotWidth;
            float lotMax = lotMin + actualLotWidth;

            // Clip against "min" plane: keep points where proj >= lotMin
            UT_Vector3F linePointMin(primaryAxis.x() * lotMin, 0,
                                     primaryAxis.z() * lotMin);
            // Normal points in the direction of increasing projection
            std::vector<UT_Vector3F> clipped = clipPolygonByLine(
                block.boundary, linePointMin, primaryAxis);

            // Clip against "max" plane: keep points where proj <= lotMax
            UT_Vector3F linePointMax(primaryAxis.x() * lotMax, 0,
                                     primaryAxis.z() * lotMax);
            // Normal points in the direction of decreasing projection
            UT_Vector3F negAxis(-primaryAxis.x(), 0, -primaryAxis.z());
            clipped = clipPolygonByLine(clipped, linePointMax, negAxis);

            if ((int)clipped.size() < 3) continue;

            CityBlock parcel;
            parcel.id = parcelId++;
            parcel.boundary = clipped;
            parcel.computeArea();
            parcel.assignZone(UT_Vector3F(0, 0, 0), myState.commercialRadius);

            // Discard tiny parcels
            if (parcel.area < 2.0f) continue;

            myState.blocks.push_back(std::move(parcel));
        }
    }
}
