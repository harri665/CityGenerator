#pragma once
#include <UT/UT_Vector3.h>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct GridStorage {
    float cellSize;
    std::unordered_map<uint64_t, std::vector<UT_Vector3F>> cells;

    GridStorage(float cellSize_ = 20.0f) : cellSize(cellSize_) {}

    void addPoint(float x, float z) {
        const UT_Vector3F pt(x, 0, z);
        addPoint(pt);
    }

    void addPoint(const UT_Vector3F& pt) {
        const uint64_t k = cellKey(pt.x(), pt.z());
        cells[k].push_back(pt);
    }

    bool isTooClose(float x, float z, float dist) const {
        return isTooClose(UT_Vector3F(x, 0, z), dist);
    }

    bool isTooClose(const UT_Vector3F& pt, float dist) const {
        const int32_t cx = static_cast<int32_t>(std::floor(pt.x() / cellSize));
        const int32_t cz = static_cast<int32_t>(std::floor(pt.z() / cellSize));
        const int reach = (int)std::ceil(dist / cellSize) + 1;
        const float d2 = dist * dist;

        for (int dx = -reach; dx <= reach; ++dx) {
            for (int dz = -reach; dz <= reach; ++dz) {
                const uint64_t k = cellKey(cx + dx, cz + dz);
                auto it = cells.find(k);
                if (it == cells.end()) continue;

                for (const auto& p : it->second) {
                    const float ex = p.x() - pt.x();
                    const float ez = p.z() - pt.z();
                    if (ex * ex + ez * ez < d2) return true;
                }
            }
        }
        return false;
    }

    void getNearbyPoints(const UT_Vector3F& pt, float maxDist,
                         std::vector<UT_Vector3F>& out) const {
        const int32_t cx = static_cast<int32_t>(std::floor(pt.x() / cellSize));
        const int32_t cz = static_cast<int32_t>(std::floor(pt.z() / cellSize));
        const int reach = (int)std::ceil(maxDist / cellSize) + 1;
        const float d2 = maxDist * maxDist;

        for (int dx = -reach; dx <= reach; ++dx) {
            for (int dz = -reach; dz <= reach; ++dz) {
                const uint64_t k = cellKey(cx + dx, cz + dz);
                auto it = cells.find(k);
                if (it == cells.end()) continue;

                for (const auto& p : it->second) {
                    const float ex = p.x() - pt.x();
                    const float ez = p.z() - pt.z();
                    if (ex * ex + ez * ez < d2) out.push_back(p);
                }
            }
        }
    }

    bool nearestPoint(const UT_Vector3F& pt, float maxDist, UT_Vector3F& out) const {
        const int32_t cx = static_cast<int32_t>(std::floor(pt.x() / cellSize));
        const int32_t cz = static_cast<int32_t>(std::floor(pt.z() / cellSize));
        const int reach = (int)std::ceil(maxDist / cellSize) + 1;
        float best = maxDist * maxDist;
        bool found = false;

        for (int dx = -reach; dx <= reach; ++dx) {
            for (int dz = -reach; dz <= reach; ++dz) {
                const uint64_t k = cellKey(cx + dx, cz + dz);
                auto it = cells.find(k);
                if (it == cells.end()) continue;

                for (const auto& p : it->second) {
                    const float ex = p.x() - pt.x();
                    const float ez = p.z() - pt.z();
                    const float d2 = ex * ex + ez * ez;
                    if (d2 < best) { best = d2; out = p; found = true; }
                }
            }
        }
        return found;
    }

    void clear() { cells.clear(); }

private:
    uint64_t cellKey(float x, float z) const {
        const int32_t ix = static_cast<int32_t>(std::floor(x / cellSize));
        const int32_t iz = static_cast<int32_t>(std::floor(z / cellSize));
        return cellKey(ix, iz);
    }

    static uint64_t cellKey(int32_t ix, int32_t iz) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(ix)) << 32)
             |  static_cast<uint64_t>(static_cast<uint32_t>(iz));
    }
};
