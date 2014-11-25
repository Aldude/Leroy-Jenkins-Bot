#include "Common.h"
#include "StarcraftBuildOrderSearchManager.h"

typedef std::map<MetaType, UnitCountType> mapType;

// get an instance of this
StarcraftBuildOrderSearchManager & StarcraftBuildOrderSearchManager::Instance() 
{
	static StarcraftBuildOrderSearchManager instance;
	return instance;
}

// constructor
StarcraftBuildOrderSearchManager::StarcraftBuildOrderSearchManager() 
{
	createUnitMap();
}

void StarcraftBuildOrderSearchManager::createUnitMap()
{
	buildUnits[0] = BWAPI::UnitTypes::Zerg_Drone;
	buildUnits[1] = BWAPI::UnitTypes::Zerg_Overlord;
	buildUnits[2] = BWAPI::UnitTypes::Zerg_Hatchery;
	buildUnits[3] = BWAPI::UnitTypes::Zerg_Spawning_Pool;
	buildUnits[4] = BWAPI::UnitTypes::Zerg_Zergling;
	buildUnits[5] = BWAPI::UnitTypes::Zerg_Extractor;
	buildUnits[6] = BWAPI::UnitTypes::Zerg_Lair;
	buildUnits[7] = BWAPI::UnitTypes::Zerg_Hydralisk_Den;
	buildUnits[8] = BWAPI::UnitTypes::Zerg_Spire;
	buildUnits[9] = BWAPI::UnitTypes::Zerg_Hydralisk;
	buildUnits[10] = BWAPI::UnitTypes::Zerg_Mutalisk;
	buildUnits[11] = BWAPI::UnitTypes::Zerg_Creep_Colony;
	buildUnits[12] = BWAPI::UnitTypes::Zerg_Sunken_Colony;
	buildUnits[13] = BWAPI::UnitTypes::Zerg_Spore_Colony;
	buildUnits[14] = BWAPI::UnitTypes::Zerg_Hive;
	buildUnits[15] = BWAPI::UpgradeTypes::Ventral_Sacs;
}

void StarcraftBuildOrderSearchManager::update(double timeLimit)
{
	if (!goal.empty()) {
		startFrame = BWAPI::Broodwar->getFrameCount();
		search(timeLimit);
	}
}

// function which is called from the bot
std::vector<MetaType> StarcraftBuildOrderSearchManager::findBuildOrder(const std::pair<MetaType, UnitCountType> & goalUnits)
{
	if (goal.empty())
	{
		goal.push_back(goalUnits.first);
		numWanted = goalUnits.second;
	}

	if (finished)
	{
		finished = false;
		goal.pop_back();
		return result;
	}

	return buildNone;
}

MetaType StarcraftBuildOrderSearchManager::getMetaType(int action)
{
	if (action < 0) {
		return BWAPI::UnitTypes::None;
	}

	return buildUnits[action];
}

std::vector<MetaType> StarcraftBuildOrderSearchManager::getOpeningBuildOrder()
{	
	return getMetaVector(StrategyManager::Instance().getOpeningBook());
}

std::vector<MetaType> StarcraftBuildOrderSearchManager::getMetaVector(std::string buildString)
{
	std::stringstream ss;
	ss << buildString;
	std::vector<MetaType> meta;

	int action(0);
	while (ss >> action)
	{
		meta.push_back(getMetaType(action));
	}

	return meta;
}

void StarcraftBuildOrderSearchManager::drawSearchInformation(int x, int y) { }

void StarcraftBuildOrderSearchManager::search(double timeLimit) {
	double timeRemaining = timeLimit - ((BWAPI::Broodwar->getFrameCount() - startFrame) * BWAPI::Broodwar->getFPS());
	BWAPI::UnitType builder = goal.back().unitType.whatBuilds().first;
	int needed = goal.back().unitType.whatBuilds().second;

	while (timeRemaining > 0)
	{
		if (BWAPI::Broodwar->self()->completedUnitCount(builder) >= needed)
		{
			break;
		}
		else
		{
			needed -= BWAPI::Broodwar->self()->completedUnitCount(builder);

			while (needed > 0)
			{
				result.push_back(builder);
				--needed;
			}
		}

		needed = builder.whatBuilds().second;
		builder = builder.whatBuilds().first;
		timeRemaining = timeLimit - ((BWAPI::Broodwar->getFrameCount() - startFrame) * BWAPI::Broodwar->getFPS());
	}

	if (timeRemaining < 0)
	{
		return;
	}

	finished = true;

	for (int i(0); i < numWanted; ++i)
	{
		result.push_back(goal.back());
	}
}