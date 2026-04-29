#pragma once
#include "Tensor.h"
#include <UT/UT_Vector3.h>

struct BasisField {
    UT_Vector3F center;
    float size;
    float decay;

    BasisField() : center(0, 0, 0), size(100.0f), decay(1.0f) {}
    BasisField(UT_Vector3F c, float s, float d) : center(c), size(s), decay(d) {}

    virtual ~BasisField() = default;
    virtual Tensor getWeightedTensor(float x, float z) const = 0;

protected:
    float weight(float x, float z) const {
        const float dx = x - center.x();
        const float dz = z - center.z();
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > size) return 0.0f;
        const float t = 1.0f - (dist / size);
        return std::pow(t, decay);
    }
};

struct GridBasisField : public BasisField {
    float theta;

    GridBasisField() : BasisField(), theta(0.0f) {}
    GridBasisField(UT_Vector3F c, float s, float d, float t)
        : BasisField(c, s, d), theta(t) {}

    Tensor getWeightedTensor(float x, float z) const override {
        const float w = weight(x, z);
        return Tensor::fromAngle(theta, w);
    }
};

struct RadialBasisField : public BasisField {
    RadialBasisField() : BasisField() {}
    RadialBasisField(UT_Vector3F c, float s, float d) : BasisField(c, s, d) {}

    Tensor getWeightedTensor(float x, float z) const override {
        const float dx = x - center.x();
        const float dz = z - center.z();
        const float angle = std::atan2(dz, dx);
        const float w = weight(x, z);
        return Tensor::fromAngle(angle, w);
    }
};
