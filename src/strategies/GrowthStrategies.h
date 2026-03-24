#pragma once
#include "IGrowthStrategy.h"
#include "../core/TensorField.h"

// ─────────────────────────────────────────────────────────────────────────────
// GridGrowthStrategy — Manhattan-style axis-aligned grid expansion
// Each tick extends the outermost frontier nodes by one block unit along X or Z
// ─────────────────────────────────────────────────────────────────────────────
class GridGrowthStrategy : public IGrowthStrategy
{
public:
    explicit GridGrowthStrategy(float blockSize = 5.0f)
        : myBlockSize(blockSize) {}

    bool        grow(SimulationState& state) override;
    const char* name() const override { return "grid"; }

private:
    float myBlockSize;
    std::vector<int> myFrontier;
};


// ─────────────────────────────────────────────────────────────────────────────
// OrganicGrowthStrategy — L-system inspired branching with angle jitter
// Produces irregular, naturally curved road networks
// ─────────────────────────────────────────────────────────────────────────────
class OrganicGrowthStrategy : public IGrowthStrategy
{
public:
    explicit OrganicGrowthStrategy(float segmentLen   = 4.0f,
                                    float angleJitter  = 0.3f,
                                    float radialWeight = 0.4f,
                                    float riverX       = 25.0f)
        : mySegmentLen(segmentLen)
        , myAngleJitter(angleJitter)
    {
        myField.radialWeight = radialWeight;
        myField.riverX       = riverX;
    }

    bool        grow(SimulationState& state) override;
    const char* name() const override { return "organic"; }

private:
    float mySegmentLen;
    float myAngleJitter;

    struct GrowthTip {
        int   nodeId;
        float heading;
        int   tier;      // 0=arterial, 1=collector, 2=local
    };
    std::vector<GrowthTip> myTips;
    TensorField            myField;
};


// ─────────────────────────────────────────────────────────────────────────────
// RadialGrowthStrategy — spoke-and-ring pattern radiating from the seed
// Alternates between adding spokes outward and ring roads connecting them
// ─────────────────────────────────────────────────────────────────────────────
class RadialGrowthStrategy : public IGrowthStrategy
{
public:
    explicit RadialGrowthStrategy(int spokesPerRing = 8, float ringSpacing = 6.0f)
        : mySpokesPerRing(spokesPerRing), myRingSpacing(ringSpacing) {}

    bool        grow(SimulationState& state) override;
    const char* name() const override { return "radial"; }

private:
    int   mySpokesPerRing;
    float myRingSpacing;
    int myCurrentRing = 0;
    std::vector<int> mySpokeNodeIds;
};
