#pragma once
#include "Tensor.h"
#include <UT/UT_Vector3.h>
#include <cmath>

struct StreamlineTensorField;  // forward decl

struct RK4Integrator {
    const StreamlineTensorField& field;
    float dstep;

    RK4Integrator(const StreamlineTensorField& f, float step) : field(f), dstep(step) {}

    UT_Vector3F step(const UT_Vector3F& pos, const UT_Vector3F& prevDir, bool major) const;

private:
    UT_Vector3F getDirection(const UT_Vector3F& pos, bool major) const;
};
