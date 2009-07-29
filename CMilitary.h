#ifndef MILITARY_H
#define MILITARY_H

#include <map>
#include <vector>
#include <stack>

#include "ARegistrar.h"
#include "CGroup.h"
#include "CE323AI.h"

class CMilitary: public ARegistrar {
	public:
		CMilitary(AIClasses *ai): ARegistrar(200);
		~CMilitary(){};

		/* factory->group->unit <factory, <group, CGroup> > */
		std::map<int, std::map<int, CGroup*> > groups;

		/* factory->unit->group <factory, <unit, group> > */
		std::map<int, std::map<int, int> > units;

		/* Overload */
		void remove(ARegistrar &obj);

		/* Returns a fresh CGroup instance */
		CUnit* requestGroup(groupType type);

		/* update callin */
		void update(int frame);

		/* add a unit to a certain group */
		void addToGroup(int factory, int unit, groupType type);

		/* remove a unit from its group, erase group when empty and not 
		 * current
		 */
		void removeFromGroup(int factory, int unit);

		/* initializes groups[factory] subgroups */
		void initSubGroups(int factory);

	private:
		AIClasses *ai;

		/* The unit container */
		std::vector<CGroup> ingameGroups;

		/* The <unitid, vectoridx> table */
		std::map<int, int>  lookup;

		/* The free slots (CUnit instances that are zombie-ish) */
		std::stack<int>     free;

		/* the current group per factory */
		std::map<int, int> attackGroup;

		/* the current scout per factory */
		std::map<int, int> scoutGroup;

		/* Instantiates a new group */
		void createNewGroup(groupType type, int factory);

		/* Scout and annoy >:) */
		int selectHarrasTarget(CGroup &G);

		/* All targets in a certain order */
		int selectAttackTarget(CGroup &G);

		/* Subfunction for select*Target */
		int selectTarget(float3 &ourPos, std::vector<int> &targets,
						 std::vector<int> &occupied);

		/* Group size (while respecting the enemy strength) */
		int groupSize;
		/* Minimal group size */
		int minGroupSize;
		/* Maximal group size */
		int maxGroupSize;

		/* Request a random unit for building */
		unsigned randomUnit();

		char buf[1024];
};

#endif
