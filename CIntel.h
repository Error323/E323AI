#ifndef INTEL_H
#define INTEL_H

#include <vector>
#include <list>
#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"
#include "CMilitary.h"
#include "CUnitTable.h"
#include "CCataloguer.h"

class AIClasses;
class CUnit;

// NOTE: actually it is a simple StrategyManager
class CIntel {

public:
	typedef std::map<MilitaryGroupBehaviour, std::vector<CategoryMatcher> > TargetCategoryMap;

	CIntel(AIClasses *ai);

	bool strategyTechUp;

	CCataloguer enemies;
		// all enemies sorted by requested categories
	std::multimap<float, unitCategory> roulette; // <weight, unit_cat>
		// containts counter-enemy unit categories sorted by weight
	std::list<unitCategory> allowedFactories;
		// contains allowed factories for current map, and also preferable
		// order of their appearance
	TargetCategoryMap targets;
		// contains lists of targets per each military group behaviour

	void update(int frame);
	void init();
	bool enemyInbound();
	float3 getEnemyVector();
	unsigned int getEnemyCount(unitCategory c) { return enemyCounter[c]; }
	void onEnemyCreated(int enemy);
	void onEnemyDestroyed(int enemy, int attacker);

protected:
    AIClasses *ai;

private:
	bool initialized;

	unsigned int totalEnemyCount;
		// total number of enemy mobile military units
	unsigned int totalCounterCount;
		// total number of potential counter-enemy units
	float3 enemyvector;
		// general direction towards enemy
	std::map<unitCategory, unsigned int, UnitCategoryCompare> enemyCounter;
		// counters for enemy mobile military units per category
	std::map<unitCategory, unsigned int, UnitCategoryCompare> counterCounter;
		// counters for counter-enemy units per category
	std::vector<unitCategory> selector;
		// list of unit categories which are tracked by enemy counters

	/* Reset enemy unit counters */
	void resetCounters();
	/* Count enemy units */
	void updateCounters(unitCategory c);
	/* Get unit category counterpart (can be implemented via map) */
	unitCategory counter(unitCategory ecats);

	void updateRoulette();

	void updateEnemyVector();
};

#endif
