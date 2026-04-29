#include "BlockExtraction.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Sutherland-Hodgman polygon clipping against a single line in the XZ plane.
// The line is defined by a point on the line and a normal vector. Points
// where dot(pos - linePoint, normal) >= 0 are "inside".
static std::vector<UT_Vector3F> clipPolygonByLine(
    const std::vector<UT_Vector3F>& poly,
    const UT_Vector3F& linePoint,
    const UT_Vector3F& normal)
{
    if (poly.empty()) return {};

    std::vector<UT_Vector3F> output = poly;

    for (size_t i = 0; i < poly.size(); ++i) {
        if (output.empty()) break;

        std::vector<UT_Vector3F> input = output;
        output.clear();

        const UT_Vector3F& A0 = input[input.size() - 1];
        UT_Vector3F A = A0;
        for (size_t j = 0; j < input.size(); ++j) {
            const UT_Vector3F& B = input[j];

            const float dA = (A - linePoint).dot(normal);
            const float dB = (B - linePoint).dot(normal);

            const bool A_inside = dA >= -0.001f;
            const bool B_inside = dB >= -0.001f;

            if (B_inside) {
                if (!A_inside && std::abs(dB - dA) > 0.001f) {
                    const float t = dA / (dA - dB);
                    output.push_back(A + (B - A) * t);
                }
                output.push_back(B);
            } else if (A_inside && std::abs(dB - dA) > 0.001f) {
                const float t = dA / (dA - dB);
                output.push_back(A + (B - A) * t);
            }

            A = B;
        }
    }

    return output;
}

namespace BlockExtraction
{

std::vector<CityBlock> rebuild(const RoadGraph& g,
                                float commercialRadius,
                                const UT_Vector3F& cityCenter)
{
    std::vector<CityBlock> blocks;
    if (g.nodeCount() < 3 || g.edgeCount() < 3) return blocks;

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 1: face extraction by half-edge traversal in the XZ plane.
    // ─────────────────────────────────────────────────────────────────────────
    struct Out { int to; float angle; };
    std::unordered_map<int, std::vector<Out>> adj;

    auto angleXZ = [](const UT_Vector3F& a, const UT_Vector3F& b) {
        float ang = std::atan2(b.z() - a.z(), b.x() - a.x());
        if (ang < 0.0f) ang += 2.0f * (float)M_PI;
        return ang;
    };

    for (const auto& e : g.edges()) {
        const RoadNode* a = g.node(e.fromNode);
        const RoadNode* b = g.node(e.toNode);
        if (!a || !b) continue;
        adj[a->id].push_back({b->id, angleXZ(a->position, b->position)});
        adj[b->id].push_back({a->id, angleXZ(b->position, a->position)});
    }

    for (auto& kv : adj) {
        std::sort(kv.second.begin(), kv.second.end(),
                  [](const Out& l, const Out& r){ return l.angle < r.angle; });
    }

    auto findOutIdx = [&](int from, int to) -> int {
        const auto& v = adj[from];
        for (size_t i = 0; i < v.size(); ++i) if (v[i].to == to) return (int)i;
        return -1;
    };

    auto edgeKey = [](int a, int b) -> uint64_t {
        return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b;
    };

    std::unordered_set<uint64_t> visited;
    std::vector<std::vector<UT_Vector3F>> rawBlocks;

    for (const auto& kv : adj) {
        const int startFrom = kv.first;
        for (const auto& oe : kv.second) {
            const uint64_t startKey = edgeKey(startFrom, oe.to);
            if (visited.count(startKey)) continue;

            std::vector<int> faceNodes;
            int curFrom = startFrom;
            int curTo   = oe.to;
            bool closed = false;

            for (int safety = 0; safety < 256; ++safety) {
                const uint64_t k = edgeKey(curFrom, curTo);
                if (!faceNodes.empty() && k == startKey) { closed = true; break; }
                if (visited.count(k)) break;
                visited.insert(k);
                faceNodes.push_back(curTo);

                const int idxBack = findOutIdx(curTo, curFrom);
                if (idxBack < 0) break;
                const auto& nbrs = adj[curTo];
                const int idxNext = (idxBack + 1) % (int)nbrs.size();
                const int nextTo = nbrs[idxNext].to;

                curFrom = curTo;
                curTo   = nextTo;
            }

            if (!closed || faceNodes.size() < 3) continue;

            std::vector<UT_Vector3F> poly;
            poly.reserve(faceNodes.size());
            bool ok = true;
            for (int n : faceNodes) {
                const RoadNode* node = g.node(n);
                if (!node) { ok = false; break; }
                poly.push_back(node->position);
            }
            if (ok && poly.size() >= 3) rawBlocks.push_back(std::move(poly));
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 2: Filter by signed area, assign zone, parcel-subdivide
    // ─────────────────────────────────────────────────────────────────────────
    for (const auto& poly : rawBlocks) {
        if (poly.size() < 3) continue;

        float signedArea = 0.0f;
        for (size_t i = 0; i < poly.size(); ++i) {
            const auto& p0 = poly[i];
            const auto& p1 = poly[(i + 1) % poly.size()];
            signedArea += p0.x() * p1.z() - p1.x() * p0.z();
        }
        signedArea *= 0.5f;

        if (signedArea > -1.0f) continue;     // outer face or too small
        const float absArea = -signedArea;

        CityBlock block;
        block.id = (int)blocks.size();
        block.boundary = poly;
        block.area = absArea;
        block.assignZone(cityCenter, commercialRadius);

        if (block.zone == ZoneType::Park) {
            blocks.push_back(block);
            continue;
        }

        float longestLen = 0.0f;
        int longestEdge = 0;
        for (size_t i = 0; i < poly.size(); ++i) {
            const auto& p0 = poly[i];
            const auto& p1 = poly[(i + 1) % poly.size()];
            const float dx = p1.x() - p0.x();
            const float dz = p1.z() - p0.z();
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len > longestLen) {
                longestLen = len;
                longestEdge = (int)i;
            }
        }

        const auto& p0 = poly[longestEdge];
        const auto& p1 = poly[(longestEdge + 1) % poly.size()];
        UT_Vector3F direction = p1 - p0;
        direction.normalize();
        UT_Vector3F primaryAxis = direction;
        UT_Vector3F secondaryAxis(-primaryAxis.z(), 0.0f, primaryAxis.x());

        float lotWidth = 4.0f;
        if (block.zone == ZoneType::Commercial) lotWidth = 8.0f;
        if (block.zone == ZoneType::Industrial) lotWidth = 12.0f;

        const UT_Vector3F bc = block.centroid() - cityCenter;
        const float distFromCenter = std::sqrt(bc.x() * bc.x() + bc.z() * bc.z());
        const float densityScale = 0.6f + 0.4f * std::min(
            1.0f, distFromCenter / (commercialRadius * 4.0f));
        lotWidth *= densityScale;

        std::vector<std::vector<UT_Vector3F>> strips;
        for (float i = 0; i < longestLen; i += lotWidth) {
            UT_Vector3F linePt = p0 + primaryAxis * i;
            std::vector<UT_Vector3F> strip = poly;
            strip = clipPolygonByLine(strip, linePt, primaryAxis);
            strip = clipPolygonByLine(strip, linePt + primaryAxis * lotWidth, -primaryAxis);
            if (strip.size() >= 3) strips.push_back(strip);
        }

        float depthRatio = 1.5f;
        if (block.zone == ZoneType::Commercial) depthRatio = 1.0f;
        if (block.zone == ZoneType::Industrial) depthRatio = 2.0f;
        const float depthLot = lotWidth * depthRatio;

        for (const auto& strip : strips) {
            for (float j = 0; j < 200.0f; j += depthLot) {
                std::vector<UT_Vector3F> parcel = strip;
                parcel = clipPolygonByLine(parcel, strip[0] + secondaryAxis * j, secondaryAxis);
                parcel = clipPolygonByLine(parcel, strip[0] + secondaryAxis * (j + depthLot), -secondaryAxis);

                if (parcel.size() >= 3) {
                    float parcArea = 0.0f;
                    for (size_t i = 0; i < parcel.size(); ++i) {
                        const auto& pa = parcel[i];
                        const auto& pb = parcel[(i + 1) % parcel.size()];
                        parcArea += pa.x() * pb.z() - pb.x() * pa.z();
                    }
                    parcArea = std::abs(parcArea * 0.5f);

                    if (parcArea >= 2.0f) {
                        CityBlock parc;
                        parc.id = (int)blocks.size();
                        parc.boundary = parcel;
                        parc.area = parcArea;
                        parc.assignZone(cityCenter, commercialRadius);
                        blocks.push_back(parc);
                    }
                }
            }
        }
    }

    return blocks;
}

} // namespace BlockExtraction
