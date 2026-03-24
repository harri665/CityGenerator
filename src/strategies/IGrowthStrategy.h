#pragma once
#include "../core/SimulationState.h"

// ─────────────────────────────────────────────────────────────────────────────
// IGrowthStrategy — STRATEGY PATTERN
//
// Defines the contract for a single growth tick. Concrete strategies implement
// their own road-extension logic without the simulator knowing which one it has.
// New growth modes are added by subclassing, never by modifying CitySimulator.
// ─────────────────────────────────────────────────────────────────────────────
class IGrowthStrategy
{
public:
    virtual ~IGrowthStrategy() = default;

    // Advance the simulation by one tick, mutating state.graph in place.
    // Returns true if new geometry was added (false = fully grown / saturated).
    virtual bool grow(SimulationState& state) = 0;

    // Human-readable name used by StrategyFactory and HDA dropdown label
    virtual const char* name() const = 0;
};
