#include "Common.h"
#include "StrategyManager.h"

// constructor
StrategyManager::StrategyManager() 
	: firstAttackSent(true)
	, currentStrategy(0)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	addStrategies();
	setStrategy();
}

// get an instance of this
StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
}

void StrategyManager::addStrategies() 
{
	zergOpeningBook = std::vector<std::string>(NumZergStrategies);
	
	zergOpeningBook[FourPoolRush] = "3 4 4 4 4 4 4";
	zergOpeningBook[FivePoolRush] = "0 3 0 0 4 4 4 1 4 4 0";
	zergOpeningBook[Overpool] = "0 0 0 0 0 1 3 0 0 4 4 4 2 2 5 1";
	zergOpeningBook[MutaRush] = "0 0 0 0 0 1 0 0 0 0 3 5 0 2 6 4 4 0 0 1 0 11 0 8 12 0 0 1 1 5 10 10 10 10 10 10";

	results = std::vector<std::vector<IntPair>>(3);

	for (int i(0); i < 3; ++i)
	{
		results[i] = std::vector<IntPair>(NumZergStrategies);
	}

	usableStrategies.push_back(FourPoolRush);
	usableStrategies.push_back(FivePoolRush);

	if (enemyRace == BWAPI::Races::Terran)
	{
		usableStrategies.push_back(MutaRush);
	}
	else if (enemyRace == BWAPI::Races::Protoss)
	{
		usableStrategies.push_back(Overpool);
	}

	if (Options::Modules::USING_STRATEGY_IO)
	{
		readResults();
	}
}

void StrategyManager::readResults()
{
	// read in the name of the read and write directories from settings file
	struct stat buf;

	// if the file doesn't exist something is wrong so just set them to default settings
	if (stat(Options::FileIO::FILE_SETTINGS, &buf) == -1)
	{
		readDir = "bwapi-data/testio/read/";
		writeDir = "bwapi-data/testio/write/";
	}
	else
	{
		std::ifstream f_in(Options::FileIO::FILE_SETTINGS);
		getline(f_in, readDir);
		getline(f_in, writeDir);
		f_in.close();
	}

	// the file corresponding to the enemy's previous results
	std::string readFile = readDir + BWAPI::Broodwar->enemy()->getName() + ".txt";

	// if the file doesn't exist, set the results to zeros
	if (stat(readFile.c_str(), &buf) == -1)
	{
		for (int i(0); i < 3; ++i)
		{
			std::fill(results[i].begin(), results[i].end(), IntPair(0, 0));
		}
	}
	// otherwise read in the results
	else
	{
		std::ifstream f_in(readFile.c_str());
		std::string line;

		for (int i(0); i < 3; ++i)
		{
			getline(f_in, line);
			getline(f_in, line);
			results[i][FourPoolRush].first = atoi(line.c_str());
			getline(f_in, line);
			results[i][FourPoolRush].second = atoi(line.c_str());
			getline(f_in, line);
			results[i][FivePoolRush].first = atoi(line.c_str());
			getline(f_in, line);
			results[i][FivePoolRush].second = atoi(line.c_str());
			getline(f_in, line);
			results[i][Overpool].first = atoi(line.c_str());
			getline(f_in, line);
			results[i][Overpool].second = atoi(line.c_str());
			getline(f_in, line);
			results[i][MutaRush].first = atoi(line.c_str());
			getline(f_in, line);
			results[i][MutaRush].second = atoi(line.c_str());
			getline(f_in, line);
		}
	}

	for (int i(0); i < 3; ++i)
	{
		BWAPI::Broodwar->printf("Results (%s): %d Player Maps: (%d %d) (%d %d) (%d %d) (%d %d)",
			BWAPI::Broodwar->enemy()->getName().c_str(), i + 2,
			results[i][0].first, results[i][0].second,
			results[i][1].first, results[i][1].second,
			results[i][2].first, results[i][2].second,
			results[i][3].first, results[i][3].second);
	}
}

void StrategyManager::writeResults()
{
	std::string writeFile = writeDir + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::ofstream f_out(writeFile.c_str());

	for (int i(0); i < 3; ++i)
	{
		f_out << BWTA::getStartLocations().size() << " Player Maps" << "\n";
		f_out << results[i][FourPoolRush].first << "\n";
		f_out << results[i][FourPoolRush].second << "\n";
		f_out << results[i][FivePoolRush].first << "\n";
		f_out << results[i][FivePoolRush].second << "\n";
		f_out << results[i][Overpool].first << "\n";
		f_out << results[i][Overpool].second << "\n";
		f_out << results[i][MutaRush].first << "\n";
		f_out << results[i][MutaRush].second << "\n";
	}

	f_out.close();
}

void StrategyManager::setStrategy()
{
	// if we are using file io to determine strategy, do so
	if (Options::Modules::USING_STRATEGY_IO)
	{
		double bestUCB = -1;
		int bestStrategyIndex = 0;

		// UCB requires us to try everything once before using the formula
		for (size_t strategyIndex(0); strategyIndex < usableStrategies.size(); ++strategyIndex)
		{
			int sum = results[BWTA::getStartLocations().size()][usableStrategies[strategyIndex]].first +
				results[BWTA::getStartLocations().size()][usableStrategies[strategyIndex]].second;

			if (sum == 0)
			{
				currentStrategy = usableStrategies[strategyIndex];
				return;
			}
		}

		// if we have tried everything once, set the maximizing ucb value
		for (size_t strategyIndex(0); strategyIndex<usableStrategies.size(); ++strategyIndex)
		{
			double ucb = getUCBValue(usableStrategies[strategyIndex]);

			if (ucb > bestUCB)
			{
				bestUCB = ucb;
				bestStrategyIndex = strategyIndex;
			}
		}
		
		currentStrategy = usableStrategies[bestStrategyIndex];
	}
	else
	{
		currentStrategy = usableStrategies[FourPoolRush];
	}

}

void StrategyManager::onEnd(const bool isWinner)
{
	// write the win/loss data to file if we're using IO
	if (Options::Modules::USING_STRATEGY_IO)
	{
		// if the game ended before the tournament time limit
		if (BWAPI::Broodwar->getFrameCount() < Options::Tournament::GAME_END_FRAME)
		{
			if (isWinner)
			{
				results[BWTA::getStartLocations().size()][getCurrentStrategy()].first += 1;
			}
			else
			{
				results[BWTA::getStartLocations().size()][getCurrentStrategy()].second += 1;
			}
		}
		// otherwise game timed out so use in-game score
		else
		{
			if (getScore(BWAPI::Broodwar->self()) > getScore(BWAPI::Broodwar->enemy()))
			{
				results[BWTA::getStartLocations().size()][getCurrentStrategy()].first += 1;
			}
			else
			{
				results[BWTA::getStartLocations().size()][getCurrentStrategy()].second += 1;
			}
		}
		
		writeResults();
	}
}

const double StrategyManager::getUCBValue(const size_t & strategy) const
{
	double totalTrials(0);
	for (size_t s(0); s<usableStrategies.size(); ++s)
	{
		totalTrials += results[BWTA::getStartLocations().size()][usableStrategies[s]].first + results[BWTA::getStartLocations().size()][usableStrategies[s]].second;
	}

	double C = 0.7;
	double wins = results[BWTA::getStartLocations().size()][strategy].first;
	double trials = results[BWTA::getStartLocations().size()][strategy].first + results[BWTA::getStartLocations().size()][strategy].second;

	double ucb = (wins / trials) + C * sqrt(std::log(totalTrials) / trials);

	return ucb;
}

const int StrategyManager::getScore(BWAPI::Player * player) const
{
	return player->getBuildingScore() + player->getKillScore() + player->getRazingScore() + player->getUnitScore();
}

const std::string StrategyManager::getOpeningBook() const
{
	return zergOpeningBook[currentStrategy];
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
const int StrategyManager::defendWithWorkers()
{
	if (!Options::Micro::WORKER_DEFENSE)
	{
		return false;
	}

	// our home hatchery position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of hatchery
	int defenseRadius = 200;

	// fill the set with the types of units we're concerned about
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a worker or tier 1 unit we want to defend
		if ((unit->getType() == BWAPI::Broodwar->enemy()->getRace().getWorker()) ||
			(unit->getType() == BWAPI::UnitTypes::Protoss_Zealot) ||
			(unit->getType() == BWAPI::UnitTypes::Terran_Marine) ||
			(unit->getType() == BWAPI::UnitTypes::Zerg_Zergling))
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers;
}

// called by combat commander to determine whether or not to send an attack force
// freeUnits are the units available to do this attack
const bool StrategyManager::doAttack(const std::set<BWAPI::Unit *> & freeUnits)
{
	bool doattack = false;

	if (currentStrategy == Overpool)
	{
		return (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) > 0);
	}

	int ourForceSize = (int)freeUnits.size();
	int numUnitsNeededForAttack = 1;
	doattack = ourForceSize >= numUnitsNeededForAttack;

	return doattack;
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
	return getZergBuildOrderGoal();
}

const MetaPairVector StrategyManager::getZergBuildOrderGoal()
{
	// the goal to return
	std::vector<std::pair<MetaType, UnitCountType>> goal;

	if (BWAPI::Broodwar->getFrameCount() % 36 != 0)
	{
		return goal;
	}

	if ((currentStrategy == FourPoolRush) || (currentStrategy == FivePoolRush))
	{
		if (BWAPI::Broodwar->getFrameCount() > 6500)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) < 2)
			{
				if (!isConstructing(BWAPI::UnitTypes::Zerg_Hatchery))
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hatchery, 1));
				}
			}
		}
		
		if (BWAPI::Broodwar->getFrameCount() > 5000)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Extractor) < 1)
			{
				if (!isConstructing(BWAPI::UnitTypes::Zerg_Extractor))
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Extractor, 1));
				}
			}
			else if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost) < 1)
			{
				if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Metabolic_Boost))
				{
					goal.push_back(MetaPair(BWAPI::UpgradeTypes::Metabolic_Boost, 1));
				}
			}
			else if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Melee_Attacks) < 1)
			{
				if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Zerg_Melee_Attacks))
				{
					goal.push_back(MetaPair(BWAPI::UpgradeTypes::Zerg_Melee_Attacks, 1));
				}
			}
		}

		if (BWAPI::Broodwar->getFrameCount() < 6800)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Drone) < 15)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}
		else
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Drone) < 28)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}

		goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Zergling, 1));
	}

	if (currentStrategy == Overpool)
	{
		if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) < 2)
		{
			if (!isConstructing(BWAPI::UnitTypes::Zerg_Hatchery))
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hatchery, 1));
			}
		}

		if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Extractor) < 1)
		{
			if (!isConstructing(BWAPI::UnitTypes::Zerg_Extractor))
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Extractor, 1));
			}
		}

		if (BWAPI::Broodwar->getFrameCount() > 9000)
		{
			if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks) < 2)
			{
				if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Zerg_Missile_Attacks))
				{
					goal.push_back(MetaPair(BWAPI::UpgradeTypes::Zerg_Missile_Attacks, 1));
				}
			}
		}

		if (BWAPI::Broodwar->getFrameCount() > 7000)
		{
			if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Missile_Attacks) < 1)
			{
				if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Zerg_Missile_Attacks))
				{
					goal.push_back(MetaPair(BWAPI::UpgradeTypes::Zerg_Missile_Attacks, 1));
				}
			}
		}

		if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) < 1)
		{
			if (!isConstructing(BWAPI::UnitTypes::Zerg_Hydralisk_Den))
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hydralisk_Den, 1));
			}
			else if (BWAPI::Broodwar->self()->supplyUsed() / 2 <= 26)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}
		else if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Muscular_Augments) < 1)
		{
			if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Muscular_Augments))
			{
				goal.push_back(MetaPair(BWAPI::UpgradeTypes::Muscular_Augments, 1));
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Extractor, 1));
			}
			else if (BWAPI::Broodwar->self()->supplyUsed() / 2 <= 26)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}
		else if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) < 1)
		{
			if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Grooved_Spines))
			{
				goal.push_back(MetaPair(BWAPI::UpgradeTypes::Grooved_Spines, 1));
			}
			else
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hydralisk, 2));
			}
		}
		else
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Drone) < 42)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}

			goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hydralisk, 2));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Zergling, 1));
		}
	}

	if (currentStrategy == MutaRush)
	{
		if (BWAPI::Broodwar->getFrameCount() > 6500)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) < 2)
			{
				if (!isConstructing(BWAPI::UnitTypes::Zerg_Hatchery))
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hatchery, 1));
				}
			}
		}

		if (BWAPI::Broodwar->getFrameCount() > 5000)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Extractor) < 1)
			{
				if (!isConstructing(BWAPI::UnitTypes::Zerg_Extractor))
				{
					goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Extractor, 1));
				}
			}
			else if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks) < 1)
			{
				if (!BWAPI::Broodwar->self()->isUpgrading(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks))
				{
					goal.push_back(MetaPair(BWAPI::UpgradeTypes::Zerg_Flyer_Attacks, 1));
				}
			}
		}

		if (BWAPI::Broodwar->getFrameCount() < 6800)
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Drone) < 28)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}
		else
		{
			if (BWAPI::Broodwar->self()->visibleUnitCount(BWAPI::UnitTypes::Zerg_Drone) < 42)
			{
				goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Drone, 1));
			}
		}

		goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Mutalisk, 1));
	}

	return (const std::vector<std::pair<MetaType, UnitCountType>>)goal;
}

 const int StrategyManager::getCurrentStrategy()
 {
	 return currentStrategy;
 }

 bool StrategyManager::isConstructing(BWAPI::UnitType unitT)
 {
	 BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
	 {
		 if (unit->isMorphing())
		 {
			 if ((unit->getBuildType() == unitT) || (unit->getType() == unitT))
			 {
				 return true;
			 }
		 }

		 if (unit->getLastCommand().getUnitType() == unitT)
		 {
			 return true;
		 }

		 if (unit->isBeingConstructed())
		 {
			 if ((unit->getBuildType() == unitT) || (unit->getType() == unitT))
			 {
				 return true;
			 }
		 }
	 }

	 return BuildingManager::Instance().isBeingBuilt(unitT);
 }