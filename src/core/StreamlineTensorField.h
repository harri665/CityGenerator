#pragma once
#include "Tensor.h"
#include "BasisField.h"
#include <vector>
#include <memory>
#include <cmath>

struct StreamlineTensorField {
    std::vector<std::unique_ptr<BasisField>> fields;
    float riverX = 25.0f;
    float riverWidth = 6.0f;

    StreamlineTensorField() = default;

    void addField(std::unique_ptr<BasisField> field) {
        fields.push_back(std::move(field));
    }

    Tensor sample(float x, float z) const {
        Tensor result = Tensor::zero();
        for (const auto& f : fields) {
            result = result + f->getWeightedTensor(x, z);
        }
        return result;
    }

    float majorAngle(float x, float z) const {
        return sample(x, z).majorAngle();
    }

    float minorAngle(float x, float z) const {
        return sample(x, z).minorAngle();
    }

    bool onLand(float x, float z) const {
        // Simple river check: north-south band
        const float dx = x - riverX;
        return std::abs(dx) > riverWidth;
    }
};
