#pragma once
#include "../core/SimulationState.h"


class ISimObserver
{
public:
    virtual ~ISimObserver() = default;
    virtual void onStateChanged(const SimulationState& state) = 0;
};
