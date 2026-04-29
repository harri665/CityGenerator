#include "RK4Integrator.h"
#include "StreamlineTensorField.h"
#include <cmath>

UT_Vector3F RK4Integrator::getDirection(const UT_Vector3F& pos, bool major) const {
    const float angle = major ? field.majorAngle(pos.x(), pos.z()) : field.minorAngle(pos.x(), pos.z());
    const float dx = std::cos(angle);
    const float dz = std::sin(angle);
    return UT_Vector3F(dx, 0, dz);
}

UT_Vector3F RK4Integrator::step(const UT_Vector3F& pos, const UT_Vector3F& prevDir, bool major) const {
    // RK4 integration through the tensor field
    const UT_Vector3F k1 = getDirection(pos, major);
    const UT_Vector3F k2 = getDirection(pos + k1 * (dstep * 0.5f), major);
    const UT_Vector3F k3 = getDirection(pos + k2 * (dstep * 0.5f), major);
    const UT_Vector3F k4 = getDirection(pos + k3 * dstep, major);

    const UT_Vector3F direction = (k1 + k2 * 2.0f + k3 * 2.0f + k4) * (dstep / 6.0f);
    UT_Vector3F nextPos = pos + direction;

    // Flip direction if makes > 90° turn
    const float dot = direction.dot(prevDir);
    if (dot < 0 && prevDir.length() > 0.001f) {
        nextPos = pos - direction;
    }

    return nextPos;
}
