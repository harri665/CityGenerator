#pragma once
#include "../core/SimulationState.h"
#include "../strategies/IGrowthStrategy.h"
#include "../observers/ISimObserver.h"
#include "../persistence/Persistence.h"
#include <memory>
#include <vector>
#include <string>

// CitySimulator — the central simulation class
//
// DEPENDENCY INJECTION: all three collaborators are injected via the
// constructor. CitySimulator owns the growth strategy and persistence handler
// (unique_ptr) and holds non-owning pointers to observers. It never
// instantiates a concrete strategy, persistence, or observer itself.
//
// POLYMORPHISM: grow(), save(), load(), and onStateChanged() are all called
// through abstract interfaces — no if/else on type anywhere in this class.
//
// OBSERVER: after every mutation, notifyObservers() fires, decoupling the
// simulator from any rendering or UI side-effects.
class CitySimulator
{
public:
    CitySimulator(std::unique_ptr<IGrowthStrategy> growthStrategy,
                  std::unique_ptr<IPersistence>    persistence,
                  std::vector<ISimObserver*>        observers);

    // Advance simulation by one tick. Returns false if fully grown.
    bool step();

    // Advance by N ticks
    void run(int steps);

    // Reset to a new seed, clearing all state
    void reset(unsigned int seed);

    // Save current state to file
    bool save(const std::string& path);

    // Load state from file and notify observers
    bool load(const std::string& path);

    // Swap the growth strategy at runtime (called when HDA dropdown changes)
    void setGrowthStrategy(std::unique_ptr<IGrowthStrategy> strategy);

    // Read-only access for commands that need to inspect state
    const SimulationState& state() const { return myState; }
    SimulationState&       state()       { return myState; }

    // Observer registration (used for observers added after construction)
    void addObserver(ISimObserver* obs);
    void removeObserver(ISimObserver* obs);

private:
    void notifyObservers();
    void rebuildBlocks();

    SimulationState                     myState;
    std::unique_ptr<IGrowthStrategy>    myGrowthStrategy;
    std::unique_ptr<IPersistence>       myPersistence;
    std::vector<ISimObserver*>          myObservers;
};
