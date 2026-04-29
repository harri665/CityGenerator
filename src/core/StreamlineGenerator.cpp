#include "StreamlineGenerator.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <tuple>
#include <limits>

StreamlineGenerator::StreamlineGenerator(StreamlineTensorField& field, const StreamlineParams& params)
    : myField(field)
    , myParams(params)
    , myIntegrator(field, params.dstep)
    , myGridMajor(params.dsep)
    , myGridMinor(params.dsep)
{
    // Match MapGenerator: dtest cannot exceed dsep
    if (myParams.dtest > myParams.dsep) myParams.dtest = myParams.dsep;
}

void StreamlineGenerator::generate(RoadGraph& graph) {
    myMajorStreamlines.clear();
    myMinorStreamlines.clear();
    myGridMajor.clear();
    myGridMinor.clear();
    myCandidateSeeds.clear();

    std::mt19937 rng(myParams.seed);
    std::uniform_real_distribution<float> dist(-myParams.worldSize / 2.0f, myParams.worldSize / 2.0f);

    for (int i = 0; i < myParams.seedTries * 2; ++i) {
        myCandidateSeeds.push_back(UT_Vector3F(dist(rng), 0, dist(rng)));
    }

    generateRoads(true);
    generateRoads(false);

    // Bridge dangling endpoints to nearby roads — this is what makes the
    // network actually connected. Runs after all tracing is done.
    joinDanglingStreamlines();

    buildGraphFromStreamlines(graph);
}

void StreamlineGenerator::generateRoads(bool major) {
    int seedsTried = 0;
    while (!myCandidateSeeds.empty() && seedsTried < myParams.seedTries) {
        const UT_Vector3F seed = myCandidateSeeds.front();
        myCandidateSeeds.pop_front();
        ++seedsTried;

        if (!isValidSeed(seed, major)) continue;

        const auto streamline = traceStreamline(seed, major);
        if (streamline.size() < 2) continue;

        if (major) {
            myMajorStreamlines.push_back(streamline);
            for (const auto& p : streamline) myGridMajor.addPoint(p);
        } else {
            myMinorStreamlines.push_back(streamline);
            for (const auto& p : streamline) myGridMinor.addPoint(p);
        }

        addSeedsFromStreamline(streamline, major);
    }
}

// Trace forward + backward from seed.
// Termination uses dtest (smaller than dsep) and pushes the failing point
// onto the streamline before stopping — joinDanglingStreamlines will then
// bridge any remaining gap.
std::vector<UT_Vector3F> StreamlineGenerator::traceStreamline(const UT_Vector3F& seed, bool major) {
    auto traceOneSide = [&](const UT_Vector3F& startDir) {
        std::vector<UT_Vector3F> out;
        UT_Vector3F pos = seed;
        UT_Vector3F prevDir = startDir;

        for (int iter = 0; iter < myParams.pathIterations; ++iter) {
            const UT_Vector3F nextPos = myIntegrator.step(pos, prevDir, major);

            const bool inBounds =
                myField.onLand(nextPos.x(), nextPos.z())
                && std::abs(nextPos.x()) < myParams.worldSize / 2.0f
                && std::abs(nextPos.z()) < myParams.worldSize / 2.0f;

            const GridStorage& sameTier = major ? myGridMajor : myGridMinor;
            bool collides = sameTier.isTooClose(nextPos, myParams.dtest);
            if (!major) collides = collides || myGridMajor.isTooClose(nextPos, myParams.dtest);

            if (!inBounds || collides) {
                // Push the failing point so the endpoint sits ~dtest from existing road
                if (inBounds) out.push_back(nextPos);
                break;
            }

            out.push_back(nextPos);
            UT_Vector3F dir = nextPos - pos;
            if (dir.length() > 1e-6f) prevDir = dir.normalize();
            pos = nextPos;
        }
        return out;
    };

    std::vector<UT_Vector3F> forward  = traceOneSide(UT_Vector3F( 1, 0, 0));
    std::vector<UT_Vector3F> backward = traceOneSide(UT_Vector3F(-1, 0, 0));

    std::vector<UT_Vector3F> result;
    result.reserve(backward.size() + 1 + forward.size());
    for (auto it = backward.rbegin(); it != backward.rend(); ++it) result.push_back(*it);
    result.push_back(seed);
    for (const auto& p : forward) result.push_back(p);

    return result;
}

void StreamlineGenerator::addSeedsFromStreamline(const std::vector<UT_Vector3F>& streamline, bool major) {
    if (streamline.empty()) return;
    const float angle = major ? myField.majorAngle(streamline[0].x(), streamline[0].z())
                              : myField.minorAngle(streamline[0].x(), streamline[0].z());
    const float perpAngle = angle + (float)M_PI * 0.5f;
    const float perpX = std::cos(perpAngle);
    const float perpZ = std::sin(perpAngle);

    for (int i = 0; i < (int)streamline.size(); i += 2) {
        const auto& pt = streamline[i];
        myCandidateSeeds.push_back(pt + UT_Vector3F( perpX * myParams.dsep, 0,  perpZ * myParams.dsep));
        myCandidateSeeds.push_back(pt + UT_Vector3F(-perpX * myParams.dsep, 0, -perpZ * myParams.dsep));
    }
}

bool StreamlineGenerator::isValidSeed(const UT_Vector3F& seed, bool major) {
    if (!myField.onLand(seed.x(), seed.z())) return false;
    if (std::abs(seed.x()) > myParams.worldSize / 2.0f) return false;
    if (std::abs(seed.z()) > myParams.worldSize / 2.0f) return false;

    const GridStorage& sameTier = major ? myGridMajor : myGridMinor;
    if (sameTier.isTooClose(seed, myParams.dsep)) return false;
    if (!major && myGridMajor.isTooClose(seed, myParams.dtest)) return false;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// joinDanglingStreamlines — port of MapGenerator's algorithm.
// For each streamline, search both endpoints for nearby road points and bridge
// the gap with intermediate points. This is what makes the road network
// actually connected.
// ─────────────────────────────────────────────────────────────────────────────
void StreamlineGenerator::joinDanglingStreamlines() {
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = (pass == 0);
        auto& streams = major ? myMajorStreamlines : myMinorStreamlines;
        GridStorage& grid = major ? myGridMajor : myGridMinor;

        for (auto& sl : streams) {
            if ((int)sl.size() < 5) continue;

            // Extend the start endpoint
            UT_Vector3F newStart;
            if (getBestNextPoint(sl.front(), sl[4], newStart)) {
                const auto bridge = pointsBetween(sl.front(), newStart, myParams.dstep);
                for (const auto& p : bridge) {
                    sl.insert(sl.begin(), p);
                    grid.addPoint(p);
                }
            }

            // Extend the end endpoint
            const size_t n = sl.size();
            if (n >= 5) {
                UT_Vector3F newEnd;
                if (getBestNextPoint(sl[n - 1], sl[n - 5], newEnd)) {
                    const auto bridge = pointsBetween(sl.back(), newEnd, myParams.dstep);
                    for (const auto& p : bridge) {
                        sl.push_back(p);
                        grid.addPoint(p);
                    }
                }
            }
        }
    }
}

bool StreamlineGenerator::getBestNextPoint(const UT_Vector3F& point,
                                            const UT_Vector3F& previousPoint,
                                            UT_Vector3F& out) const {
    std::vector<UT_Vector3F> nearby;
    myGridMajor.getNearbyPoints(point, myParams.dlookahead, nearby);
    myGridMinor.getNearbyPoints(point, myParams.dlookahead, nearby);

    UT_Vector3F direction = point - previousPoint;
    const float dirLen = direction.length();
    if (dirLen < 1e-6f) return false;

    const float dstep2 = myParams.dstep * myParams.dstep;
    const float invDirLen = 1.0f / dirLen;

    bool found = false;
    float closestDist2 = std::numeric_limits<float>::infinity();
    UT_Vector3F closest;

    for (const auto& sample : nearby) {
        const UT_Vector3F diff = sample - point;
        const float diffLen2 = diff.length2();
        if (diffLen2 < 1e-8f) continue;
        if ((sample - previousPoint).length2() < 1e-8f) continue;

        // Must be in the forward direction
        const float dot = diff.x() * direction.x() + diff.z() * direction.z();
        if (dot < 0) continue;

        // Very close → accept immediately
        if (diffLen2 < 2.0f * dstep2) {
            closest = sample;
            found = true;
            closestDist2 = diffLen2;
            break;
        }

        // Within join-angle cone
        const float diffLen = std::sqrt(diffLen2);
        const float cosA = dot * invDirLen / diffLen;
        const float clamped = std::max(-1.0f, std::min(1.0f, cosA));
        const float angle = std::acos(clamped);

        if (angle < myParams.joinAngle && diffLen2 < closestDist2) {
            closestDist2 = diffLen2;
            closest = sample;
            found = true;
        }
    }

    if (found) out = closest;
    return found;
}

std::vector<UT_Vector3F> StreamlineGenerator::pointsBetween(const UT_Vector3F& v1,
                                                              const UT_Vector3F& v2,
                                                              float dstep) const {
    const UT_Vector3F dv = v2 - v1;
    const float dist = dv.length();
    const int n = (int)std::floor(dist / dstep);
    if (n <= 0) return {};

    std::vector<UT_Vector3F> out;
    out.reserve(n);
    for (int i = 1; i <= n; ++i) {
        const float t = (float)i / (float)n;
        const UT_Vector3F p = v1 + dv * t;
        if (!myField.onLand(p.x(), p.z())) break;
        out.push_back(p);
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildGraphFromStreamlines — convert all streamlines into a planar RoadGraph.
// All-pairs segment intersection in XZ inserts a shared node at every crossing.
// ─────────────────────────────────────────────────────────────────────────────
void StreamlineGenerator::buildGraphFromStreamlines(RoadGraph& graph) {
    graph.clear();

    struct StreamRef { const std::vector<UT_Vector3F>* poly; bool major; };
    std::vector<StreamRef> streams;
    streams.reserve(myMajorStreamlines.size() + myMinorStreamlines.size());
    for (const auto& s : myMajorStreamlines) streams.push_back({&s, true});
    for (const auto& s : myMinorStreamlines) streams.push_back({&s, false});

    using Split = std::tuple<int, float, UT_Vector3F>; // segment index, t in segment, position
    std::vector<std::vector<Split>> splits(streams.size());

    for (size_t a = 0; a < streams.size(); ++a) {
        const auto& A = *streams[a].poly;
        if (A.size() < 2) continue;
        for (size_t b = a + 1; b < streams.size(); ++b) {
            const auto& B = *streams[b].poly;
            if (B.size() < 2) continue;

            for (size_t i = 0; i + 1 < A.size(); ++i) {
                const float p1x = A[i].x(),     p1z = A[i].z();
                const float rx  = A[i+1].x() - p1x;
                const float rz  = A[i+1].z() - p1z;

                for (size_t j = 0; j + 1 < B.size(); ++j) {
                    const float p3x = B[j].x(),     p3z = B[j].z();
                    const float sx  = B[j+1].x() - p3x;
                    const float sz  = B[j+1].z() - p3z;

                    const float denom = rx * sz - rz * sx;
                    if (std::abs(denom) < 1e-9f) continue;

                    const float qx = p3x - p1x;
                    const float qz = p3z - p1z;
                    const float t = (qx * sz - qz * sx) / denom;
                    const float u = (qx * rz - qz * rx) / denom;
                    if (t < 0.f || t > 1.f || u < 0.f || u > 1.f) continue;

                    UT_Vector3F ix(p1x + t * rx, 0.f, p1z + t * rz);
                    splits[a].emplace_back((int)i, t, ix);
                    splits[b].emplace_back((int)j, u, ix);
                }
            }
        }
    }

    for (auto& sp : splits) {
        std::sort(sp.begin(), sp.end(), [](const Split& a, const Split& b) {
            const int sa = std::get<0>(a), sb = std::get<0>(b);
            if (sa != sb) return sa < sb;
            return std::get<1>(a) < std::get<1>(b);
        });
    }

    const float snap  = std::max(0.05f, myParams.dstep * 0.5f);
    const float snap2 = snap * snap;

    auto findOrAdd = [&](const UT_Vector3F& pt) -> int {
        for (const auto& n : graph.nodes()) {
            const float dx = n.position.x() - pt.x();
            const float dz = n.position.z() - pt.z();
            if (dx * dx + dz * dz < snap2) return n.id;
        }
        return graph.addNode(pt);
    };

    for (size_t s = 0; s < streams.size(); ++s) {
        const auto& poly = *streams[s].poly;
        if (poly.size() < 2) continue;

        const float width = streams[s].major ? 3.0f : 1.0f;
        const auto& sp = splits[s];

        int prev = findOrAdd(poly[0]);
        size_t spIdx = 0;

        for (size_t i = 0; i + 1 < poly.size(); ++i) {
            while (spIdx < sp.size() && std::get<0>(sp[spIdx]) == (int)i) {
                int n = findOrAdd(std::get<2>(sp[spIdx]));
                if (n != prev) graph.addEdge(prev, n, width);
                prev = n;
                ++spIdx;
            }
            int n = findOrAdd(poly[i + 1]);
            if (n != prev) graph.addEdge(prev, n, width);
            prev = n;
        }
    }
}
