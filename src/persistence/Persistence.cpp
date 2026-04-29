#include "Persistence.h"
#include <UT/UT_JSONWriter.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONValue.h>
#include <UT/UT_JSONValueMap.h>
#include <UT/UT_JSONValueArray.h>
#include <UT/UT_IStream.h>
#include <fstream>

// Save
bool JsonPersistence::save(const SimulationState& state, const std::string& path)
{
    UT_JSONWriter* writer = UT_JSONWriter::allocWriter(path.c_str(), /*binary=*/false);
    if (!writer) return false;

    writer->jsonBeginMap();

    // Scalars
    writer->jsonKeyToken("commercialRadius");writer->jsonReal(state.commercialRadius);

    // Nodes
    writer->jsonKeyToken("nodes");
    writer->jsonBeginArray();
    for (const auto& n : state.graph.nodes())
    {
        writer->jsonBeginMap();
        writer->jsonKeyToken("id"); writer->jsonInt(n.id);
        writer->jsonKeyToken("x");  writer->jsonReal(n.position.x());
        writer->jsonKeyToken("y");  writer->jsonReal(n.position.y());
        writer->jsonKeyToken("z");  writer->jsonReal(n.position.z());
        writer->jsonEndMap();
    }
    writer->jsonEndArray();

    // Edges
    writer->jsonKeyToken("edges");
    writer->jsonBeginArray();
    for (const auto& e : state.graph.edges())
    {
        writer->jsonBeginMap();
        writer->jsonKeyToken("id");    writer->jsonInt(e.id);
        writer->jsonKeyToken("from");  writer->jsonInt(e.fromNode);
        writer->jsonKeyToken("to");    writer->jsonInt(e.toNode);
        writer->jsonKeyToken("width"); writer->jsonReal(e.width);
        writer->jsonEndMap();
    }
    writer->jsonEndArray();

    // Blocks
    writer->jsonKeyToken("blocks");
    writer->jsonBeginArray();
    for (const auto& b : state.blocks)
    {
        writer->jsonBeginMap();
        writer->jsonKeyToken("id");   writer->jsonInt(b.id);
        writer->jsonKeyToken("zone"); writer->jsonString(zoneTypeName(b.zone).c_str());
        writer->jsonKeyToken("boundary");
        writer->jsonBeginArray();
        for (const auto& pt : b.boundary)
        {
            writer->jsonBeginArray();
            writer->jsonReal(pt.x());
            writer->jsonReal(pt.z());
            writer->jsonEndArray();
        }
        writer->jsonEndArray();
        writer->jsonEndMap();
    }
    writer->jsonEndArray();

    writer->jsonEndMap();

    delete writer;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Load
// ─────────────────────────────────────────────────────────────────────────────
bool JsonPersistence::load(SimulationState& state, const std::string& path)
{
    UT_IFStream ifs;
    if (!ifs.open(path.c_str())) return false;

    UT_JSONValue root;
    UT_JSONParser parser;
    if (!root.parseValue(parser, &ifs)) return false;

    if (root.getType() != UT_JSONValue::JSON_MAP) return false;

    state.graph.clear();
    state.blocks.clear();

    const UT_JSONValueMap* map = root.getMap();

    // Scalars
    if (auto* v = map->get("commercialRadius"))state.commercialRadius = (float)v->getF();

    // Nodes — we rebuild via addNode so adjacency is re-established
    // Store id->new-id mapping in case JSON ids are non-contiguous
    std::unordered_map<int, int> nodeIdMap;
    if (auto* nodesVal = map->get("nodes"))
    {
        const UT_JSONValueArray* arr = nodesVal->getArray();
        if (arr)
        {
            for (int i = 0; i < arr->size(); ++i)
            {
                const UT_JSONValueMap* nm = (*arr)[i]->getMap();
                if (!nm) continue;
                int   origId = nm->get("id") ? (int)nm->get("id")->getI() : i;
                float x = nm->get("x") ? (float)nm->get("x")->getF() : 0.f;
                float y = nm->get("y") ? (float)nm->get("y")->getF() : 0.f;
                float z = nm->get("z") ? (float)nm->get("z")->getF() : 0.f;
                int newId = state.graph.addNode(UT_Vector3F(x, y, z));
                nodeIdMap[origId] = newId;
            }
        }
    }

    // Edges
    if (auto* edgesVal = map->get("edges"))
    {
        const UT_JSONValueArray* arr = edgesVal->getArray();
        if (arr)
        {
            for (int i = 0; i < arr->size(); ++i)
            {
                const UT_JSONValueMap* em = (*arr)[i]->getMap();
                if (!em) continue;
                int   fromOrig = em->get("from")  ? (int)em->get("from")->getI()  : -1;
                int   toOrig   = em->get("to")    ? (int)em->get("to")->getI()    : -1;
                float width    = em->get("width") ? (float)em->get("width")->getF(): 1.f;
                if (nodeIdMap.count(fromOrig) && nodeIdMap.count(toOrig))
                    state.graph.addEdge(nodeIdMap[fromOrig], nodeIdMap[toOrig], width);
            }
        }
    }

    // Blocks
    if (auto* blocksVal = map->get("blocks"))
    {
        const UT_JSONValueArray* arr = blocksVal->getArray();
        if (arr)
        {
            for (int i = 0; i < arr->size(); ++i)
            {
                const UT_JSONValueMap* bm = (*arr)[i]->getMap();
                if (!bm) continue;

                CityBlock block;
                block.id = bm->get("id") ? (int)bm->get("id")->getI() : i;

                std::string zoneName = bm->get("zone")
                    ? std::string(bm->get("zone")->getS())
                    : "empty";
                block.zone = zoneFromString(zoneName);

                if (auto* bdry = bm->get("boundary"))
                {
                    const UT_JSONValueArray* pts = bdry->getArray();
                    if (pts)
                    {
                        for (int j = 0; j < pts->size(); ++j)
                        {
                            const UT_JSONValueArray* pt = (*pts)[j]->getArray();
                            if (pt && pt->size() >= 2)
                            {
                                float px = (float)(*pt)[0]->getF();
                                float pz = (float)(*pt)[1]->getF();
                                block.boundary.push_back(UT_Vector3F(px, 0, pz));
                            }
                        }
                    }
                }

                block.computeArea();
                state.blocks.push_back(std::move(block));
            }
        }
    }

    return true;
}

ZoneType JsonPersistence::zoneFromString(const std::string& s) const
{
    if (s == "residential") return ZoneType::Residential;
    if (s == "commercial")  return ZoneType::Commercial;
    if (s == "industrial")  return ZoneType::Industrial;
    if (s == "park")        return ZoneType::Park;
    return ZoneType::Empty;
}
