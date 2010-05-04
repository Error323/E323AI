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

enum MilitaryGroupBehaviour {
	SCOUT,
	ENGAGE,
	BOMBER
};

class CMilitary: public ARegistrar {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary();

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a unit, place it in the correct group */
		void addUnit(CUnit &unit);

		/* Returns a fresh CGroup instance */
		CGroup* requestGroup(MilitaryGroupBehaviour type);

		/* update callin */
		void update(int groupsize);

		int idleScoutGroupsNum();

	private:
		AIClasses *ai;

		void prepareTargets(std::vector<int> &all, std::vector<int> &harass, std::vector<int> &defense);

		/* Current group per factory <factory, CGroup*> */
		std::map<int, CGroup*> assemblingGroups;

		/* The ingame scout groups */
		std::map<int, CGroup*> activeScoutGroups;

		/* The ingame attack groups */
		std::map<int, CGroup*> activeAttackGroups;

		/* The ingame attack groups */
		std::map<int, CGroup*> activeBomberGroups;

		/* Occupied targets */
		std::vector<int> occupiedTargets;

		/* Mergable groups */
		std::map<int,CGroup*> mergeGroups;

		/* Select a target */
		int selectTarget(CGroup &group, float radius, std::vector<int> &targets);

		void filterOccupiedTargets(std::vector<int> &source, std::vector<int> &dest);

		/* Request a unit for building using a roulette wheel system */
		unsigned requestUnit(unsigned int basecat);

		bool isAssemblingGroup(CGroup *group);

		char buf[1024];
};

#endif
