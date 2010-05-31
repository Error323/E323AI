#ifndef INTEL_H
#define INTEL_H

#include <vector>
#include <list>
#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"
#include "CMilitary.h"

class AIClasses;
class CUnit;

class CIntel {
	public:
		CIntel(AIClasses *ai);
		~CIntel() {};

		void update(int frame);
		void init();
		bool enemyInbound();
		float3 getEnemyVector();

		bool strategyTechUp;
		// TODO: replace this shit below with universal cataloguer
		std::vector<int> factories;
		std::vector<int> attackers;
		std::vector<int> mobileBuilders;
		std::vector<int> metalMakers;
		std::vector<int> energyMakers;
		std::vector<int> navalUnits;
		std::vector<int> underwaterUnits;
		std::vector<int> restUnarmedUnits;
		std::vector<int> rest;
		std::vector<int> defenseGround;
		std::vector<int> defenseAntiAir;
		std::vector<int> commanders;

		std::multimap<float,unitCategory> roulette;

		std::list<unitCategory> allowedFactories;

		std::map<MilitaryGroupBehaviour, std::vector<std::vector<int>* > > targets;

	private:
		AIClasses *ai;

		bool initialized;
		int *units;
		std::map<unitCategory,unsigned> counts;
		std::vector<unitCategory> selector;
		unsigned totalCount;
		float3 enemyvector;

		/* Reset enemy unit counters */
		void resetCounters();
		/* Count enemy units */
		void updateCounts(unsigned c);
		/* Get unit category counterpart (can be implemented via map) */
		unitCategory counter(unitCategory c);
};

#endif
