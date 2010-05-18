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

		bool switchDebugMode();

	private:
		AIClasses *ai;

		/* Current group per factory <factory, CGroup*> */
		std::map<int, CGroup*> assemblingGroups;

		/* The ingame scout groups */
		std::map<int, CGroup*> activeScoutGroups;
		/* The ingame attack groups */
		std::map<int, CGroup*> activeAttackGroups;
		/* The ingame attack groups */
		std::map<int, CGroup*> activeBomberGroups;

		std::map<MilitaryGroupBehaviour, std::map<int, CGroup*>* > groups;

		/* Mergable groups */
		std::map<int,CGroup*> mergeGroups;

		bool drawTasks;

		/* Request a unit for building using a roulette wheel system */
		unsigned int requestUnit(unsigned int basecat);

		bool isAssemblingGroup(CGroup *group);

		void visualizeTasks(CGroup*);
};

#endif
