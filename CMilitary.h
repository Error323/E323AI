#ifndef MILITARY_H
#define MILITARY_H

#include <map>
#include <vector>
#include <stack>

#include "ARegistrar.h"
#include "headers/HEngine.h"
#include "headers/Defines.h"

class CUnit;
class CGroup;
class AIClasses;

enum groupType{SCOUT, ENGAGE};

class CMilitary: public ARegistrar {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary();

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a unit, place it in the correct group */
		void addUnit(CUnit &unit);

		/* Returns a fresh CGroup instance */
		CGroup* requestGroup(groupType type);

		/* update callin */
		void update(int groupsize);

		int idleScoutGroupsNum();

	private:
		AIClasses *ai;

		void prepareTargets(std::vector<int> &all, std::vector<int> &harass);

		/* Current group per factory <factory, CGroup*> */
		std::map<int, CGroup*> assemblingGroups;

		/* The ingame scout groups */
		std::map<int, CGroup*> activeScoutGroups;

		/* The ingame attack groups */
		std::map<int, CGroup*> activeAttackGroups;

		/* Occupied targets */
		std::vector<int> occupiedTargets;

		/* Mergable groups */
		std::map<int,CGroup*> mergeScouts, mergeGroups;

		/* Select a target */
		int selectTarget(float3 &ourPos, float radius, bool scout, std::vector<int> &targets);

		void filterOccupiedTargets(std::vector<int> &source, std::vector<int> &dest);

		/* Request a unit for building using a roulette wheel system */
		unsigned requestUnit(unsigned int basecat);

		char buf[1024];
};

#endif
