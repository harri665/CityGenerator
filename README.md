# CitySimulator — HDK Build & Setup Guide
### Houdini 21.0.440 · Windows · Visual Studio 2022

---

## Prerequisites

| Tool | Required version | Notes |
|------|-----------------|-------|
| Houdini 21.0.440 | exact | Install from SideFX |
| Visual Studio 2022 | 17.12 (vc143) | Community edition is fine — install the **"Desktop development with C++"** workload |
| CMake | 3.6+ | https://cmake.org/download — add to PATH during install |

Houdini 21 uses **vc143 (MSVC 19.42)**. You can confirm after install by opening
Houdini's Command Line Tools and running:
```
hcustom --output_msvcdir
hcustom --output_compiler_range
```

---

## Build steps

Open **Houdini 21.0.440 Command Line Tools** from the Start menu.
This shell sets `%HFS%` and all Houdini environment variables automatically.

```bat
cd /d C:\path\to\CitySimulator

mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="%HFS%/toolkit/cmake"

cmake --build . --config Release
```

`houdini_configure_target()` in CMakeLists.txt automatically copies the built
`.dll` into `%USERPROFILE%/Documents/houdini21.0/dso/`.

---

## Loading in Houdini

1. Start Houdini 21.
2. In a Geometry node, press **Tab** and search for **"City Simulator"**.
3. The SOP appears with all parameters.

If the node doesn't appear, check `%USERPROFILE%/Documents/houdini21.0/dso/`
contains `SOP_CitySimulator.dll`. If missing, re-run the CMake build from
the Houdini Command Line Tools shell.

---

## HDA Type Properties setup (one-time)

After the SOP loads, you can promote it to a proper HDA with a clean UI:

1. Right-click the node → **Create Digital Asset**
2. Name: `city_simulator`, Label: `City Simulator`
3. In **Type Properties → Parameters**, the parameters defined in
   `myTemplateList` are already wired. Arrange them into folders:
   - **Simulation**: Growth Mode, Random Seed, Block Size, Commercial Radius
   - **Controls**: Step, Run (+ Run Steps), Reset
   - **File I/O**: Save/Load File, Save button, Load button
   - **Info** (read-only): Tick, Nodes, Edges, Blocks

---

## Project layout

```
CitySimulator/
├── CMakeLists.txt
├── src/
│   ├── core/           RoadGraph, CityBlock, ZoneType, SimulationState
│   ├── strategies/     IGrowthStrategy + Grid/Organic/Radial
│   ├── factory/        StrategyFactory, BuildingFactory
│   ├── commands/       ICommand + Step/Run/Reset/Export
│   ├── observers/      ISimObserver + Geometry/UI observers
│   ├── persistence/    IPersistence + JsonPersistence
│   ├── simulator/      CitySimulator (DI core)
│   └── SOP_CitySimulator.h/.cpp  (HDK entry point)
└── README.md
```

---

## Where to start filling in logic

The scaffold compiles and the plugin loads — all pattern plumbing is in place.
Fill in actual simulation logic in this order:

1. **`GrowthStrategies.cpp`** — implement `GridGrowthStrategy::grow()` first
   (it's the simplest). Test with Step button.
2. **`CitySimulator::rebuildBlocks()`** — implement the planar face traversal
   to extract real city blocks from the road graph.
3. **`Factories.cpp` → `extrudeBlock()`** — replace the placeholder with real
   GU_Detail extrusion for building geometry.
4. **`OrganicGrowthStrategy` and `RadialGrowthStrategy`** — once grid works.

---

## Design pattern reference

| Pattern | Files | Key interface |
|---------|-------|---------------|
| Strategy | `strategies/` | `IGrowthStrategy::grow()` |
| Factory | `factory/` | `StrategyFactory::create()`, `BuildingFactory::create()` |
| Command | `commands/` | `ICommand::execute()` |
| Observer | `observers/` | `ISimObserver::onStateChanged()` |

Dependency injection happens in `SOP_CitySimulator::ensureSimulator()` —
that single function is where all concrete types are named.
Everything else talks to interfaces only.
