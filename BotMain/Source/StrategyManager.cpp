#include "Common.h"
#include "StrategyManager.h"

// constructor
StrategyManager::StrategyManager() 
	: firstAttackSent(false)
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
	
	zergOpeningBook[FourPoolRush] = "3 4 4 4 4 4 1";
	zergOpeningBook[Overpool] = "0 0 0 0 0 1 3 0 0 4 4 4 2 2 5 1";

	results = std::vector<IntPair>(NumZergStrategies);

	usableStrategies.push_back(FourPoolRush);
	usableStrategies.push_back(Overpool);

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
		std::fill(results.begin(), results.end(), IntPair(0,0));
	}
	// otherwise read in the results
	else
	{
		std::ifstream f_in(readFile.c_str());
		std::string line;
		getline(f_in, line);
		results[FourPoolRush].first = atoi(line.c_str());
		getline(f_in, line);
		results[FourPoolRush].second = atoi(line.c_str());
		getline(f_in, line);
		results[Overpool].first = atoi(line.c_str());
		getline(f_in, line);
		results[Overpool].second = atoi(line.c_str());
		getline(f_in, line);
	}

	BWAPI::Broodwar->printf("Results (%s): (%d %d) (%d %d) (%d %d)", BWAPI::Broodwar->enemy()->getName().c_str(), 
		results[0].first, results[0].second, results[1].first, results[1].second, results[2].first, results[2].second);
}

void StrategyManager::writeResults()
{
	std::string writeFile = writeDir + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::ofstream f_out(writeFile.c_str());

	f_out << results[FourPoolRush].first   << "\n";
	f_out << results[FourPoolRush].second << "\n";
	f_out << results[Overpool].first << "\n";
	f_out << results[Overpool].second << "\n";

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
			int sum = results[usableStrategies[strategyIndex]].first + results[usableStrategies[strategyIndex]].second;

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
		currentStrategy = usableStrategies[Overpool];
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
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
			}
		}
		// otherwise game timed out so use in-game score
		else
		{
			if (getScore(BWAPI::Broodwar->self()) > getScore(BWAPI::Broodwar->enemy()))
			{
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
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
		totalTrials += results[usableStrategies[s]].first + results[usableStrategies[s]].second;
	}

	double C = 0.7;
	double wins = results[strategy].first;
	double trials = results[strategy].first + results[strategy].second;

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

	if (!firstAttackSent)
	{
		if (currentStrategy == FourPoolRush)
		{
			bool doattack = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Zergling) == 3;
		}

		if (currentStrategy == Overpool)
		{
			bool doattack = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Zergling) == 3;
		}

		if (doattack)
		{
			firstAttackSent = true;
		}
	}
	else
	{
		if (currentStrategy == Overpool)
		{
			return (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Grooved_Spines) > 1);
		}

		int ourForceSize = (int)freeUnits.size();
		int numUnitsNeededForAttack = 1;
		doattack = ourForceSize >= numUnitsNeededForAttack;
	}

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

	if (currentStrategy == FourPoolRush)
	{

	}

	if (currentStrategy == Overpool)
	{
		if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk_Den) < 1)
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
			goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Hydralisk, 3));
			goal.push_back(MetaPair(BWAPI::UnitTypes::Zerg_Zergling, 2));
		}
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