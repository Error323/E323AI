#ifndef MILITARY_H
#define MILITARY_H

#include <map>
#include <vector>
#include <stack>

#include "ARegistrar.h"
#include "CGroup.h"
#include "CE323AI.h"

enum groupType{SCOUT, ENGAGE};

class CMilitary: public ARegistrar {
	public:
		CMilitary(AIClasses *ai);
		~CMilitary(){};

		/* Overload */
		void remove(ARegistrar &group);

		/* Add a unit, place it in the correct group */
		void addUnit(CUnit &unit);

		/* Returns a fresh CGroup instance */
		CUnit* requestGroup(groupType type);

		/* update callin */
		void update(int frame);

	private:
		AIClasses *ai;

		/* Current group per factory <factory, CGroup*> */
		std::map<int, CGroup*> currentGroups;

		/* The group container */
		std::vector<CGroup> groups;

		/* The <unitid, vectoridx> table */
		std::map<int, int>  lookup;

		/* The free slots (CUnit instances that are zombie-ish) */
		std::stack<int>     free;

		/* The ingame scout groups */
		std::map<int, CGroup*> activeScoutGroups;

		/* The ingame attack groups */
		std::map<int, CGroup*> activeAttackGroups;

		/* Occupied targets */
		std::vector<int> occupiedTargets;

		/* Scout and annoy >:) */
		int selectHarrasTarget(CGroup &group);

		/* All targets in a certain order */
		int selectAttackTarget(CGroup &group);

		/* Subfunction for select*Target */
		int selectTarget(float3 &ourPos, std::vector<int> &targets,
						 std::vector<int> &occupied);

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
