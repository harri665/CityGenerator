#include "SOP_CityBuilding.h"
#include "factory/Factories.h"
#include <GU/GU_Detail.h>
#include <GA/GA_Handle.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_Vector3.h> // Ensure we have vector3 if needed, but not DSOVersion

PRM_Template SOP_CityBuilding::myTemplateList[] = {
    PRM_Template() // sentinel
};

void SOP_CityBuilding::installSOP(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(
        "city_building",           // internal name
        "City Building",           // label
        myConstructor,
        myTemplateList,
        1,                         // min inputs
        1                          // max inputs
    ));
}

OP_Node* SOP_CityBuilding::myConstructor(OP_Network* net,
                                          const char* name,
                                          OP_Operator* op)
{
    return new SOP_CityBuilding(net, name, op);
}

SOP_CityBuilding::SOP_CityBuilding(OP_Network* net,
                                    const char* name,
                                    OP_Operator* op)
    : SOP_Node(net, name, op)
{
}

SOP_CityBuilding::~SOP_CityBuilding() = default;

OP_ERROR SOP_CityBuilding::cookMySop(OP_Context& context)
{
    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();

    // Clear the output geometry
    gdp->clearAndDestroy();

    // Get the input geometry
    const GU_Detail* inputGdp = inputGeo(0);
    if (!inputGdp || inputGdp->getNumPrimitives() == 0)
    {
        unlockInputs();
        return error();
    }

    // Look for attributes on input primitives
    GA_ROHandleS zoneAttr(inputGdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "zone", 1));
    GA_ROHandleI idAttr(inputGdp->findIntTuple(GA_ATTRIB_PRIMITIVE, "id", 1));

    // Iterate over input primitives
    for (GA_Iterator it(inputGdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
        const GA_Primitive* prim = inputGdp->getPrimitive(it.getOffset());
        if (!prim) continue;

        // Only process closed polygons (parcels)
        if (prim->getTypeId() != GA_PRIMPOLY) continue;

        std::vector<UT_Vector3F> boundary;
        boundary.reserve(prim->getVertexCount());
        for (GA_Size i = 0; i < prim->getVertexCount(); ++i)
        {
            boundary.push_back(inputGdp->getPos3(prim->getPointOffset(i)));
        }

        if (boundary.size() < 3) continue;

        // Determine zone
        ZoneType zone = ZoneType::Residential; // Default
        if (zoneAttr.isValid())
        {
            const char* zoneStr = zoneAttr.get(prim->getMapOffset());
            if (zoneStr)
            {
                std::string s(zoneStr);
                if (s == "commercial")       zone = ZoneType::Commercial;
                else if (s == "industrial")  zone = ZoneType::Industrial;
                else if (s == "park")        zone = ZoneType::Park;
                else if (s == "empty")       zone = ZoneType::Empty;
                else if (s == "residential") zone = ZoneType::Residential;
            }
        }

        // Determine seed/id
        int seed = 0;
        if (idAttr.isValid())
        {
            seed = idAttr.get(prim->getMapOffset());
        }

        // Generate building geometry
        auto buildingStrat = BuildingFactory::create(zone);
        if (buildingStrat)
        {
            buildingStrat->generate(gdp, boundary, seed);
        }
    }

    unlockInputs();
    return error();
}
