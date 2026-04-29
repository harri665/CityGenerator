#pragma once
#include "RoadGraph.h"
#include "StreamlineTensorField.h"
#include "GridStorage.h"
#include "RK4Integrator.h"
#include <vector>
#include <deque>

struct StreamlineParams {
    float dsep         = 20.0f;   // seed-placement separation
    float dtest        = 10.0f;   // integration-time collision distance
    float dstep        = 1.0f;    // RK4 step size
    float dcirclejoin  = 5.0f;    // self-collision detection (currently unused)
    float dlookahead   = 25.0f;   // dangling-endpoint join search radius
    float joinAngle    = 0.5f;    // max angle (rad) between streamline dir and join candidate
    int   pathIterations = 1500;
    int   seedTries    = 300;
    float worldSize    = 300.0f;
    unsigned int seed  = 42;
};

class StreamlineGenerator {
public:
    StreamlineGenerator(StreamlineTensorField& field, const StreamlineParams& params);

    void generate(RoadGraph& graph);

private:
    StreamlineTensorField& myField;
    StreamlineParams       myParams;
    RK4Integrator          myIntegrator;

    GridStorage myGridMajor;
    GridStorage myGridMinor;

    std::vector<std::vector<UT_Vector3F>> myMajorStreamlines;
    std::vector<std::vector<UT_Vector3F>> myMinorStreamlines;
    std::deque<UT_Vector3F>               myCandidateSeeds;

    void generateRoads(bool major);
    std::vector<UT_Vector3F> traceStreamline(const UT_Vector3F& seed, bool major);
    void addSeedsFromStreamline(const std::vector<UT_Vector3F>& streamline, bool major);
    bool isValidSeed(const UT_Vector3F& seed, bool major);

    // Post-process: extend dangling endpoints toward nearby roads (port of
    // MapGenerator's joinDanglingStreamlines / getBestNextPoint / pointsBetween).
    void joinDanglingStreamlines();
    bool getBestNextPoint(const UT_Vector3F& point,
                          const UT_Vector3F& previousPoint,
                          UT_Vector3F& out) const;
    std::vector<UT_Vector3F> pointsBetween(const UT_Vector3F& v1,
                                           const UT_Vector3F& v2,
                                           float dstep) const;

    void buildGraphFromStreamlines(RoadGraph& graph);
};
