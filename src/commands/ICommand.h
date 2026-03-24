#pragma once

// ICommand — COMMAND PATTERN
//
// Each HDA button maps to a concrete command. The SOP creates and executes
// the right one based on which button was pressed — no if/else on button name.
// Commands are self-contained: they hold a reference to the simulator and know
// how to execute (and optionally undo) their own action.
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;

    // Undo is optional — not all commands are reversible (e.g. Export)
    virtual bool canUndo() const { return false; }
    virtual void undo() {}
};
