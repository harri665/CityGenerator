#include "Commands.h"
#include "../simulator/CitySimulator.h"

void GenerateCommand::execute()
{
    mySim.generate();
}

void ResetCommand::execute()
{
    mySim.reset();
}

void ExportCommand::execute()
{
    mySim.save(myPath);
}

void ImportCommand::execute()
{
    mySim.load(myPath);
}
