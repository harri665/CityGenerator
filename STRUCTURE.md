# CitySimulator — Project Structure

```
CitySimulator/
├── CMakeLists.txt               # HDK CMake build
├── hda/
│   └── CitySimulator.hda        # Houdini Digital Asset (generated after first build)
├── src/
│   ├── core/
│   │   ├── RoadGraph.h/.cpp         # Node/edge graph data structure
│   │   ├── CityBlock.h/.cpp         # Block extracted from road loops
│   │   ├── ZoneType.h               # Enum + zone attribute helpers
│   │   └── SimulationState.h/.cpp   # Snapshot of sim at a given tick
│   │
│   ├── strategies/                  # PATTERN: Strategy
│   │   ├── IGrowthStrategy.h        # Pure abstract interface
│   │   ├── GridGrowthStrategy.h/.cpp
│   │   ├── OrganicGrowthStrategy.h/.cpp
│   │   └── RadialGrowthStrategy.h/.cpp
│   │
│   ├── factory/                     # PATTERN: Factory
│   │   ├── StrategyFactory.h/.cpp   # Creates IGrowthStrategy from string key
│   │   └── BuildingFactory.h/.cpp   # Creates building geometry per zone type
│   │
│   ├── commands/                    # PATTERN: Command
│   │   ├── ICommand.h               # Pure abstract interface
│   │   ├── StepCommand.h/.cpp
│   │   ├── RunCommand.h/.cpp
│   │   ├── ResetCommand.h/.cpp
│   │   └── ExportCommand.h/.cpp
│   │
│   ├── observers/                   # PATTERN: Observer
│   │   ├── ISimObserver.h           # Pure abstract interface
│   │   ├── GeometryObserver.h/.cpp  # Writes road/building geo to SOP output
│   │   └── UIObserver.h/.cpp        # Updates HDA parameter UI state
│   │
│   ├── persistence/
│   │   ├── IPersistence.h           # Pure abstract interface
│   │   └── JsonPersistence.h/.cpp   # Save/load SimulationState to JSON (UT_JSON)
│   │
│   ├── simulator/
│   │   └── CitySimulator.h/.cpp     # Core class — DI wired, drives the sim loop
│   │
│   └── SOP_CitySimulator.h/.cpp     # HDK SOP node — entry point, wires everything
│
└── README.md
```

## Design pattern map

| Pattern  | Where                                      | Why                                                      |
|----------|--------------------------------------------|----------------------------------------------------------|
| Strategy | `IGrowthStrategy` + 3 concrete classes     | Swap growth algorithm at runtime from HDA dropdown       |
| Factory  | `StrategyFactory`, `BuildingFactory`       | Create correct concrete type from string key, no if/else |
| Command  | `ICommand` + Step/Run/Reset/Export         | Encapsulate HDA button actions, enables undo later       |
| Observer | `ISimObserver` + Geometry/UI observers     | Decouple sim state changes from output side-effects      |

## Dependency injection map

`CitySimulator` constructor accepts:
- `std::unique_ptr<IGrowthStrategy>` — injected by SOP from StrategyFactory
- `std::unique_ptr<IPersistence>`    — injected by SOP (JsonPersistence)
- `std::vector<ISimObserver*>`       — injected observers (geo + UI)
