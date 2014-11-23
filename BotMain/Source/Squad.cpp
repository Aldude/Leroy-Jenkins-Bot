#include "Common.h"
#include "Squad.h"

int  Squad::lastRetreatSwitch = 0;
bool Squad::lastRetreatSwitchVal = false;

Squad::Squad(const UnitVector & units, SquadOrder order) 
	: units(units)
	, order(order)
{
}

void Squad::update()
{
	// update all necessary unit information within this squad
	updateUnits();

	// determine whether or not we should regroup
	const bool needToRegroup(needsToRegroup());
	
	// draw some debug info
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG && order.type == SquadOrder::Attack) 
	{
		BWAPI::Broodwar->drawTextScreen(200, 330, "%s", regroupStatus.c_str());

		BWAPI::Unit * closest = unitClosestToEnemy();
		if (closest && (BWAPI::Broodwar->getFrameCount() % 24 == 0))
		{
			//BWAPI::Broodwar->setScreenPosition(closest->getPosition().x() - 320, closest->getPosition().y() - 200);
		}
	}

	// if we do need to regroup, do it
	if (needToRegroup)
	{
		InformationManager::Instance().lastFrameRegroup = 1;

		const BWAPI::Position regroupPosition(calcRegroupPosition());
		BWAPI::Broodwar->drawTextScreen(200, 150, "REGROUP");

		BWAPI::Broodwar->drawCircleMap(regroupPosition.x(), regroupPosition.y(), 30, BWAPI::Colors::Purple, true);

		meleeManager.regroup(regroupPosition);
		rangedManager.regroup(regroupPosition);
	}
	else // otherwise, execute micro
	{
		InformationManager::Instance().lastFrameRegroup = 1;

		meleeManager.execute(order);
		rangedManager.execute(order);
		transportManager.execute(order);

		detectorManager.setUnitClosestToEnemy(unitClosestToEnemy());
		detectorManager.execute(order);
	}
}

void Squad::updateUnits()
{
	setAllUnits();
	setNearEnemyUnits();
	setManagerUnits();
}

void Squad::setAllUnits()
{
	// clean up the units vector just in case one of them died
	UnitVector goodUnits;
	BOOST_FOREACH(BWAPI::Unit * unit, units)
	{
		if( unit->isCompleted() && 
			(unit->getHitPoints() > 0) && 
			unit->exists() &&
			unit->getPosition().isValid() &&
			(unit->getType() != BWAPI::UnitTypes::Unknown))
		{
			goodUnits.push_back(unit);
		}
	}
	units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
	nearEnemy.clear();
	BOOST_FOREACH(BWAPI::Unit * unit, units)
	{
		int x = unit->getPosition().x();
		int y = unit->getPosition().y();

		int left = unit->getType().dimensionLeft();
		int right = unit->getType().dimensionRight();
		int top = unit->getType().dimensionUp();
		int bottom = unit->getType().dimensionDown();

		nearEnemy[unit] = unitNearEnemy(unit);
		if (nearEnemy[unit])
		{
			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawBoxMap(x-left, y - top, x + right, y + bottom, Options::Debug::COLOR_UNIT_NEAR_ENEMY);
		}
		else
		{
			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawBoxMap(x-left, y - top, x + right, y + bottom, Options::Debug::COLOR_UNIT_NOTNEAR_ENEMY);
		}
	}
}

void Squad::setManagerUnits()
{
	UnitVector meleeUnits;
	UnitVector rangedUnits;
	UnitVector detectorUnits;
	UnitVector transportUnits;

	// add units to micro managers
	BOOST_FOREACH(BWAPI::Unit * unit, units)
	{
		if(unit->isCompleted() && (unit->getHitPoints() > 0) && unit->exists())
		{
			// select detector units
			if (unit->getType().isDetector() && !unit->getType().isBuilding())
			{
				detectorUnits.push_back(unit);
			}
			// select transport units
			else if ((unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) && (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Ventral_Sacs > 0)))
			{
				transportUnits.push_back(unit);
			}
			// select ranged units
			else if (unit->getType().groundWeapon().maxRange() > 32)
			{
				rangedUnits.push_back(unit);
			}
			// select melee units
			else if (unit->getType().groundWeapon().maxRange() <= 32)
			{
				meleeUnits.push_back(unit);
			}
		}
	}

	meleeManager.setUnits(meleeUnits);
	rangedManager.setUnits(rangedUnits);
	detectorManager.setUnits(detectorUnits);
	transportManager.setUnits(detectorUnits);
}

// calculates whether or not to regroup
bool Squad::needsToRegroup()
{
	// if we are not attacking, never regroup
	if (units.empty() || (order.type != SquadOrder::Attack))
	{
		regroupStatus = std::string("\x04 No combat units available");
		return false;
	}

	// if we are Zergling rushing
	if ((StrategyManager::Instance().getCurrentStrategy() == StrategyManager::ZergZerglingRush ||
         StrategyManager::Instance().getCurrentStrategy() == StrategyManager::ZergZerglingRush2) &&
		(BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Zerg_Zergling) <= 10))
	{
		regroupStatus = std::string("\x04 LINGS HOOOOO!");
		return false;
	}

	BWAPI::Unit * unitClosest = unitClosestToEnemy();

	if (!unitClosest)
	{
		regroupStatus = std::string("\x04 No closest unit");
		return false;
	}

	CombatSimulation sim;
	sim.setCombatUnits(unitClosest->getPosition(), Options::Micro::COMBAT_REGROUP_RADIUS + InformationManager::Instance().lastFrameRegroup*300);
	ScoreType score = sim.simulateCombat();

    bool retreat = score < 0;
    int switchTime = 100;

    // we should not attack unless 5 seconds have passed since a retreat
    if (retreat != lastRetreatSwitchVal)
    {
        if (retreat == false && (BWAPI::Broodwar->getFrameCount() - lastRetreatSwitch < switchTime))
        {
            retreat = lastRetreatSwitchVal;
        }
        else
        {
            lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
            lastRetreatSwitchVal = retreat;
        }
    }
	
	if (retreat)
	{
		regroupStatus = std::string("\x04 Retreat - simulation predicts defeat");
	}
	else
	{
		regroupStatus = std::string("\x04 Attack - simulation predicts success");
	}

	return retreat;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
	order = so;
}

bool Squad::unitNearEnemy(BWAPI::Unit * unit)
{
	assert(unit);

	UnitVector enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), 400, false, true);

	return enemyNear.size() > 0;
}

BWAPI::Position Squad::calcCenter()
{
	BWAPI::Position accum(0,0);
	BOOST_FOREACH(BWAPI::Unit * unit, units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x() / units.size(), accum.y() / units.size());
}

BWAPI::Position Squad::calcRegroupPosition()
{
	BWAPI::Position regroup(0,0);
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	BWTA::Region * enemyRegion = enemyBaseLocation ? enemyBaseLocation->getRegion() : NULL;


	int minDist(100000);

	BOOST_FOREACH(BWAPI::Unit * unit, units)
	{
		BWAPI::TilePosition unitTile(unit->getPosition());
		BWTA::Region * unitRegion = unitTile.isValid() ? BWTA::getRegion(unitTile) : NULL;

		if(unitRegion==enemyRegion){
			std::vector<GroundThreat> groundThreats;
			fillGroundThreats(groundThreats, unit->getPosition());

			BWAPI::Position fleeTo = calcFleePosition(groundThreats, unit);
				if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(fleeTo.x(), fleeTo.y(), 10, BWAPI::Colors::Red);

				BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->getUnitsInRadius(fleeTo, 10))
				{
					if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawCircleMap(unit->getPosition().x(), unit->getPosition().y(), 5, BWAPI::Colors::Cyan, true);
				}
				regroup = fleeTo;
		}
		else if (!nearEnemy[unit])
		{
			int dist = unit->getDistance(order.position);
			if (dist < minDist)
			{
				minDist = dist;
				regroup = unit->getPosition();
			}
		}
	}

	if (regroup == BWAPI::Position(0,0))
	{
		return BWTA::getRegion(BWTA::getStartLocation(BWAPI::Broodwar->self())->getTilePosition())->getCenter();
	}
	else
	{
		return regroup;
	}
}
BWAPI::Position Squad::calcFleePosition(const std::vector<GroundThreat> & threats, BWAPI::Unit * target) 
{
	// calculate the standard flee vector
	double2 fleeVector = getFleeVector(threats,target);

	// vector to the target, if one exists
	double2 targetVector(0,0);

	// normalise the target vector

	int mult = 1;

	if (target->getID() % 2) 
	{
		mult = -1;
	}

	// rotate the flee vector by 30 degrees, this will allow us to circle around and come back at a target
	//fleeVector.rotate(mult*30);
	double2 newFleeVector;

	int r = 0;
	int sign = 1;
	int iterations = 0;
		
	// keep rotating the vector until the new position is able to be walked on
	while (r <= 360) 
	{
		// rotate the flee vector
		fleeVector.rotate(mult*r);

		// re-normalize it
		fleeVector.normalise();

		// new vector should semi point back at the target
		newFleeVector = fleeVector * 2 + targetVector;

		// the position we will attempt to go to
		BWAPI::Position test(target->getPosition() + newFleeVector * 24);

		// draw the debug vector
		//if (drawDebugVectors) 
		//{
			if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawLineMap(target->getPosition().x(), target->getPosition().y(), test.x(), test.y(), BWAPI::Colors::Cyan);
		//}

		// if the position is able to be walked on, break out of the loop
		if (isValidFleePosition(test))
		{
			break;
		}

		r = r + sign * (r + 10);
		sign = sign * -1;

		if (++iterations > 36)
		{
			break;
		}
	}

	// go to the calculated 'good' position
	BWAPI::Position fleeTo(target->getPosition() + newFleeVector * 24);
	
	
	if (Options::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawLineMap(target->getPosition().x(), target->getPosition().y(), fleeTo.x(), fleeTo.y(), BWAPI::Colors::Orange);
	

	return fleeTo;
}

bool Squad::isValidFleePosition(BWAPI::Position pos) 
{

	// get x and y from the position
	int x(pos.x()), y(pos.y());

	// walkable tiles exist every 8 pixels
	bool good = BWAPI::Broodwar->isWalkable(x/8, y/8);
	
	// if it's not walkable throw it out
	if (!good) return false;
	
	// for each of those units, if it's a building or an attacking enemy unit we don't want to go there
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->getUnitsOnTile(x/32, y/32)) 
	{
		if	(unit->getType().isBuilding() || unit->getType().isResourceContainer() || 
			(unit->getPlayer() != BWAPI::Broodwar->self() && unit->getType().groundWeapon() != BWAPI::WeaponTypes::None)) 
		{		
				return false;
		}
	}

	int geyserDist = 50;
	int mineralDist = 32;

	BWTA::BaseLocation * enemyLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

	BOOST_FOREACH (BWAPI::Unit * mineral, enemyLocation->getMinerals())
	{
		if (mineral->getDistance(pos) < mineralDist)
		{
			return false;
		}
	}

	BWTA::Region * enemyRegion = enemyLocation->getRegion();
	if (enemyRegion && BWTA::getRegion(BWAPI::TilePosition(pos)) != enemyRegion)
	{
		return false;
	}

	// otherwise it's okay
	return true;
}
double2 Squad::getFleeVector(const std::vector<GroundThreat> & threats, BWAPI::Unit * unit)
{
	double2 fleeVector(0,0);

	BOOST_FOREACH (const GroundThreat & threat, threats)
	{
		// Get direction from enemy to mutalisk
		const double2 direction(unit->getPosition() - threat.unit->getPosition());

		// Divide direction by square distance, weighting closer enemies higher
		//  Dividing once normalises the vector
		//  Dividing a second time reduces the effect of far away units
		const double distanceSq(direction.lenSq());
		if(distanceSq > 0)
		{
			// Enemy influence is direction to flee from enemy weighted by danger posed by enemy
			const double2 enemyInfluence( (direction / distanceSq) * threat.weight );

			fleeVector = fleeVector + enemyInfluence;
		}
	}

	if(fleeVector.lenSq() == 0)
	{
		// Flee towards our base
		fleeVector = double2(1,0);	
	}

	fleeVector.normalise();

	BWAPI::Position p1(unit->getPosition());
	BWAPI::Position p2(p1 + fleeVector * 100);

	return fleeVector;
}	
// fills the GroundThreat vector within a radius of a target
void Squad::fillGroundThreats(std::vector<GroundThreat> & threats, BWAPI::Position target)
{
	// radius of caring
	const int radiusWeCareAbout(1000);
	const int radiusSq(radiusWeCareAbout * radiusWeCareAbout);

	// for each of the candidate units
	const std::set<BWAPI::Unit*> & candidates(BWAPI::Broodwar->enemy()->getUnits());
	BOOST_FOREACH (BWAPI::Unit * e, candidates)
	{
		// if they're not within the radius of caring, who cares?
		const BWAPI::Position delta(e->getPosition() - target);
		if(delta.x() * delta.x() + delta.y() * delta.y() > radiusSq)
		{
			continue;
		}

		// default threat
		GroundThreat threat;
		threat.unit		= e;
		threat.weight	= 1;

		// get the air weapon of the unit
		BWAPI::WeaponType groundWeapon(e->getType().groundWeapon());

		// if it's a bunker, weight it as if it were 4 marines
		if(e->getType() == BWAPI::UnitTypes::Terran_Bunker)
		{
			groundWeapon	= BWAPI::WeaponTypes::Gauss_Rifle;
			threat.weight	= 4;
		}

		// weight the threat based on the highest DPS
		if(groundWeapon != BWAPI::WeaponTypes::None)
		{
			threat.weight *= (static_cast<double>(groundWeapon.damageAmount()) / groundWeapon.damageCooldown());
			threats.push_back(threat);
		}
	}
}


BWAPI::Unit * Squad::unitClosestToEnemy()
{
	BWAPI::Unit * closest = NULL;
	int closestDist = 100000;

	BOOST_FOREACH (BWAPI::Unit * unit, units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		// the distance to the order position
		int dist = MapTools::Instance().getGroundDistance(unit->getPosition(), order.position);

		if (dist != -1 && (!closest || dist < closestDist))
		{
			closest = unit;
			closestDist = dist;
		}
	}

	return closest;
}

int Squad::squadUnitsNear(BWAPI::Position p)
{
	int numUnits = 0;

	BOOST_FOREACH (BWAPI::Unit * unit, units)
	{
		if (unit->getDistance(p) < 600)
		{
			numUnits++;
		}
	}

	return numUnits;
}

bool Squad::squadObserverNear(BWAPI::Position p)
{
	BOOST_FOREACH (BWAPI::Unit * unit, units)
	{
		if (unit->getType().isDetector() && unit->getDistance(p) < 300)
		{
			return true;
		}
	}

	return false;
}

const UnitVector &Squad::getUnits() const	
{ 
	return units; 
} 

const SquadOrder & Squad::getSquadOrder()	const			
{ 
	return order; 
}