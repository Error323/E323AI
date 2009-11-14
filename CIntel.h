#ifndef INTEL_H
#define INTEL_H

#include <vector>
#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"

class AIClasses;
class CUnit;

class CIntel {
	public:
		CIntel(AIClasses *ai);
		~CIntel(){};

		void update(int frame);
		void init();
		bool enemyInbound();
		float3 getEnemyVector();

		/* Get the primary unit type, veh, kbot etc */
		unitCategory getUnitType();

		std::vector<int> factories;
		std::vector<int> attackers;
		std::vector<int> mobileBuilders;
		std::vector<int> metalMakers;
		std::vector<int> energyMakers;
		std::vector<int> rest;

		std::multimap<float,unitCategory> roulette;

		int numUnits;


	private:
		AIClasses *ai;

		int *units;
		std::map<unitCategory,unsigned> counts;
		std::vector<unitCategory> selector;
		unsigned totalCount;
		float3 enemyvector;
		unitCategory primaryType;

		/* Reset enemy unit counters */
		void resetCounters();

		/* Count enemy units */
		void updateCounts(unsigned c);

		unitCategory counter(unitCategory c);

};

#endif
