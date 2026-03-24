#pragma once
#include "../core/SimulationState.h"
#include <string>

// IPersistence — abstract save/load interface
// CitySimulator calls save/load through this interface so the storage
// mechanism is swappable (JSON today, binary or database tomorrow)
class IPersistence
{
public:
    virtual ~IPersistence() = default;

    virtual bool save(const SimulationState& state, const std::string& path) = 0;
    virtual bool load(SimulationState& state,       const std::string& path) = 0;
};


// JsonPersistence — saves/loads SimulationState as a JSON file
// Uses Houdini's built-in UT_JSONWriter / UT_JSONParser so no external
// library is needed. The JSON schema is:
//
// {
//   "tick": 42,
//   "seed": 12345,
//   "commercialRadius": 10.0,
//   "nodes": [ { "id": 0, "x": 0.0, "y": 0.0, "z": 0.0 }, ... ],
//   "edges": [ { "id": 0, "from": 0, "to": 1, "width": 1.0 }, ... ],
//   "blocks": [
//     {
//       "id": 0,
//       "zone": "commercial",
//       "boundary": [ [x,z], ... ]
//     }, ...
//   ]
// }
class JsonPersistence : public IPersistence
{
public:
    bool save(const SimulationState& state, const std::string& path) override;
    bool load(SimulationState& state,       const std::string& path) override;

private:
    ZoneType zoneFromString(const std::string& s) const;
};
