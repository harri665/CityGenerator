#pragma once
#include <SOP/SOP_Node.h>
#include "observers/Observers.h"
#include <memory>

// SOP_CitySimulator — HDK custom SOP node
//
// Streamline-based city generator (port of MapGenerator's algorithm):
// reads parameters, configures a tensor field of basis fields, runs RK4
// streamline integration to produce a road network, detects polygonal
// blocks and emits geometry through the GeometryObserver.
//
// One-shot Generate button drives the whole pipeline; no tick model.

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
    void ensureSimulator();
    void readParamsIntoState(fpreal t);

    static int  buttonCallback(void* data, int index, float time,
                               const PRM_Template* tplate);

    enum PendingAction { ACTION_NONE, ACTION_GENERATE,
                         ACTION_RESET, ACTION_SAVE, ACTION_LOAD };
    PendingAction myPendingAction = ACTION_NONE;

    std::unique_ptr<CitySimulator>    mySim;
    std::unique_ptr<GeometryObserver> myGeoObserver;
    std::unique_ptr<UIObserver>       myUIObserver;
};
