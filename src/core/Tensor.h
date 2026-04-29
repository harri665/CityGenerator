#pragma once
#include <cmath>

struct Tensor {
    float a = 0, b = 0;  // a = R*cos(2θ), b = R*sin(2θ)

    Tensor() = default;
    Tensor(float a_, float b_) : a(a_), b(b_) {}

    Tensor operator+(const Tensor& o) const { return Tensor(a + o.a, b + o.b); }
    Tensor operator*(float s) const { return Tensor(a * s, b * s); }
    Tensor scaled(float s) const { return Tensor(a * s, b * s); }

    float majorAngle() const { return std::atan2(b, a) * 0.5f; }
    float minorAngle() const { return majorAngle() + M_PI * 0.5f; }

    static Tensor fromAngle(float theta, float r = 1.0f) {
        const float twoTheta = theta * 2.0f;
        return Tensor(r * std::cos(twoTheta), r * std::sin(twoTheta));
    }

    static Tensor zero() { return Tensor(0, 0); }
};
