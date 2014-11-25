#pragma once

#include "Common.h"
#include "BuildOrderQueue.h"
#include "WorkerManager.h"
#include "../StrategyManager.h"

class StarcraftBuildOrderSearchManager
{

	std::map<int, MetaType>		buildUnits;
	int							startFrame;
	bool						finished;
	std::vector<MetaType>		result;
	std::vector<MetaType>		goal;
	int							numWanted;
	std::vector<MetaType>		buildNone;

	void						createUnitMap();
	std::vector<MetaType>		getMetaVector(std::string buildString);
	MetaType					getMetaType(int action);
	void						search(double timeLimit);
	
	StarcraftBuildOrderSearchManager();

public:

	static StarcraftBuildOrderSearchManager &	Instance();

	void						update(double timeLimit);

	void						reset();

	void						drawSearchInformation(int x, int y);

	std::vector<MetaType>		getOpeningBuildOrder();
	
	std::vector<MetaType>		findBuildOrder(const std::pair<MetaType, UnitCountType> & goalUnits);
};