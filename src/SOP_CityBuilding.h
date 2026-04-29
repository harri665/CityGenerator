#pragma once
#include <SOP/SOP_Node.h>
#include <PRM/PRM_Template.h>

// SOP_CityBuilding — Takes block/parcel polygons as input and generates building geometry.
// This node reads the "zone" and "id" attributes from the input primitives
// to determine which building strategy to use.

class SOP_CityBuilding : public SOP_Node
{
public:
    static OP_Node*     myConstructor(OP_Network*, const char*, OP_Operator*);
    static PRM_Template myTemplateList[];

    static void installSOP(OP_OperatorTable* table);

protected:
    SOP_CityBuilding(OP_Network* net, const char* name, OP_Operator* op);
    ~SOP_CityBuilding() override;

    OP_ERROR cookMySop(OP_Context& context) override;
};
