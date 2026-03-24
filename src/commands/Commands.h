#pragma once
#include "ICommand.h"
#include <string>

// Forward declaration — commands hold a reference to the simulator
class CitySimulator;

// StepCommand — advance the simulation by one tick
class StepCommand : public ICommand
{
public:
    explicit StepCommand(CitySimulator& sim) : mySim(sim) {}
    void execute() override;
    bool canUndo() const override { return true; }
    void undo() override;

private:
    CitySimulator& mySim;
};

// RunCommand — advance N ticks continuously
class RunCommand : public ICommand
{
public:
    RunCommand(CitySimulator& sim, int steps) : mySim(sim), mySteps(steps) {}
    void execute() override;

private:
    CitySimulator& mySim;
    int            mySteps;
};

// ResetCommand — wipe state back to a clean seed
class ResetCommand : public ICommand
{
public:
    ResetCommand(CitySimulator& sim, unsigned int newSeed)
        : mySim(sim), mySeed(newSeed) {}
    void execute() override;

private:
    CitySimulator& mySim;
    unsigned int   mySeed;
};

// ExportCommand — serialize current state to JSON at a given path
class ExportCommand : public ICommand
{
public:
    ExportCommand(CitySimulator& sim, const std::string& filePath)
        : mySim(sim), myPath(filePath) {}
    void execute() override;

private:
    CitySimulator& mySim;
    std::string    myPath;
};
