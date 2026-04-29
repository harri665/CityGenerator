#pragma once
#include "ICommand.h"
#include <string>

class CitySimulator;

// GenerateCommand — run the full streamline-based city generation pass
class GenerateCommand : public ICommand
{
public:
    explicit GenerateCommand(CitySimulator& sim) : mySim(sim) {}
    void execute() override;

private:
    CitySimulator& mySim;
};

// ResetCommand — wipe state, leaving an empty graph and no blocks
class ResetCommand : public ICommand
{
public:
    explicit ResetCommand(CitySimulator& sim) : mySim(sim) {}
    void execute() override;

private:
    CitySimulator& mySim;
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

// ImportCommand — deserialize state from JSON at a given path
class ImportCommand : public ICommand
{
public:
    ImportCommand(CitySimulator& sim, const std::string& filePath)
        : mySim(sim), myPath(filePath) {}
    void execute() override;

private:
    CitySimulator& mySim;
    std::string    myPath;
};
