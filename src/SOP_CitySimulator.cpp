#include "SOP_CitySimulator.h"
#include "SOP_CityBuilding.h"
#include "simulator/CitySimulator.h"
#include "persistence/Persistence.h"
#include "observers/Observers.h"
#include "commands/Commands.h"

#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <UT/UT_DSOVersion.h>
#include <SYS/SYS_Math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Parameter tokens
// ─────────────────────────────────────────────────────────────────────────────

// Generation params
static PRM_Name parmSeed       ("seed",         "Random Seed");
static PRM_Name parmWorldSize  ("world_size",   "World Size");
static PRM_Name parmDsepMajor  ("dsep_major",   "Major Road Spacing");
static PRM_Name parmDtest      ("dtest",        "Major/Minor Test Distance");
static PRM_Name parmDstep      ("dstep",        "Integration Step");
static PRM_Name parmPathIter   ("path_iter",    "Max Path Iterations");
static PRM_Name parmSeedTries  ("seed_tries",   "Seed Tries");

// Tensor field params
static PRM_Name parmGridTheta  ("grid_theta",   "Grid Angle (rad)");
static PRM_Name parmGridSize   ("grid_size",    "Grid Influence");
static PRM_Name parmRadialCxz  ("radial_center","Radial Center XZ");
static PRM_Name parmRadialSize ("radial_size",  "Radial Influence");

// Zoning
static PRM_Name parmCommRadius ("comm_radius",  "Commercial Radius");

// Persistence
static PRM_Name parmFilePath   ("file_path",    "Save/Load File");

// Buttons
static PRM_Name btnGenerate ("btn_generate", "Generate");
static PRM_Name btnReset    ("btn_reset",    "Reset");
static PRM_Name btnSave     ("btn_save",     "Save");
static PRM_Name btnLoad     ("btn_load",     "Load");

// Read-only info display
static PRM_Name infoNodes  ("info_nodes",  "Nodes");
static PRM_Name infoEdges  ("info_edges",  "Edges");
static PRM_Name infoBlocks ("info_blocks", "Blocks");

// ─────────────────────────────────────────────────────────────────────────────
// Defaults
// ─────────────────────────────────────────────────────────────────────────────
static PRM_Default defaultSeed      (42);
static PRM_Default defaultWorldSize (300.0f);
static PRM_Default defaultDsepMajor (20.0f);
static PRM_Default defaultDtest     (10.0f);
static PRM_Default defaultDstep     (1.0f);
static PRM_Default defaultPathIter  (1500);
static PRM_Default defaultSeedTries (300);

static PRM_Default defaultGridTheta (0.0f);
static PRM_Default defaultGridSize  (200.0f);
static PRM_Default defaultRadialCxz[]   = { PRM_Default(0.0f), PRM_Default(0.0f) };
static PRM_Default defaultRadialSize(80.0f);

static PRM_Default defaultCommRadius(10.0f);

static PRM_Default defaultBtnZero  (0);
static PRM_Default defaultFilePath (0, "$HIP/city.json");
static PRM_Default defaultInfoZero (0, "0");

// ─────────────────────────────────────────────────────────────────────────────
// Parameter template
// ─────────────────────────────────────────────────────────────────────────────
PRM_Template SOP_CitySimulator::myTemplateList[] = {
    PRM_Template(PRM_INT,   1, &parmSeed,       &defaultSeed),
    PRM_Template(PRM_FLT,   1, &parmWorldSize,  &defaultWorldSize),
    PRM_Template(PRM_FLT,   1, &parmDsepMajor,  &defaultDsepMajor),
    PRM_Template(PRM_FLT,   1, &parmDtest,      &defaultDtest),
    PRM_Template(PRM_FLT,   1, &parmDstep,      &defaultDstep),
    PRM_Template(PRM_INT,   1, &parmPathIter,   &defaultPathIter),
    PRM_Template(PRM_INT,   1, &parmSeedTries,  &defaultSeedTries),

    PRM_Template(PRM_FLT,   1, &parmGridTheta,  &defaultGridTheta),
    PRM_Template(PRM_FLT,   1, &parmGridSize,   &defaultGridSize),
    PRM_Template(PRM_XYZ,   2, &parmRadialCxz,  defaultRadialCxz),
    PRM_Template(PRM_FLT,   1, &parmRadialSize, &defaultRadialSize),

    PRM_Template(PRM_FLT,   1, &parmCommRadius, &defaultCommRadius),
    PRM_Template(PRM_FILE,  1, &parmFilePath,   &defaultFilePath),

    PRM_Template(PRM_CALLBACK, 1, &btnGenerate, &defaultBtnZero, 0, 0,
                 &SOP_CitySimulator::buttonCallback),
    PRM_Template(PRM_CALLBACK, 1, &btnReset,    &defaultBtnZero, 0, 0,
                 &SOP_CitySimulator::buttonCallback),
    PRM_Template(PRM_CALLBACK, 1, &btnSave,     &defaultBtnZero, 0, 0,
                 &SOP_CitySimulator::buttonCallback),
    PRM_Template(PRM_CALLBACK, 1, &btnLoad,     &defaultBtnZero, 0, 0,
                 &SOP_CitySimulator::buttonCallback),

    PRM_Template(PRM_STRING, 1, &infoNodes,  &defaultInfoZero),
    PRM_Template(PRM_STRING, 1, &infoEdges,  &defaultInfoZero),
    PRM_Template(PRM_STRING, 1, &infoBlocks, &defaultInfoZero),

    PRM_Template() // sentinel
};

// ─────────────────────────────────────────────────────────────────────────────
// HDK registration
// ─────────────────────────────────────────────────────────────────────────────
void newSopOperator(OP_OperatorTable* table)
{
    SOP_CitySimulator::installSOP(table);
    SOP_CityBuilding::installSOP(table);
}

void SOP_CitySimulator::installSOP(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(
        "city_simulator",
        "City Simulator",
        myConstructor,
        myTemplateList,
        0,
        0,
        nullptr));
}

OP_Node* SOP_CitySimulator::myConstructor(OP_Network* net,
                                           const char* name,
                                           OP_Operator* op)
{
    return new SOP_CitySimulator(net, name, op);
}

SOP_CitySimulator::SOP_CitySimulator(OP_Network* net,
                                      const char* name,
                                      OP_Operator* op)
    : SOP_Node(net, name, op)
{
}

SOP_CitySimulator::~SOP_CitySimulator() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Button handling
// ─────────────────────────────────────────────────────────────────────────────
int SOP_CitySimulator::buttonCallback(void* data, int /*index*/,
                                       float /*time*/,
                                       const PRM_Template* tplate)
{
    auto* sop = static_cast<SOP_CitySimulator*>(data);
    if (!sop || !tplate) return 0;

    const char* token = tplate->getToken();
    if      (!strcmp(token, "btn_generate")) sop->myPendingAction = ACTION_GENERATE;
    else if (!strcmp(token, "btn_reset"))    sop->myPendingAction = ACTION_RESET;
    else if (!strcmp(token, "btn_save"))     sop->myPendingAction = ACTION_SAVE;
    else if (!strcmp(token, "btn_load"))     sop->myPendingAction = ACTION_LOAD;

    sop->forceRecook();
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cook
// ─────────────────────────────────────────────────────────────────────────────
OP_ERROR SOP_CitySimulator::cookMySop(OP_Context& context)
{
    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();

    ensureSimulator();
    readParamsIntoState(context.getTime());

    PendingAction action = myPendingAction;
    myPendingAction = ACTION_NONE;

    std::unique_ptr<ICommand> cmd;
    switch (action)
    {
        case ACTION_GENERATE:
            cmd = std::make_unique<GenerateCommand>(*mySim);
            break;
        case ACTION_RESET:
            cmd = std::make_unique<ResetCommand>(*mySim);
            break;
        case ACTION_SAVE:
        {
            UT_String filePath;
            evalString(filePath, "file_path", 0, context.getTime());
            cmd = std::make_unique<ExportCommand>(*mySim, std::string(filePath.c_str()));
            break;
        }
        case ACTION_LOAD:
        {
            UT_String filePath;
            evalString(filePath, "file_path", 0, context.getTime());
            cmd = std::make_unique<ImportCommand>(*mySim, std::string(filePath.c_str()));
            break;
        }
        default:
            break;
    }

    if (cmd) cmd->execute();

    unlockInputs();
    return error();
}

void SOP_CitySimulator::ensureSimulator()
{
    if (mySim) return;

    auto persistence = std::make_unique<JsonPersistence>();
    auto geoObs = std::make_unique<GeometryObserver>(gdp);
    auto uiObs  = std::make_unique<UIObserver>(this);

    std::vector<ISimObserver*> observers = { geoObs.get(), uiObs.get() };

    myGeoObserver = std::move(geoObs);
    myUIObserver  = std::move(uiObs);

    mySim = std::make_unique<CitySimulator>(std::move(persistence), observers);
}

void SOP_CitySimulator::readParamsIntoState(fpreal t)
{
    if (!mySim) return;
    auto& s = mySim->state();

    s.params.seed           = (unsigned int)evalInt("seed",        0, t);
    s.params.worldSize      = (float)evalFloat("world_size",       0, t);
    s.params.dsep           = (float)evalFloat("dsep_major",       0, t);
    s.params.dtest          = (float)evalFloat("dtest",            0, t);
    s.params.dstep          = (float)evalFloat("dstep",            0, t);
    s.params.pathIterations = (int)evalInt("path_iter",            0, t);
    s.params.seedTries      = (int)evalInt("seed_tries",           0, t);

    s.field.gridTheta  = (float)evalFloat("grid_theta",    0, t);
    s.field.gridSize   = (float)evalFloat("grid_size",     0, t);
    s.field.radialCx   = (float)evalFloat("radial_center", 0, t);
    s.field.radialCz   = (float)evalFloat("radial_center", 1, t);
    s.field.radialSize = (float)evalFloat("radial_size",   0, t);

    s.commercialRadius = (float)evalFloat("comm_radius",   0, t);
}
