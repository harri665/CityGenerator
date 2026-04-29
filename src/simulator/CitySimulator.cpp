#include "CitySimulator.h"
#include "BlockExtraction.h"
#include "../core/StreamlineGenerator.h"
#include <algorithm>

CitySimulator::CitySimulator(std::unique_ptr<IPersistence> persistence,
                             std::vector<ISimObserver*> observers)
    : myPersistence(std::move(persistence))
    , myObservers(std::move(observers))
{
}

void CitySimulator::generate()
{
    myState.reset();

    myTensorField.fields.clear();
    const FieldConfig& fc = myState.field;

    myTensorField.addField(std::make_unique<GridBasisField>(
        UT_Vector3F(0, 0, 0), fc.gridSize, fc.gridDecay, fc.gridTheta));

    myTensorField.addField(std::make_unique<RadialBasisField>(
        UT_Vector3F(fc.radialCx, 0, fc.radialCz), fc.radialSize, fc.radialDecay));

    StreamlineGenerator gen(myTensorField, myState.params);
    gen.generate(myState.graph);

    rebuildBlocks();

    notifyObservers();
}

void CitySimulator::reset()
{
    myState.reset();
    notifyObservers();
}

bool CitySimulator::save(const std::string& path)
{
    if (!myPersistence) return false;
    return myPersistence->save(myState, path);
}

bool CitySimulator::load(const std::string& path)
{
    if (!myPersistence) return false;
    bool ok = myPersistence->load(myState, path);
    if (ok) {
        rebuildBlocks();
        notifyObservers();
    }
    return ok;
}

void CitySimulator::addObserver(ISimObserver* obs)
{
    myObservers.push_back(obs);
}

void CitySimulator::removeObserver(ISimObserver* obs)
{
    myObservers.erase(
        std::remove(myObservers.begin(), myObservers.end(), obs),
        myObservers.end());
}

void CitySimulator::notifyObservers()
{
    for (ISimObserver* obs : myObservers)
        obs->onStateChanged(myState);
}

void CitySimulator::rebuildBlocks()
{
    myState.blocks = BlockExtraction::rebuild(
        myState.graph,
        myState.commercialRadius,
        UT_Vector3F(0, 0, 0));
}
