cmake_minimum_required(VERSION 3.6)
project(CitySimulator)

# ── Houdini install path ────────────────────────────────────────────────────
# Set HFS to your Houdini install dir, e.g. via:
#   cmake -DCMAKE_PREFIX_PATH="C:/Program Files/Side Effects Software/Houdini 21.0.440/toolkit/cmake" ..
# OR set the HFS environment variable before running cmake.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake")

find_package(Houdini REQUIRED)

# ── Source files ────────────────────────────────────────────────────────────
set(SOURCES
    src/core/RoadGraph.cpp
    src/core/CityBlock.cpp
    src/core/RK4Integrator.cpp
    src/core/StreamlineGenerator.cpp
    src/factory/Factories.cpp
    src/commands/Commands.cpp
    src/observers/Observers.cpp
    src/persistence/Persistence.cpp
    src/simulator/CitySimulator.cpp
    src/simulator/BlockExtraction.cpp
    src/SOP_CitySimulator.cpp
    src/SOP_CityBuilding.cpp
)

# ── Build shared library (HDK DSO plugin) ───────────────────────────────────
add_library(SOP_CitySimulator SHARED ${SOURCES})

# Link against the Houdini imported target (handles all include/lib paths)
target_link_libraries(SOP_CitySimulator Houdini)

target_include_directories(SOP_CitySimulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── HDK install conventions ─────────────────────────────────────────────────
# houdini_configure_target sets the correct output name, suffix (.dll on Windows),
# and installs the DSO into $HOME/houdini21.0/dso automatically.
houdini_configure_target(SOP_CitySimulator)
