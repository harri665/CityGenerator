#pragma once
#include <UT/UT_Vector3.h>
#include <cmath>

// Samples a 2D tensor field at position (x, z) to return a preferred road
// heading angle in radians. Combines three influences:
//   1. FBM noise — gives organic curvature
//   2. Radial pull toward cityCenter — produces hub-and-spoke tendency
//   3. Water mask — sine-wave river running roughly along Z axis
struct TensorField
{
    UT_Vector3F cityCenter   = UT_Vector3F(0, 0, 0);
    float       radialWeight = 0.4f;   // 0 = pure noise, 1 = pure radial
    float       noiseScale   = 0.08f;  // spatial frequency of FBM
    float       riverWidth   = 6.0f;   // half-width of water exclusion zone
    float       riverX       = 25.0f;  // X position of river centerline

    // Returns preferred heading angle at (x, z)
    float sampleHeading(float x, float z, unsigned int seed) const;

    // Returns 0.0 in open land, 1.0 inside water — roads should not cross water
    float waterMask(float x, float z) const;

private:
    float fbm(float x, float z, int octaves, unsigned int seed) const;
    float smoothNoise(float x, float z, unsigned int seed) const;
};
