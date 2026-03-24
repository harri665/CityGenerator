#include "Commands.h"
#include "../simulator/CitySimulator.h"

// StepCommand
void StepCommand::execute()
{
    mySim.step();
}

void StepCommand::undo()
{
    // TODO: implement undo by keeping a snapshot stack in CitySimulator.
    // For now this is a no-op — add a pushSnapshot()/popSnapshot() pair
    // to CitySimulator and call popSnapshot() here.
}

// RunCommand
void RunCommand::execute()
{
    mySim.run(mySteps);
}

// ResetCommand
void ResetCommand::execute()
{
    mySim.reset(mySeed);
}

// ExportCommand
void ExportCommand::execute()
{
    mySim.save(myPath);
}
