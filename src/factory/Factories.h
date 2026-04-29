#pragma once
#include "../core/ZoneType.h"
#include <GU/GU_Detail.h>
#include <memory>
#include <string>



// IBuildingStrategy — abstract building geometry generator per zone type
// BuildingFactory creates the right one; CitySimulator never switches on ZoneType
class IBuildingStrategy
{
public:
    virtual ~IBuildingStrategy() = default;
    virtual void generate(GU_Detail* gdp,
                          const std::vector<UT_Vector3F>& blockBoundary,
                          int seed) = 0;
    virtual const char* zoneName() const = 0;
};

class ResidentialBuildingStrategy : public IBuildingStrategy
{
public:
    void        generate(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& blockBoundary,
                         int seed) override;
    const char* zoneName() const override { return "residential"; }
};

class CommercialBuildingStrategy : public IBuildingStrategy
{
public:
    void        generate(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& blockBoundary,
                         int seed) override;
    const char* zoneName() const override { return "commercial"; }
};

class IndustrialBuildingStrategy : public IBuildingStrategy
{
public:
    void        generate(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& blockBoundary,
                         int seed) override;
    const char* zoneName() const override { return "industrial"; }
};

class ParkBuildingStrategy : public IBuildingStrategy
{
public:
    void        generate(GU_Detail* gdp,
                         const std::vector<UT_Vector3F>& blockBoundary,
                         int seed) override;
    const char* zoneName() const override { return "park"; }
};


// BuildingFactory — creates IBuildingStrategy from ZoneType
class BuildingFactory
{
public:
    static std::unique_ptr<IBuildingStrategy> create(ZoneType zone);
};
