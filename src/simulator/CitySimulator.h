#pragma once
#include "../core/SimulationState.h"
#include "../core/StreamlineTensorField.h"
#include "../observers/ISimObserver.h"
#include "../persistence/Persistence.h"
#include <memory>
#include <vector>
#include <string>

class CitySimulator
{
public:
    CitySimulator(std::unique_ptr<IPersistence> persistence,
                  std::vector<ISimObserver*> observers);

    // Generate entire city from current params
    void generate();

    // Reset to a new seed, clearing all state
    void reset();

    // Save current state to file
    bool save(const std::string& path);

    // Load state from file and notify observers
    bool load(const std::string& path);

    // Read-only access for commands that need to inspect state
    const SimulationState& state() const { return myState; }
    SimulationState& state() { return myState; }

    // Observer registration (used for observers added after construction)
    void addObserver(ISimObserver* obs);
    void removeObserver(ISimObserver* obs);

private:
    void notifyObservers();
    void rebuildBlocks();

    SimulationState myState;
    std::unique_ptr<IPersistence> myPersistence;
    std::vector<ISimObserver*> myObservers;
    StreamlineTensorField myTensorField;
};
