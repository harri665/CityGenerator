#include "SOP_CitySimulator.h"
#include "simulator/CitySimulator.h"
#include "factory/Factories.h"
#include "persistence/Persistence.h"
#include "observers/Observers.h"
#include "commands/Commands.h"

#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <UT/UT_DSOVersion.h>
#include <SYS/SYS_Math.h>

// HDA Parameter definitions
// These names must exactly match what you define in the HDA Type Properties UI.

// Growth mode dropdown choices
static PRM_Name growthModeChoices[] = {
    PRM_Name("grid",    "Grid"),
    PRM_Name("organic", "Organic"),
    PRM_Name("radial",  "Radial"),
    PRM_Name(nullptr)
};
static PRM_ChoiceList growthModeMenu(PRM_CHOICELIST_SINGLE, growthModeChoices);

static PRM_Name  parmGrowthMode ("growth_mode",  "Growth Mode");
static PRM_Name  parmSeed       ("seed",         "Random Seed");
static PRM_Name  parmRunSteps   ("run_steps",    "Run Steps");
static PRM_Name  parmFilePath   ("file_path",    "Save/Load File");
static PRM_Name  parmBlockSize  ("block_size",   "Block Size");
static PRM_Name  parmCommRadius ("comm_radius",  "Commercial Radius");

// Buttons — Houdini buttons are integer parms that bump when clicked
static PRM_Name  btnStep        ("btn_step",     "Step");
static PRM_Name  btnRun         ("btn_run",      "Run");
static PRM_Name  btnReset       ("btn_reset",    "Reset");
static PRM_Name  btnSave        ("btn_save",     "Save");
static PRM_Name  btnLoad        ("btn_load",     "Load");

// Read-only info display (label parms)
static PRM_Name  infoTick       ("info_tick",    "Tick");
static PRM_Name  infoNodes      ("info_nodes",   "Nodes");
static PRM_Name  infoEdges      ("info_edges",   "Edges");
static PRM_Name  infoBlocks     ("info_blocks",  "Blocks");

static PRM_Default defaultSeed      (42);
static PRM_Default defaultRunSteps  (10);
static PRM_Default defaultBlockSize (5.0f);
static PRM_Default defaultCommRadius(10.0f);
static PRM_Default defaultBtnZero   (0);
static PRM_Default defaultFilePath  (0, "$HIP/city.json");
static PRM_Default defaultInfoZero  (0, "0");

PRM_Template SOP_CitySimulator::myTemplateList[] = {
    PRM_Template(PRM_ORD,   1, &parmGrowthMode,  PRMzeroDefaults, &growthModeMenu),
    PRM_Template(PRM_INT,   1, &parmSeed,         &defaultSeed),
    PRM_Template(PRM_FLT,   1, &parmBlockSize,    &defaultBlockSize),
    PRM_Template(PRM_FLT,   1, &parmCommRadius,   &defaultCommRadius),
    PRM_Template(PRM_FILE,  1, &parmFilePath,     &defaultFilePath),
    PRM_Template(PRM_INT,   1, &parmRunSteps,     &defaultRunSteps),

    // Buttons
    PRM_Template(PRM_CALLBACK, 1, &btnStep,  &defaultBtnZero),
    PRM_Template(PRM_CALLBACK, 1, &btnRun,   &defaultBtnZero),
    PRM_Template(PRM_CALLBACK, 1, &btnReset, &defaultBtnZero),
    PRM_Template(PRM_CALLBACK, 1, &btnSave,  &defaultBtnZero),
    PRM_Template(PRM_CALLBACK, 1, &btnLoad,  &defaultBtnZero),

    // Read-only info display
    PRM_Template(PRM_STRING, 1, &infoTick,   &defaultInfoZero),
    PRM_Template(PRM_STRING, 1, &infoNodes,  &defaultInfoZero),
    PRM_Template(PRM_STRING, 1, &infoEdges,  &defaultInfoZero),
    PRM_Template(PRM_STRING, 1, &infoBlocks, &defaultInfoZero),

    PRM_Template() // sentinel
};

// ─────────────────────────────────────────────────────────────────────────────
// HDK registration — called by Houdini at DSO load time
// ─────────────────────────────────────────────────────────────────────────────
void newSopOperator(OP_OperatorTable* table)
{
    SOP_CitySimulator::installSOP(table);
}

void SOP_CitySimulator::installSOP(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(
        "city_simulator",          // internal name (TAB menu key)
        "City Simulator",          // label shown in TAB menu
        myConstructor,
        myTemplateList,
        0,                         // min inputs
        0,                         // max inputs
        nullptr                    // variables
    ));
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
// cookMySop — called by Houdini whenever a parameter changes or a cook
// is requested. Reads button states and dispatches the right Command.
// ─────────────────────────────────────────────────────────────────────────────
OP_ERROR SOP_CitySimulator::cookMySop(OP_Context& context)
{
    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();

    ensureSimulator(context);

    // ── Read button states ───────────────────────────────────────────────────
    // Houdini CALLBACK buttons return non-zero when they have been clicked
    // since the last cook. We dispatch polymorphically through ICommand.

    int stepClicked  = evalInt("btn_step",  0, context.getTime());
    int runClicked   = evalInt("btn_run",   0, context.getTime());
    int resetClicked = evalInt("btn_reset", 0, context.getTime());
    int saveClicked  = evalInt("btn_save",  0, context.getTime());
    int loadClicked  = evalInt("btn_load",  0, context.getTime());

    std::unique_ptr<ICommand> cmd;

    // COMMAND PATTERN: construct and execute the appropriate command.
    // No if/else chain on button name — each branch constructs a different
    // concrete ICommand and we call execute() polymorphically.
    if (resetClicked)
    {
        unsigned int seed = (unsigned int)evalInt("seed", 0, context.getTime());
        cmd = std::make_unique<ResetCommand>(*mySim, seed);
    }
    else if (loadClicked)
    {
        UT_String filePath;
        evalString(filePath, "file_path", 0, context.getTime());
        cmd = std::make_unique<ExportCommand>(*mySim, std::string(filePath.c_str()));
        // Note: for load we reuse ExportCommand's path but call load — 
        // replace with a dedicated LoadCommand for cleaner separation
        // TODO: add LoadCommand : ICommand that calls mySim.load()
    }
    else if (saveClicked)
    {
        UT_String filePath;
        evalString(filePath, "file_path", 0, context.getTime());
        cmd = std::make_unique<ExportCommand>(*mySim, std::string(filePath.c_str()));
    }
    else if (runClicked)
    {
        int steps = evalInt("run_steps", 0, context.getTime());
        cmd = std::make_unique<RunCommand>(*mySim, steps);
    }
    else if (stepClicked)
    {
        cmd = std::make_unique<StepCommand>(*mySim);
    }

    if (cmd)
        cmd->execute();

    // GeometryObserver has already written into gdp via notifyObservers()
    // inside CitySimulator. Nothing more to do here on the geometry side.

    unlockInputs();
    return error();
}

// ensureSimulator — lazy construction with DEPENDENCY INJECTION
// Creates a fresh CitySimulator with all three collaborators injected.
// Re-creates only if the growth mode changed since last cook.
void SOP_CitySimulator::ensureSimulator(OP_Context& context)
{
    std::string mode = growthMode();

    if (mySim && mode == myLastGrowthMode) return;

    // Read params for strategy construction
    float blockSize   = (float)evalFloat("block_size",  0, context.getTime());
    float commRadius  = (float)evalFloat("comm_radius", 0, context.getTime());
    unsigned int seed = (unsigned int)evalInt("seed",   0, context.getTime());

    // FACTORY PATTERN: create strategy from string key — no new calls in this fn
    auto strategy = StrategyFactory::create(mode, blockSize, 0.3f);

    // DEPENDENCY INJECTION: inject all three collaborators into CitySimulator
    auto persistence = std::make_unique<JsonPersistence>();

    // Build observers — GeometryObserver needs our gdp pointer
    auto geoObs = std::make_unique<GeometryObserver>(gdp);
    auto uiObs  = std::make_unique<UIObserver>(this);

    std::vector<ISimObserver*> observers = { geoObs.get(), uiObs.get() };

    // CitySimulator owns the strategy and persistence;
    // we keep the observers alive as members below
    myGeoObserver = std::move(geoObs);
    myUIObserver  = std::move(uiObs);

    mySim = std::make_unique<CitySimulator>(
        std::move(strategy),
        std::move(persistence),
        observers
    );

    mySim->state().seed             = seed;
    mySim->state().commercialRadius = commRadius;
    myLastGrowthMode = mode;
}

std::string SOP_CitySimulator::growthMode() const
{
    int idx = evalInt("growth_mode", 0, 0);
    switch (idx)
    {
        case 1:  return "organic";
        case 2:  return "radial";
        default: return "grid";
    }
}
