#pragma once
#include <SOP/SOP_Node.h>
#include "observers/Observers.h"
#include <memory>
#include <string>

// SOP_CitySimulator — HDK custom SOP node
//
// This is the Houdini-facing entry point. It:
//   1. Reads HDA parameters (growth mode, seed, steps, file path, button state)
//   2. Constructs the CitySimulator with injected dependencies on first cook
//   3. Dispatches the correct Command based on which button was pressed
//   4. The GeometryObserver writes into gdp on every state change
//
// The SOP owns the CitySimulator instance across cooks via a member unique_ptr.

class CitySimulator;

class SOP_CitySimulator : public SOP_Node
{
public:
    static OP_Node*     myConstructor(OP_Network*, const char*, OP_Operator*);
    static PRM_Template myTemplateList[];

    static void installSOP(OP_OperatorTable* table);

protected:
    SOP_CitySimulator(OP_Network* net, const char* name, OP_Operator* op);
    ~SOP_CitySimulator() override;

    OP_ERROR cookMySop(OP_Context& context) override;

private:
    void        ensureSimulator(OP_Context& context);
    std::string growthMode() const;

    // Button callback — Houdini calls this when any PRM_CALLBACK button is clicked
    static int  buttonCallback(void* data, int index, float time,
                               const PRM_Template* tplate);

    // Which action the last button click requested
    enum PendingAction { ACTION_NONE, ACTION_STEP, ACTION_RUN,
                         ACTION_RESET, ACTION_SAVE, ACTION_LOAD };
    PendingAction myPendingAction = ACTION_NONE;

    // Simulator owns the algorithm and persistence;
    // SOP owns the observers (they need SOP-lifetime pointers to gdp and this)
    std::unique_ptr<CitySimulator>      mySim;
    std::unique_ptr<GeometryObserver>   myGeoObserver;
    std::unique_ptr<UIObserver>         myUIObserver;
    std::string                         myLastGrowthMode;
};
